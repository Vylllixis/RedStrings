#include "engine/anim_clip.h"

#include <SDL3/SDL.h>
#include <cgltf.h>
#include <math.h>
#include <string.h>

#include "engine/asset_format.h"
#include "engine/files.h"
#include "engine/log.h"
#include "engine/skeleton.h"

#define QUAT_TOLERANCE 2e-3F /* |q|² - 1 */
#define MAX_PATH_BYTES 255U
#define NO_CHANNEL UINT32_MAX

typedef struct DiskInfo {
    float duration;
    uint32_t channel_count;
    uint32_t skeleton_path_offset;
    uint32_t reserved;
} DiskInfo;

_Static_assert(sizeof(DiskInfo) == 16, "INFO : 16 octets");
_Static_assert(sizeof(EngAnimChannel) == 16, "CHAN : 16 octets par canal");

uint32_t eng_anim_path_components(uint32_t path)
{
    switch (path) {
    case ENG_ANIM_TRANSLATION:
    case ENG_ANIM_SCALE:
        return 3;
    case ENG_ANIM_ROTATION:
        return 4;
    default:
        return 0;
    }
}

static bool finite_n(const float *v, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(v[i])) {
            return false;
        }
    }
    return true;
}

/* ── Validation (partagée par l'encodeur, le décodeur et l'import) ── */

static const char *invalid_reason(const EngAnimClip *clip)
{
    if (clip == NULL) {
        return "clip vide";
    }
    if (!isfinite(clip->duration) || clip->duration < 0.0F) {
        return "durée négative ou non finie";
    }
    if (clip->channel_count < 1 || clip->channel_count > ENG_ANIM_MAX_CHANNELS ||
        clip->channels == NULL || clip->value_first == NULL) {
        return "nombre de canaux hors de [1 ; 768]";
    }
    if (clip->skeleton_path == NULL || clip->skeleton_path[0] == '\0' ||
        SDL_strnlen(clip->skeleton_path, MAX_PATH_BYTES + 1U) > MAX_PATH_BYTES) {
        return "chemin du squelette absent ou trop long";
    }
    uint64_t keys = 0;
    uint64_t floats = 0;
    for (uint32_t c = 0; c < clip->channel_count; c++) {
        const EngAnimChannel *ch = &clip->channels[c];
        const uint32_t comps = eng_anim_path_components(ch->path);
        if (comps == 0) {
            return "propriété inconnue";
        }
        if (ch->joint >= 256U) {
            return "indice d'articulation >= 256";
        }
        if (c > 0) {
            const EngAnimChannel *prev = &clip->channels[c - 1U];
            if (ch->joint < prev->joint || (ch->joint == prev->joint && ch->path <= prev->path)) {
                return "canaux non rangés ou en double";
            }
        }
        if (ch->key_count < 1 || ch->key_count > ENG_ANIM_MAX_KEYS) {
            return "nombre de clés d'un canal hors de [1 ; 1048576]";
        }
        if (ch->first_key != keys || clip->value_first[c] != floats) {
            return "indices de première clé incohérents";
        }
        keys += ch->key_count;
        floats += (uint64_t)ch->key_count * comps;
        if (keys > ENG_ANIM_MAX_TOTAL_KEYS) {
            return "trop de clés dans le clip";
        }
    }
    if (clip->key_total != keys || clip->times == NULL || clip->values == NULL) {
        return "nombre total de clés incohérent";
    }
    for (uint32_t c = 0; c < clip->channel_count; c++) {
        const EngAnimChannel *ch = &clip->channels[c];
        const uint32_t comps = eng_anim_path_components(ch->path);
        const float *times = &clip->times[ch->first_key];
        const float *values = &clip->values[clip->value_first[c]];
        for (uint32_t k = 0; k < ch->key_count; k++) {
            if (!isfinite(times[k]) || times[k] < 0.0F || times[k] > clip->duration ||
                (k > 0 && !(times[k] > times[k - 1U]))) {
                return "instants non finis, négatifs, > durée ou non strictement croissants";
            }
            const float *v = &values[(size_t)k * comps];
            if (!finite_n(v, comps)) {
                return "valeur non finie";
            }
            if (ch->path == ENG_ANIM_ROTATION) {
                const float len2 = (v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]) + (v[3] * v[3]);
                if (fabsf(len2 - 1.0F) > QUAT_TOLERANCE) {
                    return "quaternion non unitaire";
                }
            }
        }
    }
    return NULL;
}

/* ── Échantillonnage ── */

static void slerp(const float *a, const float *b, float f, float *out)
{
    float dot = (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]) + (a[3] * b[3]);
    float sign = 1.0F;
    if (dot < 0.0F) { /* plus court chemin */
        sign = -1.0F;
        dot = -dot;
    }
    float wa = 1.0F - f;
    float wb = f;
    if (dot < 0.9995F) {
        const float theta = acosf(dot);
        const float inv = 1.0F / sinf(theta);
        wa = sinf((1.0F - f) * theta) * inv;
        wb = sinf(f * theta) * inv;
    }
    wb *= sign;
    float len2 = 0.0F;
    for (int i = 0; i < 4; i++) {
        out[i] = (wa * a[i]) + (wb * b[i]);
        len2 += out[i] * out[i];
    }
    const float inv_len = 1.0F / sqrtf(len2);
    for (int i = 0; i < 4; i++) {
        out[i] *= inv_len;
    }
}

static void sample_track(const float *times, const float *values, uint32_t count, uint32_t path,
                         float time, float out[4])
{
    const uint32_t comps = eng_anim_path_components(path);
    if (count == 1 || time <= times[0]) {
        memcpy(out, values, comps * sizeof(float));
        return;
    }
    if (time >= times[count - 1U]) {
        memcpy(out, &values[(size_t)(count - 1U) * comps], comps * sizeof(float));
        return;
    }
    uint32_t lo = 0; /* invariant : times[lo] <= time < times[hi] */
    uint32_t hi = count - 1U;
    while (hi - lo > 1U) {
        const uint32_t mid = lo + ((hi - lo) / 2U);
        if (times[mid] <= time) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    const float f = (time - times[lo]) / (times[hi] - times[lo]);
    const float *a = &values[(size_t)lo * comps];
    const float *b = &values[(size_t)hi * comps];
    if (path == ENG_ANIM_ROTATION) {
        slerp(a, b, f, out);
    } else {
        for (uint32_t i = 0; i < comps; i++) {
            out[i] = a[i] + ((b[i] - a[i]) * f);
        }
    }
}

uint32_t eng_anim_find_channel(const EngAnimClip *clip, uint32_t joint, uint32_t path)
{
    if (clip == NULL || clip->channels == NULL) {
        return NO_CHANNEL;
    }
    uint32_t lo = 0;
    uint32_t hi = clip->channel_count;
    while (lo < hi) {
        const uint32_t mid = lo + ((hi - lo) / 2U);
        const EngAnimChannel *c = &clip->channels[mid];
        if (c->joint < joint || (c->joint == joint && c->path < path)) {
            lo = mid + 1U;
        } else if (c->joint == joint && c->path == path) {
            return mid;
        } else {
            hi = mid;
        }
    }
    return NO_CHANNEL;
}

uint32_t eng_anim_sample(const EngAnimClip *clip, uint32_t channel, float time, float out[4])
{
    if (clip == NULL || out == NULL || channel >= clip->channel_count || !isfinite(time)) {
        return 0;
    }
    const EngAnimChannel *ch = &clip->channels[channel];
    sample_track(&clip->times[ch->first_key], &clip->values[clip->value_first[channel]],
                 ch->key_count, ch->path, time, out);
    return eng_anim_path_components(ch->path);
}

/* ── Encodage ── */

bool eng_anim_encode(EngArena *arena, const EngAnimClip *clip, const void **out, size_t *out_size)
{
    if (out != NULL) {
        *out = NULL;
    }
    if (out_size != NULL) {
        *out_size = 0;
    }
    const char *why = invalid_reason(clip);
    if (arena == NULL || out == NULL || out_size == NULL || why != NULL) {
        ENG_LOG_ERROR("clip : encodage refusé : %s", why != NULL ? why : "argument NULL");
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    const size_t path_size = strlen(clip->skeleton_path) + 1U;
    uint64_t floats = 0;
    for (uint32_t c = 0; c < clip->channel_count; c++) {
        floats += (uint64_t)clip->channels[c].key_count *
                  eng_anim_path_components(clip->channels[c].path);
    }
    DiskInfo *info = ENG_ARENA_NEW(arena, DiskInfo);
    char *strings = eng_arena_alloc(arena, path_size, 1);
    if (info == NULL || strings == NULL) {
        ENG_LOG_ERROR("clip : mémoire insuffisante pour l'encodage");
        eng_arena_reset_to(arena, mark);
        return false;
    }
    *info = (DiskInfo){.duration = clip->duration, .channel_count = clip->channel_count};
    memcpy(strings, clip->skeleton_path, path_size);
    const EngAssetSectionDesc sections[5] = {
        {ENG_ASSET_TAG_INFO, info, sizeof(*info), 1},
        {ENG_ASSET_TAG_CHAN, clip->channels,
         (uint32_t)(sizeof(EngAnimChannel) * clip->channel_count), clip->channel_count},
        {ENG_ASSET_TAG_TIME, clip->times, (uint32_t)(sizeof(float) * clip->key_total),
         clip->key_total},
        {ENG_ASSET_TAG_VALS, clip->values, (uint32_t)(sizeof(float) * floats), clip->key_total},
        {ENG_ASSET_TAG_STRS, strings, (uint32_t)path_size, (uint32_t)path_size},
    };
    const size_t total = eng_asset_file_size(sections, 5);
    void *file = total > 0 ? eng_arena_alloc(arena, total, ENG_ASSET_ALIGN) : NULL;
    size_t written = 0;
    if (file == NULL || !eng_asset_build(ENG_ASSET_MAGIC_ANIM, ENG_ASSET_VERSION_ANIM, sections, 5,
                                         file, total, &written)) {
        ENG_LOG_ERROR("clip : écriture du conteneur impossible");
        eng_arena_reset_to(arena, mark);
        return false;
    }
    *out = file;
    *out_size = written;
    return true;
}

/* ── Décodage ── */

static bool refuse(const char *name, const char *why)
{
    ENG_LOG_ERROR("%s : clip refusé : %s", name != NULL ? name : "(mémoire)", why);
    return false;
}

bool eng_anim_decode(EngArena *arena, const void *data, size_t size, const char *name,
                     EngAnimClip *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngAnimClip){0};
    if (arena == NULL) {
        return refuse(name, "arène NULL");
    }
    EngAssetView view;
    if (eng_asset_open(data, size, ENG_ASSET_MAGIC_ANIM, ENG_ASSET_VERSION_ANIM, name, &view) !=
        ENG_ASSET_OK) {
        return false; /* déjà journalisé */
    }
    EngAssetSection info;
    EngAssetSection chan;
    EngAssetSection time;
    EngAssetSection vals;
    EngAssetSection strs;
    if (!eng_asset_find_section(&view, ENG_ASSET_TAG_INFO, &info) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_CHAN, &chan) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_TIME, &time) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_VALS, &vals) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_STRS, &strs)) {
        return refuse(name, "section INFO, CHAN, TIME, VALS ou STRS absente");
    }
    if (info.size != sizeof(DiskInfo)) {
        return refuse(name, "taille de INFO inattendue");
    }
    DiskInfo di;
    memcpy(&di, info.data, sizeof(di));
    const uint32_t n = di.channel_count;
    if (n < 1 || n > ENG_ANIM_MAX_CHANNELS || di.reserved != 0) {
        return refuse(name, "nombre de canaux hors de [1 ; 768] ou champ réservé non nul");
    }
    if (chan.size != sizeof(EngAnimChannel) * n || chan.count != n) {
        return refuse(name, "taille ou nombre de CHAN incohérent avec INFO");
    }
    if (strs.size == 0 || strs.data[strs.size - 1U] != '\0' ||
        di.skeleton_path_offset >= strs.size) {
        return refuse(name, "STRS vide, non terminée par zéro, ou chemin hors de la table");
    }
    /* totaux d'après les canaux (validés plus bas) : en 64 bits, sans débordement */
    uint64_t keys = 0;
    uint64_t floats = 0;
    for (uint32_t c = 0; c < n; c++) {
        EngAnimChannel ch;
        memcpy(&ch, chan.data + ((size_t)c * sizeof(ch)), sizeof(ch));
        keys += ch.key_count;
        floats += (uint64_t)ch.key_count * eng_anim_path_components(ch.path);
        if (keys > ENG_ANIM_MAX_TOTAL_KEYS) {
            return refuse(name, "trop de clés dans le clip");
        }
    }
    if (time.size != sizeof(float) * keys || time.count != keys ||
        vals.size != sizeof(float) * floats || vals.count != keys) {
        return refuse(name, "taille ou nombre de TIME / VALS incohérent avec CHAN");
    }

    const EngArenaMark mark = eng_arena_mark(arena);
    EngAnimChannel *channels = ENG_ARENA_NEW_ARRAY(arena, EngAnimChannel, n);
    uint32_t *value_first = ENG_ARENA_NEW_ARRAY(arena, uint32_t, n);
    float *times = eng_arena_alloc(arena, time.size, 16);
    float *values = eng_arena_alloc(arena, vals.size, 16);
    char *path = eng_arena_alloc(arena, strs.size - di.skeleton_path_offset, 1);
    if (channels == NULL || value_first == NULL || times == NULL || values == NULL ||
        path == NULL) {
        eng_arena_reset_to(arena, mark);
        return refuse(name, "mémoire insuffisante");
    }
    memcpy(channels, chan.data, chan.size);
    memcpy(times, time.data, time.size);
    memcpy(values, vals.data, vals.size);
    memcpy(path, strs.data + di.skeleton_path_offset, strs.size - di.skeleton_path_offset);
    uint32_t first = 0;
    for (uint32_t c = 0; c < n; c++) {
        value_first[c] = first;
        first += channels[c].key_count * eng_anim_path_components(channels[c].path);
    }
    const EngAnimClip candidate = {.duration = di.duration,
                                   .channel_count = n,
                                   .channels = channels,
                                   .key_total = (uint32_t)keys,
                                   .times = times,
                                   .values = values,
                                   .value_first = value_first,
                                   .skeleton_path = path};
    const char *why = invalid_reason(&candidate);
    if (why != NULL) {
        eng_arena_reset_to(arena, mark);
        return refuse(name, why);
    }
    *out = candidate;
    return true;
}

bool eng_anim_load(EngArena *arena, EngArena *scratch, const char *path, EngAnimClip *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngAnimClip){0};
    if (arena == NULL || scratch == NULL || path == NULL) {
        return refuse(path, "argument NULL");
    }
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData file;
    bool ok = false;
    if (eng_file_read_all(scratch, path, &file)) { /* erreur déjà journalisée sinon */
        ok = eng_anim_decode(arena, file.data, file.size, path, out);
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}

/* ── Import glTF ── */

static void *gltf_alloc(void *user, cgltf_size size)
{
    (void)user;
    return SDL_malloc(size);
}

static void gltf_release(void *user, void *ptr)
{
    (void)user;
    SDL_free(ptr);
}

static cgltf_data *gltf_open(const void *data, size_t size, const char *base_path,
                             bool load_buffers)
{
    if (data == NULL || size == 0) {
        ENG_LOG_ERROR("glTF : données vides");
        return NULL;
    }
    cgltf_options options = {0};
    options.memory.alloc_func = gltf_alloc;
    options.memory.free_func = gltf_release;
    cgltf_data *gltf = NULL;
    if (cgltf_parse(&options, data, size, &gltf) != cgltf_result_success) {
        ENG_LOG_ERROR("glTF : analyse impossible");
        return NULL;
    }
    if (load_buffers && cgltf_load_buffers(&options, gltf, base_path) != cgltf_result_success) {
        ENG_LOG_ERROR("glTF : tampons illisibles");
        cgltf_free(gltf);
        return NULL;
    }
    if (cgltf_validate(gltf) != cgltf_result_success) {
        ENG_LOG_ERROR("glTF : fichier invalide (validation)");
        cgltf_free(gltf);
        return NULL;
    }
    return gltf;
}

uint32_t eng_anim_gltf_clip_count(const void *data, size_t size)
{
    cgltf_data *gltf = gltf_open(data, size, NULL, false);
    if (gltf == NULL) {
        return 0;
    }
    const uint32_t count = (uint32_t)gltf->animations_count;
    cgltf_free(gltf);
    return count;
}

uint32_t eng_anim_gltf_find(const void *data, size_t size, const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return UINT32_MAX;
    }
    cgltf_data *gltf = gltf_open(data, size, NULL, false);
    if (gltf == NULL) {
        return UINT32_MAX;
    }
    uint32_t found = UINT32_MAX;
    for (cgltf_size i = 0; i < gltf->animations_count; i++) {
        const char *n = gltf->animations[i].name;
        if (n != NULL && strcmp(n, name) == 0) {
            found = (uint32_t)i;
            break;
        }
    }
    cgltf_free(gltf);
    return found;
}

/* Une série de clés en cours de construction (mémoire de travail). */
typedef struct Track {
    uint32_t joint; /* NO_CHANNEL : supprimée (remplacée par un recalcul) */
    uint32_t path;
    uint32_t count;
    float *times;
    float *values;
} Track;

typedef struct ImportJoint {
    const cgltf_node *node;
    mat4 folded;     /* ancêtres non articulations repliés par l'import du squelette */
    bool has_folded; /* folded != identité */
} ImportJoint;

static bool is_identity(mat4 m)
{
    mat4 id;
    glm_mat4_identity(id);
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            if (fabsf(m[c][r] - id[c][r]) > 1e-6F) {
                return false;
            }
        }
    }
    return true;
}

/* Même parcours que l'import du squelette : produit des ancêtres non articulations. */
static void folded_ancestors(const cgltf_data *gltf, const cgltf_node *node,
                             const int32_t *node_skin, mat4 out)
{
    glm_mat4_identity(out);
    for (const cgltf_node *p = node->parent; p != NULL; p = p->parent) {
        if (node_skin[p - gltf->nodes] >= 0) {
            return;
        }
        mat4 l;
        mat4 next;
        cgltf_node_transform_local(p, (cgltf_float *)l);
        glm_mat4_mul(l, out, next);
        glm_mat4_copy(next, out);
    }
}

static bool read_track(const cgltf_animation_channel *channel, EngArena *scratch, uint32_t path,
                       uint32_t joint, Track *out)
{
    const cgltf_animation_sampler *sampler = channel->sampler;
    if (sampler->interpolation != cgltf_interpolation_type_linear) {
        ENG_LOG_ERROR("clip : interpolation %s non prise en charge (version 1 : LINEAR "
                      "seulement) ; réexporter l'animation en LINEAR",
                      sampler->interpolation == cgltf_interpolation_type_step ? "STEP"
                                                                              : "CUBICSPLINE");
        return false;
    }
    const uint32_t comps = eng_anim_path_components(path);
    const cgltf_size count = sampler->input->count;
    if (count < 1 || count > ENG_ANIM_MAX_KEYS || sampler->output->count != count ||
        sampler->input->type != cgltf_type_scalar ||
        sampler->output->type != (comps == 4 ? cgltf_type_vec4 : cgltf_type_vec3)) {
        ENG_LOG_ERROR("clip : canal mal formé (nombre de clés ou types d'accesseurs)");
        return false;
    }
    float *times = ENG_ARENA_NEW_ARRAY(scratch, float, count);
    float *values = ENG_ARENA_NEW_ARRAY(scratch, float, count *comps);
    if (times == NULL || values == NULL) {
        ENG_LOG_ERROR("clip : mémoire insuffisante pour un canal");
        return false;
    }
    if (cgltf_accessor_unpack_floats(sampler->input, times, count) != count ||
        cgltf_accessor_unpack_floats(sampler->output, values, count * comps) != count * comps) {
        ENG_LOG_ERROR("clip : accesseur illisible");
        return false;
    }
    for (cgltf_size k = 0; k < count; k++) {
        if (!isfinite(times[k]) || times[k] < 0.0F || (k > 0 && !(times[k] > times[k - 1U]))) {
            ENG_LOG_ERROR("clip : instants négatifs, non finis ou non strictement croissants");
            return false;
        }
    }
    if (!finite_n(values, count * comps)) {
        ENG_LOG_ERROR("clip : valeur non finie");
        return false;
    }
    if (path == ENG_ANIM_ROTATION) {
        for (cgltf_size k = 0; k < count; k++) {
            float *q = &values[k * 4U];
            const float len = sqrtf((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]));
            if (!(len > 1e-6F)) {
                ENG_LOG_ERROR("clip : quaternion nul");
                return false;
            }
            for (int i = 0; i < 4; i++) {
                q[i] /= len;
            }
        }
    }
    *out = (Track){
        .joint = joint, .path = path, .count = (uint32_t)count, .times = times, .values = values};
    return true;
}

/* Recalcule les pistes d'une articulation dont les ancêtres ont été repliés : échantillonne à
 * l'union des instants, compose avec `folded`, redécompose. Ajoute 3 pistes à `tracks`. */
static bool rebake_joint(const ImportJoint *ij, uint32_t joint, Track *tracks,
                         uint32_t *track_count, uint32_t track_capacity, EngArena *scratch)
{
    const Track *own[ENG_ANIM_PATH_COUNT] = {NULL, NULL, NULL};
    size_t total = 0;
    for (uint32_t t = 0; t < *track_count; t++) {
        if (tracks[t].joint == joint) {
            own[tracks[t].path] = &tracks[t];
            total += tracks[t].count;
        }
    }
    /* repos du nœud SEUL (sans ancêtres) pour les propriétés sans canal */
    mat4 local;
    cgltf_node_transform_local(ij->node, (cgltf_float *)local);
    float rest[ENG_ANIM_PATH_COUNT][4];
    if (!eng_skeleton_decompose_trs(local, rest[ENG_ANIM_TRANSLATION], rest[ENG_ANIM_ROTATION],
                                    rest[ENG_ANIM_SCALE])) {
        ENG_LOG_ERROR("clip : pose de repos d'une articulation non décomposable");
        return false;
    }
    /* union triée des instants (fusion à trois pointeurs) */
    float *union_times = ENG_ARENA_NEW_ARRAY(scratch, float, total);
    if (union_times == NULL || *track_count + ENG_ANIM_PATH_COUNT > track_capacity) {
        ENG_LOG_ERROR("clip : mémoire insuffisante pour le recalcul");
        return false;
    }
    uint32_t pos[ENG_ANIM_PATH_COUNT] = {0, 0, 0};
    uint32_t n = 0;
    for (;;) {
        bool any = false;
        float next = 0.0F;
        for (uint32_t p = 0; p < ENG_ANIM_PATH_COUNT; p++) {
            if (own[p] != NULL && pos[p] < own[p]->count) {
                const float t = own[p]->times[pos[p]];
                if (!any || t < next) {
                    next = t;
                }
                any = true;
            }
        }
        if (!any) {
            break;
        }
        union_times[n++] = next;
        for (uint32_t p = 0; p < ENG_ANIM_PATH_COUNT; p++) {
            if (own[p] != NULL && pos[p] < own[p]->count && own[p]->times[pos[p]] == next) {
                pos[p]++;
            }
        }
    }
    if (n < 1 || n > ENG_ANIM_MAX_KEYS) {
        ENG_LOG_ERROR("clip : trop de clés après recalcul");
        return false;
    }
    Track *out[ENG_ANIM_PATH_COUNT];
    for (uint32_t p = 0; p < ENG_ANIM_PATH_COUNT; p++) {
        Track *t = &tracks[*track_count + p];
        t->joint = joint;
        t->path = p;
        t->count = n;
        t->times = union_times;
        t->values = ENG_ARENA_NEW_ARRAY(scratch, float, (size_t)n *eng_anim_path_components(p));
        if (t->values == NULL) {
            ENG_LOG_ERROR("clip : mémoire insuffisante pour le recalcul");
            return false;
        }
        out[p] = t;
    }
    for (uint32_t k = 0; k < n; k++) {
        float tr[4];
        float ro[4];
        float sc[4];
        memcpy(tr, rest[ENG_ANIM_TRANSLATION], sizeof tr);
        memcpy(ro, rest[ENG_ANIM_ROTATION], sizeof ro);
        memcpy(sc, rest[ENG_ANIM_SCALE], sizeof sc);
        if (own[ENG_ANIM_TRANSLATION] != NULL) {
            sample_track(own[0]->times, own[0]->values, own[0]->count, ENG_ANIM_TRANSLATION,
                         union_times[k], tr);
        }
        if (own[ENG_ANIM_ROTATION] != NULL) {
            sample_track(own[1]->times, own[1]->values, own[1]->count, ENG_ANIM_ROTATION,
                         union_times[k], ro);
        }
        if (own[ENG_ANIM_SCALE] != NULL) {
            sample_track(own[2]->times, own[2]->values, own[2]->count, ENG_ANIM_SCALE,
                         union_times[k], sc);
        }
        mat4 t;
        mat4 r;
        mat4 s;
        mat4 rs;
        mat4 trs;
        mat4 world;
        glm_translate_make(t, (vec3){tr[0], tr[1], tr[2]});
        glm_quat_mat4((versor){ro[0], ro[1], ro[2], ro[3]}, r);
        glm_scale_make(s, (vec3){sc[0], sc[1], sc[2]});
        glm_mat4_mul(r, s, rs);
        glm_mat4_mul(t, rs, trs);
        glm_mat4_mul((vec4 *)ij->folded, trs, world);
        float *vt = &out[ENG_ANIM_TRANSLATION]->values[(size_t)k * 3U];
        float *vr = &out[ENG_ANIM_ROTATION]->values[(size_t)k * 4U];
        float *vs = &out[ENG_ANIM_SCALE]->values[(size_t)k * 3U];
        if (!eng_skeleton_decompose_trs(world, vt, vr, vs)) {
            ENG_LOG_ERROR("clip : pose recalculée non décomposable (échelle nulle, négative ou "
                          "miroir)");
            return false;
        }
        if (k > 0 &&
            ((vr[0] * vr[-4]) + (vr[1] * vr[-3]) + (vr[2] * vr[-2]) + (vr[3] * vr[-1])) < 0.0F) {
            for (int i = 0; i < 4; i++) { /* continuité : même hémisphère que la clé précédente */
                vr[i] = -vr[i];
            }
        }
    }
    for (uint32_t t = 0; t < *track_count; t++) {
        if (tracks[t].joint == joint) {
            tracks[t].joint = NO_CHANNEL; /* remplacée */
        }
    }
    *track_count += ENG_ANIM_PATH_COUNT;
    return true;
}

static int track_order(const void *a, const void *b)
{
    const Track *x = a;
    const Track *y = b;
    if (x->joint != y->joint) {
        return x->joint < y->joint ? -1 : 1;
    }
    if (x->path != y->path) {
        return x->path < y->path ? -1 : 1;
    }
    return 0;
}

static uint32_t path_of(cgltf_animation_path_type type)
{
    switch (type) {
    case cgltf_animation_path_type_translation:
        return ENG_ANIM_TRANSLATION;
    case cgltf_animation_path_type_rotation:
        return ENG_ANIM_ROTATION;
    case cgltf_animation_path_type_scale:
        return ENG_ANIM_SCALE;
    default:
        return ENG_ANIM_PATH_COUNT;
    }
}

/* Construit le clip (dans `arena`) à partir de `tracks` rangées sans doublon. */
static bool emit_clip(EngArena *arena, const Track *tracks, uint32_t track_count,
                      const char *skeleton_path, EngAnimClip *out)
{
    uint32_t channels = 0;
    uint64_t keys = 0;
    uint64_t floats = 0;
    for (uint32_t t = 0; t < track_count; t++) {
        if (tracks[t].joint != NO_CHANNEL) {
            channels++;
            keys += tracks[t].count;
            floats += (uint64_t)tracks[t].count * eng_anim_path_components(tracks[t].path);
        }
    }
    if (channels < 1 || channels > ENG_ANIM_MAX_CHANNELS || keys > ENG_ANIM_MAX_TOTAL_KEYS) {
        ENG_LOG_ERROR("clip : aucun canal utilisable, ou trop de canaux / de clés");
        return false;
    }
    EngAnimChannel *ch = ENG_ARENA_NEW_ARRAY(arena, EngAnimChannel, channels);
    uint32_t *value_first = ENG_ARENA_NEW_ARRAY(arena, uint32_t, channels);
    float *times = eng_arena_alloc(arena, sizeof(float) * keys, 16);
    float *values = eng_arena_alloc(arena, sizeof(float) * floats, 16);
    const size_t path_size = strlen(skeleton_path) + 1U;
    char *path = eng_arena_alloc(arena, path_size, 1);
    if (ch == NULL || value_first == NULL || times == NULL || values == NULL || path == NULL) {
        ENG_LOG_ERROR("clip : mémoire insuffisante");
        return false;
    }
    memcpy(path, skeleton_path, path_size);
    uint32_t c = 0;
    uint32_t key = 0;
    uint32_t value = 0;
    float duration = 0.0F;
    for (uint32_t t = 0; t < track_count; t++) {
        if (tracks[t].joint == NO_CHANNEL) {
            continue;
        }
        const Track *tr = &tracks[t];
        const uint32_t comps = eng_anim_path_components(tr->path);
        ch[c] = (EngAnimChannel){
            .joint = tr->joint, .path = tr->path, .key_count = tr->count, .first_key = key};
        value_first[c] = value;
        memcpy(&times[key], tr->times, sizeof(float) * tr->count);
        memcpy(&values[value], tr->values, sizeof(float) * tr->count * comps);
        duration = tr->times[tr->count - 1U] > duration ? tr->times[tr->count - 1U] : duration;
        c++;
        key += tr->count;
        value += tr->count * comps;
    }
    const EngAnimClip clip = {.duration = duration,
                              .channel_count = channels,
                              .channels = ch,
                              .key_total = key,
                              .times = times,
                              .values = values,
                              .value_first = value_first,
                              .skeleton_path = path};
    const char *why = invalid_reason(&clip);
    if (why != NULL) {
        ENG_LOG_ERROR("clip : clip obtenu invalide : %s", why);
        return false;
    }
    *out = clip;
    return true;
}

bool eng_anim_from_gltf(EngArena *arena, EngArena *scratch, const void *data, size_t size,
                        const char *base_path, uint32_t anim_index, uint32_t skin_index,
                        const char *skeleton_path, EngAnimClip *out, EngAnimImportStats *stats)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngAnimClip){0};
    EngAnimImportStats local_stats = {0};
    EngAnimImportStats *st = stats != NULL ? stats : &local_stats;
    *st = (EngAnimImportStats){0};
    if (arena == NULL || scratch == NULL || skeleton_path == NULL || skeleton_path[0] == '\0' ||
        SDL_strnlen(skeleton_path, MAX_PATH_BYTES + 1U) > MAX_PATH_BYTES) {
        return refuse(base_path, "argument NULL ou chemin du squelette absent / trop long");
    }
    cgltf_data *gltf = gltf_open(data, size, base_path, true);
    if (gltf == NULL) {
        return false;
    }
    bool ok = false;
    const EngArenaMark arena_mark = eng_arena_mark(arena);
    const EngArenaMark scratch_mark = eng_arena_mark(scratch);
    do {
        if (anim_index >= gltf->animations_count) {
            ENG_LOG_ERROR("glTF : l'animation %u n'existe pas (%u animation(s))",
                          (unsigned)anim_index, (unsigned)gltf->animations_count);
            break;
        }
        if (skin_index >= gltf->skins_count) {
            ENG_LOG_ERROR("glTF : le skin %u n'existe pas (%u skin(s))", (unsigned)skin_index,
                          (unsigned)gltf->skins_count);
            break;
        }
        const cgltf_animation *anim = &gltf->animations[anim_index];
        const cgltf_skin *skin = &gltf->skins[skin_index];
        const uint32_t joint_total = (uint32_t)skin->joints_count;

        /* Ordre des articulations du squelette : celui de eng_skeleton_from_gltf */
        EngSkeleton skeleton;
        uint32_t *remap = NULL;
        if (!eng_skeleton_from_gltf(arena, scratch, data, size, base_path, skin_index, &skeleton,
                                    &remap)) {
            break;
        }
        uint32_t *remap_copy = ENG_ARENA_NEW_ARRAY(scratch, uint32_t, joint_total);
        if (remap_copy == NULL) {
            ENG_LOG_ERROR("clip : mémoire insuffisante");
            break;
        }
        memcpy(remap_copy, remap, sizeof(uint32_t) * joint_total);
        eng_arena_reset_to(arena, arena_mark); /* le squelette n'est pas conservé */

        const size_t node_count = gltf->nodes_count;
        int32_t *node_skin = ENG_ARENA_NEW_ARRAY(scratch, int32_t, node_count);
        ImportJoint *joints = eng_arena_alloc_array(scratch, sizeof(ImportJoint), joint_total, 16);
        const uint32_t track_capacity = (uint32_t)anim->channels_count + (3U * joint_total) + 3U;
        Track *tracks = eng_arena_alloc_array(scratch, sizeof(Track), track_capacity, 8);
        if (node_skin == NULL || joints == NULL || tracks == NULL) {
            ENG_LOG_ERROR("clip : mémoire insuffisante");
            break;
        }
        for (size_t i = 0; i < node_count; i++) {
            node_skin[i] = -1;
        }
        for (uint32_t j = 0; j < joint_total; j++) {
            node_skin[skin->joints[j] - gltf->nodes] = (int32_t)j;
        }
        for (uint32_t j = 0; j < joint_total; j++) {
            ImportJoint *ij = &joints[remap_copy[j]];
            ij->node = skin->joints[j];
            folded_ancestors(gltf, ij->node, node_skin, ij->folded);
            ij->has_folded = !is_identity(ij->folded);
        }

        st->gltf_channels = (uint32_t)anim->channels_count;
        uint32_t track_count = 0;
        bool failed = false;
        for (cgltf_size c = 0; c < anim->channels_count && !failed; c++) {
            const cgltf_animation_channel *channel = &anim->channels[c];
            const uint32_t path = path_of(channel->target_path);
            const int32_t skin_joint =
                channel->target_node != NULL ? node_skin[channel->target_node - gltf->nodes] : -1;
            if (path == ENG_ANIM_PATH_COUNT || skin_joint < 0) {
                st->ignored_channels++;
                continue;
            }
            failed =
                !read_track(channel, scratch, path, remap_copy[skin_joint], &tracks[track_count]);
            track_count += failed ? 0U : 1U;
        }
        if (failed) {
            break;
        }
        /* deux canaux sur la même cible : refusé AVANT tout recalcul (qui les fusionnerait) */
        qsort(tracks, track_count, sizeof(Track), track_order);
        for (uint32_t t = 1; t < track_count; t++) {
            if (tracks[t].joint == tracks[t - 1U].joint && tracks[t].path == tracks[t - 1U].path) {
                ENG_LOG_ERROR("glTF : deux canaux animent la même propriété de la même "
                              "articulation");
                failed = true;
            }
        }
        if (failed) {
            break;
        }
        /* recalcul des articulations à ancêtres repliés qui ont au moins un canal */
        for (uint32_t j = 0; j < joint_total && !failed; j++) {
            if (!joints[j].has_folded) {
                continue;
            }
            bool animated = false;
            for (uint32_t t = 0; t < track_count; t++) {
                animated = animated || tracks[t].joint == j;
            }
            if (animated) {
                failed =
                    !rebake_joint(&joints[j], j, tracks, &track_count, track_capacity, scratch);
                st->rebaked_joints += failed ? 0U : 1U;
            }
        }
        if (failed) {
            break;
        }
        qsort(tracks, track_count, sizeof(Track), track_order);
        if (!emit_clip(arena, tracks, track_count, skeleton_path, out)) {
            break;
        }
        st->kept_channels = out->channel_count;
        if (st->ignored_channels > 0) {
            ENG_LOG_WARN("glTF : %u canal(aux) ignoré(s) (cible hors du skin, ou « weights »)",
                         (unsigned)st->ignored_channels);
        }
        ok = true;
    } while (false);
    eng_arena_reset_to(scratch, scratch_mark);
    if (!ok) {
        eng_arena_reset_to(arena, arena_mark);
        *out = (EngAnimClip){0};
    }
    cgltf_free(gltf);
    return ok;
}
