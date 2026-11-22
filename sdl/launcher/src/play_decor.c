/* Décor des zones de la grande carte dans le mode jouable (engine/zone_decor.h) : les boîtes sont
 * posées sur le relief, les solides deviennent des corps statiques (joueur, caméra) et des
 * obstacles pour les ennemis ; toutes sont dessinées avec une boîte unité par matière. */
#include <math.h>
#include <string.h>

#include "engine/log.h"
#include "play_mode.h"

/* matières : r, g, b, rugosité, émissif r, g, b */
static const float DECOR_COLORS[ENG_DECOR_COLORS][7] = {
    [ENG_DECOR_STONE] = {0.42F, 0.4F, 0.38F, 0.85F, 0.0F, 0.0F, 0.0F},
    [ENG_DECOR_ROOF] = {0.12F, 0.13F, 0.17F, 0.7F, 0.0F, 0.0F, 0.0F},
    [ENG_DECOR_WOOD] = {0.4F, 0.26F, 0.14F, 0.8F, 0.0F, 0.0F, 0.0F},
    [ENG_DECOR_CLOTH] = {0.55F, 0.1F, 0.1F, 0.9F, 0.0F, 0.0F, 0.0F},
    [ENG_DECOR_ICE] = {0.62F, 0.8F, 0.92F, 0.25F, 0.0F, 0.0F, 0.0F},
    [ENG_DECOR_SANDSTONE] = {0.72F, 0.56F, 0.36F, 0.85F, 0.0F, 0.0F, 0.0F},
    [ENG_DECOR_BASALT] = {0.09F, 0.08F, 0.09F, 0.75F, 0.0F, 0.0F, 0.0F},
    [ENG_DECOR_FIRE] = {1.0F, 0.4F, 0.06F, 0.6F, 1.8F, 0.5F, 0.04F},
};

bool play_decor_load_visuals(PlayMode *play, EngRenderer *renderer, EngArena *arena,
                             const EngEnvironment *env)
{
    if (play == NULL || renderer == NULL || arena == NULL) {
        return false;
    }
    if (!play->decor_ready) {
        play->decor_ready = play_mode_box_models(renderer, arena, env, DECOR_COLORS,
                                                 (uint32_t)ENG_DECOR_COLORS, play->decor_models);
    }
    return play->decor_ready;
}

void play_decor_unload_visuals(PlayMode *play, EngRenderer *renderer)
{
    if (play == NULL || !play->decor_ready) {
        return;
    }
    for (uint32_t i = 0; i < (uint32_t)ENG_DECOR_COLORS; i++) {
        eng_model_gpu_destroy(&play->decor_models[i], renderer);
    }
    play->decor_ready = false;
}

static void place_matrix(const float center[3], const float half[3], float yaw, mat4 out)
{
    glm_translate_make(out, (vec3){center[0], center[1], center[2]});
    glm_rotate_y(out, yaw, out);
    glm_scale(out, (vec3){2.0F * half[0], 2.0F * half[1], 2.0F * half[2]});
}

/* les boîtes solides sont aussi les obstacles des ennemis (vue, chemin, collision) */
static void push_obstacles(PlayMode *play)
{
    if (play->combat == NULL) {
        return;
    }
    static float boxes[PLAY_DECOR_MAX][4];
    uint32_t n = 0;
    for (uint32_t i = 0; i < play->decor_count; i++) {
        if (play->decor_solid[i]) {
            boxes[n][0] = play->decor_center[i][0];
            boxes[n][1] = play->decor_center[i][2];
            boxes[n][2] = play->decor_half[i][0];
            boxes[n][3] = play->decor_half[i][2];
            n++;
        }
    }
    play_combat_set_obstacles(play->combat, boxes, n);
}

void play_mode_clear_decor(PlayMode *play)
{
    if (play == NULL || !play->ready) {
        return;
    }
    for (uint32_t i = 0; i < play->decor_count; i++) {
        if (play->decor_solid[i] && play->decor_body[i] != ENG_BODY_INVALID) {
            eng_physics_remove_body(play->world, play->decor_body[i]);
        }
        play->decor_body[i] = ENG_BODY_INVALID;
        play->decor_solid[i] = false;
    }
    play->decor_count = 0;
    push_obstacles(play);
}

uint32_t play_mode_add_decor(PlayMode *play, const EngDecorBox *boxes, uint32_t count, float cx,
                             float cz)
{
    if (play == NULL || !play->ready || boxes == NULL || !isfinite(cx) || !isfinite(cz)) {
        return 0U;
    }
    uint32_t placed = 0;
    for (uint32_t i = 0; i < count && play->decor_count < PLAY_DECOR_MAX; i++) {
        const EngDecorBox *b = &boxes[i];
        const float x = cx + b->x;
        const float z = cz + b->z;
        /* sol sous l'emprise : le centre et les quatre coins */
        float lo = play_mode_ground(play, x, z);
        float hi = lo;
        const float mid = lo;
        for (uint32_t corner = 0; corner < 4U; corner++) {
            const float g =
                play_mode_ground(play, x + ((corner & 1U) != 0U ? b->half[0] : -b->half[0]),
                                 z + ((corner & 2U) != 0U ? b->half[2] : -b->half[2]));
            lo = fminf(lo, g);
            hi = fmaxf(hi, g);
        }
        if (!(lo > -900.0F)) { /* relief pas encore chargé ici : l'appelant réessaiera */
            continue;
        }
        /* une boîte solide s'enfonce jusqu'au sol le plus bas et garde sa hauteur au-dessus du plus
         * haut ; une boîte de décor suit le sol en son centre */
        const float bottom = b->solid ? lo - 0.5F : mid + b->lift;
        const float top =
            b->solid ? hi + b->lift + (2.0F * b->half[1]) : bottom + (2.0F * b->half[1]);
        const uint32_t k = play->decor_count;
        play->decor_center[k][0] = x;
        play->decor_center[k][1] = 0.5F * (bottom + top);
        play->decor_center[k][2] = z;
        play->decor_half[k][0] = b->half[0];
        play->decor_half[k][1] = 0.5F * (top - bottom);
        play->decor_half[k][2] = b->half[2];
        play->decor_yaw[k] = b->yaw;
        play->decor_color[k] = b->color < (uint8_t)ENG_DECOR_COLORS ? b->color : 0U;
        play->decor_body[k] = ENG_BODY_INVALID;
        play->decor_solid[k] = false;
        if (b->solid) {
            play->decor_body[k] = eng_physics_add_box(play->world, play->decor_half[k],
                                                      play->decor_center[k], ENG_LAYER_STATIC);
            if (play->decor_body[k] == ENG_BODY_INVALID) {
                ENG_LOG_WARN("décor : corps physique refusé pour la boîte %u", i);
                continue; /* ni dessinée ni comptée : pas de mur fantôme */
            }
            play->decor_solid[k] = true;
        }
        place_matrix(play->decor_center[k], play->decor_half[k], b->yaw, play->decor_place[k]);
        play->decor_count++;
        placed++;
    }
    push_obstacles(play);
    return placed;
}

void play_decor_shift(PlayMode *play, const float shift[3])
{
    if (play == NULL || !play->ready || shift == NULL) {
        return;
    }
    for (uint32_t i = 0; i < play->decor_count; i++) {
        for (int a = 0; a < 3; a++) {
            play->decor_center[i][a] += shift[a];
        }
        place_matrix(play->decor_center[i], play->decor_half[i], play->decor_yaw[i],
                     play->decor_place[i]);
        if (play->decor_solid[i]) { /* corps statique : retiré puis recréé à sa nouvelle place */
            eng_physics_remove_body(play->world, play->decor_body[i]);
            play->decor_body[i] = eng_physics_add_box(play->world, play->decor_half[i],
                                                      play->decor_center[i], ENG_LAYER_STATIC);
        }
    }
    /* les obstacles des ennemis sont décalés par play_combat_shift */
}

void play_decor_draw(const PlayMode *play, EngRenderQueue *queue, mat4 view_proj,
                     const EngLighting *light, const EngModelShadow *shadow, bool shadow_pass)
{
    if (play == NULL || !play->decor_ready || play->avatars == NULL) {
        return;
    }
    for (uint32_t i = 0; i < play->decor_count; i++) {
        const EngModelGpu *model = &play->decor_models[play->decor_color[i]];
        mat4 place;
        glm_mat4_copy((vec4 *)play->decor_place[i], place);
        if (shadow_pass) {
            (void)eng_model_gpu_draw_depth(model, queue, play->avatars->prop_shadow, place,
                                           view_proj);
        } else {
            (void)eng_model_gpu_draw_shadowed(model, queue, play->avatars->prop, place, view_proj,
                                              light, shadow);
        }
    }
}
