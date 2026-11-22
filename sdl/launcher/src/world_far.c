/* Vue lointaine de la carte du monde : voir world_far.h. */
#include "world_far.h"

#include <math.h>
#include <string.h>

#include "engine/heightmap.h"
#include "engine/log.h"
#include "engine/splatmap.h"
#include "engine/terrain.h"

#define FAR_SINK_M 1.5F
#define CHUNK_CELLS 8U /* 8 x 32 m = 256 m : un morceau par cellule du monde */

static bool far_heightmap(EngArena *scratch, const EngWorldMap *map, EngHeightmap *out)
{
    const uint32_t size = (uint32_t)lroundf(map->side / WORLD_FAR_SPACING) + 1U;
    uint16_t *samples = eng_arena_alloc_array(scratch, sizeof(uint16_t), (size_t)size * size, 2U);
    if (samples == NULL) {
        return false;
    }
    const float lo = map->height_min - FAR_SINK_M - 1.0F;
    const float hi = map->height_max + 1.0F;
    const double half = 0.5 * (double)map->side;
    for (uint32_t j = 0; j < size; j++) {
        for (uint32_t i = 0; i < size; i++) {
            const double x = -half + ((double)i * WORLD_FAR_SPACING);
            const double z = -half + ((double)j * WORLD_FAR_SPACING);
            float h = eng_worldmap_height(map, x, z) - FAR_SINK_M;
            /* près de l'eau (une maille lointaine autour), le relief lointain passe sous la
             * surface : sinon ses grandes mailles cacheraient les rivières et le bord des lacs */
            if (eng_worldmap_near_water(map, x, z, (double)WORLD_FAR_SPACING)) {
                h = fminf(h, map->water - 1.0F);
            }
            samples[((size_t)j * size) + i] = (uint16_t)lroundf(65535.0F * (h - lo) / (hi - lo));
        }
    }
    *out = (EngHeightmap){.size = size,
                          .spacing = WORLD_FAR_SPACING,
                          .altitude_min = lo,
                          .altitude_max = hi,
                          .samples = samples};
    return true;
}

/* six matières pour chaque sommet de la grille lointaine (comme les cellules : world_view.c) */
static void far_materials(const EngWorldMap *map, EngSplatmap *splat, uint8_t *splat2)
{
    const double half = 0.5 * (double)map->side;
    for (uint32_t j = 0; j < splat->size; j++) {
        for (uint32_t i = 0; i < splat->size; i++) {
            const size_t at = ((size_t)j * splat->size) + i;
            uint8_t *px = &splat->pixels[at * 4U];
            float base[ENG_SPLAT_LAYERS];
            for (uint32_t l = 0; l < ENG_SPLAT_LAYERS; l++) {
                base[l] = (float)px[l] / 255.0F;
            }
            const double x = -half + ((double)i * WORLD_FAR_SPACING);
            const double z = -half + ((double)j * WORLD_FAR_SPACING);
            float w[ENG_BIOME_COUNT];
            float path = 0.0F;
            eng_worldmap_weights(map, x, z, w, &path);
            float mixed[ENG_TERRAIN_MATERIALS];
            eng_biome_splat6(w, base, path, eng_worldmap_lava(map, x, z), mixed);
            eng_splat6_quantize(mixed, eng_biome_shade(w), px, &splat2[at * 4U]);
        }
    }
}

bool world_far_init(WorldFar *f, EngRenderer *renderer, EngArena *arena, EngArena *scratch,
                    const EngWorldMap *map, const EngEnvironment *env)
{
    memset(f, 0, sizeof *f);
    if (renderer == NULL || map == NULL || map->heights == NULL) {
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngHeightmap hm;
    EngTerrain terrain;
    EngSplatmap splat;
    const EngModelMaterial material = eng_terrain_splat_material();
    const EngTerrainParams params = {.chunk_cells = CHUNK_CELLS, .material = &material};
    const EngSplatParams sp = eng_splat_default_params();
    bool ok = far_heightmap(scratch, map, &hm) &&
              eng_terrain_build(scratch, &hm, &params, &terrain) &&
              eng_splatmap_generate(scratch, &hm, &sp, &splat) &&
              terrain.chunks_per_side * terrain.chunks_per_side <= WORLD_FAR_MAX_CHUNKS;
    uint8_t *splat2 =
        ok ? eng_arena_alloc_array(scratch, 4U, (size_t)splat.size * splat.size, 4U) : NULL;
    ok = ok && splat2 != NULL;
    if (ok) {
        far_materials(map, &splat, splat2);
        ok = eng_model_gpu_create(&f->model, renderer, arena, &terrain.model, 8.0F);
    }
    if (ok) {
        const EngTextureDesc d1 = {.name = "monde-lointain-splat",
                                   .width = splat.size,
                                   .height = splat.size,
                                   .format = ENG_TEXTURE_RGBA8,
                                   .pixels = splat.pixels,
                                   .mip_levels = 1};
        const EngTextureDesc d2 = {.name = "monde-lointain-splat2",
                                   .width = splat.size,
                                   .height = splat.size,
                                   .format = ENG_TEXTURE_RGBA8,
                                   .pixels = splat2,
                                   .mip_levels = 1};
        f->splat = eng_renderer_create_texture(renderer, &d1);
        f->splat2 = eng_renderer_create_texture(renderer, &d2);
        ok = eng_renderer_texture_is_valid(renderer, f->splat) &&
             eng_renderer_texture_is_valid(renderer, f->splat2);
    }
    if (ok) {
        const int32_t first = (int32_t)lround(-0.5 * (double)map->side / ENG_CELL_SIZE);
        f->chunk_count = terrain.chunks_per_side * terrain.chunks_per_side;
        for (uint32_t cz = 0; cz < terrain.chunks_per_side; cz++) {
            for (uint32_t cx = 0; cx < terrain.chunks_per_side; cx++) {
                const uint32_t k = eng_terrain_chunk_index(&terrain, cx, cz);
                memcpy(f->chunk_min[k], terrain.chunks[k].bounds_min, sizeof f->chunk_min[k]);
                memcpy(f->chunk_max[k], terrain.chunks[k].bounds_max, sizeof f->chunk_max[k]);
                f->chunk_cell[k] = (EngCellCoord){first + (int32_t)cx, first + (int32_t)cz};
            }
        }
        f->corner[0] = -0.5 * (double)map->side;
        f->corner[2] = -0.5 * (double)map->side;
        if (env != NULL) {
            (void)eng_model_gpu_set_environment(&f->model, renderer, env);
        }
    }
    eng_arena_reset_to(scratch, mark);
    if (!ok) {
        ENG_LOG_WARN("monde : vue lointaine indisponible");
        world_far_shutdown(f, renderer);
        return false;
    }
    f->ready = true;
    ENG_LOG_INFO("monde : vue lointaine : %u x %u sommets (tous les %.0f m), %u morceaux", hm.size,
                 hm.size, (double)WORLD_FAR_SPACING, f->chunk_count);
    return true;
}

void world_far_shutdown(WorldFar *f, EngRenderer *renderer)
{
    if (f == NULL || renderer == NULL) {
        return;
    }
    if (f->model.part_count > 0U) {
        eng_model_gpu_destroy(&f->model, renderer);
    }
    const EngTextureHandle tex[2] = {f->splat, f->splat2};
    for (int i = 0; i < 2; i++) {
        if (tex[i].handle.generation != 0) {
            (void)eng_renderer_destroy_texture(renderer, tex[i]);
        }
    }
    memset(f, 0, sizeof *f);
}

void world_far_draw(WorldFar *f, EngRenderQueue *queue, EngPipelineHandle pipeline,
                    const EngTerrainGpu *shared, const EngWorldOrigin *origin, mat4 view_proj,
                    const EngLighting *light, const EngModelShadow *shadow,
                    const EngFrustum *frustum, const bool *skip)
{
    if (f == NULL || !f->ready || queue == NULL || shared == NULL || origin == NULL) {
        return;
    }
    float corner[3];
    eng_origin_to_local(origin, f->corner, corner);
    static uint32_t parts[WORLD_FAR_MAX_CHUNKS];
    uint32_t count = 0;
    for (uint32_t k = 0; k < f->chunk_count; k++) {
        if (skip != NULL && skip[k]) {
            continue;
        }
        float lo[3];
        float hi[3];
        for (int a = 0; a < 3; a++) {
            lo[a] = f->chunk_min[k][a] + corner[a];
            hi[a] = f->chunk_max[k][a] + corner[a];
        }
        if (frustum == NULL || eng_frustum_intersects_aabb(frustum, lo, hi)) {
            parts[count++] = k;
        }
    }
    f->drawn = count;
    if (count == 0U) {
        return;
    }
    const EngTextureHandle textures[5] = {f->splat, shared->layers[0], shared->layers[1],
                                          shared->layers[2], shared->layers[3]};
    EngTextureHandle tail[ENG_MAX_TAIL_TEXTURES];
    eng_terrain_gpu_tail(shared, f->splat2, tail);
    const EngModelDrawOptions options = {.textures = textures,
                                         .tail_textures = tail,
                                         .sampler = shared->sampler,
                                         .user = {shared->tile_meters, shared->sharpness, 0.0F},
                                         .fragment_tail = shared->fog,
                                         .fragment_tail_floats = ENG_FOG_UNIFORM_FLOATS,
                                         .parts = parts,
                                         .part_count = count};
    mat4 model;
    glm_translate_make(model, (vec3){corner[0], corner[1], corner[2]});
    (void)eng_model_gpu_draw_ex(&f->model, queue, pipeline, model, view_proj, light, shadow,
                                &options);
}
