#include "engine/model.h"

#include <SDL3/SDL.h>
#include <cgltf.h>
#include <math.h>
#include <stdalign.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"
#include "engine/math.h"

/* cgltf alloue par SDL_malloc / SDL_free, comme le reste du moteur (compteur d'allocations). */
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

static const char *gltf_error(cgltf_result result)
{
    switch (result) {
    case cgltf_result_data_too_short:
        return "données trop courtes";
    case cgltf_result_unknown_format:
        return "format inconnu (ni .gltf ni .glb)";
    case cgltf_result_invalid_json:
        return "JSON invalide";
    case cgltf_result_invalid_gltf:
        return "glTF invalide";
    case cgltf_result_file_not_found:
        return "fichier externe introuvable";
    case cgltf_result_io_error:
        return "erreur de lecture";
    case cgltf_result_out_of_memory:
        return "mémoire insuffisante";
    case cgltf_result_legacy_gltf:
        return "glTF 1.0 non géré";
    default:
        return "erreur inconnue";
    }
}

/* ── Primitives ── */

typedef struct PrimInfo {
    const cgltf_accessor *position;
    const cgltf_accessor *normal;
    const cgltf_accessor *uv;
    const cgltf_accessor *tangent;
    uint32_t vertex_count;
    uint32_t index_count;
} PrimInfo;

typedef enum PrimResult {
    PRIM_OK,
    PRIM_SKIP, /* pas un triangle : ignorée */
    PRIM_ERROR,
} PrimResult;

static PrimResult inspect_primitive(const cgltf_primitive *prim, PrimInfo *info)
{
    *info = (PrimInfo){0};
    if (prim->type != cgltf_primitive_type_triangles) {
        ENG_LOG_WARN("glTF : primitive ignorée (seul le mode triangles est géré, mode %d)",
                     (int)prim->type);
        return PRIM_SKIP;
    }
    if (prim->has_draco_mesh_compression) {
        ENG_LOG_ERROR("glTF : compression Draco non gérée");
        return PRIM_ERROR;
    }
    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
        const cgltf_attribute *a = &prim->attributes[i];
        if (a->type == cgltf_attribute_type_position && info->position == NULL) {
            info->position = a->data;
        } else if (a->type == cgltf_attribute_type_normal && info->normal == NULL) {
            info->normal = a->data;
        } else if (a->type == cgltf_attribute_type_texcoord && a->index == 0 && info->uv == NULL) {
            info->uv = a->data;
        } else if (a->type == cgltf_attribute_type_tangent && info->tangent == NULL) {
            info->tangent = a->data;
        }
    }
    if (info->position == NULL || info->position->type != cgltf_type_vec3) {
        ENG_LOG_ERROR("glTF : primitive sans POSITION (vec3)");
        return PRIM_ERROR;
    }
    const cgltf_size count = info->position->count;
    if (count == 0 || count > ENG_MODEL_MAX_VERTICES) {
        ENG_LOG_ERROR("glTF : %zu sommets (maximum %u par primitive)", (size_t)count,
                      ENG_MODEL_MAX_VERTICES);
        return PRIM_ERROR;
    }
    if ((info->normal != NULL &&
         (info->normal->type != cgltf_type_vec3 || info->normal->count != count)) ||
        (info->uv != NULL && (info->uv->type != cgltf_type_vec2 || info->uv->count != count)) ||
        (info->tangent != NULL &&
         (info->tangent->type != cgltf_type_vec4 || info->tangent->count != count))) {
        ENG_LOG_ERROR("glTF : NORMAL / TEXCOORD_0 / TANGENT incohérents avec POSITION");
        return PRIM_ERROR;
    }
    info->vertex_count = (uint32_t)count;
    const cgltf_size indices = prim->indices != NULL ? prim->indices->count : count;
    if (indices == 0 || indices % 3 != 0 || indices > ENG_MODEL_MAX_INDICES) {
        ENG_LOG_ERROR("glTF : %zu index (non multiple de 3, nul ou au-delà de %u)", (size_t)indices,
                      ENG_MODEL_MAX_INDICES);
        return PRIM_ERROR;
    }
    info->index_count = (uint32_t)indices;
    return PRIM_OK;
}

/* Matrice monde du nœud, et sa matrice normale (inverse transposée du 3x3). false si singulière.
 * `flip` : la matrice inverse l'orientation (déterminant négatif). */
static bool node_matrices(const cgltf_node *node, mat4 world, mat3 normal_matrix, bool *flip)
{
    cgltf_float raw[16];
    cgltf_node_transform_world(node, raw);
    memcpy(world, raw, sizeof(mat4));
    mat3 linear;
    glm_mat4_pick3(world, linear);
    const float det = glm_mat3_det(linear);
    if (!isfinite(det) || fabsf(det) < 1.0e-12F) {
        return false;
    }
    glm_mat3_inv(linear, normal_matrix);
    glm_mat3_transpose(normal_matrix);
    *flip = det < 0.0F;
    return true;
}

/* Normales lissées : somme des normales de faces (pondérées par l'aire), normalisées. Un sommet
 * sans face garde (0, 1, 0). */
static void smooth_normals(EngModelVertex *vertices, uint32_t vertex_count, const uint32_t *indices,
                           uint32_t index_count, uint32_t base)
{
    for (uint32_t v = 0; v < vertex_count; v++) {
        glm_vec3_zero(vertices[base + v].normal);
    }
    for (uint32_t i = 0; i + 2 < index_count; i += 3) {
        EngModelVertex *a = &vertices[indices[i]];
        EngModelVertex *b = &vertices[indices[i + 1]];
        EngModelVertex *c = &vertices[indices[i + 2]];
        vec3 ab;
        vec3 ac;
        vec3 n;
        glm_vec3_sub(b->position, a->position, ab);
        glm_vec3_sub(c->position, a->position, ac);
        glm_vec3_cross(ab, ac, n);
        glm_vec3_add(a->normal, n, a->normal);
        glm_vec3_add(b->normal, n, b->normal);
        glm_vec3_add(c->normal, n, c->normal);
    }
    for (uint32_t v = 0; v < vertex_count; v++) {
        float *n = vertices[base + v].normal;
        if (glm_vec3_norm2(n) > 0.0F) {
            glm_vec3_normalize(n);
        } else {
            n[0] = 0.0F;
            n[1] = 1.0F;
            n[2] = 0.0F;
        }
    }
}

/* Un vecteur unitaire orthogonal à `n` (unitaire) : l'axe le moins aligné avec `n`, projeté. */
static void any_orthogonal(const float n[3], float out[3])
{
    vec3 axis = {1.0F, 0.0F, 0.0F};
    if (fabsf(n[0]) > 0.9F) {
        axis[0] = 0.0F;
        axis[1] = 1.0F;
    }
    const float along = (axis[0] * n[0]) + (axis[1] * n[1]) + (axis[2] * n[2]);
    for (int a = 0; a < 3; a++) {
        out[a] = axis[a] - (n[a] * along);
    }
    glm_vec3_normalize(out);
}

/* Tangente et signe de bitangente d'un sommet à partir des sommes T (dP/du) et B (-dP/dv, la
 * bitangente d'une normal map pointe vers les v décroissants) : T est orthogonalisé à la normale
 * (Gram-Schmidt), w est le signe de cross(N, T) . B. Somme nulle ou colinéaire à la normale : un
 * axe quelconque orthogonal à la normale, w = 1. */
static void finish_tangent(EngModelVertex *vertex, const float t_sum[3], const float b_sum[3])
{
    const float *n = vertex->normal;
    const float along = (t_sum[0] * n[0]) + (t_sum[1] * n[1]) + (t_sum[2] * n[2]);
    vec3 t = {t_sum[0] - (n[0] * along), t_sum[1] - (n[1] * along), t_sum[2] - (n[2] * along)};
    if (!(glm_vec3_norm2(t) > 1e-20F)) {
        any_orthogonal(n, vertex->tangent);
        vertex->tangent[3] = 1.0F;
        return;
    }
    glm_vec3_normalize(t);
    vec3 cross;
    glm_vec3_cross((float *)n, t, cross);
    glm_vec3_copy(t, vertex->tangent);
    vertex->tangent[3] = glm_vec3_dot(cross, (float *)b_sum) < 0.0F ? -1.0F : 1.0F;
}

/* Tangentes CALCULÉES (le fichier n'en a pas) : pour chaque triangle aux UV non dégénérés,
 * T = dP/du et B = -dP/dv, sommés sur les sommets du triangle (sans pondération : ce sont des
 * dérivées, indépendantes de la taille du triangle), puis finish_tangent. Les normales sont déjà
 * définitives. false si la mémoire de travail manque. */
static bool compute_tangents(EngModel *model, EngArena *scratch, const uint32_t *indices,
                             uint32_t index_count, uint32_t base, uint32_t vertex_count)
{
    const EngArenaMark mark = eng_arena_mark(scratch);
    float *sums =
        eng_arena_alloc_array(scratch, sizeof(float), (size_t)vertex_count * 6U, alignof(float));
    if (sums == NULL) {
        ENG_LOG_ERROR("glTF : arène de travail trop petite (tangentes)");
        return false;
    }
    memset(sums, 0, (size_t)vertex_count * 6U * sizeof(float));
    for (uint32_t i = 0; i + 2 < index_count; i += 3) {
        const EngModelVertex *v[3] = {&model->vertices[indices[i]],
                                      &model->vertices[indices[i + 1]],
                                      &model->vertices[indices[i + 2]]};
        vec3 e1;
        vec3 e2;
        glm_vec3_sub((float *)v[1]->position, (float *)v[0]->position, e1);
        glm_vec3_sub((float *)v[2]->position, (float *)v[0]->position, e2);
        const float du1 = v[1]->uv[0] - v[0]->uv[0];
        const float dv1 = v[1]->uv[1] - v[0]->uv[1];
        const float du2 = v[2]->uv[0] - v[0]->uv[0];
        const float dv2 = v[2]->uv[1] - v[0]->uv[1];
        const float det = (du1 * dv2) - (du2 * dv1);
        if (!(fabsf(det) > 1e-20F)) {
            continue; /* UV dégénérés : ce triangle n'oriente rien */
        }
        const float inv = 1.0F / det;
        vec3 t;
        vec3 b;
        for (int a = 0; a < 3; a++) {
            t[a] = ((e1[a] * dv2) - (e2[a] * dv1)) * inv;
            b[a] = -(((e2[a] * du1) - (e1[a] * du2)) * inv);
        }
        if (!isfinite(t[0] + t[1] + t[2] + b[0] + b[1] + b[2])) {
            continue;
        }
        for (int k = 0; k < 3; k++) {
            float *sum = &sums[(size_t)(indices[i + (uint32_t)k] - base) * 6U];
            for (int a = 0; a < 3; a++) {
                sum[a] += t[a];
                sum[3 + a] += b[a];
            }
        }
    }
    for (uint32_t v = 0; v < vertex_count; v++) {
        finish_tangent(&model->vertices[base + v], &sums[(size_t)v * 6U],
                       &sums[((size_t)v * 6U) + 3U]);
    }
    eng_arena_reset_to(scratch, mark);
    return true;
}

typedef struct Totals {
    uint64_t vertices;
    uint64_t indices;
    uint32_t parts;
} Totals;

static bool count_totals(const cgltf_data *data, Totals *totals)
{
    *totals = (Totals){0};
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        const cgltf_mesh *mesh = data->nodes[n].mesh;
        if (mesh == NULL) {
            continue;
        }
        for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
            PrimInfo info;
            const PrimResult r = inspect_primitive(&mesh->primitives[p], &info);
            if (r == PRIM_ERROR) {
                return false;
            }
            if (r == PRIM_OK) {
                totals->vertices += info.vertex_count;
                totals->indices += info.index_count;
                totals->parts++;
            }
        }
    }
    if (totals->parts == 0) {
        ENG_LOG_ERROR("glTF : aucun triangle à charger");
        return false;
    }
    if (totals->vertices > ENG_MODEL_MAX_VERTICES || totals->indices > ENG_MODEL_MAX_INDICES ||
        totals->parts > ENG_MODEL_MAX_PARTS) {
        ENG_LOG_ERROR("glTF : modèle trop gros (%llu sommets, %llu index, %u parties)",
                      (unsigned long long)totals->vertices, (unsigned long long)totals->indices,
                      totals->parts);
        return false;
    }
    return true;
}

typedef struct Cursor {
    uint32_t vertex;
    uint32_t index;
    uint32_t part;
} Cursor;

static bool fill_primitive(EngModel *model, EngArena *scratch, const cgltf_data *data,
                           const cgltf_node *node, const cgltf_primitive *prim,
                           const PrimInfo *info, Cursor *cursor)
{
    mat4 world;
    mat3 normal_matrix;
    bool flip = false;
    if (!node_matrices(node, world, normal_matrix, &flip)) {
        ENG_LOG_ERROR("glTF : matrice de nœud singulière");
        return false;
    }
    const uint32_t vcount = info->vertex_count;
    const uint32_t base = cursor->vertex;
    EngModelVertex *vertices = &model->vertices[base];

    const EngArenaMark mark = eng_arena_mark(scratch);
    const size_t floats = (size_t)vcount * 3U;
    float *tmp = eng_arena_alloc_array(scratch, sizeof(float), (size_t)vcount * 4U, alignof(float));
    if (tmp == NULL) {
        ENG_LOG_ERROR("glTF : arène de travail trop petite");
        return false;
    }
    bool ok = cgltf_accessor_unpack_floats(info->position, tmp, floats) == floats;
    for (uint32_t v = 0; ok && v < vcount; v++) {
        glm_mat4_mulv3(world, &tmp[(size_t)v * 3U], 1.0F, vertices[v].position);
        for (int a = 0; a < 3; a++) {
            if (!isfinite(vertices[v].position[a])) {
                ok = false;
            }
        }
    }
    if (ok && info->normal != NULL) {
        ok = cgltf_accessor_unpack_floats(info->normal, tmp, floats) == floats;
        for (uint32_t v = 0; ok && v < vcount; v++) {
            glm_mat3_mulv(normal_matrix, &tmp[(size_t)v * 3U], vertices[v].normal);
            glm_vec3_normalize(vertices[v].normal);
        }
    }
    if (ok && info->uv != NULL) {
        const size_t uv_floats = (size_t)vcount * 2U;
        ok = cgltf_accessor_unpack_floats(info->uv, tmp, uv_floats) == uv_floats;
        for (uint32_t v = 0; ok && v < vcount; v++) {
            vertices[v].uv[0] = tmp[(size_t)v * 2U];
            vertices[v].uv[1] = tmp[((size_t)v * 2U) + 1U];
        }
    } else if (ok) {
        for (uint32_t v = 0; v < vcount; v++) {
            vertices[v].uv[0] = 0.0F;
            vertices[v].uv[1] = 0.0F;
        }
    }
    bool file_tangents = false;
    if (ok && info->tangent != NULL) {
        const size_t tangent_floats = (size_t)vcount * 4U;
        ok = cgltf_accessor_unpack_floats(info->tangent, tmp, tangent_floats) == tangent_floats;
        file_tangents = ok;
        for (uint32_t v = 0; ok && v < vcount; v++) {
            EngModelVertex *vertex = &vertices[v];
            glm_mat4_mulv3(world, &tmp[(size_t)v * 4U], 0.0F, vertex->tangent);
            if (!(glm_vec3_norm2(vertex->tangent) > 0.0F) || !isfinite(vertex->tangent[0]) ||
                !isfinite(vertex->tangent[1]) || !isfinite(vertex->tangent[2])) {
                file_tangents = false; /* inutilisable : on les calcule */
                break;
            }
            glm_vec3_normalize(vertex->tangent);
            /* une matrice qui inverse l'orientation inverse aussi la bitangente */
            const float w = tmp[((size_t)v * 4U) + 3U] < 0.0F ? -1.0F : 1.0F;
            vertex->tangent[3] = flip ? -w : w;
        }
    }
    eng_arena_reset_to(scratch, mark);
    if (!ok) {
        ENG_LOG_ERROR("glTF : accessor illisible (tampon trop court ou valeurs invalides)");
        return false;
    }

    /* Index : lus tels quels, vérifiés, puis décalés vers les sommets globaux. */
    uint32_t *indices = &model->indices[cursor->index];
    if (prim->indices != NULL) {
        if (cgltf_accessor_unpack_indices(prim->indices, indices, sizeof(uint32_t),
                                          info->index_count) != info->index_count) {
            ENG_LOG_ERROR("glTF : index illisibles");
            return false;
        }
    } else {
        for (uint32_t i = 0; i < info->index_count; i++) {
            indices[i] = i;
        }
    }
    for (uint32_t i = 0; i < info->index_count; i++) {
        if (indices[i] >= vcount) {
            ENG_LOG_ERROR("glTF : index %u hors limites (%u sommets)", indices[i], vcount);
            return false;
        }
        indices[i] += base;
    }
    if (flip) {
        for (uint32_t i = 0; i < info->index_count; i += 3) {
            const uint32_t t = indices[i + 1];
            indices[i + 1] = indices[i + 2];
            indices[i + 2] = t;
        }
    }
    if (info->normal == NULL) {
        smooth_normals(model->vertices, vcount, indices, info->index_count, base);
    }
    if (!file_tangents &&
        !compute_tangents(model, scratch, indices, info->index_count, base, vcount)) {
        return false;
    }

    EngModelPart *part = &model->parts[cursor->part];
    part->first_index = cursor->index;
    part->index_count = info->index_count;
    part->material =
        prim->material != NULL ? 1U + (uint32_t)cgltf_material_index(data, prim->material) : 0U;
    cursor->vertex += vcount;
    cursor->index += info->index_count;
    cursor->part++;
    return true;
}

/* ── Images ── */

static bool decode_data_uri(const char *uri, EngImage *out)
{
    const char *comma = strchr(uri, ',');
    if (comma == NULL || comma - uri < 7 || strstr(uri, ";base64") == NULL) {
        ENG_LOG_ERROR("glTF : data: URI d'image non base64");
        return false;
    }
    const char *b64 = comma + 1;
    const size_t length = strlen(b64);
    size_t padding = 0;
    while (padding < length && padding < 2 && b64[length - 1 - padding] == '=') {
        padding++;
    }
    const cgltf_size size = ((length / 4U) * 3U) - padding;
    cgltf_options options = {0};
    options.memory.alloc_func = gltf_alloc;
    options.memory.free_func = gltf_release;
    void *bytes = NULL;
    if (cgltf_load_buffer_base64(&options, size, b64, &bytes) != cgltf_result_success) {
        ENG_LOG_ERROR("glTF : base64 d'image invalide");
        return false;
    }
    const bool ok = eng_image_decode(bytes, size, out);
    SDL_free(bytes);
    return ok;
}

static bool load_image(const cgltf_image *image, const char *directory, EngArena *scratch,
                       EngImage *out)
{
    if (image->buffer_view != NULL) {
        const cgltf_buffer_view *view = image->buffer_view;
        if (view->buffer->data == NULL) {
            ENG_LOG_ERROR("glTF : tampon d'image non chargé");
            return false;
        }
        return eng_image_decode((const uint8_t *)view->buffer->data + view->offset, view->size,
                                out);
    }
    if (image->uri == NULL) {
        ENG_LOG_ERROR("glTF : image sans données");
        return false;
    }
    if (strncmp(image->uri, "data:", 5) == 0) {
        return decode_data_uri(image->uri, out);
    }
    if (directory == NULL) {
        ENG_LOG_ERROR("glTF : image externe « %s » sans chemin de fichier d'origine", image->uri);
        return false;
    }
    char decoded[512];
    if (strlen(image->uri) >= sizeof decoded) {
        ENG_LOG_ERROR("glTF : URI d'image trop longue");
        return false;
    }
    SDL_strlcpy(decoded, image->uri, sizeof decoded);
    cgltf_decode_uri(decoded);
    char path[1024];
    if (SDL_snprintf(path, sizeof path, "%s%s", directory, decoded) >= (int)sizeof path) {
        ENG_LOG_ERROR("glTF : chemin d'image trop long");
        return false;
    }
    return eng_image_load(scratch, path, out);
}

/* Dossier du fichier, séparateur final inclus ; "" si le chemin n'en a pas. */
static void directory_of(const char *path, char *out, size_t out_size)
{
    SDL_strlcpy(out, path, out_size);
    char *slash = strrchr(out, '/');
    char *backslash = strrchr(out, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash)) {
        slash = backslash;
    }
    if (slash != NULL) {
        slash[1] = '\0';
    } else {
        out[0] = '\0';
    }
}

/* Matériau par défaut de la spécification glTF 2.0 : blanc, métal 1, rugosité 1, sans émission. */
static EngModelMaterial default_material(void)
{
    return (EngModelMaterial){.base_color = {1.0F, 1.0F, 1.0F, 1.0F},
                              .image = -1,
                              .metallic = 1.0F,
                              .roughness = 1.0F,
                              .metallic_roughness_image = -1,
                              .normal_scale = 1.0F,
                              .normal_image = -1,
                              .occlusion_strength = 1.0F,
                              .occlusion_image = -1,
                              .emissive = {0.0F, 0.0F, 0.0F},
                              .emissive_image = -1};
}

/* Indice de l'image d'une texture de matériau (décodée si besoin) dans *out, -1 sans texture.
 * false si l'image est illisible. */
static bool material_image(EngModel *model, EngArena *scratch, const cgltf_data *data,
                           const cgltf_texture_view *view, const char *directory,
                           cgltf_size material, int32_t *out)
{
    *out = -1;
    if (view->texture == NULL || view->texture->image == NULL) {
        return true;
    }
    if (view->texcoord != 0) {
        ENG_LOG_WARN("glTF : matériau %zu : seul TEXCOORD_0 est géré", (size_t)material);
    }
    const cgltf_size image_index = cgltf_image_index(data, view->texture->image);
    if (model->images == NULL || image_index >= model->image_count) {
        ENG_LOG_ERROR("glTF : matériau %zu : image introuvable", (size_t)material);
        return false;
    }
    if (model->images[image_index].pixels == NULL &&
        !load_image(view->texture->image, directory, scratch, &model->images[image_index])) {
        return false;
    }
    *out = (int32_t)image_index;
    return true;
}

/* ── API ── */

static bool build(EngModel *model, EngArena *arena, EngArena *scratch, cgltf_data *data,
                  const char *base_path)
{
    Totals totals;
    if (!count_totals(data, &totals)) {
        return false;
    }
    model->vertex_count = (uint32_t)totals.vertices;
    model->index_count = (uint32_t)totals.indices;
    model->part_count = totals.parts;
    model->material_count = 1U + (uint32_t)data->materials_count;
    model->image_count = (uint32_t)data->images_count;
    model->vertices = ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, model->vertex_count);
    model->indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, model->index_count);
    model->parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, model->part_count);
    model->materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, model->material_count);
    model->images =
        model->image_count != 0
            ? eng_arena_alloc_array(arena, sizeof(EngImage), model->image_count, alignof(EngImage))
            : NULL;
    if (model->vertices == NULL || model->indices == NULL || model->parts == NULL ||
        model->materials == NULL || (model->image_count != 0 && model->images == NULL)) {
        ENG_LOG_ERROR("glTF : arène trop petite pour le modèle");
        return false;
    }
    for (uint32_t i = 0; i < model->image_count; i++) {
        model->images[i] = (EngImage){0};
    }

    /* Matériaux : le 0 est celui par défaut (valeurs par défaut de la spécification glTF). */
    model->materials[0] = default_material();
    char directory[512] = "";
    if (base_path != NULL) {
        directory_of(base_path, directory, sizeof directory);
    }
    const char *image_directory = base_path != NULL ? directory : NULL;
    for (cgltf_size m = 0; m < data->materials_count; m++) {
        const cgltf_material *src = &data->materials[m];
        EngModelMaterial *dst = &model->materials[m + 1];
        *dst = default_material();
        if (src->has_pbr_metallic_roughness) {
            const cgltf_pbr_metallic_roughness *pbr = &src->pbr_metallic_roughness;
            for (int c = 0; c < 4; c++) {
                dst->base_color[c] = pbr->base_color_factor[c];
            }
            dst->metallic = pbr->metallic_factor;
            dst->roughness = pbr->roughness_factor;
            if (!material_image(model, scratch, data, &pbr->base_color_texture, image_directory, m,
                                &dst->image) ||
                !material_image(model, scratch, data, &pbr->metallic_roughness_texture,
                                image_directory, m, &dst->metallic_roughness_image)) {
                return false;
            }
        }
        /* l'échelle et la force n'ont de valeur que si la texture existe (cgltf les laisse à 0) */
        if (src->normal_texture.texture != NULL) {
            dst->normal_scale = src->normal_texture.scale;
        }
        if (src->occlusion_texture.texture != NULL) {
            dst->occlusion_strength = src->occlusion_texture.scale;
        }
        const float strength =
            src->has_emissive_strength ? src->emissive_strength.emissive_strength : 1.0F;
        for (int c = 0; c < 3; c++) {
            dst->emissive[c] = src->emissive_factor[c] * strength;
        }
        if (!material_image(model, scratch, data, &src->normal_texture, image_directory, m,
                            &dst->normal_image) ||
            !material_image(model, scratch, data, &src->occlusion_texture, image_directory, m,
                            &dst->occlusion_image) ||
            !material_image(model, scratch, data, &src->emissive_texture, image_directory, m,
                            &dst->emissive_image)) {
            return false;
        }
    }

    Cursor cursor = {0};
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        const cgltf_node *node = &data->nodes[n];
        if (node->mesh == NULL) {
            continue;
        }
        for (cgltf_size p = 0; p < node->mesh->primitives_count; p++) {
            PrimInfo info;
            if (inspect_primitive(&node->mesh->primitives[p], &info) != PRIM_OK) {
                continue; /* déjà signalée par count_totals */
            }
            if (!fill_primitive(model, scratch, data, node, &node->mesh->primitives[p], &info,
                                &cursor)) {
                return false;
            }
        }
    }

    for (int a = 0; a < 3; a++) {
        model->bounds_min[a] = model->vertices[0].position[a];
        model->bounds_max[a] = model->vertices[0].position[a];
    }
    for (uint32_t v = 1; v < model->vertex_count; v++) {
        for (int a = 0; a < 3; a++) {
            const float p = model->vertices[v].position[a];
            model->bounds_min[a] = fminf(model->bounds_min[a], p);
            model->bounds_max[a] = fmaxf(model->bounds_max[a], p);
        }
    }
    if (data->animations_count != 0 || data->skins_count != 0) {
        ENG_LOG_INFO("glTF : %zu animation(s) et %zu skin(s) ignorée(s)",
                     (size_t)data->animations_count, (size_t)data->skins_count);
    }
    return true;
}

static bool unsupported_extension(const cgltf_data *data)
{
    static const char *const unsupported[] = {"KHR_draco_mesh_compression",
                                              "EXT_meshopt_compression", "KHR_texture_basisu"};
    for (cgltf_size i = 0; i < data->extensions_required_count; i++) {
        for (size_t u = 0; u < sizeof unsupported / sizeof unsupported[0]; u++) {
            if (strcmp(data->extensions_required[i], unsupported[u]) == 0) {
                ENG_LOG_ERROR("glTF : extension obligatoire non gérée : %s", unsupported[u]);
                return true;
            }
        }
    }
    return false;
}

bool eng_model_parse(EngArena *arena, EngArena *scratch, const void *data, size_t size,
                     const char *base_path, EngModel *model)
{
    *model = (EngModel){0};
    if (data == NULL || size == 0) {
        ENG_LOG_ERROR("glTF : données vides");
        return false;
    }
    cgltf_options options = {0};
    options.memory.alloc_func = gltf_alloc;
    options.memory.free_func = gltf_release;
    cgltf_data *gltf = NULL;
    cgltf_result result = cgltf_parse(&options, data, size, &gltf);
    if (result != cgltf_result_success) {
        ENG_LOG_ERROR("glTF : analyse impossible : %s", gltf_error(result));
        return false;
    }
    bool ok = !unsupported_extension(gltf);
    if (ok) {
        result = cgltf_load_buffers(&options, gltf, base_path);
        if (result != cgltf_result_success) {
            ENG_LOG_ERROR("glTF : tampons illisibles : %s", gltf_error(result));
            ok = false;
        }
    }
    if (ok) {
        result = cgltf_validate(gltf);
        if (result != cgltf_result_success) {
            ENG_LOG_ERROR("glTF : fichier invalide (validation)");
            ok = false;
        }
    }
    if (ok) {
        const EngArenaMark arena_mark = eng_arena_mark(arena);
        const EngArenaMark scratch_mark = eng_arena_mark(scratch);
        ok = build(model, arena, scratch, gltf, base_path);
        eng_arena_reset_to(scratch, scratch_mark);
        if (!ok) {
            eng_model_free(model);
            eng_arena_reset_to(arena, arena_mark);
        }
    }
    cgltf_free(gltf);
    if (!ok) {
        *model = (EngModel){0};
    }
    return ok;
}

bool eng_model_load(EngArena *arena, EngArena *scratch, const char *path, EngModel *model)
{
    *model = (EngModel){0};
    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        ENG_LOG_ERROR("glTF : chemin « %s » trop long", path);
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData file;
    bool ok = false;
    if (eng_file_read_all(scratch, resolved, &file)) {
        /* le fichier reste dans `scratch` pendant l'analyse, qui s'en sert aussi : on réserve la
         * lecture, l'analyse restitue ensuite sa propre part */
        ok = eng_model_parse(arena, scratch, file.data, file.size, resolved, model);
    } else {
        ENG_LOG_ERROR("glTF : lecture impossible : %s", resolved);
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}

void eng_model_free(EngModel *model)
{
    if (model->images != NULL) {
        for (uint32_t i = 0; i < model->image_count; i++) {
            eng_image_free(&model->images[i]);
        }
    }
    *model = (EngModel){0};
}

bool eng_model_make_box(EngArena *arena, const float box_min[3], const float box_max[3],
                        const EngModelMaterial *material, EngModel *model)
{
    *model = (EngModel){0};
    for (int a = 0; a < 3; a++) {
        if (!isfinite(box_min[a]) || !isfinite(box_max[a]) || !(box_max[a] > box_min[a])) {
            ENG_LOG_ERROR("modèle procédural : boîte vide ou non numérique");
            return false;
        }
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    EngModelVertex *vertices = ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, 24);
    uint32_t *indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, 36);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, 1);
    EngModelMaterial *materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, 2);
    if (vertices == NULL || indices == NULL || parts == NULL || materials == NULL) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("modèle procédural : arène trop petite");
        return false;
    }
    float center[3];
    float half[3];
    for (int a = 0; a < 3; a++) {
        center[a] = 0.5F * (box_min[a] + box_max[a]);
        half[a] = 0.5F * (box_max[a] - box_min[a]);
    }
    /* une face par direction : normale n, tangente t (un axe), bitangente b = n x t ; les quatre
     * coins (-t, -b), (+t, -b), (+t, +b), (-t, +b) tournent dans le sens direct vu de l'extérieur
     */
    for (int face = 0; face < 6; face++) {
        const int axis = face / 2;
        const float sign = (face % 2) == 0 ? 1.0F : -1.0F;
        const int t_axis = axis == 0 ? 1 : 0;
        vec3 n = {0.0F, 0.0F, 0.0F};
        vec3 t = {0.0F, 0.0F, 0.0F};
        n[axis] = sign;
        t[t_axis] = 1.0F;
        vec3 b;
        glm_vec3_cross(n, t, b);
        for (int corner = 0; corner < 4; corner++) {
            const float st = (corner == 1 || corner == 2) ? 1.0F : -1.0F;
            const float sb = corner >= 2 ? 1.0F : -1.0F;
            EngModelVertex *v = &vertices[(face * 4) + corner];
            for (int a = 0; a < 3; a++) {
                v->position[a] =
                    center[a] + (n[a] * half[a]) + (t[a] * st * half[a]) + (b[a] * sb * half[a]);
                v->normal[a] = n[a];
                v->tangent[a] = t[a];
            }
            v->tangent[3] = 1.0F;
            v->uv[0] = (st + 1.0F) * 0.5F;
            v->uv[1] = (sb + 1.0F) * 0.5F;
        }
        const uint32_t base = (uint32_t)face * 4U;
        const uint32_t quad[6] = {0, 1, 2, 0, 2, 3};
        for (uint32_t i = 0; i < 6; i++) {
            indices[((uint32_t)face * 6U) + i] = base + quad[i];
        }
    }
    parts[0] = (EngModelPart){.first_index = 0, .index_count = 36, .material = 1};
    materials[0] = default_material();
    materials[1] = material != NULL ? *material : default_material();
    model->vertices = vertices;
    model->vertex_count = 24;
    model->indices = indices;
    model->index_count = 36;
    model->parts = parts;
    model->part_count = 1;
    model->materials = materials;
    model->material_count = 2;
    for (int a = 0; a < 3; a++) {
        model->bounds_min[a] = box_min[a];
        model->bounds_max[a] = box_max[a];
    }
    return true;
}
