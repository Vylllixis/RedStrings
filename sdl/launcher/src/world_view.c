#include "world_view.h"

#include <SDL3/SDL_timer.h>
#include <math.h>
#include <string.h>

#include "engine/clock.h"
#include "engine/frustum.h"
#include "engine/heightmap.h"
#include "engine/log.h"
#include "engine/mesh_lod.h"
#include "engine/noise.h"
#include "engine/splatmap.h"
#include "engine/terrain.h"

#define CELL_ARENA_BYTES ((size_t)64 << 10)
#define SCRATCH_BYTES ((size_t)8 << 20)
#define SHARED_ARENA_BYTES ((size_t)4 << 20)
#define LOAD_RADIUS 2
/* octets envoyés au GPU pour une cellule : sommets (48 o), index (2 triangles par cellule de la
 * grille, 4 o par index) et splatmap RGBA */
#define CELL_UPLOAD_BYTES                                                                          \
    ((uint32_t)((ENG_CELL_RES * ENG_CELL_RES * 48U) +                                              \
                ((ENG_CELL_RES - 1U) * (ENG_CELL_RES - 1U) * 6U * 4U) +                            \
                (ENG_CELL_RES * ENG_CELL_RES * 4U)))

/* Toutes les cellules quantifient leurs hauteurs sur la MÊME plage : deux cellules voisines donnent
 * exactement le même échantillon 16 bits à leur bord partagé (pas de marche ni de fissure, ni au
 * rendu ni dans Jolt), et la splatmap (altitude relative) est cohérente d'une cellule à l'autre. */
static void height_range(const WorldView *w, float *lo, float *hi)
{
    *lo = -w->gen.altitude - 1.0F;
    *hi = w->gen.altitude + 1.0F;
}

static WorldCellView *find(WorldView *w, EngCellCoord c)
{
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        if (w->cells[i].used && eng_cell_equal(w->cells[i].coord, c)) {
            return &w->cells[i];
        }
    }
    return NULL;
}

/* Coin (x, z) LOCAL d'une cellule. */
static void cell_corner(const WorldView *w, EngCellCoord c, float out[2])
{
    out[0] = (float)(((double)c.x * ENG_CELL_SIZE) - w->origin.x);
    out[1] = (float)(((double)c.z * ENG_CELL_SIZE) - w->origin.z);
}

static void on_ready(void *user, const EngCellData *cell)
{
    WorldView *w = user;
    if (find(w, cell->coord) != NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        WorldCellView *v = &w->cells[i];
        if (!v->used) {
            v->used = true;
            v->uploaded = false;
            v->coord = cell->coord;
            v->body = ENG_BODY_INVALID;
            float corner[2];
            cell_corner(w, cell->coord, corner);
            const float cx = corner[0] + (0.5F * (float)ENG_CELL_SIZE);
            const float cz = corner[1] + (0.5F * (float)ENG_CELL_SIZE);
            (void)eng_upq_push(&w->uploads, i, CELL_UPLOAD_BYTES, sqrtf((cx * cx) + (cz * cz)));
            return;
        }
    }
    ENG_LOG_WARN("monde : plus d'emplacement de rendu pour la cellule (%d, %d)", (int)cell->coord.x,
                 (int)cell->coord.z);
}

static void release(WorldView *w, WorldCellView *v)
{
    if (v->uploaded) {
        eng_model_gpu_destroy(&v->model, w->renderer);
        (void)eng_renderer_destroy_texture(w->renderer, v->splat);
        if (v->body != ENG_BODY_INVALID) {
            eng_physics_remove_body(w->physics, v->body);
            w->bodies_removed++;
        }
        w->uploaded_cells--;
    }
    eng_arena_reset(&v->arena);
    v->used = false;
    v->uploaded = false;
    v->body = ENG_BODY_INVALID;
}

static void on_unload(void *user, const EngCellData *cell)
{
    WorldView *w = user;
    WorldCellView *v = find(w, cell->coord);
    if (v == NULL) {
        return;
    }
    if (!v->uploaded) {
        (void)eng_upq_cancel(&w->uploads, (uint64_t)(v - w->cells));
    }
    release(w, v);
}

/* Champ de hauteurs 16 bits d'une cellule (dans `scratch`). */
static bool cell_heightmap(WorldView *w, const EngCellData *cell, EngHeightmap *map)
{
    uint16_t *samples =
        ENG_ARENA_NEW_ARRAY(&w->scratch, uint16_t, (size_t)ENG_CELL_RES * ENG_CELL_RES);
    if (samples == NULL) {
        return false;
    }
    float lo = 0.0F;
    float hi = 0.0F;
    height_range(w, &lo, &hi);
    for (uint32_t i = 0; i < ENG_CELL_RES * ENG_CELL_RES; i++) {
        const float t = (cell->heights[i] - lo) / (hi - lo);
        const float clamped = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
        samples[i] = (uint16_t)lroundf(clamped * 65535.0F);
    }
    *map = (EngHeightmap){.size = ENG_CELL_RES,
                          .spacing = ENG_CELL_SPACING,
                          .altitude_min = lo,
                          .altitude_max = hi,
                          .samples = samples};
    return true;
}

/* Corps Jolt d'une cellule, au coin LOCAL courant. */
static EngBody add_body(WorldView *w, const EngHeightmap *map, EngCellCoord c)
{
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w->physics, map);
    if (shape == NULL) {
        return ENG_BODY_INVALID;
    }
    float corner[2];
    cell_corner(w, c, corner);
    EngPhysicsPose pose = eng_physics_pose_identity();
    pose.position[0] = corner[0];
    pose.position[2] = corner[1];
    const EngBody body = eng_physics_add_static_shape(w->physics, shape, &pose);
    eng_physics_shape_destroy(shape);
    if (body != ENG_BODY_INVALID) {
        w->bodies_added++;
    }
    return body;
}

/* Grande carte : les matières de la splatmap d'une cellule transformées par le biome de chaque
 * texel (neige en montagne, sable au désert, roche au volcan), fondues aux frontières. */
static void biome_materials(const WorldView *w, EngCellCoord c, EngSplatmap *splat)
{
    const double ox = (double)c.x * ENG_CELL_SIZE;
    const double oz = (double)c.z * ENG_CELL_SIZE;
    for (uint32_t z = 0; z < ENG_CELL_RES; z++) {
        for (uint32_t x = 0; x < ENG_CELL_RES; x++) {
            uint8_t *px = &splat->pixels[(((size_t)z * ENG_CELL_RES) + x) * ENG_SPLAT_LAYERS];
            float base[ENG_SPLAT_LAYERS];
            for (uint32_t l = 0; l < ENG_SPLAT_LAYERS; l++) {
                base[l] = (float)px[l] / 255.0F;
            }
            float weights[ENG_BIOME_COUNT];
            eng_biome_weights(&w->gen.biomes, ox + ((double)x * ENG_CELL_SPACING),
                              oz + ((double)z * ENG_CELL_SPACING), weights);
            float mixed[ENG_SPLAT_LAYERS];
            eng_biome_splat(weights, base, mixed);
            eng_splat_quantize(mixed, px);
        }
    }
}

/* Hauteur du monde au point (x, z) : le même bruit que la génération des cellules (world_cell.c),
 * évalué directement : connu même si la cellule de ce point n'est pas chargée. */
static float gen_height(const WorldView *w, double x, double z)
{
    return eng_world_gen_height(&w->gen, x, z); /* bruit, altitude et échelle du biome */
}

/* Normales des sommets du BORD recalculées par différences centrées À TRAVERS la frontière, avec
 * les hauteurs du générateur (comme le terrain de M3 sur sa grille globale) : deux cellules
 * voisines calculent la même normale pour leurs sommets communs, chargée ou non, donc pas de
 * couture d'éclairage. Renvoie le nombre de sommets corrigés. */
static uint32_t fix_edge_normals(const WorldView *w, EngCellCoord c, EngModel *model)
{
    const uint32_t res = ENG_CELL_RES;
    const double x0 = (double)c.x * ENG_CELL_SIZE;
    const double z0 = (double)c.z * ENG_CELL_SIZE;
    const double s = (double)ENG_CELL_SPACING;
    uint32_t fixed = 0;
    for (uint32_t gz = 0; gz < res; gz++) {
        for (uint32_t gx = 0; gx < res; gx++) {
            if (gx != 0U && gz != 0U && gx != res - 1U && gz != res - 1U) {
                continue;
            }
            EngModelVertex *v = &model->vertices[(gz * res) + gx];
            const double px = x0 + ((double)gx * s);
            const double pz = z0 + ((double)gz * s);
            const float dx =
                (gen_height(w, px + s, pz) - gen_height(w, px - s, pz)) / (2.0F * (float)s);
            const float dz =
                (gen_height(w, px, pz + s) - gen_height(w, px, pz - s)) / (2.0F * (float)s);
            float n[3] = {-dx, 1.0F, -dz};
            const float len = sqrtf((n[0] * n[0]) + (n[1] * n[1]) + (n[2] * n[2]));
            for (int k = 0; k < 3; k++) {
                n[k] /= len;
            }
            float t[3] = {1.0F, 0.0F, 0.0F}; /* sens des u croissants, orthogonalisé */
            for (int k = 0; k < 3; k++) {
                t[k] -= n[k] * n[0];
            }
            const float tl = sqrtf((t[0] * t[0]) + (t[1] * t[1]) + (t[2] * t[2]));
            for (int k = 0; k < 3; k++) {
                v->normal[k] = n[k];
                v->tangent[k] = t[k] / tl;
            }
            v->tangent[3] = 1.0F;
            fixed++;
        }
    }
    return fixed;
}

/* M7-05 : distances (m, caméra -> centre de cellule) au-delà desquelles on passe au niveau
 * suivant ; hystérésis de 10 % (pas de clignotement sur un seuil). */
static const float LOD_DISTANCES[WORLD_VIEW_LOD_LEVELS - 1U] = {360.0F, 720.0F};
#define LOD_HYSTERESIS 0.1F
#define SKIRT_DEPTH 6.0F /* m : jupe sous le bord, cache les fissures entre niveaux */

/* Mesure d'une étape de l'envoi d'une cellule : garde le pire temps, renvoie le nouvel instant. */
static uint64_t stage(WorldView *w, int k, uint64_t since)
{
    const uint64_t now = eng_clock_counter();
    const double ms = (double)(now - since) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    w->worst_stage_ms[k] = ms > w->worst_stage_ms[k] ? ms : w->worst_stage_ms[k];
    w->stage_sum_ms[k] += ms;
    if (k == 2) {
        ENG_LOG_DEBUG("monde : envoi GPU %.2f ms", ms);
    }
    return now;
}

static bool upload(WorldView *w, WorldCellView *v)
{
    const EngCellData *cell = eng_stream_cell(&w->stream, v->coord);
    if (cell == NULL) {
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(&w->scratch);
    EngHeightmap map;
    EngTerrain terrain;
    EngSplatmap splat;
    const EngModelMaterial material = eng_terrain_splat_material();
    const EngTerrainParams params = {.chunk_cells = ENG_CELL_RES - 1U, .material = &material};
    const EngSplatParams sp = eng_splat_default_params();
    uint64_t t = eng_clock_counter();
    bool ok =
        cell_heightmap(w, cell, &map) && eng_terrain_build(&w->scratch, &map, &params, &terrain);
    if (ok) {
        (void)fix_edge_normals(w, v->coord, &terrain.model);
    }
    t = stage(w, 0, t);
    ok = ok && eng_splatmap_generate(&w->scratch, &map, &sp, &splat);
    if (ok && w->gen.biomes.enabled) {
        biome_materials(w, v->coord, &splat); /* grande carte : matières du biome */
    }
    t = stage(w, 1, t);
    EngModel lod_model;
    ok = ok &&
         eng_terrain_build_lod(&w->scratch, &terrain.model, ENG_CELL_RES, WORLD_VIEW_LOD_LEVELS,
                               SKIRT_DEPTH, &lod_model) &&
         eng_model_gpu_create(&v->model, w->renderer, &v->arena, &lod_model, 8.0F);
    v->lod = WORLD_VIEW_LOD_LEVELS - 1U; /* choisi au premier dessin */
    if (ok) {
        const EngTextureDesc desc = {.name = "monde-splat",
                                     .width = ENG_CELL_RES,
                                     .height = ENG_CELL_RES,
                                     .format = ENG_TEXTURE_RGBA8,
                                     .pixels = splat.pixels,
                                     .mip_levels = 1};
        v->splat = eng_renderer_create_texture(w->renderer, &desc);
        ok = eng_renderer_texture_is_valid(w->renderer, v->splat);
        if (!ok) {
            eng_model_gpu_destroy(&v->model, w->renderer);
        } else if (eng_environment_is_valid(&w->shared.model.environment, w->renderer)) {
            (void)eng_model_gpu_set_environment(&v->model, w->renderer,
                                                &w->shared.model.environment);
        }
    }
    t = stage(w, 2, t);
    if (ok) {
        v->body = add_body(w, &map, v->coord);
        (void)stage(w, 3, t);
        v->height_min = cell->height_min;
        v->height_max = cell->height_max;
        v->uploaded = true;
        w->uploaded_cells++;
        w->uploads_done++;
    } else {
        eng_arena_reset(&v->arena);
        ENG_LOG_ERROR("monde : cellule (%d, %d) impossible à envoyer au GPU", (int)v->coord.x,
                      (int)v->coord.z);
    }
    eng_arena_reset_to(&w->scratch, mark);
    return ok;
}

bool world_view_init(WorldView *w, int32_t seed, EngRenderer *renderer, EngPhysics *physics,
                     const EngEnvironment *env)
{
    if (w == NULL || renderer == NULL || physics == NULL) {
        return false;
    }
    memset(w, 0, sizeof *w);
    w->renderer = renderer;
    w->physics = physics;
    w->gen = eng_world_gen_default(seed);
    w->gen.biomes = eng_biome_map_default(); /* grande carte : plaine, montagne, désert, volcan */
    eng_origin_init(&w->origin, WORLD_VIEW_REBASE_M);
    eng_upq_init(&w->uploads, WORLD_VIEW_UPLOAD_BUDGET);
    if (!eng_arena_init(&w->scratch, "monde-cellule", SCRATCH_BYTES) ||
        !eng_arena_init(&w->shared_arena, "monde-partage", SHARED_ARENA_BYTES)) {
        world_view_shutdown(w);
        return false;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        if (!eng_arena_init(&w->cells[i].arena, "monde-cellule-gpu", CELL_ARENA_BYTES)) {
            world_view_shutdown(w);
            return false;
        }
        w->cells[i].body = ENG_BODY_INVALID;
    }
    /* couches et sampler partagés : un terrain plat de la taille d'une cellule, jamais dessiné */
    const EngArenaMark mark = eng_arena_mark(&w->scratch);
    EngCellData flat;
    memset(&flat, 0, sizeof flat);
    EngHeightmap map;
    EngTerrain terrain;
    EngSplatmap splat;
    const EngModelMaterial material = eng_terrain_splat_material();
    const EngTerrainParams params = {.chunk_cells = ENG_CELL_RES - 1U, .material = &material};
    const EngSplatParams sp = eng_splat_default_params();
    bool ok = cell_heightmap(w, &flat, &map) &&
              eng_terrain_build(&w->scratch, &map, &params, &terrain) &&
              eng_splatmap_generate(&w->scratch, &map, &sp, &splat);
    if (ok) {
        const EngTerrainGpuParams gp = {.splat = &splat, .anisotropy = 8.0F};
        ok = eng_terrain_gpu_create(&w->shared, renderer, &w->shared_arena, &w->scratch, &terrain,
                                    &gp);
    }
    eng_arena_reset_to(&w->scratch, mark);
    if (ok && env != NULL) {
        (void)eng_terrain_gpu_set_environment(&w->shared, renderer, env);
    }
    EngPipelineDesc desc;
    eng_terrain_pipeline_desc(&desc);
    w->pipeline = eng_renderer_create_pipeline(renderer, &desc);
    eng_model_shadow_pipeline_desc(&desc);
    w->shadow_pipeline = eng_renderer_create_pipeline(renderer, &desc);
    ok = ok && eng_renderer_pipeline_is_valid(renderer, w->pipeline) &&
         eng_renderer_pipeline_is_valid(renderer, w->shadow_pipeline);
    const uint32_t workers = 4U;
    ok = ok && eng_jobs_init(&w->jobs, workers) &&
         eng_stream_init(&w->stream, &w->gen, &w->jobs, LOAD_RADIUS, NULL);
    if (!ok) {
        ENG_LOG_ERROR("monde : initialisation impossible");
        world_view_shutdown(w);
        return false;
    }
    w->stream.on_ready = on_ready;
    w->stream.on_unload = on_unload;
    w->stream.user = w;
    w->ready = true;
    ENG_LOG_INFO(
        "monde : graine %d, cellules de %.0f m, rayon %d (%u cellules), budget d'upload %u "
        "o/image, recentrage à %.0f m",
        (int)seed, ENG_CELL_SIZE, LOAD_RADIUS,
        (unsigned)((2 * LOAD_RADIUS + 1) * (2 * LOAD_RADIUS + 1)),
        (unsigned)WORLD_VIEW_UPLOAD_BUDGET, WORLD_VIEW_REBASE_M);
    return true;
}

void world_view_shutdown(WorldView *w)
{
    if (w == NULL) {
        return;
    }
    if (w->ready) {
        eng_stream_shutdown(&w->stream); /* appelle on_unload pour chaque cellule */
        eng_jobs_shutdown(&w->jobs);
        eng_terrain_gpu_destroy(&w->shared, w->renderer);
    }
    if (w->renderer != NULL) {
        if (eng_renderer_pipeline_is_valid(w->renderer, w->pipeline)) {
            (void)eng_renderer_destroy_pipeline(w->renderer, w->pipeline);
        }
        if (eng_renderer_pipeline_is_valid(w->renderer, w->shadow_pipeline)) {
            (void)eng_renderer_destroy_pipeline(w->renderer, w->shadow_pipeline);
        }
        w->pipeline = ENG_PIPELINE_NULL;
        w->shadow_pipeline = ENG_PIPELINE_NULL;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        if (w->renderer != NULL && w->cells[i].used) {
            release(w, &w->cells[i]);
        }
        eng_arena_shutdown(&w->cells[i].arena);
    }
    eng_arena_shutdown(&w->shared_arena);
    eng_arena_shutdown(&w->scratch);
    w->ready = false;
}

bool world_view_update(WorldView *w, const double player_world[3], float shift[3])
{
    shift[0] = 0.0F;
    shift[1] = 0.0F;
    shift[2] = 0.0F;
    if (w == NULL || !w->ready || player_world == NULL) {
        return false;
    }
    const uint64_t t_start = eng_clock_counter();
    w->frame_uploads = 0;
    (void)eng_stream_update(&w->stream, player_world[0], player_world[2]);
    const bool rebased = eng_origin_update(&w->origin, player_world, shift);
    w->frame_rebased = rebased;
    if (rebased) {
        /* les corps Jolt sont recréés au nouveau coin local de leur cellule */
        for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
            WorldCellView *v = &w->cells[i];
            if (!v->uploaded) {
                continue;
            }
            const EngCellData *cell = eng_stream_cell(&w->stream, v->coord);
            if (v->body != ENG_BODY_INVALID) {
                eng_physics_remove_body(w->physics, v->body);
                w->bodies_removed++;
                v->body = ENG_BODY_INVALID;
            }
            const EngArenaMark mark = eng_arena_mark(&w->scratch);
            EngHeightmap map;
            if (cell != NULL && cell_heightmap(w, cell, &map)) {
                v->body = add_body(w, &map, v->coord);
            }
            eng_arena_reset_to(&w->scratch, mark);
        }
    }
    /* distances à jour (les plus proches d'abord), puis les uploads de l'image */
    float local[3];
    eng_origin_to_local(&w->origin, player_world, local);
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldCellView *v = &w->cells[i];
        if (v->used && !v->uploaded) {
            float corner[2];
            cell_corner(w, v->coord, corner);
            const float dx = corner[0] + (0.5F * (float)ENG_CELL_SIZE) - local[0];
            const float dz = corner[1] + (0.5F * (float)ENG_CELL_SIZE) - local[2];
            (void)eng_upq_set_distance(&w->uploads, i, sqrtf((dx * dx) + (dz * dz)));
        }
    }
    uint64_t ids[8];
    const uint32_t n = eng_upq_take(&w->uploads, ids, 8);
    for (uint32_t k = 0; k < n; k++) {
        if (ids[k] < ENG_STREAM_SLOTS && w->cells[ids[k]].used && !w->cells[ids[k]].uploaded) {
            const uint64_t t0 = eng_clock_counter();
            (void)upload(w, &w->cells[ids[k]]);
            w->frame_uploads++;
            const double ms =
                (double)(eng_clock_counter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
            w->worst_upload_ms = ms > w->worst_upload_ms ? ms : w->worst_upload_ms;
        }
    }
    w->frame_update_ms =
        (double)(eng_clock_counter() - t_start) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    return rebased;
}

bool world_view_prime(WorldView *w, const double player_world[3], int32_t radius, double timeout_s)
{
    if (w == NULL || !w->ready || player_world == NULL) {
        return false;
    }
    const uint64_t t0 = eng_clock_counter();
    const double freq = (double)SDL_GetPerformanceFrequency();
    float shift[3];
    for (;;) {
        (void)world_view_update(w, player_world, shift);
        bool all = eng_stream_ready_around(&w->stream, player_world[0], player_world[2], radius);
        if (all) {
            const EngCellCoord c0 = eng_cell_of(player_world[0], player_world[2]);
            for (int32_t dz = -radius; dz <= radius && all; dz++) {
                for (int32_t dx = -radius; dx <= radius && all; dx++) {
                    const WorldCellView *v = find(w, (EngCellCoord){c0.x + dx, c0.z + dz});
                    all = v != NULL && v->uploaded;
                }
            }
        }
        if (all) {
            return true;
        }
        if ((double)(eng_clock_counter() - t0) / freq > timeout_s) {
            ENG_LOG_ERROR("monde : chargement initial trop long (> %.0f s)", timeout_s);
            return false;
        }
        SDL_Delay(1);
    }
}

static void cell_matrix(const WorldView *w, const WorldCellView *v, mat4 out)
{
    float corner[2];
    cell_corner(w, v->coord, corner);
    glm_translate_make(out, (vec3){corner[0], 0.0F, corner[1]});
}

/* Boîte englobante LOCALE d'une cellule. */
static void cell_bounds(const WorldView *w, const WorldCellView *v, float min[3], float max[3])
{
    float corner[2];
    cell_corner(w, v->coord, corner);
    min[0] = corner[0];
    min[1] = v->height_min;
    min[2] = corner[1];
    max[0] = corner[0] + (float)ENG_CELL_SIZE;
    max[1] = v->height_max;
    max[2] = corner[1] + (float)ENG_CELL_SIZE;
}

void world_view_set_fog(WorldView *w, const EngFog *fog, const EngAtmosphere *atmosphere,
                        float sky_intensity, const float true_sun[3])
{
    if (w != NULL && w->ready) {
        (void)eng_terrain_gpu_set_fog(&w->shared, fog, atmosphere, sky_intensity);
        eng_fog_set_sun(w->shared.fog, true_sun); /* M8-07 : la nuit, pas la lune */
    }
}

void world_view_draw(WorldView *w, EngRenderQueue *queue, mat4 view_proj, const EngLighting *light,
                     const EngModelShadow *shadow)
{
    if (w == NULL || !w->ready || queue == NULL) {
        return;
    }
    EngFrustum frustum;
    const bool cull = eng_frustum_from_view_proj(&frustum, view_proj);
    w->cells_drawn = 0;
    w->triangles_drawn = 0;
    for (uint32_t l = 0; l < WORLD_VIEW_LOD_LEVELS; l++) {
        w->lod_drawn[l] = 0;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        WorldCellView *v = &w->cells[i];
        if (!v->uploaded) {
            continue;
        }
        float bmin[3];
        float bmax[3];
        cell_bounds(w, v, bmin, bmax);
        if (cull && !eng_frustum_intersects_aabb(&frustum, bmin, bmax)) {
            continue;
        }
        w->cells_drawn++;
        /* M7-05 : niveau selon la distance caméra -> centre de la cellule */
        const float cx = (bmin[0] + bmax[0]) * 0.5F;
        const float cz = (bmin[2] + bmax[2]) * 0.5F;
        const float dx = cx - (light != NULL ? light->camera_position[0] : 0.0F);
        const float dz = cz - (light != NULL ? light->camera_position[2] : 0.0F);
        v->lod = eng_mesh_lod_select(v->lod, sqrtf((dx * dx) + (dz * dz)), LOD_DISTANCES,
                                     WORLD_VIEW_LOD_LEVELS, LOD_HYSTERESIS);
        w->lod_drawn[v->lod]++;
        w->triangles_drawn += v->model.parts[v->lod].index_count / 3U;
        const uint32_t part = v->lod;
        const EngTextureHandle textures[5] = {v->splat, w->shared.layers[0], w->shared.layers[1],
                                              w->shared.layers[2], w->shared.layers[3]};
        const EngModelDrawOptions options = {
            .textures = textures,
            .sampler = w->shared.sampler,
            .user = {w->shared.tile_meters, w->shared.sharpness, 0.0F},
            .fragment_tail = w->shared.fog,
            .fragment_tail_floats = ENG_FOG_UNIFORM_FLOATS,
            .parts = &part,
            .part_count = 1U};
        mat4 model;
        cell_matrix(w, v, model);
        (void)eng_model_gpu_draw_ex(&v->model, queue, w->pipeline, model, view_proj, light, shadow,
                                    &options);
    }
}

void world_view_draw_depth(const WorldView *w, EngRenderQueue *queue, mat4 light_view_proj)
{
    if (w == NULL || !w->ready || queue == NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldCellView *v = &w->cells[i];
        if (v->uploaded) {
            mat4 model;
            cell_matrix(w, v, model);
            /* M7-05 : le niveau choisi par le dernier dessin principal (une seule partie) */
            mat4 mvp;
            glm_mat4_mul(light_view_proj, model, mvp);
            const EngModelPart *part = &v->model.parts[v->lod];
            const EngDrawCmd cmd = {.pipeline = w->shadow_pipeline,
                                    .vertex_buffer = v->model.vertices,
                                    .index_buffer = v->model.indices,
                                    .index_type = ENG_INDEX_U32,
                                    .count = part->index_count,
                                    .first = part->first_index};
            (void)eng_render_draw_uniforms(queue, &cmd, mvp, sizeof(mat4));
        }
    }
}

void world_view_note_frame(WorldView *w, float frame_ms, float cpu_ms)
{
    if (w == NULL || !(frame_ms >= 0.0F)) {
        return;
    }
    const uint32_t bucket =
        frame_ms >= (float)WORLD_VIEW_HIST_MS ? WORLD_VIEW_HIST_MS : (uint32_t)frame_ms;
    w->frame_hist[bucket]++;
    w->frames++;
    w->frame_sum_ms += (double)frame_ms;
    if (frame_ms > 25.0F) {
        w->frames_over_25++;
        ENG_LOG_INFO("monde : image de %.1f ms (CPU %.1f ms) ; mise à jour %.2f ms, %u envoi(s), "
                     "recentrage %s",
                     (double)frame_ms, (double)cpu_ms, w->frame_update_ms, w->frame_uploads,
                     w->frame_rebased ? "oui" : "non");
    }
    w->worst_frame_ms = frame_ms > w->worst_frame_ms ? frame_ms : w->worst_frame_ms;
    w->worst_cpu_ms = cpu_ms > w->worst_cpu_ms ? cpu_ms : w->worst_cpu_ms;
}

/* Centile de l'histogramme (borne haute du seau, ms). */
static uint32_t percentile_ms(const WorldView *w, double p)
{
    const double target = p * (double)w->frames;
    uint64_t seen = 0;
    for (uint32_t i = 0; i <= WORLD_VIEW_HIST_MS; i++) {
        seen += w->frame_hist[i];
        if ((double)seen >= target) {
            return i + 1U;
        }
    }
    return WORLD_VIEW_HIST_MS + 1U;
}

void world_view_log_summary(const WorldView *w, double travelled_m)
{
    if (w == NULL || !w->ready) {
        return;
    }
    ENG_LOG_INFO("MONDE : %.0f m parcourus, %u image(s) mesurée(s), moyenne %.2f ms, pire %.2f ms "
                 "(CPU %.2f ms), "
                 "p99 < %u ms, %u image(s) > 25 ms ; %llu cellule(s) envoyée(s) (pire %.2f ms), "
                 "%llu corps Jolt ajoutés / %llu retirés, %u recentrage(s), %u emplacement(s) "
                 "au plus, %llu demande(s) refusée(s) ; pire étape : maillage %.2f ms, splatmap "
                 "%.2f ms, GPU %.2f ms, Jolt %.2f ms",
                 travelled_m, w->frames, w->frames > 0U ? w->frame_sum_ms / (double)w->frames : 0.0,
                 (double)w->worst_frame_ms, (double)w->worst_cpu_ms,
                 (unsigned)percentile_ms(w, 0.99), w->frames_over_25,
                 (unsigned long long)w->uploads_done, w->worst_upload_ms,
                 (unsigned long long)w->bodies_added, (unsigned long long)w->bodies_removed,
                 (unsigned)w->origin.rebases, (unsigned)w->stream.peak_slots,
                 (unsigned long long)w->stream.refused, w->worst_stage_ms[0], w->worst_stage_ms[1],
                 w->worst_stage_ms[2], w->worst_stage_ms[3]);
    ENG_LOG_INFO(
        "MONDE : dernière image : %u cellule(s) dessinée(s) (niveaux de détail %u / %u / %u), "
        "%u triangles",
        w->cells_drawn, w->lod_drawn[0], w->lod_drawn[1], w->lod_drawn[2], w->triangles_drawn);
}

bool world_view_height(const WorldView *w, float x, float z, float *out)
{
    if (w == NULL || !w->ready || out == NULL) {
        return false;
    }
    const float local[3] = {x, 0.0F, z};
    double world[3];
    eng_origin_to_world(&w->origin, local, world);
    return eng_stream_height(&w->stream, world[0], world[2], out);
}
