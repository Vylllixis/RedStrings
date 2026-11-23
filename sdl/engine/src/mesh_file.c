#include "engine/mesh_file.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"
#include "engine/tex_file.h"

/* Dispositions sur disque (docs/asset-formats.md §3) : des tableaux de ces structures, sans
 * remplissage (vérifié ci-dessous), lus et écrits tels quels. */
typedef struct DiskInfo {
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t index_size; /* 2 ou 4 */
    uint32_t part_count;
    uint32_t material_count;
    uint32_t texture_ref_count;
    uint32_t flags;
    uint32_t reserved;
    float bounds_min[3];
    float bounds_max[3];
} DiskInfo;

typedef struct DiskPart {
    uint32_t first_index;
    uint32_t index_count;
    uint32_t material;
    uint32_t flags;
    float bounds_min[3];
    float bounds_max[3];
} DiskPart;

typedef struct DiskMaterial {
    float base_color[4];
    float metallic;
    float roughness;
    float normal_scale;
    float occlusion_strength;
    float emissive[3];
    int32_t base_color_tex;
    int32_t metallic_roughness_tex;
    int32_t normal_tex;
    int32_t occlusion_tex;
    int32_t emissive_tex;
} DiskMaterial;

typedef struct DiskTexRef {
    uint32_t path_offset;
    uint32_t flags;
} DiskTexRef;

#define TEXREF_SRGB 1U
#define MAX_MATERIALS 65536U

_Static_assert(sizeof(DiskInfo) == 56, "INFO d'un .mesh : 56 octets");
_Static_assert(sizeof(DiskPart) == 40, "PART : 40 octets");
_Static_assert(sizeof(DiskMaterial) == 64, "MATL : 64 octets");
_Static_assert(sizeof(DiskTexRef) == 8, "TREF : 8 octets");
_Static_assert(sizeof(EngModelVertex) == 48, "VERT : 48 octets par sommet");
_Static_assert(offsetof(EngModelVertex, normal) == 12 && offsetof(EngModelVertex, uv) == 24 &&
                   offsetof(EngModelVertex, tangent) == 32,
               "VERT : position @0, normale @12, uv @24, tangente @32");

static bool finite_array(const float *values, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (!isfinite(values[i])) {
            return false;
        }
    }
    return true;
}

static bool bounds_valid(const float bmin[3], const float bmax[3])
{
    if (!finite_array(bmin, 3) || !finite_array(bmax, 3)) {
        return false;
    }
    return bmin[0] <= bmax[0] && bmin[1] <= bmax[1] && bmin[2] <= bmax[2];
}

/* Chemin de texture sûr : relatif, « / » comme séparateur, sans composant vide, « . » ni « .. »,
 * sans lettre de lecteur ni caractère de contrôle. */
static bool path_is_safe(const char *path, size_t length)
{
    if (length == 0 || length > ENG_MESH_MAX_PATH || path[0] == '/') {
        return false;
    }
    size_t start = 0;
    for (size_t i = 0; i <= length; i++) {
        const unsigned char c = i < length ? (unsigned char)path[i] : (unsigned char)'/';
        if (i < length && (c < 0x20U || c == '\\' || c == ':')) {
            return false;
        }
        if (c == '/') {
            const size_t part = i - start;
            if (part == 0 || (part == 1 && path[start] == '.') ||
                (part == 2 && path[start] == '.' && path[start + 1] == '.')) {
                return false;
            }
            start = i + 1;
        }
    }
    return true;
}

/* Règles de cohérence communes à l'écriture et à la lecture. `what` : « écriture » / nom du
 * fichier, pour le journal. */
static bool check_model(const EngModel *m, uint32_t texture_count, const char *what)
{
    if (m->vertices == NULL || m->indices == NULL || m->parts == NULL || m->materials == NULL ||
        m->vertex_count == 0 || m->vertex_count > ENG_MODEL_MAX_VERTICES || m->index_count < 3U ||
        m->index_count > ENG_MODEL_MAX_INDICES || (m->index_count % 3U) != 0U ||
        m->part_count == 0 || m->part_count > ENG_MODEL_MAX_PARTS || m->material_count == 0 ||
        m->material_count > MAX_MATERIALS || texture_count > ENG_MESH_MAX_TEXTURES) {
        ENG_LOG_ERROR("mesh %s : tailles du modèle hors limites ou tableaux absents", what);
        return false;
    }
    if (!bounds_valid(m->bounds_min, m->bounds_max)) {
        ENG_LOG_ERROR("mesh %s : boîte englobante invalide", what);
        return false;
    }
    for (uint32_t i = 0; i < m->index_count; i++) {
        if (m->indices[i] >= m->vertex_count) {
            ENG_LOG_ERROR("mesh %s : l'index %u vise le sommet %u (il y en a %u)", what,
                          (unsigned)i, (unsigned)m->indices[i], (unsigned)m->vertex_count);
            return false;
        }
    }
    for (uint32_t i = 0; i < m->part_count; i++) {
        const EngModelPart *p = &m->parts[i];
        if (p->index_count < 3U || (p->index_count % 3U) != 0U ||
            (uint64_t)p->first_index + p->index_count > m->index_count ||
            p->material >= m->material_count) {
            ENG_LOG_ERROR("mesh %s : partie %u invalide (index %u + %u sur %u, matériau %u sur %u)",
                          what, (unsigned)i, (unsigned)p->first_index, (unsigned)p->index_count,
                          (unsigned)m->index_count, (unsigned)p->material,
                          (unsigned)m->material_count);
            return false;
        }
    }
    for (uint32_t i = 0; i < m->material_count; i++) {
        const EngModelMaterial *mat = &m->materials[i];
        const int32_t images[5] = {mat->image, mat->metallic_roughness_image, mat->normal_image,
                                   mat->occlusion_image, mat->emissive_image};
        for (int k = 0; k < 5; k++) {
            if (images[k] < -1 || images[k] >= (int32_t)texture_count) {
                ENG_LOG_ERROR("mesh %s : le matériau %u désigne la texture %d (il y en a %u)", what,
                              (unsigned)i, (int)images[k], (unsigned)texture_count);
                return false;
            }
        }
        if (!finite_array(mat->base_color, 4) || !finite_array(mat->emissive, 3) ||
            !isfinite(mat->metallic) || !isfinite(mat->roughness) || !isfinite(mat->normal_scale) ||
            !isfinite(mat->occlusion_strength)) {
            ENG_LOG_ERROR("mesh %s : le matériau %u a un facteur non numérique", what, (unsigned)i);
            return false;
        }
    }
    return true;
}

/* ── Écriture ── */

bool eng_mesh_encode(EngArena *arena, EngArena *scratch, const EngModel *model,
                     const EngMeshTexture *textures, uint32_t texture_count, const void **out,
                     size_t *out_size)
{
    *out = NULL;
    *out_size = 0;
    if (!check_model(model, texture_count, "(écriture)") ||
        (texture_count > 0 && textures == NULL)) {
        return false;
    }
    size_t strings_size = 0;
    for (uint32_t i = 0; i < texture_count; i++) {
        const size_t length = textures[i].path != NULL ? strlen(textures[i].path) : 0;
        if (textures[i].path == NULL || !path_is_safe(textures[i].path, length)) {
            ENG_LOG_ERROR("mesh : chemin de texture %u invalide (relatif, sans « .. » ni « \\ »)",
                          (unsigned)i);
            return false;
        }
        strings_size += length + 1U;
    }

    const EngArenaMark arena_mark = eng_arena_mark(arena);
    const EngArenaMark scratch_mark = eng_arena_mark(scratch);
    bool ok = false;

    const bool wide = model->vertex_count > 65536U;
    const uint32_t index_size = wide ? 4U : 2U;
    const size_t index_bytes = (size_t)model->index_count * index_size;
    uint8_t *indices = wide ? NULL : eng_arena_alloc(scratch, index_bytes, 16);
    DiskPart *parts = ENG_ARENA_NEW_ARRAY(scratch, DiskPart, model->part_count);
    DiskMaterial *materials = ENG_ARENA_NEW_ARRAY(scratch, DiskMaterial, model->material_count);
    DiskTexRef *refs =
        texture_count > 0 ? ENG_ARENA_NEW_ARRAY(scratch, DiskTexRef, texture_count) : NULL;
    char *strings = texture_count > 0 ? eng_arena_alloc_zero(scratch, strings_size, 1) : NULL;
    if ((!wide && indices == NULL) || parts == NULL || materials == NULL ||
        (texture_count > 0 && (refs == NULL || strings == NULL))) {
        ENG_LOG_ERROR("mesh : mémoire de travail insuffisante");
        goto done;
    }
    if (!wide) {
        for (uint32_t i = 0; i < model->index_count; i++) {
            const uint16_t v = (uint16_t)model->indices[i]; /* < vertex_count <= 65536 : vérifié */
            memcpy(indices + (size_t)i * sizeof v, &v, sizeof v);
        }
    }
    for (uint32_t i = 0; i < model->part_count; i++) {
        const EngModelPart *p = &model->parts[i];
        float bmin[3] = {INFINITY, INFINITY, INFINITY};
        float bmax[3] = {-INFINITY, -INFINITY, -INFINITY};
        for (uint32_t k = 0; k < p->index_count; k++) {
            const float *pos = model->vertices[model->indices[p->first_index + k]].position;
            for (int a = 0; a < 3; a++) {
                if (!isfinite(pos[a])) { /* fminf ignorerait un NaN : on le refuse ici */
                    ENG_LOG_ERROR("mesh : la partie %u a une position non numérique", (unsigned)i);
                    goto done;
                }
                bmin[a] = fminf(bmin[a], pos[a]);
                bmax[a] = fmaxf(bmax[a], pos[a]);
            }
        }
        if (!bounds_valid(bmin, bmax)) {
            ENG_LOG_ERROR("mesh : la partie %u a une position non numérique", (unsigned)i);
            goto done;
        }
        parts[i] = (DiskPart){
            .first_index = p->first_index, .index_count = p->index_count, .material = p->material};
        memcpy(parts[i].bounds_min, bmin, sizeof bmin);
        memcpy(parts[i].bounds_max, bmax, sizeof bmax);
    }
    for (uint32_t i = 0; i < model->material_count; i++) {
        const EngModelMaterial *m = &model->materials[i];
        DiskMaterial *d = &materials[i];
        memcpy(d->base_color, m->base_color, sizeof d->base_color);
        d->metallic = m->metallic;
        d->roughness = m->roughness;
        d->normal_scale = m->normal_scale;
        d->occlusion_strength = m->occlusion_strength;
        memcpy(d->emissive, m->emissive, sizeof d->emissive);
        d->base_color_tex = m->image;
        d->metallic_roughness_tex = m->metallic_roughness_image;
        d->normal_tex = m->normal_image;
        d->occlusion_tex = m->occlusion_image;
        d->emissive_tex = m->emissive_image;
    }
    size_t at = 0;
    for (uint32_t i = 0; strings != NULL && refs != NULL && i < texture_count; i++) {
        const size_t length = strlen(textures[i].path);
        memcpy(strings + at, textures[i].path, length); /* l'octet nul est déjà là (alloc_zero) */
        refs[i] =
            (DiskTexRef){.path_offset = (uint32_t)at, .flags = textures[i].srgb ? TEXREF_SRGB : 0U};
        at += length + 1U;
    }

    DiskInfo info = {.vertex_count = model->vertex_count,
                     .index_count = model->index_count,
                     .index_size = index_size,
                     .part_count = model->part_count,
                     .material_count = model->material_count,
                     .texture_ref_count = texture_count};
    memcpy(info.bounds_min, model->bounds_min, sizeof info.bounds_min);
    memcpy(info.bounds_max, model->bounds_max, sizeof info.bounds_max);

    EngAssetSectionDesc sections[7];
    uint32_t n = 0;
    sections[n++] = (EngAssetSectionDesc){ENG_ASSET_TAG_INFO, &info, sizeof info, 1};
    sections[n++] = (EngAssetSectionDesc){ENG_ASSET_TAG_VERT, model->vertices,
                                          (uint32_t)(model->vertex_count * sizeof(EngModelVertex)),
                                          model->vertex_count};
    sections[n++] = (EngAssetSectionDesc){
        ENG_ASSET_TAG_INDX, wide ? (const void *)model->indices : (const void *)indices,
        (uint32_t)index_bytes, model->index_count};
    sections[n++] =
        (EngAssetSectionDesc){ENG_ASSET_TAG_PART, parts,
                              (uint32_t)(model->part_count * sizeof(DiskPart)), model->part_count};
    sections[n++] = (EngAssetSectionDesc){ENG_ASSET_TAG_MATL, materials,
                                          (uint32_t)(model->material_count * sizeof(DiskMaterial)),
                                          model->material_count};
    if (texture_count > 0) {
        sections[n++] =
            (EngAssetSectionDesc){ENG_ASSET_TAG_TREF, refs,
                                  (uint32_t)(texture_count * sizeof(DiskTexRef)), texture_count};
        sections[n++] = (EngAssetSectionDesc){ENG_ASSET_TAG_STRS, strings, (uint32_t)strings_size,
                                              (uint32_t)strings_size};
    }
    const size_t file_size = eng_asset_file_size(sections, n);
    uint8_t *file = file_size > 0 ? eng_arena_alloc(arena, file_size, 16) : NULL;
    if (file == NULL || !eng_asset_build(ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, sections, n,
                                         file, file_size, NULL)) {
        ENG_LOG_ERROR(
            "mesh : écriture impossible (modèle de plus de 4 Gio ou mémoire insuffisante)");
        goto done;
    }
    *out = file;
    *out_size = file_size;
    ok = true;
done:
    eng_arena_reset_to(scratch, scratch_mark);
    if (!ok) {
        eng_arena_reset_to(arena, arena_mark);
    }
    return ok;
}

/* ── Lecture ── */

static bool find_section(const EngAssetView *asset, uint32_t tag, EngAssetSection *section,
                         const char *name)
{
    if (!eng_asset_find_section(asset, tag, section)) {
        ENG_LOG_ERROR("mesh %s : section %c%c%c%c absente", name, (int)(tag & 0xFFU),
                      (int)((tag >> 8) & 0xFFU), (int)((tag >> 16) & 0xFFU),
                      (int)((tag >> 24) & 0xFFU));
        return false;
    }
    return true;
}

static bool section_is(const EngAssetSection *s, uint64_t count, size_t element, const char *name)
{
    if (s->count != count || s->size != count * element) {
        ENG_LOG_ERROR("mesh %s : section %c%c%c%c incohérente (%u éléments, %u octets, attendu %u "
                      "éléments de %zu octets)",
                      name, (int)(s->tag & 0xFFU), (int)((s->tag >> 8) & 0xFFU),
                      (int)((s->tag >> 16) & 0xFFU), (int)((s->tag >> 24) & 0xFFU),
                      (unsigned)s->count, (unsigned)s->size, (unsigned)count, element);
        return false;
    }
    return true;
}

bool eng_mesh_decode(EngArena *arena, const void *data, size_t size, const char *name,
                     EngMeshFile *out)
{
    *out = (EngMeshFile){0};
    const char *shown = name != NULL ? name : "(sans nom)";
    EngAssetView asset;
    if (eng_asset_open(data, size, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, name, &asset) !=
        ENG_ASSET_OK) {
        return false;
    }
    EngAssetSection s_info;
    EngAssetSection s_vert;
    EngAssetSection s_indx;
    EngAssetSection s_part;
    EngAssetSection s_matl;
    if (!find_section(&asset, ENG_ASSET_TAG_INFO, &s_info, shown) ||
        !find_section(&asset, ENG_ASSET_TAG_VERT, &s_vert, shown) ||
        !find_section(&asset, ENG_ASSET_TAG_INDX, &s_indx, shown) ||
        !find_section(&asset, ENG_ASSET_TAG_PART, &s_part, shown) ||
        !find_section(&asset, ENG_ASSET_TAG_MATL, &s_matl, shown)) {
        return false;
    }
    if (s_info.size != sizeof(DiskInfo) || s_info.count != 1U) {
        ENG_LOG_ERROR("mesh %s : section INFO de taille inattendue (%u)", shown,
                      (unsigned)s_info.size);
        return false;
    }
    DiskInfo info;
    memcpy(&info, s_info.data, sizeof info);
    if (info.flags != 0U || info.reserved != 0U ||
        (info.index_size != 2U && info.index_size != 4U) ||
        (info.index_size == 2U && info.vertex_count > 65536U) ||
        info.vertex_count > ENG_MODEL_MAX_VERTICES || info.index_count > ENG_MODEL_MAX_INDICES ||
        info.part_count > ENG_MODEL_MAX_PARTS || info.material_count > MAX_MATERIALS ||
        info.texture_ref_count > ENG_MESH_MAX_TEXTURES) {
        ENG_LOG_ERROR("mesh %s : section INFO invalide ou hors limites", shown);
        return false;
    }
    if (!section_is(&s_vert, info.vertex_count, sizeof(EngModelVertex), shown) ||
        !section_is(&s_indx, info.index_count, info.index_size, shown) ||
        !section_is(&s_part, info.part_count, sizeof(DiskPart), shown) ||
        !section_is(&s_matl, info.material_count, sizeof(DiskMaterial), shown)) {
        return false;
    }
    EngAssetSection s_tref = {0};
    EngAssetSection s_strs = {0};
    const bool has_tref = eng_asset_find_section(&asset, ENG_ASSET_TAG_TREF, &s_tref);
    const bool has_strs = eng_asset_find_section(&asset, ENG_ASSET_TAG_STRS, &s_strs);
    if (info.texture_ref_count > 0) {
        if (!has_tref || !has_strs ||
            !section_is(&s_tref, info.texture_ref_count, sizeof(DiskTexRef), shown) ||
            s_strs.count != s_strs.size || s_strs.size == 0 ||
            s_strs.data[s_strs.size - 1U] != 0U) {
            ENG_LOG_ERROR("mesh %s : sections TREF / STRS absentes ou incohérentes", shown);
            return false;
        }
    } else if (has_tref && s_tref.size != 0U) {
        ENG_LOG_ERROR("mesh %s : section TREF présente mais aucune texture déclarée", shown);
        return false;
    }

    const EngArenaMark mark = eng_arena_mark(arena);
    EngModel *m = &out->model;
    m->vertex_count = info.vertex_count;
    m->index_count = info.index_count;
    m->part_count = info.part_count;
    m->material_count = info.material_count;
    m->image_count = info.texture_ref_count;
    memcpy(m->bounds_min, info.bounds_min, sizeof m->bounds_min);
    memcpy(m->bounds_max, info.bounds_max, sizeof m->bounds_max);
    m->vertices = eng_arena_alloc_array(arena, sizeof(EngModelVertex), info.vertex_count,
                                        _Alignof(EngModelVertex));
    m->indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, info.index_count);
    m->parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, info.part_count);
    m->materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, info.material_count);
    if (info.texture_ref_count > 0) {
        m->images = ENG_ARENA_NEW_ARRAY(arena, EngImage, info.texture_ref_count);
        out->textures = ENG_ARENA_NEW_ARRAY(arena, EngMeshTexture, info.texture_ref_count);
    }
    out->texture_count = info.texture_ref_count;
    if (m->vertices == NULL || m->indices == NULL || m->parts == NULL || m->materials == NULL ||
        (info.texture_ref_count > 0 && (m->images == NULL || out->textures == NULL))) {
        ENG_LOG_ERROR("mesh %s : arène trop petite", shown);
        goto fail;
    }
    memcpy(m->vertices, s_vert.data, s_vert.size);
    if (info.index_size == 4U) {
        memcpy(m->indices, s_indx.data, s_indx.size);
    } else {
        for (uint32_t i = 0; i < info.index_count; i++) {
            uint16_t v;
            memcpy(&v, s_indx.data + (size_t)i * 2U, sizeof v);
            m->indices[i] = v;
        }
    }
    for (uint32_t i = 0; i < info.part_count; i++) {
        DiskPart d;
        memcpy(&d, s_part.data + (size_t)i * sizeof d, sizeof d);
        if (d.flags != 0U || !bounds_valid(d.bounds_min, d.bounds_max)) {
            ENG_LOG_ERROR("mesh %s : la partie %u a des drapeaux ou une boîte invalides", shown,
                          (unsigned)i);
            goto fail;
        }
        m->parts[i] = (EngModelPart){d.first_index, d.index_count, d.material};
    }
    for (uint32_t i = 0; i < info.material_count; i++) {
        DiskMaterial d;
        memcpy(&d, s_matl.data + (size_t)i * sizeof d, sizeof d);
        EngModelMaterial *e = &m->materials[i];
        memcpy(e->base_color, d.base_color, sizeof e->base_color);
        e->image = d.base_color_tex;
        e->metallic = d.metallic;
        e->roughness = d.roughness;
        e->metallic_roughness_image = d.metallic_roughness_tex;
        e->normal_scale = d.normal_scale;
        e->normal_image = d.normal_tex;
        e->occlusion_strength = d.occlusion_strength;
        e->occlusion_image = d.occlusion_tex;
        memcpy(e->emissive, d.emissive, sizeof e->emissive);
        e->emissive_image = d.emissive_tex;
    }
    for (uint32_t i = 0; i < info.texture_ref_count; i++) {
        DiskTexRef d;
        memcpy(&d, s_tref.data + (size_t)i * sizeof d, sizeof d);
        if ((d.flags & ~TEXREF_SRGB) != 0U || d.path_offset >= s_strs.size) {
            ENG_LOG_ERROR("mesh %s : référence de texture %u invalide", shown, (unsigned)i);
            goto fail;
        }
        const char *path = (const char *)s_strs.data + d.path_offset;
        const size_t length = strlen(path); /* STRS se termine par un octet nul : borné */
        if (!path_is_safe(path, length)) {
            ENG_LOG_ERROR("mesh %s : chemin de texture %u refusé", shown, (unsigned)i);
            goto fail;
        }
        char *copy = eng_arena_alloc(arena, length + 1U, 1);
        if (copy == NULL) {
            ENG_LOG_ERROR("mesh %s : arène trop petite", shown);
            goto fail;
        }
        memcpy(copy, path, length + 1U);
        out->textures[i] = (EngMeshTexture){.path = copy, .srgb = (d.flags & TEXREF_SRGB) != 0U};
    }
    if (!check_model(m, info.texture_ref_count, shown)) {
        goto fail;
    }
    return true;
fail:
    eng_arena_reset_to(arena, mark);
    *out = (EngMeshFile){0};
    return false;
}

/* Dossier d'un chemin, séparateur final inclus (vide s'il n'y en a pas). */
static size_t directory_length(const char *path)
{
    size_t keep = 0;
    for (size_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            keep = i + 1U;
        }
    }
    return keep;
}

bool eng_mesh_load_visit(EngArena *arena, EngArena *scratch, const char *path, EngModel *model,
                         EngMeshFileVisitor visit, void *user)
{
    *model = (EngModel){0};
    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        ENG_LOG_ERROR("mesh : chemin « %s » trop long", path);
        return false;
    }
    const EngArenaMark arena_mark = eng_arena_mark(arena);
    const EngArenaMark scratch_mark = eng_arena_mark(scratch);
    EngMeshFile mesh = {0};
    bool ok = false;
    uint32_t loaded = 0;
    EngFileData file;
    if (!eng_file_read_all(scratch, resolved, &file)) {
        ENG_LOG_ERROR("mesh : lecture impossible : %s", resolved);
        goto done;
    }
    if (!eng_mesh_decode(arena, file.data, file.size, resolved, &mesh)) {
        goto done;
    }
    const size_t dir = directory_length(resolved);
    for (; loaded < mesh.texture_count; loaded++) {
        char tex_path[512];
        const size_t length = strlen(mesh.textures[loaded].path);
        if (dir + length + 1U > sizeof tex_path) {
            ENG_LOG_ERROR("mesh %s : chemin de texture trop long", resolved);
            goto done;
        }
        memcpy(tex_path, resolved, dir);
        memcpy(tex_path + dir, mesh.textures[loaded].path, length + 1U);

        if (visit != NULL) {
            visit(user, tex_path);
        }
        const EngArenaMark tex_mark = eng_arena_mark(scratch);
        EngFileData tex_file;
        EngTexView view;
        bool srgb = false;
        const bool read = eng_file_read_all(scratch, tex_path, &tex_file);
        if (!read) {
            ENG_LOG_ERROR("mesh %s : texture illisible : %s", resolved, tex_path);
        }
        const bool decoded = read && eng_tex_open(tex_file.data, tex_file.size, tex_path, &view) &&
                             eng_tex_to_image(&view, &mesh.model.images[loaded], &srgb);
        eng_arena_reset_to(scratch, tex_mark);
        if (!decoded) {
            goto done;
        }
        if (srgb != mesh.textures[loaded].srgb) {
            ENG_LOG_ERROR("mesh %s : %s n'est pas dans l'espace de couleur attendu (%s)", resolved,
                          tex_path, mesh.textures[loaded].srgb ? "sRGB" : "linéaire");
            loaded++; /* son image est créée : à libérer aussi */
            goto done;
        }
    }
    *model = mesh.model;
    ok = true;
done:
    if (!ok) {
        for (uint32_t i = 0; i < loaded && mesh.model.images != NULL; i++) {
            eng_image_free(&mesh.model.images[i]);
        }
        eng_arena_reset_to(arena, arena_mark);
    }
    eng_arena_reset_to(scratch, scratch_mark);
    return ok;
}

bool eng_mesh_load(EngArena *arena, EngArena *scratch, const char *path, EngModel *model)
{
    return eng_mesh_load_visit(arena, scratch, path, model, NULL, NULL);
}

/* ── Préparation de la cuisson ── */

typedef struct TexUse {
    int32_t image;
    bool srgb;
} TexUse;

/* Indice de la texture (image, rôle) dans `uses`, ajoutée si elle n'y est pas ; -1 pour « aucune ».
 */
static int32_t use_texture(TexUse *uses, uint32_t *count, int32_t image, bool srgb)
{
    if (image < 0) {
        return -1;
    }
    for (uint32_t i = 0; i < *count; i++) {
        if (uses[i].image == image && uses[i].srgb == srgb) {
            return (int32_t)i;
        }
    }
    uses[*count] = (TexUse){image, srgb};
    return (int32_t)(*count)++;
}

#define MAX_STEM 200U

bool eng_mesh_plan(EngArena *arena, const EngModel *source, const char *stem, EngMeshPlan *plan)
{
    *plan = (EngMeshPlan){0};
    const size_t stem_length = stem != NULL ? strlen(stem) : 0;
    if (stem_length == 0 || stem_length > MAX_STEM || !path_is_safe(stem, stem_length) ||
        strchr(stem, '/') != NULL) {
        ENG_LOG_ERROR("mesh : préfixe de textures invalide");
        return false;
    }
    if (source->materials == NULL || source->material_count == 0 ||
        source->material_count > MAX_MATERIALS ||
        (source->image_count > 0 && source->images == NULL)) {
        ENG_LOG_ERROR("mesh : modèle source invalide");
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    const size_t slots = 5U * (size_t)source->material_count;
    EngModelMaterial *materials =
        ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, source->material_count);
    TexUse *uses = ENG_ARENA_NEW_ARRAY(arena, TexUse, slots);
    if (materials == NULL || uses == NULL) {
        ENG_LOG_ERROR("mesh : arène trop petite");
        eng_arena_reset_to(arena, mark);
        return false;
    }
    memcpy(materials, source->materials, source->material_count * sizeof *materials);
    uint32_t count = 0;
    for (uint32_t i = 0; i < source->material_count; i++) {
        EngModelMaterial *m = &materials[i];
        const int32_t images[5] = {m->image, m->metallic_roughness_image, m->normal_image,
                                   m->occlusion_image, m->emissive_image};
        for (int k = 0; k < 5; k++) {
            if (images[k] < -1 || images[k] >= (int32_t)source->image_count ||
                (images[k] >= 0 && source->images[images[k]].pixels == NULL)) {
                ENG_LOG_ERROR("mesh : le matériau %u désigne l'image %d, absente ou non décodée",
                              (unsigned)i, (int)images[k]);
                eng_arena_reset_to(arena, mark);
                return false;
            }
        }
        m->image = use_texture(uses, &count, m->image, true);
        m->metallic_roughness_image = use_texture(uses, &count, m->metallic_roughness_image, false);
        m->normal_image = use_texture(uses, &count, m->normal_image, false);
        m->occlusion_image = use_texture(uses, &count, m->occlusion_image, false);
        m->emissive_image = use_texture(uses, &count, m->emissive_image, true);
    }
    EngMeshTexture *textures = count > 0 ? ENG_ARENA_NEW_ARRAY(arena, EngMeshTexture, count) : NULL;
    uint32_t *sources = count > 0 ? ENG_ARENA_NEW_ARRAY(arena, uint32_t, count) : NULL;
    if (count > 0 && (textures == NULL || sources == NULL)) {
        ENG_LOG_ERROR("mesh : arène trop petite");
        eng_arena_reset_to(arena, mark);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        const size_t room = stem_length + 16U; /* « .4294967295.tex » + octet nul */
        char *name = eng_arena_alloc(arena, room, 1);
        if (name == NULL) {
            ENG_LOG_ERROR("mesh : arène trop petite");
            eng_arena_reset_to(arena, mark);
            return false;
        }
        (void)snprintf(name, room, "%s.%u.tex", stem, (unsigned)i);
        textures[i] = (EngMeshTexture){.path = name, .srgb = uses[i].srgb};
        sources[i] = (uint32_t)uses[i].image;
    }
    plan->model = *source;
    plan->model.materials = materials;
    plan->textures = textures;
    plan->source_images = sources;
    plan->texture_count = count;
    return true;
}
