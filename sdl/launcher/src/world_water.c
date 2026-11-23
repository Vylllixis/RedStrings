/* Eau de la carte du monde : voir world_water.h. */
#include "world_water.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

#define FLOOR_THICKNESS 10.0F

bool world_water_init(WorldWater *w, EngRenderer *renderer, EngPhysics *physics, EngArena *arena,
                      const EngWorldMap *map)
{
    memset(w, 0, sizeof *w);
    w->floor = ENG_BODY_INVALID;
    if (renderer == NULL || arena == NULL || map == NULL || map->heights == NULL) {
        return false;
    }
    EngArena scratch; /* maillage CPU : jusqu'à 4 sommets par cellule mouillée, et la table */
    const size_t samples = (size_t)map->size * map->size;
    const uint32_t wet = eng_worldmap_wet_cells(map);
    if (wet == 0U || !eng_arena_init(&scratch, "eau-maillage",
                                     ((size_t)wet * ((4U * sizeof(EngModelVertex)) + 24U)) +
                                         (samples * 4U) + 4096U)) {
        return false;
    }
    EngModel model;
    EngPipelineDesc desc;
    eng_water_pipeline_desc(&desc);
    w->pipeline = eng_renderer_create_pipeline(renderer, &desc);
    const bool ok = eng_renderer_pipeline_is_valid(renderer, w->pipeline) &&
                    eng_worldmap_water(map, &scratch, &model) && model.vertex_count > 0U &&
                    eng_model_gpu_create(&w->model, renderer, arena, &model, 0.0F);
    eng_arena_shutdown(&scratch);
    if (!ok) {
        ENG_LOG_WARN("monde : eau indisponible");
        if (eng_renderer_pipeline_is_valid(renderer, w->pipeline)) {
            (void)eng_renderer_destroy_pipeline(renderer, w->pipeline);
        }
        memset(w, 0, sizeof *w);
        w->floor = ENG_BODY_INVALID;
        return false;
    }
    w->params = eng_water_default(map->water);
    /* lacs et rivières de la carte : un peu plus bleus que l'eau verdâtre par défaut */
    w->params.shallow_color[0] = 0.10F;
    w->params.shallow_color[1] = 0.36F;
    w->params.shallow_color[2] = 0.42F;
    w->params.deep_color[0] = 0.01F;
    w->params.deep_color[1] = 0.07F;
    w->params.deep_color[2] = 0.16F;
    w->physics = physics;
    w->side = map->side;
    w->wet_cells = wet;
    w->ready = true;
    ENG_LOG_INFO(
        "monde : eau au niveau %.1f m, %u cellules mouillées, gué à %.1f m sous la surface",
        (double)map->water, wet, (double)WORLD_WADE_DEPTH);
    return true;
}

static void remove_floor(WorldWater *w)
{
    if (w->physics != NULL && w->floor != ENG_BODY_INVALID) {
        eng_physics_remove_body(w->physics, w->floor);
    }
    w->floor = ENG_BODY_INVALID;
}

void world_water_shutdown(WorldWater *w, EngRenderer *renderer)
{
    if (w == NULL || !w->ready) {
        return;
    }
    remove_floor(w);
    if (renderer != NULL) {
        eng_model_gpu_destroy(&w->model, renderer);
        if (eng_renderer_pipeline_is_valid(renderer, w->pipeline)) {
            (void)eng_renderer_destroy_pipeline(renderer, w->pipeline);
        }
    }
    w->ready = false;
}

float world_water_floor(const WorldWater *w)
{
    return w != NULL && w->ready ? w->params.level - WORLD_WADE_DEPTH : -INFINITY;
}

void world_water_place_floor(WorldWater *w, const EngWorldOrigin *origin)
{
    if (w == NULL || !w->ready || w->physics == NULL || origin == NULL) {
        return;
    }
    remove_floor(w);
    /* une grande dalle sous toute la carte : seuls les lacs et les rivières descendent jusqu'à elle
     */
    const double center_world[3] = {0.0, (double)(world_water_floor(w) - FLOOR_THICKNESS), 0.0};
    float center[3];
    eng_origin_to_local(origin, center_world, center);
    const float half[3] = {0.5F * w->side, FLOOR_THICKNESS, 0.5F * w->side};
    w->floor = eng_physics_add_box(w->physics, half, center, ENG_LAYER_STATIC);
    if (w->floor == ENG_BODY_INVALID) {
        ENG_LOG_WARN("monde : fond de gué refusé par la physique");
    }
}

void world_water_set_sky(WorldWater *w, const EngAtmosphere *atmosphere, const float sun[3],
                         float sky_intensity)
{
    EngSkyDesc sky;
    if (w == NULL || !w->ready || atmosphere == NULL || sun == NULL ||
        !eng_atmosphere_to_sky_desc(atmosphere, sun, &sky)) {
        return;
    }
    /* à l'horizon le ciel est presque blanc : l'eau, vue de biais, serait blanche elle aussi ; on
     * lui fait refléter un ciel un peu plus haut (mélange vers le zénith) pour qu'elle reste bleue
     */
    for (int c = 0; c < 3; c++) {
        const float horizon = (0.45F * sky.horizon[c]) + (0.55F * sky.zenith[c]);
        w->sky.horizon[c] = horizon * sky_intensity * 0.85F;
        w->sky.zenith[c] = sky.zenith[c] * sky_intensity;
    }
}

void world_water_draw(const WorldWater *w, EngRenderQueue *queue, const EngWorldOrigin *origin,
                      const float fog[ENG_FOG_UNIFORM_FLOATS], mat4 view_proj,
                      const EngLighting *light)
{
    if (w == NULL || !w->ready || queue == NULL || origin == NULL || fog == NULL) {
        return;
    }
    float tail[ENG_FOG_UNIFORM_FLOATS + ENG_WATER_UNIFORM_FLOATS];
    memcpy(tail, fog, ENG_FOG_UNIFORM_FLOATS * sizeof(float));
    if (!eng_water_pack(&w->params, &w->sky, w->time_s, tail + ENG_FOG_UNIFORM_FLOATS)) {
        return;
    }
    const double zero[3] = {0.0, 0.0, 0.0}; /* les sommets sont en coordonnées de la carte */
    float corner[3];
    eng_origin_to_local(origin, zero, corner);
    mat4 model;
    glm_translate_make(model, (vec3){corner[0], corner[1], corner[2]});
    const EngModelDrawOptions options = {.fragment_tail = tail,
                                         .fragment_tail_floats =
                                             ENG_FOG_UNIFORM_FLOATS + ENG_WATER_UNIFORM_FLOATS};
    (void)eng_model_gpu_draw_ex(&w->model, queue, w->pipeline, model, view_proj, light, NULL,
                                &options);
}
