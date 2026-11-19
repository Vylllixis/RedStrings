#include "engine/skin_file.h"

#include <string.h>

#include "engine/asset_format.h"
#include "engine/files.h"
#include "engine/log.h"
#include "engine/skeleton.h"

typedef struct DiskInfo {
    uint32_t vertex_count;
    uint32_t joint_count;
    uint32_t reserved[2];
} DiskInfo;

_Static_assert(sizeof(DiskInfo) == 16, "INFO : 16 octets");
_Static_assert(sizeof(EngSkinVertex) == 8, "WGHT : 8 octets par sommet");

static bool refuse(const char *name, const char *why)
{
    ENG_LOG_ERROR("%s : poids de skinning refusés : %s", name != NULL ? name : "(mémoire)", why);
    return false;
}

/* Raison du refus d'un sommet, ou NULL. */
static const char *vertex_problem(const EngSkinVertex *v, uint32_t joint_count)
{
    unsigned sum = 0;
    for (size_t k = 0; k < 4U; k++) {
        sum += v->weights[k];
        if (v->weights[k] != 0 && v->joints[k] >= joint_count) {
            return "indice d'articulation hors du squelette";
        }
        if (v->weights[k] == 0 && v->joints[k] != 0) {
            return "influence de poids nul avec un indice non nul";
        }
    }
    return sum == 255U ? NULL : "somme des poids différente de 255";
}

bool eng_skin_encode(EngArena *arena, const EngSkinVertex *skin, uint32_t vertex_count,
                     uint32_t joint_count, const void **out, size_t *out_size)
{
    if (out != NULL) {
        *out = NULL;
    }
    if (out_size != NULL) {
        *out_size = 0;
    }
    if (arena == NULL || skin == NULL || out == NULL || out_size == NULL) {
        return refuse(NULL, "argument NULL");
    }
    if (vertex_count < 1U || vertex_count > ENG_SKIN_FILE_MAX_VERTICES) {
        return refuse(NULL, "nombre de sommets hors de [1 ; 2^30)");
    }
    if (joint_count < 1U || joint_count > ENG_SKELETON_MAX_JOINTS) {
        return refuse(NULL, "nombre d'articulations hors de [1 ; 256]");
    }
    for (uint32_t i = 0; i < vertex_count; i++) {
        const char *why = vertex_problem(&skin[i], joint_count);
        if (why != NULL) {
            ENG_LOG_ERROR("poids de skinning : sommet %u : %s", (unsigned)i, why);
            return false;
        }
    }
    const DiskInfo info = {.vertex_count = vertex_count, .joint_count = joint_count};
    const EngAssetSectionDesc sections[2] = {
        {ENG_ASSET_TAG_INFO, &info, (uint32_t)sizeof info, 1U},
        {ENG_ASSET_TAG_WGHT, skin, (uint32_t)(vertex_count * sizeof(EngSkinVertex)), vertex_count}};
    const size_t total = eng_asset_file_size(sections, 2U);
    const EngArenaMark mark = eng_arena_mark(arena);
    void *file = total > 0 ? eng_arena_alloc(arena, total, ENG_ASSET_ALIGN) : NULL;
    size_t written = 0;
    if (file == NULL || !eng_asset_build(ENG_ASSET_MAGIC_SKIN, ENG_ASSET_VERSION_SKIN, sections, 2U,
                                         file, total, &written)) {
        eng_arena_reset_to(arena, mark);
        return refuse(NULL, "écriture du conteneur impossible");
    }
    *out = file;
    *out_size = written;
    return true;
}

bool eng_skin_decode(EngArena *arena, const void *data, size_t size, const char *name,
                     uint32_t expected_vertices, EngSkinVertex **skin, uint32_t *joint_count)
{
    if (skin != NULL) {
        *skin = NULL;
    }
    if (joint_count != NULL) {
        *joint_count = 0;
    }
    if (arena == NULL || skin == NULL) {
        return refuse(name, "argument NULL");
    }
    EngAssetView view;
    if (eng_asset_open(data, size, ENG_ASSET_MAGIC_SKIN, ENG_ASSET_VERSION_SKIN, name, &view) !=
        ENG_ASSET_OK) {
        return false; /* déjà journalisé */
    }
    EngAssetSection info;
    EngAssetSection wght;
    if (!eng_asset_find_section(&view, ENG_ASSET_TAG_INFO, &info) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_WGHT, &wght)) {
        return refuse(name, "section INFO ou WGHT absente");
    }
    if (info.size != sizeof(DiskInfo)) {
        return refuse(name, "taille de INFO inattendue");
    }
    DiskInfo di;
    memcpy(&di, info.data, sizeof di);
    if (di.reserved[0] != 0 || di.reserved[1] != 0) {
        return refuse(name, "champs réservés de INFO non nuls");
    }
    if (di.joint_count < 1U || di.joint_count > ENG_SKELETON_MAX_JOINTS) {
        return refuse(name, "nombre d'articulations hors de [1 ; 256]");
    }
    if (di.vertex_count < 1U || di.vertex_count > ENG_SKIN_FILE_MAX_VERTICES) {
        return refuse(name, "nombre de sommets hors de [1 ; 2^30)");
    }
    if (wght.count != di.vertex_count ||
        wght.size != (uint64_t)di.vertex_count * sizeof(EngSkinVertex)) {
        return refuse(name, "taille ou nombre de WGHT incohérent avec INFO");
    }
    if (di.vertex_count != expected_vertices) {
        ENG_LOG_ERROR("%s : poids de skinning pour %u sommets, le maillage en a %u",
                      name != NULL ? name : "(mémoire)", (unsigned)di.vertex_count,
                      (unsigned)expected_vertices);
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    EngSkinVertex *out = ENG_ARENA_NEW_ARRAY(arena, EngSkinVertex, di.vertex_count);
    if (out == NULL) {
        eng_arena_reset_to(arena, mark);
        return refuse(name, "mémoire insuffisante");
    }
    memcpy(out, wght.data, wght.size);
    for (uint32_t i = 0; i < di.vertex_count; i++) {
        const char *why = vertex_problem(&out[i], di.joint_count);
        if (why != NULL) {
            ENG_LOG_ERROR("%s : poids de skinning : sommet %u : %s",
                          name != NULL ? name : "(mémoire)", (unsigned)i, why);
            eng_arena_reset_to(arena, mark);
            return false;
        }
    }
    *skin = out;
    if (joint_count != NULL) {
        *joint_count = di.joint_count;
    }
    return true;
}

bool eng_skin_load(EngArena *arena, EngArena *scratch, const char *path, uint32_t expected_vertices,
                   EngSkinVertex **skin, uint32_t *joint_count)
{
    if (skin != NULL) {
        *skin = NULL;
    }
    if (joint_count != NULL) {
        *joint_count = 0;
    }
    if (arena == NULL || scratch == NULL || path == NULL || skin == NULL) {
        return refuse(path, "argument NULL");
    }
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData file;
    bool ok = false;
    if (eng_file_read_all(scratch, path, &file)) {
        ok = eng_skin_decode(arena, file.data, file.size, path, expected_vertices, skin,
                             joint_count);
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}
