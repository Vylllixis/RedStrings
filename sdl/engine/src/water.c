#include "engine/water.h"

#include <math.h>

#include "engine/log.h"
#include "engine/model_gpu.h"

#define TWO_PI 6.28318530717958647692
#define PI_F 3.14159265358979323846F
#define SPEC_POWER 256.0F

EngWaterParams eng_water_default(float level)
{
    return (EngWaterParams){.level = level,
                            .shallow_color = {0.10F, 0.32F, 0.30F},
                            .deep_color = {0.01F, 0.06F, 0.10F},
                            .absorption = 0.45F,
                            .f0 = 0.02F,
                            .ripple_amplitude = 0.05F,
                            .ripple_scale = 1.3F,
                            .ripple_speed = 0.8F};
}

static bool finite_rgb(const float c[3])
{
    return isfinite(c[0]) && isfinite(c[1]) && isfinite(c[2]) && c[0] >= 0.0F && c[1] >= 0.0F &&
           c[2] >= 0.0F;
}

bool eng_water_valid(const EngWaterParams *p)
{
    return p != NULL && isfinite(p->level) && finite_rgb(p->shallow_color) &&
           finite_rgb(p->deep_color) && isfinite(p->absorption) && p->absorption > 0.0F &&
           isfinite(p->f0) && p->f0 >= 0.0F && p->f0 <= 1.0F && isfinite(p->ripple_amplitude) &&
           p->ripple_amplitude >= 0.0F && p->ripple_amplitude <= 0.5F &&
           isfinite(p->ripple_scale) && p->ripple_scale > 0.0F && isfinite(p->ripple_speed) &&
           p->ripple_speed >= 0.0F;
}

static float saturate(float x)
{
    return x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x);
}

void eng_water_body_color(const EngWaterParams *p, float depth, float out[3])
{
    const float d = depth > 0.0F ? depth : 0.0F;
    const float t = 1.0F - expf(-p->absorption * d);
    for (int c = 0; c < 3; c++) {
        out[c] = p->shallow_color[c] + ((p->deep_color[c] - p->shallow_color[c]) * t);
    }
}

float eng_water_fresnel(float f0, float cos_theta)
{
    const float m = 1.0F - saturate(cos_theta);
    const float m2 = m * m;
    return f0 + ((1.0F - f0) * m2 * m2 * m);
}

static float wrapped_phase(double rate, double time_seconds)
{
    const double turns = rate * time_seconds / TWO_PI;
    return (float)((turns - floor(turns)) * TWO_PI);
}

void eng_water_normal(const EngWaterParams *p, float x, float z, double time_seconds, float out[3])
{
    const float phase1 = wrapped_phase((double)p->ripple_speed, time_seconds);
    const float phase2 = wrapped_phase(1.3 * (double)p->ripple_speed, time_seconds);
    const float k1 = p->ripple_scale;
    const float k2 = 1.7F * p->ripple_scale;
    const float w1 = cosf((k1 * ((0.8F * x) + (0.6F * z))) + phase1);
    const float w2 = cosf((k2 * ((-0.6F * x) + (0.8F * z))) + phase2);
    const float gx = p->ripple_amplitude * ((w1 * 0.8F) + (0.5F * w2 * -0.6F));
    const float gz = p->ripple_amplitude * ((w1 * 0.6F) + (0.5F * w2 * 0.8F));
    const float length = sqrtf((gx * gx) + 1.0F + (gz * gz));
    out[0] = -gx / length;
    out[1] = 1.0F / length;
    out[2] = -gz / length;
}

void eng_water_shade(const EngWaterParams *p, const EngWaterSky *sky, const EngWaterLight *light,
                     const float camera[3], const float world[3], float depth, double time_seconds,
                     float out[3])
{
    float body[3];
    eng_water_body_color(p, depth, body);
    float n[3];
    eng_water_normal(p, world[0], world[2], time_seconds, n);
    float v[3] = {camera[0] - world[0], camera[1] - world[1], camera[2] - world[2]};
    const float vl = sqrtf((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
    if (vl > 1e-12F) {
        for (int c = 0; c < 3; c++) {
            v[c] /= vl;
        }
    } else {
        v[0] = 0.0F;
        v[1] = 1.0F;
        v[2] = 0.0F;
    }
    const float nv = (n[0] * v[0]) + (n[1] * v[1]) + (n[2] * v[2]);
    const float fresnel = eng_water_fresnel(p->f0, nv);
    float r[3];
    for (int c = 0; c < 3; c++) {
        r[c] = (2.0F * nv * n[c]) - v[c];
    }
    const float sky_mix = sqrtf(saturate(r[1]));
    const float nl = saturate((n[0] * light->sun_direction[0]) + (n[1] * light->sun_direction[1]) +
                              (n[2] * light->sun_direction[2]));
    const float rl = saturate((r[0] * light->sun_direction[0]) + (r[1] * light->sun_direction[1]) +
                              (r[2] * light->sun_direction[2]));
    const float spec = powf(rl, SPEC_POWER) * fresnel;
    for (int c = 0; c < 3; c++) {
        const float lit = body[c] * (light->ambient[c] + (light->sun_radiance[c] * nl / PI_F));
        const float reflected = sky->horizon[c] + ((sky->zenith[c] - sky->horizon[c]) * sky_mix);
        out[c] = lit + ((reflected - lit) * fresnel) + (light->sun_radiance[c] * spec);
    }
}

bool eng_water_pack(const EngWaterParams *p, const EngWaterSky *sky, double time_seconds,
                    float out[ENG_WATER_UNIFORM_FLOATS])
{
    if (!eng_water_valid(p) || !finite_rgb(sky->horizon) || !finite_rgb(sky->zenith) ||
        !isfinite(time_seconds)) {
        ENG_LOG_ERROR("eau : paramètres invalides");
        return false;
    }
    for (int c = 0; c < 3; c++) {
        out[c] = sky->horizon[c];
        out[4 + c] = sky->zenith[c];
        out[8 + c] = p->shallow_color[c];
        out[12 + c] = p->deep_color[c];
    }
    out[3] = p->f0;
    out[7] = p->absorption;
    out[11] = p->ripple_amplitude;
    out[15] = wrapped_phase((double)p->ripple_speed, time_seconds);
    out[16] = p->ripple_scale;
    out[17] = wrapped_phase(1.3 * (double)p->ripple_speed, time_seconds);
    out[18] = 0.0F;
    out[19] = 0.0F;
    return true;
}

static bool cell_is_wet(const EngHeightmap *map, float level, uint32_t x, uint32_t z)
{
    for (uint32_t dz = 0; dz < 2U; dz++) {
        for (uint32_t dx = 0; dx < 2U; dx++) {
            if (eng_heightmap_altitude(map, x + dx, z + dz) < level) {
                return true;
            }
        }
    }
    return false;
}

static bool map_valid(const EngHeightmap *map)
{
    return map != NULL && map->samples != NULL && map->size >= ENG_HEIGHTMAP_MIN_SIZE &&
           map->spacing > 0.0F && map->altitude_max > map->altitude_min;
}

uint32_t eng_water_wet_cells(const EngHeightmap *map, const EngWaterParams *p)
{
    if (!map_valid(map) || !eng_water_valid(p)) {
        return 0;
    }
    uint32_t count = 0;
    for (uint32_t z = 0; z + 1U < map->size; z++) {
        for (uint32_t x = 0; x + 1U < map->size; x++) {
            count += cell_is_wet(map, p->level, x, z) ? 1U : 0U;
        }
    }
    return count;
}

bool eng_water_build(EngArena *arena, const EngHeightmap *map, const EngWaterParams *p,
                     float origin_x, float origin_z, EngModel *model)
{
    *model = (EngModel){0};
    if (!map_valid(map) || !eng_water_valid(p) || !isfinite(origin_x) || !isfinite(origin_z)) {
        ENG_LOG_ERROR("eau : champ ou paramètres invalides");
        return false;
    }
    /* rectangle englobant des cellules mouillées */
    uint32_t x0 = map->size;
    uint32_t x1 = 0;
    uint32_t z0 = map->size;
    uint32_t z1 = 0;
    uint32_t wet = 0;
    for (uint32_t z = 0; z + 1U < map->size; z++) {
        for (uint32_t x = 0; x + 1U < map->size; x++) {
            if (cell_is_wet(map, p->level, x, z)) {
                wet++;
                x0 = x < x0 ? x : x0;
                x1 = x > x1 ? x : x1;
                z0 = z < z0 ? z : z0;
                z1 = z > z1 ? z : z1;
            }
        }
    }
    if (wet == 0) {
        return true;
    }
    const uint32_t width = x1 - x0 + 2U; /* sommets par ligne */
    const uint32_t height = z1 - z0 + 2U;
    const size_t vertex_count = (size_t)width * height;
    if (vertex_count > ENG_MODEL_MAX_VERTICES || (size_t)wet * 6U > ENG_MODEL_MAX_INDICES) {
        ENG_LOG_ERROR("eau : %zu sommets ou %u cellules : trop pour un maillage", vertex_count,
                      wet);
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    EngModelVertex *vertices = ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, vertex_count);
    uint32_t *indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, (size_t)wet * 6U);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, 1);
    EngModelMaterial *materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, 1);
    if (vertices == NULL || indices == NULL || parts == NULL || materials == NULL) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("eau : arène trop petite (%zu sommets, %u cellules)", vertex_count, wet);
        return false;
    }
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t i = 0; i < width; i++) {
            EngModelVertex *v = &vertices[((size_t)j * width) + i];
            const uint32_t sx = x0 + i;
            const uint32_t sz = z0 + j;
            v->position[0] = origin_x + ((float)sx * map->spacing);
            v->position[1] = p->level;
            v->position[2] = origin_z + ((float)sz * map->spacing);
            v->normal[0] = 0.0F;
            v->normal[1] = 1.0F;
            v->normal[2] = 0.0F;
            const float depth = p->level - eng_heightmap_altitude(map, sx, sz);
            v->uv[0] = depth > 0.0F ? depth : 0.0F;
            v->uv[1] = 0.0F;
            v->tangent[0] = 1.0F;
            v->tangent[1] = 0.0F;
            v->tangent[2] = 0.0F;
            v->tangent[3] = 1.0F;
        }
    }
    uint32_t *out = indices;
    for (uint32_t z = z0; z <= z1; z++) {
        for (uint32_t x = x0; x <= x1; x++) {
            if (!cell_is_wet(map, p->level, x, z)) {
                continue;
            }
            const uint32_t a = ((z - z0) * width) + (x - x0); /* (x, z) */
            const uint32_t b = a + 1U;                        /* (x + 1, z) */
            const uint32_t c = a + width;                     /* (x, z + 1) */
            const uint32_t d = c + 1U;
            /* anti-horaire vu du dessus (+Y) : x vers la droite, z vers le bas de l'écran */
            const uint32_t quad[6] = {a, c, b, b, c, d};
            for (int k = 0; k < 6; k++) {
                *out++ = quad[k];
            }
        }
    }
    parts[0] = (EngModelPart){.first_index = 0, .index_count = wet * 6U, .material = 0};
    materials[0] = (EngModelMaterial){.base_color = {1.0F, 1.0F, 1.0F, 1.0F},
                                      .image = -1,
                                      .metallic = 0.0F,
                                      .roughness = 0.05F,
                                      .metallic_roughness_image = -1,
                                      .normal_scale = 1.0F,
                                      .normal_image = -1,
                                      .occlusion_strength = 1.0F,
                                      .occlusion_image = -1,
                                      .emissive_image = -1};
    model->vertices = vertices;
    model->vertex_count = (uint32_t)vertex_count;
    model->indices = indices;
    model->index_count = wet * 6U;
    model->parts = parts;
    model->part_count = 1;
    model->materials = materials;
    model->material_count = 1;
    model->bounds_min[0] = origin_x + ((float)x0 * map->spacing);
    model->bounds_min[1] = p->level;
    model->bounds_min[2] = origin_z + ((float)z0 * map->spacing);
    model->bounds_max[0] = origin_x + ((float)(x1 + 1U) * map->spacing);
    model->bounds_max[1] = p->level;
    model->bounds_max[2] = origin_z + ((float)(z1 + 1U) * map->spacing);
    return true;
}

void eng_water_pipeline_desc(EngPipelineDesc *desc)
{
    eng_model_pipeline_desc(desc);
    desc->name = "water";
    desc->fragment_shader = "water.frag";
}
