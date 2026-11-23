/* Herbe de la plaine dans le monde ouvert : voir world_grass.h. */
#include "world_grass.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"
#include "engine/model.h"

/* par semis : pas de la grille (m), hauteur type (m), début et fin de la disparition (m) */
static const float SPACING[WORLD_GRASS_RINGS] = {0.45F, 1.3F};
static const float HEIGHT[WORLD_GRASS_RINGS] = {0.38F, 0.5F};
static const float FADE_START[WORLD_GRASS_RINGS] = {12.0F, 28.0F};
static const float FADE_END[WORLD_GRASS_RINGS] = {22.0F, 46.0F};
/* couleurs de la touffe (RGB linéaire) : le pied se fond dans le sol, la pointe est claire */
static const float ROOT_COLOR[3] = {0.035F, 0.11F, 0.02F};
static const float TIP_COLOR[3] = {0.2F, 0.42F, 0.075F};

bool world_grass_init(WorldGrass *g, EngRenderer *renderer, const EngEnvironment *env)
{
    memset(g, 0, sizeof *g);
    if (renderer == NULL) {
        return false;
    }
    g->renderer = renderer;
    static EngArena cpu;
    static EngArena gpu_tables;
    if (cpu.base == NULL && (!eng_arena_init(&cpu, "herbe", (size_t)1 << 20) ||
                             !eng_arena_init(&gpu_tables, "herbe-gpu", (size_t)1 << 20))) {
        return false;
    }
    eng_arena_reset(&cpu);
    EngModel model;
    bool ok = eng_grass_tuft_build(&cpu, 6U, 7U, ROOT_COLOR, TIP_COLOR, &model);
    ok = ok && eng_model_gpu_create(&g->tuft, renderer, &gpu_tables, &model, 1.0F);
    if (model.vertices != NULL) {
        eng_model_free(&model);
    }
    if (ok && env != NULL) {
        (void)eng_model_gpu_set_environment(&g->tuft, renderer, env);
    }
    EngPipelineDesc desc;
    eng_grass_pipeline_desc(&desc);
    g->pipeline = ok ? eng_renderer_create_pipeline(renderer, &desc) : (EngPipelineHandle){0};
    if (!ok || !eng_renderer_pipeline_is_valid(renderer, g->pipeline)) {
        ENG_LOG_WARN("herbe : touffe ou pipeline indisponible, monde sans herbe");
        if (ok) {
            eng_model_gpu_destroy(&g->tuft, renderer);
        }
        return false;
    }
    g->ready = true;
    return true;
}

static void release(WorldGrass *g)
{
    for (uint32_t r = 0; r < WORLD_GRASS_RINGS; r++) {
        if (eng_renderer_buffer_is_valid(g->renderer, g->ring[r].buffer)) {
            (void)eng_renderer_destroy_buffer(g->renderer, g->ring[r].buffer);
        }
        g->ring[r] = (WorldGrassRing){0};
    }
    g->tufts = 0;
    g->valid = false;
}

void world_grass_shutdown(WorldGrass *g)
{
    if (g == NULL || !g->ready) {
        return;
    }
    release(g);
    eng_model_gpu_destroy(&g->tuft, g->renderer);
    if (eng_renderer_pipeline_is_valid(g->renderer, g->pipeline)) {
        (void)eng_renderer_destroy_pipeline(g->renderer, g->pipeline);
    }
    g->ready = false;
}

void world_grass_shift(WorldGrass *g, const float shift[3])
{
    if (g == NULL || !g->ready || shift == NULL) {
        return;
    }
    for (int a = 0; a < 3; a++) {
        g->camera[a] += shift[a];
    }
    for (uint32_t i = 0; i < g->blocker_count; i++) {
        g->blockers[i][0] += shift[0];
        g->blockers[i][1] += shift[2];
    }
    g->valid = false; /* les touffes sont en coordonnées locales : à refaire */
}

void world_grass_set_blockers(WorldGrass *g, float (*boxes)[5], uint32_t count)
{
    if (g == NULL || !g->ready) {
        return;
    }
    g->blocker_count = 0;
    for (uint32_t i = 0; boxes != NULL && i < count && g->blocker_count < WORLD_GRASS_BLOCKERS;
         i++) {
        memcpy(g->blockers[g->blocker_count++], boxes[i], sizeof g->blockers[0]);
    }
    g->valid = false;
}

/* (x, z) est-il sous une boîte du décor (à 15 cm près) ? */
static bool blocked(const WorldGrass *g, float x, float z)
{
    for (uint32_t i = 0; i < g->blocker_count; i++) {
        const float *b = g->blockers[i];
        const float dx = x - b[0];
        const float dz = z - b[1];
        const float c = cosf(b[4]);
        const float s = sinf(b[4]);
        const float lx = (dx * c) - (dz * s); /* dans le repère de la boîte */
        const float lz = (dx * s) + (dz * c);
        if (fabsf(lx) < b[2] + 0.15F && fabsf(lz) < b[3] + 0.15F) {
            return true;
        }
    }
    return false;
}

void world_grass_update(WorldGrass *g, const EngWorldMap *map, const EngWorldOrigin *origin,
                        WorldGrassGround ground, void *user)
{
    if (g == NULL || !g->ready || map == NULL || origin == NULL) {
        return;
    }
    if (g->valid && hypotf(g->camera[0] - g->center[0], g->camera[2] - g->center[2]) <
                        WORLD_GRASS_MARGIN_M - 0.5F) {
        return;
    }
    release(g);
    g->blocked = 0;
    static EngMapTuft tufts[WORLD_GRASS_MAX];
    static EngInstance list[WORLD_GRASS_MAX];
    double world[3];
    eng_origin_to_world(origin, g->camera, world);
    for (uint32_t r = 0; r < WORLD_GRASS_RINGS; r++) {
        const uint32_t n =
            eng_worldmap_grass(map, world[0], world[2], FADE_END[r] + WORLD_GRASS_MARGIN_M,
                               SPACING[r], HEIGHT[r], 101U + r, tufts, WORLD_GRASS_MAX);
        uint32_t count = 0;
        for (uint32_t i = 0; i < n; i++) {
            const double at[3] = {tufts[i].x, tufts[i].y, tufts[i].z};
            float local[3];
            eng_origin_to_local(origin, at, local);
            if (blocked(g, local[0], local[2])) {
                g->blocked++;
                continue;
            }
            float y = 0.0F;
            if (ground != NULL && ground(user, local[0], local[2], &y)) {
                local[1] = y; /* le sol affiché (triangles), pas la trame interpolée */
            }
            local[1] -= 0.03F; /* le pied un peu enfoncé : jamais de jour sous la touffe */
            const uint8_t tint[4] = {tufts[i].tint[0], tufts[i].tint[1], tufts[i].tint[2], 255U};
            eng_instance_make_yaw(&list[count++], local, tufts[i].scale, tufts[i].yaw, tint);
        }
        if (count > 0U) {
            g->ring[r].buffer = eng_instances_upload(g->renderer, list, count);
            g->ring[r].count =
                eng_renderer_buffer_is_valid(g->renderer, g->ring[r].buffer) ? count : 0U;
        }
        g->tufts += g->ring[r].count;
    }
    memcpy(g->center, g->camera, sizeof g->center);
    g->valid = true;
    if (g->rebuilds++ == 0U) {
        ENG_LOG_INFO("herbe : %u touffes autour de la caméra (%u de près, %u au loin), "
                     "refaites tous les %.0f m",
                     g->tufts, g->ring[0].count, g->ring[1].count, (double)WORLD_GRASS_MARGIN_M);
    }
}

void world_grass_draw(WorldGrass *g, EngRenderQueue *queue, mat4 view_proj,
                      const EngLighting *light, const EngModelShadow *shadow)
{
    if (g == NULL || !g->ready || queue == NULL || light == NULL) {
        return;
    }
    memcpy(g->camera, light->camera_position, sizeof g->camera);
    mat4 identity;
    glm_mat4_identity(identity);
    for (uint32_t r = 0; r < WORLD_GRASS_RINGS; r++) {
        if (g->ring[r].count == 0U) {
            continue;
        }
        EngGrassStyle style = eng_grass_style_default();
        style.wind_strength = 0.3F;
        style.wind_frequency = 0.3F;
        style.wave_scale = 0.35F;
        style.fade_start = FADE_START[r];
        style.fade_end = FADE_END[r];
        float tail[ENG_GRASS_UNIFORM_FLOATS];
        if (!eng_grass_pack(&style, g->camera, g->time_s, tail)) {
            continue;
        }
        const EngModelDrawOptions options = {.instance_buffer = g->ring[r].buffer,
                                             .instance_count = g->ring[r].count,
                                             .vertex_tail = tail,
                                             .vertex_tail_floats = ENG_GRASS_UNIFORM_FLOATS};
        (void)eng_model_gpu_draw_ex(&g->tuft, queue, g->pipeline, identity, view_proj, light,
                                    shadow, &options);
    }
}
