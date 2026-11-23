/* Arbres et petit décor de la carte du monde dans le monde ouvert : voir world_trees.h. Ici : les
 * modèles, les listes par cellule et le dessin ; world_trees_near.c : le petit décor autour de la
 * caméra, les corps solides et les obstacles des ennemis. */
#include "world_trees.h"

#include <math.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"
#include "engine/model.h"

#define LOD_DISTANCE_M 220.0F    /* au-delà : la version simplifiée */
#define SHADOW_DISTANCE_M 200.0F /* au-delà : pas d'ombre */

static const char *const MODEL_PATH[ENG_TREE_KINDS] = {
    "assets/props/nature/oak.gltf",       "assets/props/nature/pine_snow.gltf",
    "assets/props/nature/acacia.gltf",    "assets/props/nature/bush.gltf",
    "assets/props/nature/bush_snow.gltf", "assets/props/nature/bush_dry.gltf",
    "assets/props/nature/rock.gltf"};
static const char *const LOD_PATH[ENG_TREE_KINDS] = {"assets/props/nature/oak_lod.gltf",
                                                     "assets/props/nature/pine_snow_lod.gltf",
                                                     "assets/props/nature/acacia_lod.gltf"};
/* taille du modèle (unités) : hauteur d'un arbre ou d'un buisson, largeur d'un rocher */
static const float MODEL_SIZE[ENG_TREE_KINDS] = {1.19F, 1.21F, 0.86F, 0.5F, 0.5F, 0.56F, 1.1F};
/* rayon de l'obstacle en fraction de la taille : tronc à sa base, bloc du rocher ; 0 : aucun */
static const float SOLID_RATIO[ENG_TREE_KINDS] = {0.055F, 0.06F, 0.05F, 0.0F, 0.0F, 0.0F, 0.4F};
/* encombrement horizontal et vertical en fraction de la taille (boîte englobante) */
static const float SPREAD[ENG_TREE_KINDS] = {0.5F, 0.5F, 0.5F, 1.0F, 1.0F, 1.0F, 0.6F};
static const float RISE[ENG_TREE_KINDS] = {1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 0.6F};

/* arbres réalistes : le tronc d'un sapin scanné est bien plus mince, un feuillu plus large */
static const float REAL_SOLID_RATIO[WORLD_REAL_KINDS] = {0.05F, 0.028F, 0.045F};
static const float REAL_SPREAD[WORLD_REAL_KINDS] = {0.65F, 0.3F, 0.3F};

float world_tree_model_size(const WorldTrees *t, uint32_t kind)
{
    return t->realistic && kind < WORLD_REAL_KINDS ? t->real[kind].size : MODEL_SIZE[kind];
}

float world_tree_solid_ratio(const WorldTrees *t, uint32_t kind)
{
    return t->realistic && kind < WORLD_REAL_KINDS ? REAL_SOLID_RATIO[kind] : SOLID_RATIO[kind];
}

float world_tree_spread(const WorldTrees *t, uint32_t kind)
{
    return t->realistic && kind < WORLD_REAL_KINDS ? REAL_SPREAD[kind] : SPREAD[kind];
}

bool world_tree_load_model(EngRenderer *renderer, const char *relative, EngModelGpu *out)
{
    char path[1024];
    if (!eng_path_resolve(path, sizeof path, relative)) {
        ENG_LOG_WARN("arbres : %s introuvable", relative);
        return false;
    }
    static EngArena cpu;
    static EngArena scratch;
    static EngArena gpu_tables;
    if (cpu.base == NULL && (!eng_arena_init(&cpu, "arbres", (size_t)8 << 20) ||
                             !eng_arena_init(&scratch, "arbres-lecture", (size_t)24 << 20) ||
                             !eng_arena_init(&gpu_tables, "arbres-gpu", (size_t)1 << 20))) {
        return false;
    }
    eng_arena_reset(&cpu);
    eng_arena_reset(&scratch);
    EngModel model;
    const bool loaded = eng_model_load(&cpu, &scratch, path, &model);
    const bool ok = loaded && eng_model_gpu_create(out, renderer, &gpu_tables, &model, 8.0F);
    if (loaded) {
        eng_model_free(&model);
    }
    if (!ok) {
        ENG_LOG_WARN("arbres : %s illisible", relative);
    }
    return ok;
}

bool world_trees_init(WorldTrees *t, EngRenderer *renderer, EngPhysics *physics,
                      const EngEnvironment *env, bool realistic)
{
    memset(t, 0, sizeof *t);
    t->renderer = renderer;
    t->physics = physics;
    bool ok = renderer != NULL && physics != NULL;
    for (uint32_t k = 0; ok && k < ENG_TREE_KINDS; k++) {
        ok = world_tree_load_model(renderer, MODEL_PATH[k], &t->models[k]) &&
             (LOD_PATH[k] == NULL || world_tree_load_model(renderer, LOD_PATH[k], &t->lods[k]));
        if (ok && env != NULL) {
            (void)eng_model_gpu_set_environment(&t->models[k], renderer, env);
            if (LOD_PATH[k] != NULL) {
                (void)eng_model_gpu_set_environment(&t->lods[k], renderer, env);
            }
        }
    }
    EngPipelineDesc desc;
    eng_instanced_pipeline_desc(&desc);
    t->pipeline = ok ? eng_renderer_create_pipeline(renderer, &desc) : (EngPipelineHandle){0};
    eng_instanced_shadow_pipeline_desc(&desc);
    t->shadow_pipeline =
        ok ? eng_renderer_create_pipeline(renderer, &desc) : (EngPipelineHandle){0};
    ok = ok && eng_renderer_pipeline_is_valid(renderer, t->pipeline) &&
         eng_renderer_pipeline_is_valid(renderer, t->shadow_pipeline);
    if (!ok) {
        ENG_LOG_WARN("arbres : modèles ou pipelines indisponibles, monde sans arbres");
        world_trees_shutdown(t);
        return false;
    }
    for (uint32_t i = 0; i < WORLD_TREE_BODIES; i++) {
        t->bodies[i] = ENG_BODY_INVALID;
    }
    t->realistic = realistic && world_trees_real_init(t, env);
    t->ready = true;
    return true;
}

/* buffers des arbres de la cellule (le petit décor est envoyé autour de la caméra) */
static void upload_slot(WorldTrees *t, WorldTreeSlot *s)
{
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
        if (eng_renderer_buffer_is_valid(t->renderer, s->buffer[k])) {
            (void)eng_renderer_destroy_buffer(t->renderer, s->buffer[k]);
        }
        s->buffer[k] = (EngBufferHandle){0};
        if (s->count[k] > 0U && !world_tree_is_small(k)) {
            s->buffer[k] = eng_instances_upload(t->renderer, &s->inst[s->first[k]], s->count[k]);
        }
    }
}

void world_trees_load_cell(WorldTrees *t, uint32_t slot, const EngWorldMap *map, EngCellCoord coord,
                           const EngWorldOrigin *origin)
{
    if (t == NULL || !t->ready || map == NULL || slot >= ENG_STREAM_SLOTS || origin == NULL) {
        return;
    }
    world_trees_unload_cell(t, slot);
    static EngMapTree trees[WORLD_TREES_PER_CELL + 1U];
    const double x0 = (double)coord.x * ENG_CELL_SIZE;
    const double z0 = (double)coord.z * ENG_CELL_SIZE;
    uint32_t n = eng_worldmap_trees(map, x0, z0, ENG_CELL_SIZE, trees, WORLD_TREES_PER_CELL + 1U);
    if (n > WORLD_TREES_PER_CELL) { /* liste pleine : le reste de la cellule manque */
        n = WORLD_TREES_PER_CELL;
        if (t->truncated++ == 0U) {
            ENG_LOG_WARN("arbres : cellule (%d, %d) tronquée à %u pieds", (int)coord.x,
                         (int)coord.z, (unsigned)WORLD_TREES_PER_CELL);
        }
    }
    WorldTreeSlot *s = &t->slots[slot];
    s->coord = coord;
    s->total = 0;
    for (int a = 0; a < 3; a++) {
        s->bmin[a] = INFINITY;
        s->bmax[a] = -INFINITY;
    }
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) { /* rangées par essence */
        s->first[k] = s->total;
        for (uint32_t i = 0; i < n; i++) {
            if (trees[i].kind != k) {
                continue;
            }
            const double world[3] = {trees[i].x, trees[i].y, trees[i].z};
            float local[3];
            eng_origin_to_local(origin, world, local);
            const float size = trees[i].scale;
            const uint8_t tint[4] = {trees[i].tint[0], trees[i].tint[1], trees[i].tint[2], 255U};
            const float solid = world_tree_solid_ratio(t, k);
            const float spread = world_tree_spread(t, k);
            eng_instance_make_yaw(&s->inst[s->total], local, size / world_tree_model_size(t, k),
                                  trees[i].yaw, tint);
            s->solid[s->total] = solid > 0.0F ? fmaxf(0.3F, size * solid) : 0.0F;
            for (int a = 0; a < 3; a++) {
                s->bmin[a] = fminf(s->bmin[a], local[a] - (a == 1 ? 0.0F : spread * size));
                s->bmax[a] =
                    fmaxf(s->bmax[a], local[a] + (a == 1 ? RISE[k] * size : spread * size));
            }
            s->total++;
        }
        s->count[k] = s->total - s->first[k];
        if (world_tree_is_small(k)) {
            t->loaded_small += s->count[k];
        }
    }
    s->used = s->total > 0U;
    s->listed = false;
    t->loaded += s->total;
    upload_slot(t, s);
    t->has_center = false; /* corps solides, petit décor et listes d'arbres à rechoisir */
    t->near_valid = false;
    t->real_valid = false;
}

void world_trees_unload_cell(WorldTrees *t, uint32_t slot)
{
    if (t == NULL || slot >= ENG_STREAM_SLOTS) {
        return;
    }
    WorldTreeSlot *s = &t->slots[slot];
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
        if (t->renderer != NULL && eng_renderer_buffer_is_valid(t->renderer, s->buffer[k])) {
            (void)eng_renderer_destroy_buffer(t->renderer, s->buffer[k]);
        }
        s->buffer[k] = (EngBufferHandle){0};
        if (world_tree_is_small(k)) {
            t->loaded_small -= s->count[k] <= t->loaded_small ? s->count[k] : t->loaded_small;
        }
        s->count[k] = 0;
    }
    t->loaded -= s->total <= t->loaded ? s->total : t->loaded;
    s->total = 0;
    s->used = false;
    s->listed = false;
    t->has_center = false;
    t->near_valid = false;
    t->real_valid = false;
}

void world_trees_shift(WorldTrees *t, const float shift[3])
{
    if (t == NULL || !t->ready || shift == NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        WorldTreeSlot *s = &t->slots[i];
        if (!s->used) {
            continue;
        }
        for (uint32_t k = 0; k < s->total; k++) {
            for (int a = 0; a < 3; a++) {
                s->inst[k].position[a] += shift[a];
            }
        }
        for (int a = 0; a < 3; a++) {
            s->bmin[a] += shift[a];
            s->bmax[a] += shift[a];
        }
        upload_slot(t, s);
    }
    world_trees_clear_bodies(t);
    t->has_center = false;
    t->near_valid = false;
    t->real_valid = false;
}

void world_trees_tick(WorldTrees *t, float dt_s)
{
    if (t != NULL && isfinite(dt_s) && dt_s > 0.0F) {
        t->time_s += (double)dt_s;
    }
}

void world_trees_draw(WorldTrees *t, EngRenderQueue *queue, mat4 view_proj,
                      const EngLighting *light, const EngModelShadow *shadow,
                      const EngFrustum *frustum, const float *fog)
{
    if (t == NULL || !t->ready || queue == NULL) {
        return;
    }
    t->drawn = 0;
    t->near_drawn = 0;
    if (light != NULL) {
        memcpy(t->camera, light->camera_position, sizeof t->camera);
    }
    mat4 identity;
    glm_mat4_identity(identity);
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used ||
            (frustum != NULL && !eng_frustum_intersects_aabb(frustum, s->bmin, s->bmax))) {
            continue;
        }
        const bool far = world_tree_slot_distance(s, t->camera) > LOD_DISTANCE_M;
        for (uint32_t k = 0; k < ENG_TREE_KINDS && !t->realistic; k++) {
            if (s->count[k] == 0U || world_tree_is_small(k)) {
                continue;
            }
            const EngModelDrawOptions options = {.instance_buffer = s->buffer[k],
                                                 .instance_count = s->count[k]};
            (void)eng_model_gpu_draw_ex(far ? &t->lods[k] : &t->models[k], queue, t->pipeline,
                                        identity, view_proj, light, shadow, &options);
            t->drawn += s->count[k];
        }
    }
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) { /* petit décor : world_trees_near_refresh */
        if (t->near[k].count == 0U) {
            continue;
        }
        const EngModelDrawOptions options = {.instance_buffer = t->near[k].buffer,
                                             .instance_count = t->near[k].count};
        (void)eng_model_gpu_draw_ex(&t->models[k], queue, t->pipeline, identity, view_proj, light,
                                    shadow, &options);
        t->near_drawn += t->near[k].count;
    }
    if (t->realistic) {
        world_trees_real_draw(t, queue, view_proj, light, shadow, frustum, fog);
    }
}

void world_trees_draw_depth(const WorldTrees *t, EngRenderQueue *queue, mat4 light_view_proj)
{
    if (t == NULL || !t->ready || queue == NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || world_tree_slot_distance(s, t->camera) > SHADOW_DISTANCE_M) {
            continue; /* ombre des arbres proches seulement */
        }
        const bool far = world_tree_slot_distance(s, t->camera) > 0.5F * SHADOW_DISTANCE_M;
        for (uint32_t k = 0; k < ENG_TREE_KINDS && !t->realistic; k++) {
            if (s->count[k] > 0U && !world_tree_is_small(k)) {
                (void)eng_model_gpu_draw_depth_instanced(far ? &t->lods[k] : &t->models[k], queue,
                                                         t->shadow_pipeline, light_view_proj,
                                                         s->buffer[k], s->count[k]);
            }
        }
    }
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
        if (t->near[k].shadow_count > 0U) {
            (void)eng_model_gpu_draw_depth_instanced(&t->models[k], queue, t->shadow_pipeline,
                                                     light_view_proj, t->near[k].buffer,
                                                     t->near[k].shadow_count);
        }
    }
    if (t->realistic) {
        world_trees_real_draw_depth(t, queue, light_view_proj);
    }
}

/* un modèle jamais créé (chargement interrompu, pas de version simplifiée) : rien à détruire */
static void destroy_model(EngModelGpu *gpu, EngRenderer *renderer)
{
    if (eng_renderer_texture_is_valid(renderer, gpu->white)) {
        eng_model_gpu_destroy(gpu, renderer);
    }
}

void world_trees_shutdown(WorldTrees *t)
{
    if (t == NULL) {
        return;
    }
    world_trees_clear_bodies(t);
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        world_trees_unload_cell(t, i);
    }
    if (t->renderer != NULL) {
        world_trees_real_shutdown(t);
        world_trees_near_release(t);
        for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
            destroy_model(&t->models[k], t->renderer);
            destroy_model(&t->lods[k], t->renderer);
        }
        if (eng_renderer_pipeline_is_valid(t->renderer, t->pipeline)) {
            (void)eng_renderer_destroy_pipeline(t->renderer, t->pipeline);
        }
        if (eng_renderer_pipeline_is_valid(t->renderer, t->shadow_pipeline)) {
            (void)eng_renderer_destroy_pipeline(t->renderer, t->shadow_pipeline);
        }
    }
    t->ready = false;
}
