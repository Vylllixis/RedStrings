/* Arbres de la carte du monde dans le monde ouvert : voir world_trees.h. */
#include "world_trees.h"

#include <math.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"
#include "engine/model.h"

#define BODY_RADIUS_M 40.0F
#define BODY_REFRESH_M 8.0F
#define LOD_DISTANCE_M 220.0F    /* au-delà : la version simplifiée */
#define SHADOW_DISTANCE_M 200.0F /* au-delà : pas d'ombre */

static const char *const MODEL_PATH[ENG_TREE_KINDS] = {"assets/props/nature/oak.gltf",
                                                       "assets/props/nature/pine_snow.gltf",
                                                       "assets/props/nature/acacia.gltf"};
static const char *const LOD_PATH[ENG_TREE_KINDS] = {"assets/props/nature/oak_lod.gltf",
                                                     "assets/props/nature/pine_snow_lod.gltf",
                                                     "assets/props/nature/acacia_lod.gltf"};
/* hauteur du modèle (unités) et rayon du tronc à sa base, en fraction de la hauteur */
static const float MODEL_HEIGHT[ENG_TREE_KINDS] = {1.19F, 1.21F, 0.86F};
static const float TRUNK_RATIO[ENG_TREE_KINDS] = {0.055F, 0.06F, 0.05F};

static bool load_model(EngRenderer *renderer, const char *relative, EngModelGpu *out)
{
    char path[1024];
    if (!eng_path_resolve(path, sizeof path, relative)) {
        ENG_LOG_WARN("arbres : %s introuvable", relative);
        return false;
    }
    static EngArena cpu;
    static EngArena scratch;
    static EngArena gpu_tables;
    if (cpu.base == NULL && (!eng_arena_init(&cpu, "arbres", (size_t)4 << 20) ||
                             !eng_arena_init(&scratch, "arbres-lecture", (size_t)8 << 20) ||
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
                      const EngEnvironment *env)
{
    memset(t, 0, sizeof *t);
    t->renderer = renderer;
    t->physics = physics;
    bool ok = renderer != NULL && physics != NULL;
    for (uint32_t k = 0; ok && k < ENG_TREE_KINDS; k++) {
        ok = load_model(renderer, MODEL_PATH[k], &t->models[k]) &&
             load_model(renderer, LOD_PATH[k], &t->lods[k]);
        if (ok && env != NULL) {
            (void)eng_model_gpu_set_environment(&t->models[k], renderer, env);
            (void)eng_model_gpu_set_environment(&t->lods[k], renderer, env);
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
    t->ready = true;
    return true;
}

static void upload_slot(WorldTrees *t, WorldTreeSlot *s)
{
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
        if (eng_renderer_buffer_is_valid(t->renderer, s->buffer[k])) {
            (void)eng_renderer_destroy_buffer(t->renderer, s->buffer[k]);
        }
        s->buffer[k] = (EngBufferHandle){0};
        if (s->count[k] > 0U) {
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
    static EngMapTree trees[WORLD_TREES_PER_CELL];
    const double x0 = (double)coord.x * ENG_CELL_SIZE;
    const double z0 = (double)coord.z * ENG_CELL_SIZE;
    const uint32_t n = eng_worldmap_trees(map, x0, z0, ENG_CELL_SIZE, trees, WORLD_TREES_PER_CELL);
    WorldTreeSlot *s = &t->slots[slot];
    s->coord = coord;
    s->total = 0;
    for (int a = 0; a < 3; a++) {
        s->bmin[a] = INFINITY;
        s->bmax[a] = -INFINITY;
    }
    static const uint8_t white[4] = {255, 255, 255, 255};
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) { /* rangées par essence */
        s->first[k] = s->total;
        for (uint32_t i = 0; i < n; i++) {
            if (trees[i].kind != k) {
                continue;
            }
            const double world[3] = {trees[i].x, trees[i].y, trees[i].z};
            float local[3];
            eng_origin_to_local(origin, world, local);
            const float scale = trees[i].scale / MODEL_HEIGHT[k];
            eng_instance_make_yaw(&s->inst[s->total], local, scale, trees[i].yaw, white);
            s->trunk[s->total] = fmaxf(0.3F, trees[i].scale * TRUNK_RATIO[k]);
            for (int a = 0; a < 3; a++) {
                s->bmin[a] = fminf(s->bmin[a], local[a] - (a == 1 ? 0.0F : 0.5F * trees[i].scale));
                s->bmax[a] =
                    fmaxf(s->bmax[a], local[a] + (a == 1 ? trees[i].scale : 0.5F * trees[i].scale));
            }
            s->total++;
        }
        s->count[k] = s->total - s->first[k];
    }
    s->used = s->total > 0U;
    t->loaded += s->total;
    upload_slot(t, s);
    t->has_center = false; /* troncs solides à rechoisir */
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
        s->count[k] = 0;
    }
    t->loaded -= s->total <= t->loaded ? s->total : t->loaded;
    s->total = 0;
    s->used = false;
    t->has_center = false;
}

static void clear_bodies(WorldTrees *t)
{
    for (uint32_t i = 0; i < t->body_count; i++) {
        if (t->bodies[i] != ENG_BODY_INVALID) {
            eng_physics_remove_body(t->physics, t->bodies[i]);
        }
        t->bodies[i] = ENG_BODY_INVALID;
    }
    t->body_count = 0;
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
    clear_bodies(t);
    t->has_center = false;
}

void world_trees_update_bodies(WorldTrees *t, const float feet[3])
{
    if (t == NULL || !t->ready || feet == NULL) {
        return;
    }
    if (t->has_center &&
        hypotf(feet[0] - t->body_center[0], feet[2] - t->body_center[1]) < BODY_REFRESH_M) {
        return;
    }
    clear_bodies(t);
    t->body_center[0] = feet[0];
    t->body_center[1] = feet[2];
    t->has_center = true;
    const float reach = BODY_RADIUS_M + BODY_REFRESH_M;
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS && t->body_count < WORLD_TREE_BODIES; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || feet[0] < s->bmin[0] - reach || feet[0] > s->bmax[0] + reach ||
            feet[2] < s->bmin[2] - reach || feet[2] > s->bmax[2] + reach) {
            continue;
        }
        for (uint32_t k = 0; k < s->total && t->body_count < WORLD_TREE_BODIES; k++) {
            const float *p = s->inst[k].position;
            if (hypotf(p[0] - feet[0], p[2] - feet[2]) > reach) {
                continue;
            }
            const float r = s->trunk[k];
            const float half = 2.5F; /* tronc de 5 m + calottes : la tête du joueur bute dessus */
            const float center[3] = {p[0], p[1] + half + r - 0.3F, p[2]};
            const EngBody b =
                eng_physics_add_capsule(t->physics, half, r, center, ENG_LAYER_STATIC);
            if (b != ENG_BODY_INVALID) {
                t->bodies[t->body_count++] = b;
            }
        }
    }
}

/* distance horizontale de `p` à la boîte de la cellule */
static float box_distance(const WorldTreeSlot *s, const float p[3])
{
    const float dx = fmaxf(fmaxf(s->bmin[0] - p[0], p[0] - s->bmax[0]), 0.0F);
    const float dz = fmaxf(fmaxf(s->bmin[2] - p[2], p[2] - s->bmax[2]), 0.0F);
    return hypotf(dx, dz);
}

void world_trees_draw(WorldTrees *t, EngRenderQueue *queue, mat4 view_proj,
                      const EngLighting *light, const EngModelShadow *shadow,
                      const EngFrustum *frustum)
{
    if (t == NULL || !t->ready || queue == NULL) {
        return;
    }
    t->drawn = 0;
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
        const bool far = box_distance(s, t->camera) > LOD_DISTANCE_M;
        for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
            if (s->count[k] == 0U) {
                continue;
            }
            const EngModelDrawOptions options = {.instance_buffer = s->buffer[k],
                                                 .instance_count = s->count[k]};
            (void)eng_model_gpu_draw_ex(far ? &t->lods[k] : &t->models[k], queue, t->pipeline,
                                        identity, view_proj, light, shadow, &options);
            t->drawn += s->count[k];
        }
    }
}

void world_trees_draw_depth(const WorldTrees *t, EngRenderQueue *queue, mat4 light_view_proj)
{
    if (t == NULL || !t->ready || queue == NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || box_distance(s, t->camera) > SHADOW_DISTANCE_M) {
            continue; /* ombre des arbres proches seulement */
        }
        const bool far = box_distance(s, t->camera) > 0.5F * SHADOW_DISTANCE_M;
        for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
            if (s->count[k] > 0U) {
                (void)eng_model_gpu_draw_depth_instanced(far ? &t->lods[k] : &t->models[k], queue,
                                                         t->shadow_pipeline, light_view_proj,
                                                         s->buffer[k], s->count[k]);
            }
        }
    }
}

void world_trees_shutdown(WorldTrees *t)
{
    if (t == NULL) {
        return;
    }
    if (t->physics != NULL) {
        clear_bodies(t);
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        world_trees_unload_cell(t, i);
    }
    if (t->renderer != NULL) {
        for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
            eng_model_gpu_destroy(&t->models[k], t->renderer);
            eng_model_gpu_destroy(&t->lods[k], t->renderer);
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
