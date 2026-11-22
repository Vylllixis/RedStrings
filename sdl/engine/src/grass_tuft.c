/* Touffe d'herbe (2026-11-22) : plusieurs brins partant d'un même pied, pour l'herbe de la carte
 * du monde. Voir engine/grass.h. */
#include <math.h>

#include "engine/grass.h"
#include "engine/log.h"

#define ROWS 3U          /* pied, milieu, pointe */
#define FACE_VERTICES 5U /* deux au pied, deux au milieu, la pointe */
#define MID_HEIGHT 0.45F
#define GRADIENT_ROWS 16U

static uint32_t next(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *state = x;
    return x;
}

static float unit(uint32_t *state)
{
    return (float)(next(state) >> 8U) / 16777216.0F; /* [0, 1[ */
}

uint32_t eng_grass_tuft_vertex_count(uint32_t blades)
{
    return blades * 2U * FACE_VERTICES;
}

uint32_t eng_grass_tuft_triangle_count(uint32_t blades)
{
    return blades * 2U * 3U; /* par face : un quadrilatère et la pointe */
}

static uint8_t srgb8(float linear)
{
    const float c = linear < 0.0F ? 0.0F : (linear > 1.0F ? 1.0F : linear);
    const float s = c <= 0.0031308F ? 12.92F * c : (1.055F * powf(c, 1.0F / 2.4F)) - 0.055F;
    return (uint8_t)lroundf(s * 255.0F);
}

/* image 1 x GRADIENT_ROWS : la pointe en haut (v = 0), le pied en bas (v = 1) */
static bool gradient(const float root[3], const float tip[3], EngImage *out)
{
    uint8_t rgba[GRADIENT_ROWS * 4U];
    for (uint32_t row = 0; row < GRADIENT_ROWS; row++) {
        const float t = 1.0F - ((float)row / (float)(GRADIENT_ROWS - 1U)); /* 1 en haut */
        for (int c = 0; c < 3; c++) {
            rgba[(row * 4U) + (uint32_t)c] = srgb8(root[c] + ((tip[c] - root[c]) * t * t));
        }
        rgba[(row * 4U) + 3U] = 255U;
    }
    return eng_image_create(1U, GRADIENT_ROWS, rgba, out);
}

/* un sommet du brin : `t` hauteur (0 pied, 1 pointe), `side` -1, 0 ou +1 */
static void blade_vertex(EngModelVertex *v, const float base[2], const float dir[2], float lean,
                         float height, float width, float t, float side, float face)
{
    const float half = t >= 1.0F ? 0.0F : 0.5F * width * (1.0F - (0.35F * t / MID_HEIGHT));
    const float out = lean * t * t; /* courbé vers l'extérieur */
    v->position[0] = base[0] + (dir[0] * out) - (dir[1] * half * side);
    v->position[1] = height * t;
    v->position[2] = base[1] + (dir[1] * out) + (dir[0] * half * side);
    /* normale tournée vers le haut, un peu vers l'extérieur du brin : l'herbe s'éclaire comme le
     * sol, sans faces noires à contre-jour */
    const float n[3] = {0.35F * dir[0] * face, 1.0F, 0.35F * dir[1] * face};
    const float len = sqrtf((n[0] * n[0]) + 1.0F + (n[2] * n[2]));
    v->normal[0] = n[0] / len;
    v->normal[1] = n[1] / len;
    v->normal[2] = n[2] / len;
    v->uv[0] = 0.5F;
    v->uv[1] = 1.0F - t;
    v->tangent[0] = -dir[1];
    v->tangent[1] = 0.0F;
    v->tangent[2] = dir[0];
    v->tangent[3] = 1.0F;
}

bool eng_grass_tuft_build(EngArena *arena, uint32_t blades, uint32_t seed, const float root[3],
                          const float tip[3], EngModel *model)
{
    if (model == NULL) {
        return false;
    }
    *model = (EngModel){0};
    if (arena == NULL || blades < 3U || blades > 12U || root == NULL || tip == NULL) {
        ENG_LOG_ERROR("herbe : touffe invalide (3 à 12 brins, couleurs du pied et de la pointe)");
        return false;
    }
    const uint32_t vertex_count = eng_grass_tuft_vertex_count(blades);
    const uint32_t triangles = eng_grass_tuft_triangle_count(blades);
    const EngArenaMark mark = eng_arena_mark(arena);
    EngModelVertex *vertices = ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, vertex_count);
    uint32_t *indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, (size_t)triangles * 3U);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, 1);
    EngModelMaterial *materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, 2);
    EngImage *images = ENG_ARENA_NEW_ARRAY(arena, EngImage, 1);
    if (vertices == NULL || indices == NULL || parts == NULL || materials == NULL ||
        images == NULL || !gradient(root, tip, &images[0])) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("herbe : arène trop petite pour la touffe");
        return false;
    }
    uint32_t state = (seed * 2654435761U) | 1U;
    uint32_t *out = indices;
    for (uint32_t b = 0; b < blades; b++) {
        const float angle = 6.2831853F * (((float)b + (0.7F * unit(&state))) / (float)blades);
        const float dir[2] = {cosf(angle), sinf(angle)};
        const float offset = 0.03F + (0.09F * unit(&state));
        const float base[2] = {dir[0] * offset, dir[1] * offset};
        const float height = b == 0U ? 1.0F : 0.55F + (0.45F * unit(&state)); /* un brin de 1 m */
        const float lean = 0.2F + (0.4F * unit(&state));
        const float width = 0.2F + (0.1F * unit(&state)); /* brins larges : herbe stylisée */
        for (uint32_t face = 0; face < 2U; face++) {
            const uint32_t first = ((b * 2U) + face) * FACE_VERTICES;
            const float sign = face == 0U ? 1.0F : -1.0F;
            EngModelVertex *v = &vertices[first];
            blade_vertex(&v[0], base, dir, lean, height, width, 0.0F, -1.0F, sign);
            blade_vertex(&v[1], base, dir, lean, height, width, 0.0F, 1.0F, sign);
            blade_vertex(&v[2], base, dir, lean, height, width, MID_HEIGHT, -1.0F, sign);
            blade_vertex(&v[3], base, dir, lean, height, width, MID_HEIGHT, 1.0F, sign);
            blade_vertex(&v[4], base, dir, lean, height, width, 1.0F, 0.0F, sign);
            /* face 0 : vue de l'extérieur (du côté de `dir`) ; face 1 : l'ordre inverse */
            const uint32_t order[9] = {0, 2, 1, 1, 2, 3, 2, 4, 3};
            for (uint32_t k = 0; k < 9U; k++) {
                *out++ = first + (face == 0U ? order[k] : order[8U - k]);
            }
        }
    }
    parts[0] = (EngModelPart){.first_index = 0, .index_count = triangles * 3U, .material = 1};
    materials[0] = eng_grass_default_material();
    materials[1] = eng_grass_default_material();
    materials[1].base_color[0] = 1.0F; /* la couleur vient du dégradé */
    materials[1].base_color[1] = 1.0F;
    materials[1].base_color[2] = 1.0F;
    materials[1].image = 0;
    model->vertices = vertices;
    model->vertex_count = vertex_count;
    model->indices = indices;
    model->index_count = triangles * 3U;
    model->parts = parts;
    model->part_count = 1;
    model->materials = materials;
    model->material_count = 2;
    model->images = images;
    model->image_count = 1;
    for (int a = 0; a < 3; a++) {
        model->bounds_min[a] = INFINITY;
        model->bounds_max[a] = -INFINITY;
    }
    for (uint32_t i = 0; i < vertex_count; i++) {
        for (int a = 0; a < 3; a++) {
            model->bounds_min[a] = fminf(model->bounds_min[a], vertices[i].position[a]);
            model->bounds_max[a] = fmaxf(model->bounds_max[a], vertices[i].position[a]);
        }
    }
    return true;
}
