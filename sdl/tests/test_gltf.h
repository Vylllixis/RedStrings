#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Constructeur de fichiers glTF EN MÉMOIRE pour les tests (test_model.c, test_model_gpu.c) : un
 * triangle de référence — positions (0,0,0) (1,0,0) (0,1,0), normales +Z, UV (0,0) (1,0) (0,1),
 * index 0 1 2 — que chaque test déforme (transformation de nœud, attributs absents, données
 * corrompues, matériau, texture...). Sortie .gltf (tampon en data: URI) ou .glb. */

#define BUF_SIZE 8192U

/* PNG 2 x 2 RGB : (255,0,0) (0,255,0) / (0,0,255) (255,255,255), voir test_image.c. */
static const uint8_t PNG_2X2[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xFD,
    0xD4, 0x9A, 0x73, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8,
    0xCF, 0xC0, 0xC0, 0x00, 0xC2, 0x0C, 0xFF, 0x81, 0x00, 0x00, 0x1F, 0xEE, 0x05, 0xFB, 0xF1,
    0xAB, 0xBA, 0x77, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

/* ── Constructeur de fichiers glTF ── */

/* PNG 1 x 1 RGB (128, 128, 128) : en sRGB, 128 vaut 0,2158 en lumière linéaire. */
static const uint8_t PNG_GRAY[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
    0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00,
    0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54, 0x78,
    0x9C, 0x63, 0x68, 0x68, 0x68, 0x00, 0x00, 0x03, 0x04, 0x01, 0x81, 0x4B, 0xD3, 0xD2,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

typedef struct Spec {
    bool no_normals;
    bool no_uvs;
    bool no_indices;
    int index_bytes;         /* 1, 2 (défaut) ou 4 */
    int index_count;         /* 0 : 3 ; sinon 4..6 (les derniers valent 0) */
    int bad_index;           /* > 0 : remplace le troisième index */
    float normal[3];         /* normale de chaque sommet ; (0,0,0) : +Z */
    const char *node_json;   /* propriétés ajoutées aux nœuds qui portent le maillage */
    const char *parent_json; /* propriétés du nœud parent, si `hierarchy` */
    bool hierarchy;          /* le nœud du maillage est l'enfant d'un nœud parent */
    int instances;           /* nœuds portant le maillage : 0 -> 1 */
    int mode;                /* mode de la primitive : 0 -> 4 (triangles) */
    bool extra_lines;        /* ajoute une seconde primitive en mode 1 (lignes) */
    const char *material;    /* objet JSON du matériau 0 ; NULL : aucun matériau */
    const char *material2;   /* matériau 1 (non utilisé par la primitive) ; NULL : aucun */
    bool gray_texture;       /* l'image 0 est le PNG gris au lieu du PNG 2 x 2 */
    int png_extra;           /* octets nuls ajoutés après le PNG de l'image 0 (data: URI) */
    bool fourth_vertex;      /* un 4e sommet (5,5,5) que les index n'utilisent pas */
    bool big_count;          /* POSITION annonce 3999 sommets pour un tampon de 3 */
    bool bad_normal_count;   /* NORMAL annonce 2 éléments pour 3 positions */
    bool bad_uv_type;        /* TEXCOORD_0 déclaré en VEC3 */
    bool uv_set1;            /* les UV sont TEXCOORD_1 et non TEXCOORD_0 */
    bool texture;            /* le matériau 0 a la texture 0 (deux images, seule la 0 sert) */
    bool glb;                /* sortie .glb au lieu de .gltf */
    bool png_in_view;        /* glb : l'image 0 est dans le tampon (bufferView) */
    bool truncate;           /* le tampon annonce plus d'octets qu'il n'en contient */
    const char *extra_top;   /* propriétés ajoutées à l'objet racine */
    const char *buffer_uri;  /* gltf : URI du tampon (NULL : data: URI) */
    const char *image_uri;   /* gltf : URI de l'image 0 (NULL : data: URI du PNG) */
    const float *uv_values; /* 6 valeurs (3 UV) à la place de (0,0) (1,0) (0,1) ; NULL : défaut */
    const float *tangent_values; /* 12 valeurs (3 vec4) : attribut TANGENT ; NULL : aucun */
    bool bad_tangent_count;      /* TANGENT annonce 2 éléments pour 3 positions */
} Spec;

static inline void b64_encode(const uint8_t *in, size_t n, char *out)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t b0 = in[i];
        const uint32_t b1 = i + 1 < n ? in[i + 1] : 0;
        const uint32_t b2 = i + 2 < n ? in[i + 2] : 0;
        const uint32_t v = (b0 << 16) | (b1 << 8) | b2;
        out[o++] = T[(v >> 18) & 63];
        out[o++] = T[(v >> 12) & 63];
        out[o++] = i + 1 < n ? T[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < n ? T[v & 63] : '=';
    }
    out[o] = '\0';
}

static inline void put_floats(uint8_t *blob, size_t *at, const float *values, size_t count)
{
    memcpy(&blob[*at], values, count * sizeof(float));
    *at += count * sizeof(float);
}

static inline void align4(size_t *at)
{
    while (*at % 4 != 0) {
        (*at)++;
    }
}

#define APP(...)                                                                                   \
    do {                                                                                           \
        pos += (size_t)SDL_snprintf(&json[pos], sizeof json - pos, __VA_ARGS__);                   \
    } while (0)

/* Écrit le fichier dans `out` ; renvoie sa taille. `blob_out` (facultatif) reçoit le tampon binaire
 * brut (pour les tests de fichiers externes). */
static inline size_t build(const Spec *s, uint8_t *out, size_t cap, uint8_t *blob_out,
                           size_t *blob_size)
{
    uint8_t blob[1024];
    memset(blob, 0, sizeof blob);
    size_t at = 0;
    const float positions[12] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 5, 5, 5};
    float normals[9];
    const float *n = s->normal;
    const bool zero_normal = n[0] == 0.0F && n[1] == 0.0F && n[2] == 0.0F;
    for (int v = 0; v < 3; v++) {
        normals[(v * 3) + 0] = zero_normal ? 0.0F : n[0];
        normals[(v * 3) + 1] = zero_normal ? 0.0F : n[1];
        normals[(v * 3) + 2] = zero_normal ? 1.0F : n[2];
    }
    const float default_uvs[6] = {0, 0, 1, 0, 0, 1};
    const float *uvs = s->uv_values != NULL ? s->uv_values : default_uvs;
    const size_t off_pos = at;
    put_floats(blob, &at, positions, s->fourth_vertex ? 12 : 9);
    const size_t off_nor = at;
    put_floats(blob, &at, normals, 9);
    const size_t off_uv = at;
    put_floats(blob, &at, uvs, 6);
    const size_t off_idx = at;
    const int ibytes = s->index_bytes != 0 ? s->index_bytes : 2;
    const int icount = s->index_count != 0 ? (s->index_count < 6 ? s->index_count : 6) : 3;
    const uint32_t idx[6] = {0, 1, s->bad_index > 0 ? (uint32_t)s->bad_index : 2U, 0, 0, 0};
    for (int i = 0; i < icount; i++) {
        if (ibytes == 1) {
            blob[at++] = (uint8_t)idx[i];
        } else if (ibytes == 2) {
            const uint16_t v = (uint16_t)idx[i];
            memcpy(&blob[at], &v, 2);
            at += 2;
        } else {
            memcpy(&blob[at], &idx[i], 4);
            at += 4;
        }
    }
    const size_t idx_bytes = at - off_idx;
    align4(&at);
    const size_t off_png = at;
    if (s->png_in_view) {
        memcpy(&blob[at], PNG_2X2, sizeof PNG_2X2);
        at += sizeof PNG_2X2;
    }
    align4(&at);
    const size_t off_tan = at;
    if (s->tangent_values != NULL) {
        put_floats(blob, &at, s->tangent_values, 12);
    }
    if (blob_out != NULL) {
        memcpy(blob_out, blob, at);
        *blob_size = at;
    }

    char json[BUF_SIZE];
    char text[2048];
    size_t pos = 0;
    APP("{\"asset\":{\"version\":\"2.0\"}");
    if (s->extra_top != NULL) {
        APP(",%s", s->extra_top);
    }
    const int instances = s->instances != 0 ? s->instances : 1;
    const int first_mesh_node = s->hierarchy ? 1 : 0;
    APP(",\"scene\":0,\"scenes\":[{\"nodes\":[0");
    for (int i = 1; i < instances + first_mesh_node; i++) {
        if (!s->hierarchy) {
            APP(",%d", i);
        }
    }
    APP("]}],\"nodes\":[");
    if (s->hierarchy) {
        APP("{\"children\":[1]%s%s},", s->parent_json != NULL ? "," : "",
            s->parent_json != NULL ? s->parent_json : "");
    }
    for (int i = 0; i < instances; i++) {
        APP("%s{\"mesh\":0%s%s}", i != 0 ? "," : "", s->node_json != NULL ? "," : "",
            s->node_json != NULL ? s->node_json : "");
    }
    APP("],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0");
    if (!s->no_normals) {
        APP(",\"NORMAL\":1");
    }
    if (!s->no_uvs) {
        APP(",\"TEXCOORD_%d\":2", s->uv_set1 ? 1 : 0);
    }
    if (s->tangent_values != NULL) {
        APP(",\"TANGENT\":4");
    }
    APP("}");
    if (!s->no_indices) {
        APP(",\"indices\":3");
    }
    if (s->material != NULL) {
        APP(",\"material\":0");
    }
    if (s->mode != 0) {
        APP(",\"mode\":%d", s->mode);
    }
    APP("}");
    if (s->extra_lines) {
        APP(",{\"attributes\":{\"POSITION\":0},\"mode\":1}");
    }
    APP("]}]");
    if (s->material != NULL) {
        APP(",\"materials\":[%s%s%s]", s->material, s->material2 != NULL ? "," : "",
            s->material2 != NULL ? s->material2 : "");
    }
    if (s->texture) {
        char png64[512];
        char image0[512];
        uint8_t padded[sizeof PNG_2X2 + 2] = {0};
        const uint8_t *image0_bytes = s->gray_texture ? PNG_GRAY : PNG_2X2;
        const size_t image0_size = s->gray_texture ? sizeof PNG_GRAY : sizeof PNG_2X2;
        memcpy(padded, image0_bytes, image0_size);
        b64_encode(padded, image0_size + (size_t)s->png_extra, image0);
        b64_encode(PNG_2X2, sizeof PNG_2X2, png64);
        APP(",\"textures\":[{\"source\":0},{\"source\":1}],\"images\":[");
        if (s->png_in_view) {
            APP("{\"bufferView\":4,\"mimeType\":\"image/png\"}");
        } else if (s->image_uri != NULL) {
            APP("{\"uri\":\"%s\"}", s->image_uri);
        } else {
            APP("{\"uri\":\"data:image/png;base64,%s\"}", image0);
        }
        APP(",{\"uri\":\"data:image/png;base64,%s\"}]", png64);
    }
    const size_t declared = s->truncate ? at + 400 : at;
    if (s->glb) {
        APP(",\"buffers\":[{\"byteLength\":%zu}]", declared);
    } else if (s->buffer_uri != NULL) {
        APP(",\"buffers\":[{\"uri\":\"%s\",\"byteLength\":%zu}]", s->buffer_uri, declared);
    } else {
        b64_encode(blob, at, text);
        APP(",\"buffers\":[{\"uri\":\"data:application/"
            "octet-stream;base64,%s\",\"byteLength\":%zu}]",
            text, declared);
    }
    APP(",\"bufferViews\":[{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%d},"
        "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu}",
        off_pos, s->fourth_vertex ? 48 : 36, off_nor, off_uv, off_idx, idx_bytes);
    if (s->png_in_view) {
        APP(",{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu}", off_png, sizeof PNG_2X2);
    }
    if (s->tangent_values != NULL) {
        APP(",{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":48}", off_tan);
    }
    const int ctype = ibytes == 1 ? 5121 : (ibytes == 2 ? 5123 : 5125);
    APP("],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":%d,\"type\":\"VEC3\","
        "\"min\":[0,0,0],\"max\":[5,5,5]},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":%d,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"%s\"},"
        "{\"bufferView\":3,\"componentType\":%d,\"count\":%d,\"type\":\"SCALAR\"}",
        s->big_count ? 3999 : (s->fourth_vertex ? 4 : 3), s->bad_normal_count ? 2 : 3,
        s->bad_uv_type ? "VEC3" : "VEC2", ctype, icount);
    if (s->tangent_values != NULL) {
        APP(",{\"bufferView\":%d,\"componentType\":5126,\"count\":%d,\"type\":\"VEC4\"}",
            s->png_in_view ? 5 : 4, s->bad_tangent_count ? 2 : 3);
    }
    APP("]}");
    if (!s->glb) {
        SDL_memcpy(out, json, pos + 1 < cap ? pos + 1 : cap);
        return pos;
    }
    /* GLB : en-tête, bloc JSON (complété par des espaces), bloc BIN (complété par des zéros). */
    size_t json_len = pos;
    while (json_len % 4 != 0) {
        json[json_len++] = ' ';
    }
    const uint32_t bin_len = (uint32_t)at;
    const uint32_t total = 12U + 8U + (uint32_t)json_len + 8U + bin_len;
    size_t o = 0;
    const uint32_t header[3] = {0x46546C67U, 2U, total};
    memcpy(&out[o], header, 12);
    o += 12;
    const uint32_t json_head[2] = {(uint32_t)json_len, 0x4E4F534AU};
    memcpy(&out[o], json_head, 8);
    o += 8;
    memcpy(&out[o], json, json_len);
    o += json_len;
    const uint32_t bin_head[2] = {bin_len, 0x004E4942U};
    memcpy(&out[o], bin_head, 8);
    o += 8;
    memcpy(&out[o], blob, bin_len);
    o += bin_len;
    return o;
}
