#include "engine/grass.h"

#include <math.h>

#include "engine/log.h"

#define TWO_PI 6.28318530717958647692
#define MAX_SEGMENTS 8U

EngGrassStyle eng_grass_style_default(void)
{
    return (EngGrassStyle){.wind_direction = {1.0F, 0.0F},
                           .wind_strength = 0.25F,
                           .wind_frequency = 0.35F,
                           .wave_scale = 0.4F,
                           .fade_start = 60.0F,
                           .fade_end = 90.0F};
}

bool eng_grass_style_valid(const EngGrassStyle *s)
{
    return s != NULL && isfinite(s->wind_direction[0]) && isfinite(s->wind_direction[1]) &&
           ((s->wind_direction[0] * s->wind_direction[0]) +
            (s->wind_direction[1] * s->wind_direction[1])) > 1e-12F &&
           isfinite(s->wind_strength) && s->wind_strength >= 0.0F && isfinite(s->wind_frequency) &&
           s->wind_frequency >= 0.0F && isfinite(s->wave_scale) && s->wave_scale >= 0.0F &&
           isfinite(s->fade_start) && s->fade_start >= 0.0F && isfinite(s->fade_end) &&
           s->fade_end > s->fade_start;
}

static float smoothstep01(float t)
{
    const float c = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
    return c * c * (3.0F - (2.0F * c));
}

float eng_grass_fade(const EngGrassStyle *s, float distance)
{
    return 1.0F - smoothstep01((distance - s->fade_start) / (s->fade_end - s->fade_start));
}

static float phase_at(const EngGrassStyle *s, double time_seconds)
{
    /* période en secondes = 1 / f : on garde la fraction de tour pour que la phase reste précise */
    const double turns = (double)s->wind_frequency * time_seconds;
    return (float)((turns - floor(turns)) * TWO_PI);
}

bool eng_grass_pack(const EngGrassStyle *s, const float camera[3], double time_seconds,
                    float out[ENG_GRASS_UNIFORM_FLOATS])
{
    if (!eng_grass_style_valid(s) || !isfinite(time_seconds)) {
        ENG_LOG_ERROR("herbe : style invalide");
        return false;
    }
    const float length = sqrtf((s->wind_direction[0] * s->wind_direction[0]) +
                               (s->wind_direction[1] * s->wind_direction[1]));
    out[0] = camera[0];
    out[1] = camera[1];
    out[2] = camera[2];
    out[3] = s->fade_start;
    out[4] = s->wind_direction[0] / length;
    out[5] = s->wind_direction[1] / length;
    out[6] = s->wind_strength;
    out[7] = s->wave_scale;
    out[8] = phase_at(s, time_seconds);
    out[9] = s->fade_end;
    out[10] = 0.0F;
    out[11] = 0.0F;
    return true;
}

static void rotate_quat(const float q[4], const float v[3], float out[3])
{
    /* v + 2 q.xyz x (q.xyz x v + q.w v) */
    const float c1[3] = {(q[1] * v[2]) - (q[2] * v[1]), (q[2] * v[0]) - (q[0] * v[2]),
                         (q[0] * v[1]) - (q[1] * v[0])};
    const float t[3] = {c1[0] + (q[3] * v[0]), c1[1] + (q[3] * v[1]), c1[2] + (q[3] * v[2])};
    const float c2[3] = {(q[1] * t[2]) - (q[2] * t[1]), (q[2] * t[0]) - (q[0] * t[2]),
                         (q[0] * t[1]) - (q[1] * t[0])};
    for (int i = 0; i < 3; i++) {
        out[i] = v[i] + (2.0F * c2[i]);
    }
}

void eng_grass_vertex_world(const EngGrassStyle *s, const float camera[3], double time_seconds,
                            const EngInstance *in, const float local[3], float out[3])
{
    float packed[ENG_GRASS_UNIFORM_FLOATS] = {0};
    (void)eng_grass_pack(s, camera, time_seconds, packed);
    const float dx = in->position[0] - camera[0];
    const float dy = in->position[1] - camera[1];
    const float dz = in->position[2] - camera[2];
    const float scale = in->scale * eng_grass_fade(s, sqrtf((dx * dx) + (dy * dy) + (dz * dz)));
    const float scaled[3] = {local[0] * scale, local[1] * scale, local[2] * scale};
    float rotated[3];
    rotate_quat(in->rotation, scaled, rotated);
    const float wave =
        0.5F + (0.5F * sinf(packed[8] + (packed[7] * ((packed[4] * in->position[0]) +
                                                      (packed[5] * in->position[2])))));
    const float bend = packed[6] * scale * local[1] * local[1] * wave;
    out[0] = rotated[0] + in->position[0] + (packed[4] * bend);
    out[1] = rotated[1] + in->position[1];
    out[2] = rotated[2] + in->position[2] + (packed[5] * bend);
}

uint32_t eng_grass_blade_vertex_count(uint32_t segments)
{
    return 2U * ((2U * segments) + 1U);
}

uint32_t eng_grass_blade_triangle_count(uint32_t segments)
{
    return 2U * ((2U * segments) - 1U);
}

EngModelMaterial eng_grass_default_material(void)
{
    return (EngModelMaterial){.base_color = {0.16F, 0.36F, 0.07F, 1.0F},
                              .image = -1,
                              .metallic = 0.0F,
                              .roughness = 0.85F,
                              .metallic_roughness_image = -1,
                              .normal_scale = 1.0F,
                              .normal_image = -1,
                              .occlusion_strength = 1.0F,
                              .occlusion_image = -1,
                              .emissive_image = -1};
}

bool eng_grass_blade_build(EngArena *arena, uint32_t segments, float width,
                           const EngModelMaterial *material, EngModel *model)
{
    *model = (EngModel){0};
    if (segments < 1U || segments > MAX_SEGMENTS || !isfinite(width) || !(width > 0.0F)) {
        ENG_LOG_ERROR("herbe : brin invalide (segments 1 à %u, largeur > 0)", MAX_SEGMENTS);
        return false;
    }
    const uint32_t per_face = (2U * segments) + 1U;
    const uint32_t vertex_count = eng_grass_blade_vertex_count(segments);
    const uint32_t triangles = eng_grass_blade_triangle_count(segments);
    const EngArenaMark mark = eng_arena_mark(arena);
    EngModelVertex *vertices = ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, vertex_count);
    uint32_t *indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, (size_t)triangles * 3U);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, 1);
    EngModelMaterial *materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, 2);
    if (vertices == NULL || indices == NULL || parts == NULL || materials == NULL) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("herbe : arène trop petite pour le brin");
        return false;
    }
    /* face 0 (normale +z, anti-horaire vue de +z) puis face 1 (normale -z) */
    for (uint32_t face = 0; face < 2U; face++) {
        const float nz = face == 0U ? 1.0F : -1.0F;
        EngModelVertex *v = &vertices[(size_t)face * per_face];
        for (uint32_t row = 0; row < segments; row++) {
            const float y = (float)row / (float)segments;
            const float half = 0.5F * width * (1.0F - y); /* se rétrécit jusqu'à la pointe */
            for (uint32_t side = 0; side < 2U; side++) {
                EngModelVertex *p = &v[(row * 2U) + side];
                p->position[0] = side == 0U ? -half : half;
                p->position[1] = y;
                p->position[2] = 0.0F;
                p->normal[0] = 0.0F;
                p->normal[1] = 0.0F;
                p->normal[2] = nz;
                p->uv[0] = (float)side;
                p->uv[1] = 1.0F - y;
                p->tangent[0] = nz;
                p->tangent[1] = 0.0F;
                p->tangent[2] = 0.0F;
                p->tangent[3] = 1.0F;
            }
        }
        EngModelVertex *tip = &v[(size_t)2U * segments];
        tip->position[0] = 0.0F;
        tip->position[1] = 1.0F;
        tip->position[2] = 0.0F;
        tip->normal[0] = 0.0F;
        tip->normal[1] = 0.0F;
        tip->normal[2] = nz;
        tip->uv[0] = 0.5F;
        tip->uv[1] = 0.0F;
        tip->tangent[0] = nz;
        tip->tangent[1] = 0.0F;
        tip->tangent[2] = 0.0F;
        tip->tangent[3] = 1.0F;
    }
    uint32_t *out = indices;
    for (uint32_t face = 0; face < 2U; face++) {
        const uint32_t base = face * per_face;
        for (uint32_t row = 0; row + 1U < segments; row++) {
            const uint32_t a = base + (row * 2U); /* gauche bas */
            const uint32_t b = a + 1U;            /* droite bas */
            const uint32_t c = a + 2U;            /* gauche haut */
            const uint32_t d = a + 3U;            /* droite haut */
            const uint32_t quad[6] = {a, b, d, a, d, c};
            for (int k = 0; k < 6; k++) {
                /* la face arrière inverse l'ordre pour rester anti-horaire vue de -z */
                *out++ = face == 0U ? quad[k] : quad[5 - k];
            }
        }
        const uint32_t a = base + ((segments - 1U) * 2U);
        const uint32_t tri[3] = {a, a + 1U, base + (2U * segments)};
        for (int k = 0; k < 3; k++) {
            *out++ = face == 0U ? tri[k] : tri[2 - k];
        }
    }
    parts[0] = (EngModelPart){.first_index = 0, .index_count = triangles * 3U, .material = 1};
    materials[0] = eng_grass_default_material();
    materials[1] = material != NULL ? *material : eng_grass_default_material();
    model->vertices = vertices;
    model->vertex_count = vertex_count;
    model->indices = indices;
    model->index_count = triangles * 3U;
    model->parts = parts;
    model->part_count = 1;
    model->materials = materials;
    model->material_count = 2;
    model->bounds_min[0] = -0.5F * width;
    model->bounds_min[1] = 0.0F;
    model->bounds_min[2] = 0.0F;
    model->bounds_max[0] = 0.5F * width;
    model->bounds_max[1] = 1.0F;
    model->bounds_max[2] = 0.0F;
    return true;
}

void eng_grass_pipeline_desc(EngPipelineDesc *desc)
{
    eng_instanced_pipeline_desc(desc);
    desc->name = "model-grass";
    desc->vertex_shader = "grass.vert";
}
