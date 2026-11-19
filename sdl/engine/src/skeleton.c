#include "engine/skeleton.h"

#include <SDL3/SDL.h>
#include <cgltf.h>
#include <math.h>
#include <string.h>

#include "engine/asset_format.h"
#include "engine/files.h"
#include "engine/log.h"

#define NO_NAME 0xFFFFFFFFU
#define QUAT_TOLERANCE 2e-3F /* |q|² - 1 */

/* Disposition sur disque (docs/asset-formats.md §5). */
typedef struct DiskInfo {
    uint32_t joint_count;
    uint32_t reserved[3];
} DiskInfo;

typedef struct DiskJoint {
    int32_t parent;
    uint32_t name_offset;
    float translation[3];
    float rotation[4];
    float scale[3];
} DiskJoint;

_Static_assert(sizeof(DiskInfo) == 16, "INFO : 16 octets");
_Static_assert(sizeof(DiskJoint) == 48, "JNTS : 48 octets par articulation");

static bool finite_n(const float *v, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(v[i])) {
            return false;
        }
    }
    return true;
}

/* Raison du refus, ou NULL si la table d'articulations est valide. */
static const char *invalid_reason(const EngSkeleton *skeleton)
{
    if (skeleton == NULL || skeleton->joints == NULL) {
        return "squelette vide";
    }
    if (skeleton->joint_count < 1 || skeleton->joint_count > ENG_SKELETON_MAX_JOINTS) {
        return "nombre d'articulations hors de [1 ; 256]";
    }
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        const EngJoint *j = &skeleton->joints[i];
        if (j->parent < -1 || j->parent >= (int32_t)i) {
            return "parent absent ou placé après son enfant";
        }
        if (!finite_n(j->translation, 3) || !finite_n(j->rotation, 4) || !finite_n(j->scale, 3) ||
            !finite_n(j->inverse_bind, 16)) {
            return "valeur non finie";
        }
        const float len2 = (j->rotation[0] * j->rotation[0]) + (j->rotation[1] * j->rotation[1]) +
                           (j->rotation[2] * j->rotation[2]) + (j->rotation[3] * j->rotation[3]);
        if (fabsf(len2 - 1.0F) > QUAT_TOLERANCE) {
            return "quaternion non unitaire";
        }
        if (fabsf(j->scale[0]) < 1e-8F || fabsf(j->scale[1]) < 1e-8F ||
            fabsf(j->scale[2]) < 1e-8F) {
            return "échelle nulle";
        }
        if (j->name != NULL &&
            strnlen(j->name, ENG_SKELETON_MAX_NAME + 1U) > ENG_SKELETON_MAX_NAME) {
            return "nom trop long";
        }
    }
    return NULL;
}

/* ── Encodage ── */

bool eng_skeleton_encode(EngArena *arena, const EngSkeleton *skeleton, const void **out,
                         size_t *out_size)
{
    if (out != NULL) {
        *out = NULL;
    }
    if (out_size != NULL) {
        *out_size = 0;
    }
    const char *why = invalid_reason(skeleton);
    if (arena == NULL || out == NULL || out_size == NULL || why != NULL) {
        ENG_LOG_ERROR("squelette : encodage refusé : %s", why != NULL ? why : "argument NULL");
        return false;
    }
    const uint32_t n = skeleton->joint_count;
    size_t strings_size = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (skeleton->joints[i].name != NULL && skeleton->joints[i].name[0] != '\0') {
            strings_size += strlen(skeleton->joints[i].name) + 1U;
        }
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    DiskInfo *info = ENG_ARENA_NEW(arena, DiskInfo);
    DiskJoint *joints = ENG_ARENA_NEW_ARRAY(arena, DiskJoint, n);
    float *ibms = eng_arena_alloc_array(arena, 16U * sizeof(float), n, 16);
    char *strings = strings_size > 0 ? eng_arena_alloc_zero(arena, strings_size, 1) : NULL;
    if (info == NULL || joints == NULL || ibms == NULL || (strings_size > 0 && strings == NULL)) {
        ENG_LOG_ERROR("squelette : mémoire insuffisante pour l'encodage");
        eng_arena_reset_to(arena, mark);
        return false;
    }
    *info = (DiskInfo){.joint_count = n};
    size_t used = 0;
    for (uint32_t i = 0; i < n; i++) {
        const EngJoint *j = &skeleton->joints[i];
        DiskJoint *d = &joints[i];
        memset(d, 0, sizeof(*d));
        d->parent = j->parent;
        d->name_offset = NO_NAME;
        if (strings != NULL && j->name != NULL && j->name[0] != '\0') {
            const size_t len = strlen(j->name) + 1U;
            memcpy(strings + used, j->name, len);
            d->name_offset = (uint32_t)used;
            used += len;
        }
        memcpy(d->translation, j->translation, sizeof(d->translation));
        memcpy(d->rotation, j->rotation, sizeof(d->rotation));
        memcpy(d->scale, j->scale, sizeof(d->scale));
        memcpy(&ibms[(size_t)i * 16U], j->inverse_bind, sizeof(j->inverse_bind));
    }
    EngAssetSectionDesc sections[4] = {
        {ENG_ASSET_TAG_INFO, info, sizeof(*info), 1},
        {ENG_ASSET_TAG_JNTS, joints, (uint32_t)(sizeof(DiskJoint) * n), n},
        {ENG_ASSET_TAG_IBMS, ibms, (uint32_t)(16U * sizeof(float) * n), n},
        {0, NULL, 0, 0},
    };
    uint32_t section_count = 3;
    if (strings_size > 0) {
        sections[section_count++] = (EngAssetSectionDesc){
            ENG_ASSET_TAG_STRS, strings, (uint32_t)strings_size, (uint32_t)strings_size};
    }
    const size_t total = eng_asset_file_size(sections, section_count);
    void *file = total > 0 ? eng_arena_alloc(arena, total, ENG_ASSET_ALIGN) : NULL;
    size_t written = 0;
    if (file == NULL || !eng_asset_build(ENG_ASSET_MAGIC_SKEL, ENG_ASSET_VERSION_SKEL, sections,
                                         section_count, file, total, &written)) {
        ENG_LOG_ERROR("squelette : écriture du conteneur impossible");
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
    ENG_LOG_ERROR("%s : squelette refusé : %s", name != NULL ? name : "(mémoire)", why);
    return false;
}

bool eng_skeleton_decode(EngArena *arena, const void *data, size_t size, const char *name,
                         EngSkeleton *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngSkeleton){0};
    if (arena == NULL) {
        return refuse(name, "arène NULL");
    }
    EngAssetView view;
    if (eng_asset_open(data, size, ENG_ASSET_MAGIC_SKEL, ENG_ASSET_VERSION_SKEL, name, &view) !=
        ENG_ASSET_OK) {
        return false; /* déjà journalisé */
    }
    EngAssetSection info;
    EngAssetSection jnts;
    EngAssetSection ibms;
    if (!eng_asset_find_section(&view, ENG_ASSET_TAG_INFO, &info) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_JNTS, &jnts) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_IBMS, &ibms)) {
        return refuse(name, "section INFO, JNTS ou IBMS absente");
    }
    if (info.size != sizeof(DiskInfo)) {
        return refuse(name, "taille de INFO inattendue");
    }
    DiskInfo di;
    memcpy(&di, info.data, sizeof(di));
    const uint32_t n = di.joint_count;
    if (n < 1 || n > ENG_SKELETON_MAX_JOINTS) {
        return refuse(name, "nombre d'articulations hors de [1 ; 256]");
    }
    if (di.reserved[0] != 0 || di.reserved[1] != 0 || di.reserved[2] != 0) {
        return refuse(name, "champs réservés de INFO non nuls");
    }
    if (jnts.size != sizeof(DiskJoint) * n || jnts.count != n ||
        ibms.size != 16U * sizeof(float) * n || ibms.count != n) {
        return refuse(name, "taille ou nombre de JNTS / IBMS incohérent avec INFO");
    }
    EngAssetSection strs = {0};
    const bool has_names = eng_asset_find_section(&view, ENG_ASSET_TAG_STRS, &strs);
    if (has_names && (strs.size == 0 || strs.data[strs.size - 1U] != '\0')) {
        return refuse(name, "STRS vide ou non terminée par zéro");
    }

    const EngArenaMark mark = eng_arena_mark(arena);
    EngJoint *joints = ENG_ARENA_NEW_ARRAY(arena, EngJoint, n);
    char *strings = has_names ? eng_arena_alloc(arena, strs.size, 1) : NULL;
    if (joints == NULL || (has_names && strings == NULL)) {
        eng_arena_reset_to(arena, mark);
        return refuse(name, "mémoire insuffisante");
    }
    if (has_names) {
        memcpy(strings, strs.data, strs.size);
    }
    for (uint32_t i = 0; i < n; i++) {
        DiskJoint d;
        memcpy(&d, jnts.data + ((size_t)i * sizeof(DiskJoint)), sizeof(d));
        EngJoint *j = &joints[i];
        *j = (EngJoint){.parent = d.parent};
        memcpy(j->translation, d.translation, sizeof(j->translation));
        memcpy(j->rotation, d.rotation, sizeof(j->rotation));
        memcpy(j->scale, d.scale, sizeof(j->scale));
        memcpy(j->inverse_bind, ibms.data + ((size_t)i * 16U * sizeof(float)),
               sizeof(j->inverse_bind));
        if (d.name_offset != NO_NAME) {
            if (!has_names || d.name_offset >= strs.size) {
                eng_arena_reset_to(arena, mark);
                return refuse(name, "nom hors de STRS");
            }
            j->name = strings + d.name_offset;
        }
    }
    const EngSkeleton candidate = {.joint_count = n, .joints = joints};
    const char *why = invalid_reason(&candidate); /* nom : borné par strnlen, STRS finit par 0 */
    if (why != NULL) {
        eng_arena_reset_to(arena, mark);
        return refuse(name, why);
    }
    *out = candidate;
    return true;
}

bool eng_skeleton_load(EngArena *arena, EngArena *scratch, const char *path, EngSkeleton *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngSkeleton){0};
    if (arena == NULL || scratch == NULL || path == NULL) {
        return refuse(path, "argument NULL");
    }
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData file;
    bool ok = false;
    if (eng_file_read_all(scratch, path, &file)) { /* erreur déjà journalisée sinon */
        ok = eng_skeleton_decode(arena, file.data, file.size, path, out);
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

uint32_t eng_skeleton_gltf_skin_count(const void *data, size_t size)
{
    cgltf_data *gltf = gltf_open(data, size, NULL, false);
    if (gltf == NULL) {
        return 0;
    }
    const uint32_t count = (uint32_t)gltf->skins_count;
    cgltf_free(gltf);
    return count;
}

/* Pose locale d'une articulation du skin, avant rangement. */
typedef struct RawJoint {
    int32_t parent; /* indice dans le skin, -1 racine */
    float translation[3];
    float rotation[4];
    float scale[3];
} RawJoint;

bool eng_skeleton_decompose_trs(mat4 m, float translation[3], float rotation[4], float scale[3])
{
    mat4 copy;
    glm_mat4_copy(m, copy);
    vec4 t;
    mat4 r;
    vec3 s;
    glm_decompose(copy, t, r, s);
    if (!(s[0] > 1e-8F) || !(s[1] > 1e-8F) || !(s[2] > 1e-8F) || glm_mat4_det(r) < 0.0F) {
        return false; /* échelle nulle, négative ou miroir */
    }
    versor q;
    glm_mat4_quat(r, q);
    glm_quat_normalize(q);
    memcpy(translation, t, 3U * sizeof(float));
    memcpy(rotation, q, 4U * sizeof(float));
    memcpy(scale, s, 3U * sizeof(float));
    return finite_n(translation, 3) && finite_n(rotation, 4) && finite_n(scale, 3);
}

static bool decompose(mat4 m, RawJoint *out)
{
    return eng_skeleton_decompose_trs(m, out->translation, out->rotation, out->scale);
}

static bool fill_raw_joints(const cgltf_data *gltf, const cgltf_skin *skin, EngArena *scratch,
                            RawJoint *raw)
{
    const size_t node_count = gltf->nodes_count;
    int32_t *node_to_skin = ENG_ARENA_NEW_ARRAY(scratch, int32_t, node_count);
    if (node_to_skin == NULL) {
        return false;
    }
    for (size_t i = 0; i < node_count; i++) {
        node_to_skin[i] = -1;
    }
    for (cgltf_size j = 0; j < skin->joints_count; j++) {
        const size_t node = (size_t)(skin->joints[j] - gltf->nodes);
        if (node >= node_count || node_to_skin[node] >= 0) {
            ENG_LOG_ERROR("glTF : le skin liste deux fois la même articulation");
            return false;
        }
        node_to_skin[node] = (int32_t)j;
    }
    for (cgltf_size j = 0; j < skin->joints_count; j++) {
        const cgltf_node *node = skin->joints[j];
        mat4 m;
        cgltf_node_transform_local(node, (cgltf_float *)m);
        int32_t parent = -1;
        for (const cgltf_node *p = node->parent; p != NULL; p = p->parent) {
            const int32_t k = node_to_skin[p - gltf->nodes];
            if (k >= 0) {
                parent = k;
                break;
            }
            /* nœud intermédiaire qui n'est pas une articulation : sa transformation est repliée */
            mat4 l;
            mat4 folded;
            cgltf_node_transform_local(p, (cgltf_float *)l);
            glm_mat4_mul(l, m, folded);
            glm_mat4_copy(folded, m);
        }
        raw[j].parent = parent;
        if (!decompose(m, &raw[j])) {
            ENG_LOG_ERROR("glTF : pose de l'articulation %u non décomposable (échelle nulle, "
                          "négative ou miroir)",
                          (unsigned)j);
            return false;
        }
    }
    return true;
}

bool eng_skeleton_from_gltf(EngArena *arena, EngArena *scratch, const void *data, size_t size,
                            const char *base_path, uint32_t skin_index, EngSkeleton *out,
                            uint32_t **remap)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngSkeleton){0};
    if (remap != NULL) {
        *remap = NULL;
    }
    if (arena == NULL || scratch == NULL) {
        return refuse(base_path, "arène NULL");
    }
    cgltf_data *gltf = gltf_open(data, size, base_path, true);
    if (gltf == NULL) {
        return false;
    }
    bool ok = false;
    const EngArenaMark arena_mark = eng_arena_mark(arena);
    const EngArenaMark scratch_mark = eng_arena_mark(scratch);
    do {
        if (skin_index >= gltf->skins_count) {
            ENG_LOG_ERROR("glTF : le skin %u n'existe pas (%u skin(s))", (unsigned)skin_index,
                          (unsigned)gltf->skins_count);
            break;
        }
        const cgltf_skin *skin = &gltf->skins[skin_index];
        const uint32_t n = (uint32_t)skin->joints_count;
        if (n < 1 || n > ENG_SKELETON_MAX_JOINTS) {
            ENG_LOG_ERROR("glTF : %u articulation(s) : hors de [1 ; %u]", (unsigned)n,
                          ENG_SKELETON_MAX_JOINTS);
            break;
        }
        RawJoint *raw = ENG_ARENA_NEW_ARRAY(scratch, RawJoint, n);
        float *ibm = eng_arena_alloc_array(scratch, 16U * sizeof(float), n, 16);
        uint32_t *skin_to_skel = ENG_ARENA_NEW_ARRAY(arena, uint32_t, n);
        EngJoint *joints = ENG_ARENA_NEW_ARRAY(arena, EngJoint, n);
        if (raw == NULL || ibm == NULL || skin_to_skel == NULL || joints == NULL) {
            ENG_LOG_ERROR("glTF : mémoire insuffisante pour le squelette");
            break;
        }
        if (!fill_raw_joints(gltf, skin, scratch, raw)) {
            break;
        }
        if (skin->inverse_bind_matrices != NULL) {
            const cgltf_accessor *acc = skin->inverse_bind_matrices;
            if (acc->type != cgltf_type_mat4 || acc->count != n ||
                cgltf_accessor_unpack_floats(acc, ibm, (cgltf_size)n * 16U) !=
                    (cgltf_size)n * 16U) {
                ENG_LOG_ERROR("glTF : matrices inverses illisibles ou en nombre différent des "
                              "articulations");
                break;
            }
        } else {
            for (uint32_t j = 0; j < n; j++) { /* règle glTF : identité */
                glm_mat4_identity((vec4 *)&ibm[(size_t)j * 16U]);
            }
        }
        /* parents d'abord : passes successives sur l'ordre du skin (stable) */
        bool *placed = ENG_ARENA_NEW_ARRAY(scratch, bool, n);
        if (placed == NULL) {
            break;
        }
        uint32_t next = 0;
        bool stuck = false;
        while (next < n && !stuck) {
            stuck = true;
            for (uint32_t j = 0; j < n; j++) {
                if (!placed[j] && (raw[j].parent < 0 || placed[raw[j].parent])) {
                    placed[j] = true;
                    skin_to_skel[j] = next++;
                    stuck = false;
                }
            }
        }
        if (next != n) {
            ENG_LOG_ERROR("glTF : hiérarchie d'articulations cyclique");
            break;
        }
        bool reordered = false;
        bool names_ok = true;
        for (uint32_t j = 0; j < n && names_ok; j++) {
            const uint32_t d = skin_to_skel[j];
            reordered = reordered || d != j;
            EngJoint *dst = &joints[d];
            *dst =
                (EngJoint){.parent = raw[j].parent < 0 ? -1 : (int32_t)skin_to_skel[raw[j].parent]};
            memcpy(dst->translation, raw[j].translation, sizeof(dst->translation));
            memcpy(dst->rotation, raw[j].rotation, sizeof(dst->rotation));
            memcpy(dst->scale, raw[j].scale, sizeof(dst->scale));
            memcpy(dst->inverse_bind, &ibm[(size_t)j * 16U], sizeof(dst->inverse_bind));
            const char *node_name = skin->joints[j]->name;
            if (node_name != NULL && node_name[0] != '\0') {
                const size_t len = strlen(node_name);
                char *copy =
                    len <= ENG_SKELETON_MAX_NAME ? eng_arena_alloc(arena, len + 1U, 1) : NULL;
                if (copy == NULL) {
                    ENG_LOG_ERROR("glTF : nom d'articulation trop long (> %u octets) ou mémoire "
                                  "insuffisante",
                                  ENG_SKELETON_MAX_NAME);
                    names_ok = false;
                } else {
                    memcpy(copy, node_name, len + 1U);
                    dst->name = copy;
                }
            }
        }
        if (!names_ok) {
            break;
        }
        const EngSkeleton candidate = {.joint_count = n, .joints = joints};
        const char *why = invalid_reason(&candidate);
        if (why != NULL) {
            ENG_LOG_ERROR("glTF : squelette obtenu invalide : %s", why);
            break;
        }
        if (reordered) {
            ENG_LOG_INFO("glTF : articulations rangées parents d'abord (ordre du skin différent) ; "
                         "permutation rendue dans `remap`");
        }
        *out = candidate;
        if (remap != NULL) {
            *remap = skin_to_skel;
        }
        ok = true;
    } while (false);
    eng_arena_reset_to(scratch, scratch_mark);
    if (!ok) {
        eng_arena_reset_to(arena, arena_mark);
    }
    cgltf_free(gltf);
    return ok;
}

/* ── Poses et dessin ── */

static void local_matrix(const EngJoint *j, mat4 out)
{
    mat4 t;
    mat4 r;
    mat4 s;
    mat4 rs;
    glm_translate_make(t, (vec3){j->translation[0], j->translation[1], j->translation[2]});
    versor q = {j->rotation[0], j->rotation[1], j->rotation[2], j->rotation[3]};
    glm_quat_mat4(q, r);
    glm_scale_make(s, (vec3){j->scale[0], j->scale[1], j->scale[2]});
    glm_mat4_mul(r, s, rs);
    glm_mat4_mul(t, rs, out);
}

bool eng_skeleton_rest_world(const EngSkeleton *skeleton, mat4 *out)
{
    if (out == NULL || invalid_reason(skeleton) != NULL) {
        return false;
    }
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        const EngJoint *j = &skeleton->joints[i];
        mat4 local;
        local_matrix(j, local);
        if (j->parent < 0) {
            glm_mat4_copy(local, out[i]);
        } else {
            glm_mat4_mul(out[j->parent], local, out[i]);
        }
    }
    return true;
}

bool eng_skeleton_bind_world(const EngSkeleton *skeleton, mat4 *out)
{
    if (out == NULL || invalid_reason(skeleton) != NULL) {
        return false;
    }
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        mat4 ibm;
        memcpy(ibm, skeleton->joints[i].inverse_bind,
               sizeof(ibm)); /* pas d'alignement SIMD garanti */
        glm_mat4_inv(ibm, out[i]);
    }
    return true;
}

float eng_skeleton_bind_mismatch(const EngSkeleton *skeleton)
{
    if (invalid_reason(skeleton) != NULL) {
        return -1.0F;
    }
    mat4 rest[ENG_SKELETON_MAX_JOINTS];
    mat4 bind[ENG_SKELETON_MAX_JOINTS];
    if (!eng_skeleton_rest_world(skeleton, rest) || !eng_skeleton_bind_world(skeleton, bind)) {
        return -1.0F;
    }
    float worst = 0.0F;
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 3; r++) {
                const float d = fabsf(rest[i][c][r] - bind[i][c][r]);
                if (d > worst || !isfinite(d)) {
                    worst = isfinite(d) ? d : INFINITY;
                }
            }
        }
    }
    return worst;
}

uint32_t eng_skeleton_debug_draw(const EngSkeleton *skeleton, mat4 *world, mat4 root,
                                 EngDebugLines *lines, EngDebugColor color)
{
    if (world == NULL || root == NULL || lines == NULL || invalid_reason(skeleton) != NULL) {
        return 0;
    }
    uint32_t bones = 0;
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        vec3 head;
        glm_mat4_mulv3(root, (vec3){world[i][3][0], world[i][3][1], world[i][3][2]}, 1.0F, head);
        static const vec3 axes[3] = {{0.02F, 0.0F, 0.0F}, {0.0F, 0.02F, 0.0F}, {0.0F, 0.0F, 0.02F}};
        for (int a = 0; a < 3; a++) {
            vec3 lo;
            vec3 hi;
            glm_vec3_sub(head, (float *)axes[a], lo);
            glm_vec3_add(head, (float *)axes[a], hi);
            (void)eng_debug_line(lines, lo, hi, color);
        }
        const int32_t parent = skeleton->joints[i].parent;
        if (parent >= 0) {
            vec3 tail;
            glm_mat4_mulv3(root,
                           (vec3){world[parent][3][0], world[parent][3][1], world[parent][3][2]},
                           1.0F, tail);
            if (eng_debug_line(lines, tail, head, color)) {
                bones++;
            }
        }
    }
    return bones;
}
