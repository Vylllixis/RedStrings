/* treebake : lecture de l'arbre source (glTF, par cgltf) et de ses images. Voir treebake.h. */
#include <cgltf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/image.h"
#include "treebake.h"

/* `name` contient-il l'un des fragments de `list` (séparés par des virgules) ? */
static bool matches(const char *name, const char *list)
{
    if (name == NULL || list == NULL) {
        return false;
    }
    const char *p = list;
    while (*p != '\0') {
        const char *end = strchr(p, ',');
        const size_t len = end != NULL ? (size_t)(end - p) : strlen(p);
        char part[TB_NAME];
        if (len > 0U && len < sizeof part) {
            memcpy(part, p, len);
            part[len] = '\0';
            if (strstr(name, part) != NULL) {
                return true;
            }
        }
        p = end != NULL ? end + 1 : p + len;
    }
    return false;
}

static void *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    void *bytes = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long n = ftell(f);
        if (n > 0 && fseek(f, 0, SEEK_SET) == 0) {
            bytes = malloc((size_t)n);
            if (bytes != NULL && fread(bytes, 1, (size_t)n, f) != (size_t)n) {
                free(bytes);
                bytes = NULL;
            }
            *size = (size_t)n;
        }
    }
    (void)fclose(f);
    return bytes;
}

/* décode l'image `path` ; les pixels sont recopiés (malloc) : `owned` les garde pour
 * tb_source_free */
static bool load_image(const char *path, EngBakeImage *out, uint8_t **owned)
{
    size_t size = 0;
    void *bytes = read_file(path, &size);
    if (bytes == NULL) {
        return false;
    }
    EngImage image;
    const bool decoded = eng_image_decode(bytes, size, &image);
    free(bytes);
    if (!decoded) {
        return false;
    }
    const size_t pixels = (size_t)image.width * image.height * 4U;
    uint8_t *copy = malloc(pixels);
    if (copy != NULL) {
        memcpy(copy, image.pixels, pixels);
        *owned = copy;
        out->rgba = copy;
        out->width = image.width;
        out->height = image.height;
    }
    eng_image_free(&image);
    return copy != NULL;
}

/* chemin du fichier d'une texture glTF, à côté du .gltf ; vide si la texture est absente */
static void texture_path(const char *gltf_path, const cgltf_texture *texture, char *out)
{
    out[0] = '\0';
    if (texture == NULL || texture->image == NULL || texture->image->uri == NULL) {
        return;
    }
    const char *slash = strrchr(gltf_path, '/');
    const int dir = slash != NULL ? (int)(slash - gltf_path) + 1 : 0;
    (void)snprintf(out, TB_PATH, "%.*s%s", dir, gltf_path, texture->image->uri);
}

static void read_materials(TbSource *s, const cgltf_data *data, const char *path, const char *trunk,
                           const char *drop)
{
    s->material_count = data->materials_count < TB_MAX_MATERIALS ? (uint32_t)data->materials_count
                                                                 : TB_MAX_MATERIALS;
    for (uint32_t m = 0; m < s->material_count; m++) {
        const cgltf_material *src = &data->materials[m];
        TbMaterial *dst = &s->materials[m];
        (void)snprintf(dst->name, sizeof dst->name, "%s", src->name != NULL ? src->name : "");
        dst->role = matches(dst->name, drop)    ? TB_DROPPED
                    : matches(dst->name, trunk) ? TB_TRUNK
                                                : TB_FOLIAGE;
        if (src->has_pbr_metallic_roughness) {
            texture_path(path, src->pbr_metallic_roughness.base_color_texture.texture,
                         dst->color_path);
            texture_path(path, src->pbr_metallic_roughness.metallic_roughness_texture.texture,
                         dst->arm_path);
        }
        texture_path(path, src->normal_texture.texture, dst->normal_path);
    }
}

static const cgltf_accessor *find_attribute(const cgltf_primitive *p, cgltf_attribute_type type)
{
    for (cgltf_size i = 0; i < p->attributes_count; i++) {
        if (p->attributes[i].type == type && p->attributes[i].index == 0) {
            return p->attributes[i].data;
        }
    }
    return NULL;
}

static bool node_selected(const cgltf_node *node, const char *filter)
{
    return node->mesh != NULL &&
           (filter == NULL || (node->name != NULL && strstr(node->name, filter) != NULL));
}

/* ajoute une primitive du nœud (matrice monde `world`) aux tableaux de la source */
static bool append_primitive(TbSource *s, const cgltf_data *data, const cgltf_primitive *p,
                             const float world[16], uint32_t *vertex_at, uint32_t *triangle_at)
{
    const cgltf_accessor *position = find_attribute(p, cgltf_attribute_type_position);
    const cgltf_accessor *normal = find_attribute(p, cgltf_attribute_type_normal);
    const cgltf_accessor *uv = find_attribute(p, cgltf_attribute_type_texcoord);
    const size_t count = position->count;
    float *buffer = malloc(count * 3U * sizeof(float));
    if (buffer == NULL) {
        return false;
    }
    const uint32_t base = *vertex_at;
    EngBakeVertex *out = &s->vertices[base];
    memset(out, 0, count * sizeof *out);
    (void)cgltf_accessor_unpack_floats(position, buffer, count * 3U);
    for (size_t i = 0; i < count; i++) {
        const float *q = &buffer[i * 3U];
        for (int a = 0; a < 3; a++) {
            out[i].position[a] =
                (world[a] * q[0]) + (world[4 + a] * q[1]) + (world[8 + a] * q[2]) + world[12 + a];
        }
        out[i].normal[1] = 1.0F;
    }
    if (normal != NULL && normal->count == count) {
        (void)cgltf_accessor_unpack_floats(normal, buffer, count * 3U);
        for (size_t i = 0; i < count; i++) {
            const float *q = &buffer[i * 3U];
            float n[3];
            for (int a = 0; a < 3; a++) {
                n[a] = (world[a] * q[0]) + (world[4 + a] * q[1]) + (world[8 + a] * q[2]);
            }
            const float length = sqrtf((n[0] * n[0]) + (n[1] * n[1]) + (n[2] * n[2]));
            if (length > 1e-12F) {
                for (int a = 0; a < 3; a++) {
                    out[i].normal[a] = n[a] / length;
                }
            }
        }
    }
    if (uv != NULL && uv->count == count) {
        (void)cgltf_accessor_unpack_floats(uv, buffer, count * 2U);
        for (size_t i = 0; i < count; i++) {
            out[i].uv[0] = buffer[i * 2U];
            out[i].uv[1] = buffer[(i * 2U) + 1U];
        }
    }
    free(buffer);
    const size_t index_count = p->indices != NULL ? p->indices->count : count;
    const uint8_t material =
        p->material != NULL ? (uint8_t)(p->material - data->materials) : (uint8_t)0U;
    for (size_t i = 0; i + 2U < index_count; i += 3U) {
        for (size_t k = 0; k < 3U; k++) {
            const size_t index =
                p->indices != NULL ? cgltf_accessor_read_index(p->indices, i + k) : i + k;
            s->indices[((size_t)*triangle_at * 3U) + k] = base + (uint32_t)index;
        }
        s->triangle_material[*triangle_at] = material;
        if (material < TB_MAX_MATERIALS) {
            s->materials[material].triangles++;
        }
        (*triangle_at)++;
    }
    *vertex_at += (uint32_t)count;
    return true;
}

bool tb_source_load(TbSource *s, const char *path, const char *node_filter, const char *trunk,
                    const char *drop)
{
    memset(s, 0, sizeof *s);
    cgltf_options options;
    memset(&options, 0, sizeof options);
    cgltf_data *data = NULL;
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success ||
        cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        (void)fprintf(stderr, "treebake : lecture de %s impossible\n", path);
        cgltf_free(data);
        return false;
    }
    read_materials(s, data, path, trunk, drop);
    /* première passe : tailles */
    size_t vertices = 0;
    size_t triangles = 0;
    for (cgltf_size n = 0; n < data->nodes_count; n++) {
        const cgltf_node *node = &data->nodes[n];
        if (!node_selected(node, node_filter)) {
            continue;
        }
        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++) {
            const cgltf_primitive *p = &node->mesh->primitives[i];
            const cgltf_accessor *position = find_attribute(p, cgltf_attribute_type_position);
            if (p->type != cgltf_primitive_type_triangles || position == NULL) {
                continue;
            }
            vertices += position->count;
            triangles += (p->indices != NULL ? p->indices->count : position->count) / 3U;
        }
    }
    if (vertices == 0U || triangles == 0U || vertices > 0xFFFFFFF0U || triangles > 0x55555550U) {
        (void)fprintf(stderr, "treebake : aucun triangle dans les nœuds « %s » de %s\n",
                      node_filter != NULL ? node_filter : "*", path);
        cgltf_free(data);
        return false;
    }
    s->vertices = malloc(vertices * sizeof *s->vertices);
    s->indices = malloc(triangles * 3U * sizeof *s->indices);
    s->triangle_material = malloc(triangles);
    bool ok = s->vertices != NULL && s->indices != NULL && s->triangle_material != NULL;
    uint32_t vertex_at = 0;
    uint32_t triangle_at = 0;
    for (cgltf_size n = 0; ok && n < data->nodes_count; n++) {
        const cgltf_node *node = &data->nodes[n];
        if (!node_selected(node, node_filter)) {
            continue;
        }
        float world[16];
        cgltf_node_transform_world(node, world);
        for (cgltf_size i = 0; ok && i < node->mesh->primitives_count; i++) {
            const cgltf_primitive *p = &node->mesh->primitives[i];
            if (p->type == cgltf_primitive_type_triangles &&
                find_attribute(p, cgltf_attribute_type_position) != NULL) {
                ok = append_primitive(s, data, p, world, &vertex_at, &triangle_at);
            }
        }
    }
    s->vertex_count = vertex_at;
    s->triangle_count = triangle_at;
    cgltf_free(data);
    for (uint32_t m = 0; ok && m < s->material_count; m++) {
        const TbMaterial *material = &s->materials[m];
        if (material->triangles == 0U || material->role == TB_DROPPED) {
            continue;
        }
        if (material->color_path[0] != '\0' &&
            !load_image(material->color_path, &s->images[m].color, &s->pixels[m][0])) {
            (void)fprintf(stderr, "treebake : image %s illisible\n", material->color_path);
            ok = false;
        }
        if (ok && material->arm_path[0] != '\0') {
            (void)load_image(material->arm_path, &s->images[m].arm, &s->pixels[m][1]);
        }
    }
    if (!ok) {
        (void)fprintf(stderr, "treebake : mémoire ou données insuffisantes pour %s\n", path);
        tb_source_free(s);
    }
    return ok;
}

EngBakeMesh tb_source_mesh(const TbSource *s)
{
    return (EngBakeMesh){.vertices = s->vertices,
                         .vertex_count = s->vertex_count,
                         .indices = s->indices,
                         .triangle_count = s->triangle_count,
                         .triangle_material = s->triangle_material,
                         .materials = s->images,
                         .material_count = s->material_count,
                         .occlusion = s->occlusion};
}

void tb_source_free(TbSource *s)
{
    for (uint32_t m = 0; m < TB_MAX_MATERIALS; m++) {
        free(s->pixels[m][0]);
        free(s->pixels[m][1]);
    }
    free(s->vertices);
    free(s->indices);
    free(s->triangle_material);
    free(s->occlusion);
    eng_bake_grid_free(&s->grid);
    memset(s, 0, sizeof *s);
}
