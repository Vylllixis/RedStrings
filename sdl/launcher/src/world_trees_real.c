/* Arbres réalistes du monde ouvert (2026-11-22) : voir world_trees.h, engine/tree_render.h et
 * docs/arbres-realistes.md. Ici : leurs modèles, les listes par niveau de détail autour de la
 * caméra, leur dessin dans l'image et dans l'ombre. */
#include <math.h>
#include <string.h>

#include "engine/log.h"
#include "world_trees.h"

#define NEAR_M 38.0F /* en deçà : l'arbre de près (tronc détaillé, trois cartes par bouquet) */
#define MID_M 95.0F  /* en deçà : l'arbre allégé ; au-delà : la vue lointaine */
#define REFRESH_M 5.0F /* les listes sont refaites quand la caméra a bougé d'autant */
#define NEAR_MAX 384U  /* arbres par essence dans chaque liste */
#define MID_MAX 2048U
#define FAR_MAX 8192U

static const char *const REAL_PATH[WORLD_REAL_KINDS] = {"assets/props/nature/real/oak.gltf",
                                                        "assets/props/nature/real/pine.gltf",
                                                        "assets/props/nature/real/acacia.gltf"};
/* hauteur du centre du houppier, en part de la hauteur de l'arbre */
static const float CROWN_HEIGHT[WORLD_REAL_KINDS] = {0.62F, 0.66F, 0.85F};

/* Les parties du modèle rangées par niveau : P de près, les P mêmes matières à mi-distance, puis
 * une partie découpée (la vue lointaine). false si le modèle n'a pas cette forme. */
static bool sort_parts(WorldRealKind *kind)
{
    const EngModelGpu *m = &kind->model;
    if (m->part_count < 3U || m->part_count % 2U == 0U) {
        return false;
    }
    const uint32_t per_lod = (m->part_count - 1U) / 2U;
    if (per_lod > WORLD_REAL_PARTS) {
        return false;
    }
    for (uint32_t i = 0; i < per_lod; i++) {
        const EngModelGpuMaterial *a = &m->materials[m->parts[i].material];
        const EngModelGpuMaterial *b = &m->materials[m->parts[per_lod + i].material];
        if (a->texture.handle.index != b->texture.handle.index ||
            a->alpha_cutoff != b->alpha_cutoff) {
            return false;
        }
        kind->parts[0][i] = i;
        kind->parts[1][i] = per_lod + i;
    }
    kind->part_count[0] = per_lod;
    kind->part_count[1] = per_lod;
    kind->parts[2][0] = m->part_count - 1U;
    kind->part_count[2] = 1U;
    return m->materials[m->parts[m->part_count - 1U].material].alpha_cutoff > 0.0F;
}

void world_trees_real_release(WorldTrees *t)
{
    for (uint32_t k = 0; k < WORLD_REAL_KINDS; k++) {
        for (uint32_t lod = 0; lod < WORLD_REAL_LODS; lod++) {
            WorldNearList *list = &t->real[k].lists[lod];
            if (t->renderer != NULL && eng_renderer_buffer_is_valid(t->renderer, list->buffer)) {
                (void)eng_renderer_destroy_buffer(t->renderer, list->buffer);
            }
            *list = (WorldNearList){0};
        }
    }
    t->real_valid = false;
}

void world_trees_real_shutdown(WorldTrees *t)
{
    world_trees_real_release(t);
    for (uint32_t k = 0; k < WORLD_REAL_KINDS; k++) {
        if (t->real[k].loaded) {
            eng_model_gpu_destroy(&t->real[k].model, t->renderer);
            t->real[k].loaded = false;
        }
    }
    if (eng_renderer_pipeline_is_valid(t->renderer, t->tree_pipeline)) {
        (void)eng_renderer_destroy_pipeline(t->renderer, t->tree_pipeline);
    }
    if (eng_renderer_pipeline_is_valid(t->renderer, t->tree_shadow_pipeline)) {
        (void)eng_renderer_destroy_pipeline(t->renderer, t->tree_shadow_pipeline);
    }
    t->tree_pipeline = (EngPipelineHandle){0};
    t->tree_shadow_pipeline = (EngPipelineHandle){0};
    t->realistic = false;
}

bool world_trees_real_init(WorldTrees *t, const EngEnvironment *env)
{
    bool ok = true;
    for (uint32_t k = 0; ok && k < WORLD_REAL_KINDS; k++) {
        WorldRealKind *kind = &t->real[k];
        ok = world_tree_load_model(t->renderer, REAL_PATH[k], &kind->model);
        kind->loaded = ok;
        if (ok && !sort_parts(kind)) {
            ENG_LOG_WARN("arbres réalistes : %s n'a pas ses trois niveaux de détail", REAL_PATH[k]);
            ok = false;
        }
        if (ok) {
            if (env != NULL) {
                (void)eng_model_gpu_set_environment(&kind->model, t->renderer, env);
            }
            kind->shape = eng_tree_shape_from_bounds(kind->model.bounds_min, kind->model.bounds_max,
                                                     CROWN_HEIGHT[k]);
            kind->size = kind->shape.height;
        }
    }
    EngPipelineDesc desc;
    eng_tree_pipeline_desc(&desc);
    t->tree_pipeline =
        ok ? eng_renderer_create_pipeline(t->renderer, &desc) : (EngPipelineHandle){0};
    eng_tree_shadow_pipeline_desc(&desc);
    t->tree_shadow_pipeline =
        ok ? eng_renderer_create_pipeline(t->renderer, &desc) : (EngPipelineHandle){0};
    ok = ok && eng_renderer_pipeline_is_valid(t->renderer, t->tree_pipeline) &&
         eng_renderer_pipeline_is_valid(t->renderer, t->tree_shadow_pipeline);
    if (!ok) {
        ENG_LOG_WARN("arbres réalistes indisponibles : arbres simples");
        world_trees_real_shutdown(t);
        return false;
    }
    ENG_LOG_INFO("arbres réalistes : %u essences chargées", (unsigned)WORLD_REAL_KINDS);
    return true;
}

/* Range les arbres de l'essence `kind` des cellules proches dans trois listes, selon leur distance
 * à la caméra ; une liste pleine déborde dans la suivante (moins détaillée). */
static void fill_lists(WorldTrees *t, uint32_t kind, EngInstance *lists[WORLD_REAL_LODS],
                       uint32_t counts[WORLD_REAL_LODS])
{
    static const uint32_t capacity[WORLD_REAL_LODS] = {NEAR_MAX, MID_MAX, FAR_MAX};
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || !s->listed) {
            continue;
        }
        for (uint32_t n = s->first[kind]; n < s->first[kind] + s->count[kind]; n++) {
            const float *p = s->inst[n].position;
            const float d = sqrtf(((p[0] - t->camera[0]) * (p[0] - t->camera[0])) +
                                  ((p[1] - t->camera[1]) * (p[1] - t->camera[1])) +
                                  ((p[2] - t->camera[2]) * (p[2] - t->camera[2])));
            uint32_t lod = d < NEAR_M ? 0U : (d < MID_M ? 1U : 2U);
            while (lod < WORLD_REAL_LODS && counts[lod] >= capacity[lod]) {
                lod++;
            }
            if (lod >= WORLD_REAL_LODS) {
                if (t->real_dropped++ == 0U) {
                    ENG_LOG_WARN("arbres réalistes : listes pleines, des arbres manquent");
                }
                continue;
            }
            lists[lod][counts[lod]++] = s->inst[n];
        }
    }
}

void world_trees_real_refresh(WorldTrees *t)
{
    if (!t->realistic) {
        return;
    }
    if (t->real_valid &&
        hypotf(hypotf(t->camera[0] - t->real_center[0], t->camera[1] - t->real_center[1]),
               t->camera[2] - t->real_center[2]) < REFRESH_M) {
        return;
    }
    world_trees_real_release(t);
    /* une cellule dont un arbre peut passer sous MID_M avant le prochain tri a ses arbres triés un
     * par un ; les autres sont dessinées d'un bloc, en vue lointaine */
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        WorldTreeSlot *s = &t->slots[i];
        s->listed = s->used && world_tree_slot_distance(s, t->camera) <= MID_M + REFRESH_M;
    }
    static EngInstance near_list[NEAR_MAX];
    static EngInstance mid_list[MID_MAX];
    static EngInstance far_list[FAR_MAX];
    EngInstance *lists[WORLD_REAL_LODS] = {near_list, mid_list, far_list};
    for (uint32_t k = 0; k < WORLD_REAL_KINDS; k++) {
        uint32_t counts[WORLD_REAL_LODS] = {0U, 0U, 0U};
        fill_lists(t, k, lists, counts);
        for (uint32_t lod = 0; lod < WORLD_REAL_LODS; lod++) {
            WorldNearList *list = &t->real[k].lists[lod];
            if (counts[lod] == 0U) {
                continue;
            }
            list->buffer = eng_instances_upload(t->renderer, lists[lod], counts[lod]);
            list->count =
                eng_renderer_buffer_is_valid(t->renderer, list->buffer) ? counts[lod] : 0U;
        }
    }
    memcpy(t->real_center, t->camera, sizeof t->real_center);
    t->real_valid = true;
}

static EngTreeDraw tree_draw(const WorldTrees *t, uint32_t kind, uint32_t lod,
                             EngBufferHandle instances, uint32_t count)
{
    EngTreeDraw draw = {.instances = instances,
                        .count = count,
                        .shape = t->real[kind].shape,
                        .wind = eng_tree_wind_default(),
                        .parts = t->real[kind].parts[lod],
                        .part_count = t->real[kind].part_count[lod]};
    draw.wind.time_s = (float)fmod(t->time_s, 3600.0); /* la précision d'un float se garde */
    return draw;
}

void world_trees_real_draw(WorldTrees *t, EngRenderQueue *queue, mat4 view_proj,
                           const EngLighting *light, const EngModelShadow *shadow,
                           const EngFrustum *frustum, const float *fog)
{
    memset(t->real_drawn, 0, sizeof t->real_drawn);
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) { /* cellules lointaines : d'un bloc */
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || s->listed ||
            (frustum != NULL && !eng_frustum_intersects_aabb(frustum, s->bmin, s->bmax))) {
            continue;
        }
        for (uint32_t k = 0; k < WORLD_REAL_KINDS; k++) {
            if (s->count[k] == 0U) {
                continue;
            }
            EngTreeDraw draw = tree_draw(t, k, 2U, s->buffer[k], s->count[k]);
            draw.fog = fog;
            (void)eng_tree_draw(&t->real[k].model, queue, t->tree_pipeline, view_proj, light,
                                shadow, &draw);
            t->real_drawn[2] += s->count[k];
        }
    }
    for (uint32_t k = 0; k < WORLD_REAL_KINDS; k++) { /* cellules proches : par distance */
        for (uint32_t lod = 0; lod < WORLD_REAL_LODS; lod++) {
            const WorldNearList *list = &t->real[k].lists[lod];
            if (list->count == 0U) {
                continue;
            }
            EngTreeDraw draw = tree_draw(t, k, lod, list->buffer, list->count);
            draw.fog = fog;
            (void)eng_tree_draw(&t->real[k].model, queue, t->tree_pipeline, view_proj, light,
                                shadow, &draw);
            t->real_drawn[lod] += list->count;
        }
    }
    t->drawn += t->real_drawn[0] + t->real_drawn[1] + t->real_drawn[2];
}

void world_trees_real_draw_depth(const WorldTrees *t, EngRenderQueue *queue, mat4 light_view_proj)
{
    /* l'ombre d'un arbre est celle du niveau en dessous du sien (moins de triangles, même
     * silhouette) ; les arbres dessinés en vue lointaine n'en projettent pas */
    for (uint32_t k = 0; k < WORLD_REAL_KINDS; k++) {
        for (uint32_t lod = 0; lod + 1U < WORLD_REAL_LODS; lod++) {
            const WorldNearList *list = &t->real[k].lists[lod];
            if (list->count == 0U) {
                continue;
            }
            const EngTreeDraw draw = tree_draw(t, k, lod + 1U, list->buffer, list->count);
            (void)eng_tree_draw_depth(&t->real[k].model, queue, t->tree_shadow_pipeline,
                                      light_view_proj, &draw);
        }
    }
}
