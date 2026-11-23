/* treebake : écriture des modèles (glTF 2.0 + .bin) et des images. Voir treebake.h. */
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "treebake.h"

#define MAX_PARTS ((2U * (TB_MAX_MATERIALS + 1U)) + 1U)
#define MAX_IMAGES (MAX_PARTS * 3U)
#define VIEWS 5 /* par partie : positions, normales, UV, tangentes, index */

bool tb_write_png(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height)
{
    stbi_write_png_compression_level = 9;
    return stbi_write_png(path, (int)width, (int)height, 4, rgba, (int)width * 4) != 0;
}

bool tb_write_jpg(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height,
                  int quality)
{
    /* le JPEG n'a pas d'alpha : on n'écrit que la couleur */
    uint8_t *rgb = malloc((size_t)width * height * 3U);
    if (rgb == NULL) {
        return false;
    }
    for (size_t i = 0; i < (size_t)width * height; i++) {
        memcpy(&rgb[i * 3U], &rgba[i * 4U], 3U);
    }
    const bool ok = stbi_write_jpg(path, (int)width, (int)height, 3, rgb, quality) != 0;
    free(rgb);
    return ok;
}

bool tb_copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    FILE *out = in != NULL ? fopen(to, "wb") : NULL;
    bool ok = in != NULL && out != NULL;
    static char block[65536];
    size_t n = 0;
    while (ok && (n = fread(block, 1, sizeof block, in)) > 0U) {
        ok = fwrite(block, 1, n, out) == n;
    }
    if (in != NULL) {
        ok = fclose(in) == 0 && ok;
    }
    if (out != NULL) {
        ok = fclose(out) == 0 && ok;
    }
    return ok;
}

/* ── glTF ── */

typedef struct Layout { /* où une partie est rangée dans le .bin */
    size_t offset[VIEWS];
    size_t bytes[VIEWS];
    float low[3];
    float high[3];
} Layout;

typedef struct Writer {
    FILE *json;
    FILE *bin;
    size_t offset; /* octets déjà écrits dans le .bin */
    const TbPart *parts[MAX_PARTS];
    Layout layout[MAX_PARTS];
    uint32_t part_count;
    const char *images[MAX_IMAGES]; /* noms de fichiers distincts */
    uint32_t image_count;
    bool ok;
} Writer;

static void write_block(Writer *w, const void *data, size_t bytes)
{
    static const char zero[4] = {0, 0, 0, 0};
    const size_t pad = (4U - (bytes % 4U)) % 4U; /* chaque vue commence sur 4 octets */
    w->ok = w->ok && fwrite(data, 1, bytes, w->bin) == bytes;
    w->ok = w->ok && fwrite(zero, 1, pad, w->bin) == pad;
    w->offset += bytes + pad;
}

/* une colonne des sommets (positions, normales ou UV) : `width` flottants à `offset` du sommet */
static void write_column(Writer *w, const TbMeshOut *m, size_t offset, size_t width, size_t *at,
                         size_t *bytes)
{
    float *column = malloc((size_t)m->vertex_count * width * sizeof(float));
    if (column == NULL) {
        w->ok = false;
        return;
    }
    for (size_t i = 0; i < m->vertex_count; i++) {
        memcpy(&column[i * width], (const char *)&m->vertices[i] + offset, width * sizeof(float));
    }
    *at = w->offset;
    *bytes = (size_t)m->vertex_count * width * sizeof(float);
    write_block(w, column, *bytes);
    free(column);
}

static void write_part_data(Writer *w, uint32_t p)
{
    const TbMeshOut *m = w->parts[p]->mesh;
    Layout *l = &w->layout[p];
    for (int a = 0; a < 3; a++) {
        l->low[a] = m->vertices[0].position[a];
        l->high[a] = m->vertices[0].position[a];
    }
    for (size_t i = 0; i < m->vertex_count; i++) {
        for (int a = 0; a < 3; a++) {
            const float v = m->vertices[i].position[a];
            l->low[a] = v < l->low[a] ? v : l->low[a];
            l->high[a] = v > l->high[a] ? v : l->high[a];
        }
    }
    write_column(w, m, offsetof(EngBakeVertex, position), 3U, &l->offset[0], &l->bytes[0]);
    write_column(w, m, offsetof(EngBakeVertex, normal), 3U, &l->offset[1], &l->bytes[1]);
    write_column(w, m, offsetof(EngBakeVertex, uv), 2U, &l->offset[2], &l->bytes[2]);
    if (m->tangents != NULL) {
        l->offset[3] = w->offset;
        l->bytes[3] = (size_t)m->vertex_count * 4U * sizeof(float);
        write_block(w, m->tangents, l->bytes[3]);
    }
    l->offset[4] = w->offset;
    l->bytes[4] = (size_t)m->triangle_count * 3U * sizeof(uint32_t);
    write_block(w, m->indices, l->bytes[4]);
}

/* indice de l'image `name` (ajoutée si elle est nouvelle) ; -1 si elle est absente */
static int image_index(Writer *w, const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    for (uint32_t i = 0; i < w->image_count; i++) {
        if (strcmp(w->images[i], name) == 0) {
            return (int)i;
        }
    }
    if (w->image_count >= MAX_IMAGES) {
        w->ok = false;
        return -1;
    }
    w->images[w->image_count] = name;
    return (int)w->image_count++;
}

static void write_accessors(Writer *w)
{
    FILE *f = w->json;
    uint32_t views = 0;
    (void)fprintf(f, "\"bufferViews\":[");
    for (uint32_t p = 0; p < w->part_count; p++) {
        for (int k = 0; k < VIEWS; k++) {
            if (w->layout[p].bytes[k] != 0U) {
                (void)fprintf(
                    f, "%s{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":%d}",
                    views++ > 0U ? "," : "", w->layout[p].offset[k], w->layout[p].bytes[k],
                    k == 4 ? 34963 : 34962);
            }
        }
    }
    (void)fprintf(f, "],\"accessors\":[");
    uint32_t view = 0;
    for (uint32_t p = 0; p < w->part_count; p++) {
        const TbMeshOut *m = w->parts[p]->mesh;
        const Layout *l = &w->layout[p];
        const unsigned n = (unsigned)m->vertex_count;
        (void)fprintf(f,
                      "%s{\"bufferView\":%u,\"componentType\":5126,\"count\":%u,\"type\":\"VEC3\","
                      "\"min\":[%.9g,%.9g,%.9g],\"max\":[%.9g,%.9g,%.9g]}",
                      p > 0U ? "," : "", view++, n, (double)l->low[0], (double)l->low[1],
                      (double)l->low[2], (double)l->high[0], (double)l->high[1],
                      (double)l->high[2]);
        (void)fprintf(f,
                      ",{\"bufferView\":%u,\"componentType\":5126,\"count\":%u,\"type\":\"VEC3\"}",
                      view++, n);
        (void)fprintf(f,
                      ",{\"bufferView\":%u,\"componentType\":5126,\"count\":%u,\"type\":\"VEC2\"}",
                      view++, n);
        if (m->tangents != NULL) {
            (void)fprintf(
                f, ",{\"bufferView\":%u,\"componentType\":5126,\"count\":%u,\"type\":\"VEC4\"}",
                view++, n);
        }
        (void)fprintf(
            f, ",{\"bufferView\":%u,\"componentType\":5125,\"count\":%u,\"type\":\"SCALAR\"}",
            view++, (unsigned)(m->triangle_count * 3U));
    }
    (void)fprintf(f, "],");
}

static void write_materials(Writer *w)
{
    FILE *f = w->json;
    int texture[MAX_PARTS][3];
    for (uint32_t p = 0; p < w->part_count; p++) {
        texture[p][0] = image_index(w, w->parts[p]->color_uri);
        texture[p][1] = image_index(w, w->parts[p]->normal_uri);
        texture[p][2] = image_index(w, w->parts[p]->arm_uri);
    }
    if (w->image_count > 0U) { /* une texture par image, même numéro */
        (void)fprintf(f, "\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987}],\"images\":[");
        for (uint32_t i = 0; i < w->image_count; i++) {
            (void)fprintf(f, "%s{\"uri\":\"%s\"}", i > 0U ? "," : "", w->images[i]);
        }
        (void)fprintf(f, "],\"textures\":[");
        for (uint32_t i = 0; i < w->image_count; i++) {
            (void)fprintf(f, "%s{\"sampler\":0,\"source\":%u}", i > 0U ? "," : "", (unsigned)i);
        }
        (void)fprintf(f, "],");
    }
    (void)fprintf(f, "\"materials\":[");
    for (uint32_t p = 0; p < w->part_count; p++) {
        const TbPart *part = w->parts[p];
        (void)fprintf(f, "%s{\"name\":\"%s\",\"pbrMetallicRoughness\":{", p > 0U ? "," : "",
                      part->material_name);
        if (texture[p][0] >= 0) {
            (void)fprintf(f, "\"baseColorTexture\":{\"index\":%d},", texture[p][0]);
        }
        if (texture[p][2] >= 0) { /* G : rugosité, B : métal ; R : occlusion (plus bas) */
            (void)fprintf(f, "\"metallicRoughnessTexture\":{\"index\":%d},", texture[p][2]);
            (void)fprintf(f, "\"metallicFactor\":1,\"roughnessFactor\":1}");
        } else {
            (void)fprintf(f, "\"metallicFactor\":0,\"roughnessFactor\":0.85}");
        }
        if (texture[p][1] >= 0) {
            (void)fprintf(f, ",\"normalTexture\":{\"index\":%d}", texture[p][1]);
        }
        if (texture[p][2] >= 0) {
            (void)fprintf(f, ",\"occlusionTexture\":{\"index\":%d}", texture[p][2]);
        }
        if (part->foliage) {
            (void)fprintf(f, ",\"alphaMode\":\"MASK\",\"alphaCutoff\":0.5,\"doubleSided\":true");
        }
        (void)fprintf(f, "}");
    }
    (void)fprintf(f, "],");
}

bool tb_write_gltf(const char *dir, const char *name, const TbPart *parts, uint32_t part_count)
{
    static Writer w; /* ~4 Kio : hors pile */
    memset(&w, 0, sizeof w);
    for (uint32_t p = 0; p < part_count && w.part_count < MAX_PARTS; p++) {
        if (parts[p].mesh != NULL && parts[p].mesh->triangle_count > 0U &&
            parts[p].mesh->vertex_count > 0U) {
            w.parts[w.part_count++] = &parts[p];
        }
    }
    if (w.part_count == 0U) {
        return false;
    }
    char path[TB_PATH];
    (void)snprintf(path, sizeof path, "%s/%s.gltf", dir, name);
    w.json = fopen(path, "wb");
    (void)snprintf(path, sizeof path, "%s/%s.bin", dir, name);
    w.bin = fopen(path, "wb");
    w.ok = w.json != NULL && w.bin != NULL;
    for (uint32_t p = 0; w.ok && p < w.part_count; p++) {
        write_part_data(&w, p);
    }
    if (w.ok) {
        (void)fprintf(w.json,
                      "{\"asset\":{\"version\":\"2.0\",\"generator\":\"treebake\"},\"scene\":0,"
                      "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0,\"name\":\"%s\"}],"
                      "\"buffers\":[{\"uri\":\"%s.bin\",\"byteLength\":%zu}],",
                      name, name, w.offset);
        write_accessors(&w);
        write_materials(&w);
        (void)fprintf(w.json, "\"meshes\":[{\"name\":\"%s\",\"primitives\":[", name);
        uint32_t accessor = 0;
        for (uint32_t p = 0; p < w.part_count; p++) {
            const bool tangents = w.parts[p]->mesh->tangents != NULL;
            (void)fprintf(w.json,
                          "%s{\"attributes\":{\"POSITION\":%u,\"NORMAL\":%u,\"TEXCOORD_0\":%u",
                          p > 0U ? "," : "", accessor, accessor + 1U, accessor + 2U);
            if (tangents) {
                (void)fprintf(w.json, ",\"TANGENT\":%u", accessor + 3U);
            }
            (void)fprintf(w.json, "},\"indices\":%u,\"material\":%u,\"mode\":4}",
                          accessor + (tangents ? 4U : 3U), (unsigned)p);
            accessor += tangents ? 5U : 4U;
        }
        (void)fprintf(w.json, "]}]}\n");
    }
    if (w.json != NULL) {
        w.ok = fclose(w.json) == 0 && w.ok;
    }
    if (w.bin != NULL) {
        w.ok = fclose(w.bin) == 0 && w.ok;
    }
    return w.ok;
}
