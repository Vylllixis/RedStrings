/* Petit décor (buissons, rochers) autour de la caméra, corps solides et obstacles des ennemis :
 * voir world_trees.h. */
#include <math.h>
#include <string.h>

#include "world_trees.h"

#define BODY_RADIUS_M 40.0F
#define BODY_REFRESH_M 8.0F
#define NEAR_REFRESH_M 4.0F
#define NEAR_SHADOW_M 70.0F /* au-delà : pas d'ombre pour le petit décor */

/* par essence : distance où le pied a toute sa taille, distance où il a disparu (m) */
static const float NEAR_FULL[ENG_TREE_KINDS] = {0.0F, 0.0F, 0.0F, 120.0F, 120.0F, 120.0F, 200.0F};
static const float NEAR_END[ENG_TREE_KINDS] = {0.0F, 0.0F, 0.0F, 170.0F, 170.0F, 170.0F, 260.0F};

bool world_tree_is_small(uint32_t kind)
{
    return kind >= (uint32_t)ENG_TREE_BUSH && kind < (uint32_t)ENG_TREE_KINDS;
}

float world_tree_slot_distance(const WorldTreeSlot *s, const float p[3])
{
    const float dx = fmaxf(fmaxf(s->bmin[0] - p[0], p[0] - s->bmax[0]), 0.0F);
    const float dz = fmaxf(fmaxf(s->bmin[2] - p[2], p[2] - s->bmax[2]), 0.0F);
    return hypotf(dx, dz);
}

void world_trees_near_release(WorldTrees *t)
{
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
        if (t->renderer != NULL && eng_renderer_buffer_is_valid(t->renderer, t->near[k].buffer)) {
            (void)eng_renderer_destroy_buffer(t->renderer, t->near[k].buffer);
        }
        t->near[k] = (WorldNearList){0};
    }
    t->near_valid = false;
}

/* les pieds de l'essence `kind` entre `lo` et `hi` mètres de la caméra, ajoutés à `out` ; leur
 * taille diminue jusqu'à zéro entre NEAR_FULL et NEAR_END */
static uint32_t gather(const WorldTrees *t, uint32_t kind, float lo, float hi, EngInstance *out,
                       uint32_t count)
{
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || s->count[kind] == 0U || world_tree_slot_distance(s, t->camera) > hi) {
            continue;
        }
        for (uint32_t n = s->first[kind]; n < s->first[kind] + s->count[kind]; n++) {
            const float d = hypotf(s->inst[n].position[0] - t->camera[0],
                                   s->inst[n].position[2] - t->camera[2]);
            if (d < lo || d >= hi || count >= WORLD_NEAR_MAX) {
                continue;
            }
            float fade = (NEAR_END[kind] - d) / (NEAR_END[kind] - NEAR_FULL[kind]);
            fade = fade > 1.0F ? 1.0F : fade;
            fade = fade * fade * (3.0F - (2.0F * fade)); /* lissé */
            if (fade < 0.03F) {
                continue;
            }
            out[count] = s->inst[n];
            out[count].scale *= fade;
            count++;
        }
    }
    return count;
}

void world_trees_near_refresh(WorldTrees *t)
{
    if (t->near_valid && hypotf(t->camera[0] - t->near_center[0],
                                t->camera[2] - t->near_center[2]) < NEAR_REFRESH_M) {
        return;
    }
    world_trees_near_release(t);
    static EngInstance list[WORLD_NEAR_MAX];
    for (uint32_t k = 0; k < ENG_TREE_KINDS; k++) {
        if (!world_tree_is_small(k)) {
            continue;
        }
        WorldNearList *n = &t->near[k];
        n->shadow_count = gather(t, k, 0.0F, NEAR_SHADOW_M, list, 0U); /* les proches d'abord */
        n->count = gather(t, k, NEAR_SHADOW_M, NEAR_END[k], list, n->shadow_count);
        if (n->count > 0U) {
            n->buffer = eng_instances_upload(t->renderer, list, n->count);
            if (!eng_renderer_buffer_is_valid(t->renderer, n->buffer)) {
                *n = (WorldNearList){0};
            }
        }
    }
    memcpy(t->near_center, t->camera, sizeof t->near_center);
    t->near_valid = true;
}

void world_trees_clear_bodies(WorldTrees *t)
{
    if (t->physics == NULL) {
        return;
    }
    for (uint32_t i = 0; i < t->body_count; i++) {
        if (t->bodies[i] != ENG_BODY_INVALID) {
            eng_physics_remove_body(t->physics, t->bodies[i]);
        }
        t->bodies[i] = ENG_BODY_INVALID;
    }
    t->body_count = 0;
}

void world_trees_update_bodies(WorldTrees *t, const float feet[3])
{
    if (t == NULL || !t->ready || feet == NULL) {
        return;
    }
    /* ici, avant tout dessin de l'image : refaire les listes détruit leurs anciens buffers, qu'un
     * dessin déjà en file (la passe d'ombre précède l'image) ne doit plus désigner */
    world_trees_near_refresh(t);
    if (t->has_center &&
        hypotf(feet[0] - t->body_center[0], feet[2] - t->body_center[1]) < BODY_REFRESH_M) {
        return;
    }
    world_trees_clear_bodies(t);
    t->body_center[0] = feet[0];
    t->body_center[1] = feet[2];
    t->has_center = true;
    const float reach = BODY_RADIUS_M + BODY_REFRESH_M;
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS && t->body_count < WORLD_TREE_BODIES; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || world_tree_slot_distance(s, feet) > reach) {
            continue;
        }
        const uint32_t rocks = s->first[ENG_TREE_ROCK];
        for (uint32_t k = 0; k < s->total && t->body_count < WORLD_TREE_BODIES; k++) {
            const float *p = s->inst[k].position;
            const float r = s->solid[k];
            if (!(r > 0.0F) || hypotf(p[0] - feet[0], p[2] - feet[2]) > reach) {
                continue;
            }
            EngBody b = ENG_BODY_INVALID;
            if (k >= rocks && k < rocks + s->count[ENG_TREE_ROCK]) {
                /* rocher : une boule à moitié enfoncée (on peut monter dessus) */
                const float center[3] = {p[0], p[1] + (0.2F * r), p[2]};
                b = eng_physics_add_sphere(t->physics, r, center, ENG_LAYER_STATIC);
            } else {
                const float half = 2.5F; /* tronc de 5 m + calottes : la tête du joueur bute */
                const float center[3] = {p[0], p[1] + half + r - 0.3F, p[2]};
                b = eng_physics_add_capsule(t->physics, half, r, center, ENG_LAYER_STATIC);
            }
            if (b != ENG_BODY_INVALID) {
                t->bodies[t->body_count++] = b;
            }
        }
    }
}

uint32_t world_trees_obstacles(const WorldTrees *t, const float center[2], float radius,
                               float (*out)[3], uint32_t max)
{
    if (t == NULL || !t->ready || center == NULL || out == NULL || !isfinite(center[0]) ||
        !isfinite(center[1]) || !(radius > 0.0F)) {
        return 0U;
    }
    const float at[3] = {center[0], 0.0F, center[1]};
    uint32_t count = 0;
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        const WorldTreeSlot *s = &t->slots[i];
        if (!s->used || world_tree_slot_distance(s, at) > radius) {
            continue;
        }
        for (uint32_t k = 0; k < s->total && count < max; k++) {
            const float *p = s->inst[k].position;
            if (s->solid[k] > 0.0F && hypotf(p[0] - center[0], p[2] - center[1]) <= radius) {
                out[count][0] = p[0];
                out[count][1] = p[2];
                out[count][2] = s->solid[k];
                count++;
            }
        }
    }
    return count;
}
