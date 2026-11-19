#include "play_mode.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

#define STATIC_MASK (1U << ENG_LAYER_STATIC)
#define GROUND_HALF 28.0F
#define GRID_HALF 3
#define GRID_SPACING 8.0F
#define MOVE_DEADZONE 0.1F

/* ── Arène ── */

static bool add_box(EngPhysics *world, float cx, float cy, float cz, float hx, float hy, float hz)
{
    const float half[3] = {hx, hy, hz};
    const float pos[3] = {cx, cy, cz};
    return eng_physics_add_box(world, half, pos, ENG_LAYER_STATIC) != ENG_BODY_INVALID;
}

/* Marche de hauteur h, dessus à PLAY_GROUND_TOP + h. */
static bool add_step(EngPhysics *world, float x_min, float x_max, float h)
{
    return add_box(world, 0.5F * (x_min + x_max), PLAY_GROUND_TOP + (0.5F * h), 4.0F,
                   0.5F * (x_max - x_min), 0.5F * h, 3.0F);
}

/* Rampe : de x = -12 (au sol) à x = -18 (2 m de haut), z de 2,5 à 5,5. Chaque triangle est donné
 * dans les deux sens : le sens de rotation n'a alors aucune importance pour Jolt. */
static bool add_ramp(EngPhysics *world)
{
    const float y0 = PLAY_GROUND_TOP;
    const float v[6][3] = {{-12.0F, y0, 2.5F},        {-12.0F, y0, 5.5F},
                           {-18.0F, y0, 5.5F},        {-18.0F, y0, 2.5F},
                           {-18.0F, y0 + 2.0F, 2.5F}, {-18.0F, y0 + 2.0F, 5.5F}};
    static const uint32_t tri[8][3] = {{0, 1, 5}, {0, 5, 4}, {3, 4, 5}, {3, 5, 2},
                                       {0, 4, 3}, {1, 2, 5}, {0, 3, 2}, {0, 2, 1}};
    uint32_t indices[16 * 3];
    uint32_t n = 0;
    for (uint32_t t = 0; t < 8; t++) {
        indices[n++] = tri[t][0];
        indices[n++] = tri[t][1];
        indices[n++] = tri[t][2];
        indices[n++] = tri[t][0];
        indices[n++] = tri[t][2];
        indices[n++] = tri[t][1];
    }
    EngPhysicsShape *shape = eng_physics_shape_mesh(world, &v[0][0], sizeof(v[0]), 6, indices, n);
    if (shape == NULL) {
        return false;
    }
    const bool ok = eng_physics_add_static_shape(world, shape, NULL) != ENG_BODY_INVALID;
    eng_physics_shape_destroy(shape);
    return ok;
}

static bool build_arena(EngPhysics *world)
{
    bool ok = add_box(world, 0.0F, PLAY_GROUND_TOP - 0.5F, 0.0F, GROUND_HALF, 0.5F, GROUND_HALF);
    /* les cubes de la démonstration (1 m, base à y = -0,5), sauf le modèle central */
    for (int gz = -GRID_HALF; gz <= GRID_HALF; gz++) {
        for (int gx = -GRID_HALF; gx <= GRID_HALF; gx++) {
            if (gx != 0 || gz != 0) {
                ok = ok && add_box(world, (float)gx * GRID_SPACING, 0.0F, (float)gz * GRID_SPACING,
                                   0.5F, 0.5F, 0.5F);
            }
        }
    }
    /* couloir z = 4 (entre les rangées de cubes), vers +X : marches de 0,15 / 0,30 / 0,45 m, puis
     * un mur de 3 m ; vers -X : plateforme de 1,2 m puis rampe */
    ok = ok && add_step(world, 10.0F, 12.0F, 0.15F);
    ok = ok && add_step(world, 13.0F, 15.0F, 0.30F);
    ok = ok && add_step(world, 16.0F, 18.0F, 0.45F);
    ok = ok && add_box(world, 24.0F, PLAY_GROUND_TOP + 1.5F, 4.0F, 0.25F, 1.5F, 3.5F);
    ok = ok && add_box(world, -6.5F, PLAY_GROUND_TOP + 0.6F, 4.0F, 1.5F, 0.6F, 1.5F);
    ok = ok && add_ramp(world);
    return ok;
}

/* ── Cycle de vie ── */

static bool build_terrain(PlayMode *play, const char *path)
{
    if (!eng_arena_init(&play->terrain_arena, "play_terrain", (size_t)16 << 20)) {
        return false;
    }
    play->on_terrain = true; /* l'arène est à nous : libérée par play_mode_shutdown */
    if (!eng_heightmap_load(&play->terrain_arena, path, &play->map)) {
        ENG_LOG_ERROR("mode jouable : terrain %s illisible", path);
        return false;
    }
    play->terrain_half = eng_heightmap_extent(&play->map) * 0.5F;
    EngPhysicsShape *shape = eng_physics_shape_heightfield(play->world, &play->map);
    if (shape == NULL) {
        return false;
    }
    EngPhysicsPose pose = eng_physics_pose_identity();
    pose.position[0] = -play->terrain_half; /* comme le rendu (demo_scene.c) */
    pose.position[2] = -play->terrain_half;
    const bool ok = eng_physics_add_static_shape(play->world, shape, &pose) != ENG_BODY_INVALID;
    eng_physics_shape_destroy(shape);
    return ok;
}

float play_mode_ground(const PlayMode *play, float x, float z)
{
    if (play != NULL && play->ground_fn != NULL) {
        float h = 0.0F;
        return play->ground_fn(play->ground_user, x, z, &h) ? h : -1000.0F;
    }
    if (play == NULL || !play->on_terrain) {
        return PLAY_GROUND_TOP;
    }
    return eng_heightmap_sample(&play->map, x + play->terrain_half, z + play->terrain_half);
}

static bool init_common(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity, bool invert_y,
                        const char *terrain_path, const float *spawn);

bool play_mode_init(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity, bool invert_y)
{
    return init_common(play, tweaks, camera_sensitivity, invert_y, NULL, NULL);
}

bool play_mode_init_terrain(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity,
                            bool invert_y, const char *heightmap_path)
{
    return heightmap_path != NULL &&
           init_common(play, tweaks, camera_sensitivity, invert_y, heightmap_path, NULL);
}

bool play_mode_init_world(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity,
                          bool invert_y, const float spawn[3],
                          bool (*ground_fn)(void *user, float x, float z, float *out),
                          void *ground_user)
{
    if (spawn == NULL || ground_fn == NULL ||
        !init_common(play, tweaks, camera_sensitivity, invert_y, NULL, spawn)) {
        return false;
    }
    play->ground_fn = ground_fn;
    play->ground_user = ground_user;
    return true;
}

void play_mode_shift(PlayMode *play, const float shift[3])
{
    if (play == NULL || !play->ready || shift == NULL) {
        return;
    }
    float feet[3];
    (void)eng_character_position(play->character, feet);
    for (int a = 0; a < 3; a++) {
        feet[a] += shift[a];
        play->orbit.target[a] += shift[a];
    }
    (void)eng_character_teleport(play->character, feet);
}

static bool init_common(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity, bool invert_y,
                        const char *terrain_path, const float *spawn)
{
    if (play == NULL) {
        return false;
    }
    memset(play, 0, sizeof(*play));
    if (!eng_physics_available()) {
        ENG_LOG_ERROR("mode jouable : le moteur est compilé sans Jolt (OW_PHYSICS=OFF)");
        return false;
    }
    play->world = eng_physics_create(NULL);
    const bool built =
        spawn != NULL /* monde streamé : les cellules ajoutent leurs corps */
        || (terrain_path != NULL ? build_terrain(play, terrain_path) : build_arena(play->world));
    if (play->world == NULL || !built) {
        ENG_LOG_ERROR("mode jouable : création de l'arène impossible");
        play_mode_shutdown(play);
        return false;
    }
    const float sx = spawn != NULL ? spawn[0] : PLAY_SPAWN_X;
    const float sz = spawn != NULL ? spawn[2] : PLAY_SPAWN_Z;
    const float spawn_y = spawn != NULL ? spawn[1] : play_mode_ground(play, sx, sz);
    const EngCharacterDesc desc = {.position = {sx, spawn_y + 0.05F, sz},
                                   .max_slope_deg = PLAY_MAX_SLOPE_DEG};
    play->character = eng_character_create(play->world, &desc);
    const float start_yaw = -1.5707963F; /* regarde vers +X, vers les marches */
    const float pivot[3] = {sx, spawn_y + PLAY_PIVOT_HEIGHT, sz};
    if (play->character == NULL ||
        !eng_orbit_camera_init(&play->orbit, pivot, start_yaw, -0.25F,
                               ENG_ORBIT_DISTANCE_DEFAULT) ||
        !eng_orbit_camera_set_sensitivity(&play->orbit, camera_sensitivity) ||
        !eng_locomotion_init(&play->loco, start_yaw) || !eng_jump_init(&play->jump)) {
        ENG_LOG_ERROR("mode jouable : initialisation du personnage ou de la caméra impossible");
        play_mode_shutdown(play);
        return false;
    }
    play->orbit.invert_y = invert_y;
    eng_stamina_init(&play->stamina);
    play->jump_speed = 6.0F;
    play->air_control = 0.35F;
    play->gravity_fall = 1.6F;
    play->cut_factor = 0.5F;
    (void)eng_character_set_gravity_scale(play->character, 1.0F, play->gravity_fall);
    if (tweaks != NULL) {
        (void)eng_stamina_register_tweaks(&play->stamina, tweaks);
        (void)eng_tweaks_add_float(tweaks, "vitesse de course (m/s)", &play->loco.params.max_speed,
                                   1.0F, 12.0F);
        (void)eng_tweaks_add_float(tweaks, "vitesse de saut (m/s)", &play->jump_speed, 2.0F, 12.0F);
        (void)eng_tweaks_add_float(tweaks, "contrôle en l'air", &play->air_control, 0.05F, 1.0F);
        (void)eng_tweaks_add_float(tweaks, "distance caméra (m)", &play->orbit.distance,
                                   ENG_ORBIT_DISTANCE_MIN, ENG_ORBIT_DISTANCE_MAX);
    }
    play->ready = true;
    ENG_LOG_INFO("mode jouable : ZQSD/WASD, souris, molette, Espace = saut, Maj = sprint ; "
                 "marches 0,15/0,30/0,45 m et mur vers +X, plateforme et rampe vers -X");
    return true;
}

#define PLAY_START_YAW (-1.5707963F) /* regarde vers +X, vers les marches */

void play_mode_spawn_point(const PlayMode *play, float feet[3], float *yaw)
{
    if (play == NULL || feet == NULL || yaw == NULL) {
        return;
    }
    feet[0] = PLAY_SPAWN_X;
    feet[1] = play_mode_ground(play, PLAY_SPAWN_X, PLAY_SPAWN_Z) + 0.05F;
    feet[2] = PLAY_SPAWN_Z;
    *yaw = PLAY_START_YAW;
}

void play_mode_teleport(PlayMode *play, const float feet[3], float yaw)
{
    if (play == NULL || feet == NULL || play->character == NULL || !isfinite(feet[0]) ||
        !isfinite(feet[1]) || !isfinite(feet[2]) || !isfinite(yaw)) {
        return;
    }
    (void)eng_character_teleport(play->character, feet);
    const float pivot[3] = {feet[0], feet[1] + PLAY_PIVOT_HEIGHT, feet[2]};
    const float sensitivity = play->orbit.sensitivity;
    const bool invert_y = play->orbit.invert_y;
    const float distance = play->orbit.distance;
    (void)eng_orbit_camera_init(&play->orbit, pivot, yaw, -0.25F,
                                distance > 0.0F ? distance : ENG_ORBIT_DISTANCE_DEFAULT);
    (void)eng_orbit_camera_set_sensitivity(&play->orbit, sensitivity);
    play->orbit.invert_y = invert_y;
    const EngLocomotionParams params = play->loco.params;
    (void)eng_locomotion_init(&play->loco, yaw);
    (void)eng_locomotion_set_params(&play->loco, &params);
    (void)eng_jump_init(&play->jump);
    play->sprinting = false;
}

void play_mode_shutdown(PlayMode *play)
{
    if (play == NULL) {
        return;
    }
    play_combat_destroy(play->combat);
    play->combat = NULL;
    eng_character_destroy(play->character); /* avant le monde */
    play->character = NULL;
    eng_physics_destroy(play->world);
    play->world = NULL;
    if (play->on_terrain) {
        eng_arena_shutdown(&play->terrain_arena);
        play->on_terrain = false;
    }
    play->ready = false;
}

static float relief_height(void *user, float x, float z)
{
    return play_mode_ground((const PlayMode *)user, x, z);
}

bool play_mode_enable_combat(PlayMode *play, bool audio)
{
    if (play == NULL || !play->ready || play->combat != NULL) {
        return false;
    }
    play->combat = play_combat_create(audio, !play->on_terrain);
    if (play->combat != NULL && play->on_terrain) {
        /* M6-03 : les ennemis vont partout où le personnage peut aller, et pas ailleurs : même
         * pente limite que le personnage Jolt */
        (void)play_combat_set_relief(play->combat, relief_height, play, PLAY_MAX_SLOPE_DEG);
    }
    return play->combat != NULL;
}

/* ── Pas fixe ── */

void play_mode_tick(PlayMode *play, const EngInputState *input, float dt)
{
    if (play == NULL || !play->ready || input == NULL || !(dt > 0.0F) || dt > 0.1F) {
        return;
    }
    float feet[3];
    (void)eng_character_position(play->character, feet);
    play->ticks++;
    EngInputState auto_input;
    if (play->autoplay && play->combat != NULL) {
        play_combat_autopilot(play->combat, feet, play->orbit.yaw, play->ticks, &auto_input);
        play->orbit.want_yaw = play->loco.facing; /* la caméra suit le dos du personnage */
        input = &auto_input;
    }
    const float pivot[3] = {feet[0], feet[1] + PLAY_PIVOT_HEIGHT, feet[2]};
    (void)eng_orbit_camera_update(&play->orbit, input, dt, pivot);
    (void)eng_orbit_camera_collide(&play->orbit, play->world, STATIC_MASK,
                                   ENG_ORBIT_COLLISION_RADIUS, dt);

    const bool grounded = eng_character_ground(play->character) == ENG_GROUND_SUPPORTED;
    const EngAxisValue move = input->axis[ENG_AXIS_MOVE];
    const bool moving = isfinite(move.x) && isfinite(move.y) &&
                        ((move.x * move.x) + (move.y * move.y)) > (MOVE_DEADZONE * MOVE_DEADZONE);
    (void)eng_stamina_update(&play->stamina, eng_action_held(input, ENG_ACTION_SPRINT), moving, dt,
                             &play->sprinting);

    float forward[3];
    float right[3];
    eng_orbit_camera_flat_forward(&play->orbit, forward);
    eng_orbit_camera_flat_right(&play->orbit, right);
    (void)eng_locomotion_update(&play->loco, input, forward, right,
                                play->sprinting ? ENG_STAMINA_SPRINT_SCALE : 1.0F,
                                grounded ? 1.0F : play->air_control, dt);

    EngJumpEvents events;
    (void)eng_jump_update(&play->jump, grounded, eng_action_pressed(input, ENG_ACTION_JUMP), dt,
                          &events);
    if (events.jump && eng_character_launch(play->character, play->jump_speed)) {
        play->jumps++;
    }
    if (events.landed) {
        play->landings++;
    }
    if (eng_action_released(input, ENG_ACTION_JUMP)) {
        (void)eng_character_cut_jump(play->character, play->cut_factor);
    }
    (void)eng_character_set_gravity_scale(play->character, 1.0F, play->gravity_fall);

    float velocity[3];
    eng_locomotion_velocity(&play->loco, velocity);
    if (play->combat != NULL) {
        const float dir[2] = {velocity[0], velocity[2]};
        PlayCombatStep step;
        play_combat_tick(play->combat, input, feet, play->loco.facing, dir, play->orbit.yaw,
                         (double)play->combat_time, &step);
        play->combat_time += dt;
        if (step.face) {
            play->loco.facing = step.facing;
        }
        if (step.locked) { /* attaque, esquive, réaction : le combat décide du déplacement */
            velocity[0] = 0.0F;
            velocity[2] = 0.0F;
        }
        velocity[0] += step.push[0] / dt; /* esquive, recul, séparation : vitesse de ce pas */
        velocity[2] += step.push[1] / dt;
    }
    (void)eng_physics_step(play->world, dt);
    (void)eng_character_update(play->character, dt, velocity);
}

void play_mode_view(const PlayMode *play, EngCamera *out)
{
    if (play != NULL && out != NULL && play->ready) {
        eng_orbit_camera_apply(&play->orbit, out);
    }
}

void play_mode_debug_draw(const PlayMode *play, EngDebugLines *lines, double alpha)
{
    if (play == NULL || lines == NULL || !play->ready) {
        return;
    }
    float feet[3];
    (void)eng_character_interpolated(play->character, alpha, feet);
    /* sur le terrain, le maillage du champ de hauteurs remplirait le budget de lignes (et
     * cacherait le personnage) : seulement les corps non statiques ; avec les personnages animés
     * (M4) et sans F3 : seulement le décor de l'arène (pas de capsule) */
    uint32_t mask =
        play->on_terrain ? (ENG_LAYERS_ALL & ~(1U << ENG_LAYER_STATIC)) : ENG_LAYERS_ALL;
    const bool models = play->avatars != NULL && play->avatar.ready;
    if (models && !play->show_debug) {
        mask &= play->arena_count > 0U ? 0U : 1U << ENG_LAYER_STATIC;
    }
    const EngPhysicsDebugDraw options = {
        .layer_mask = mask, .focus = {feet[0], feet[1], feet[2]}, .radius = 60.0F};
    if (mask != 0U) {
        (void)eng_physics_debug_draw(play->world, lines, &options);
    }
    if (!models || play->show_debug) {
        /* flèche de direction du personnage (le lacet 0 regarde vers -Z, positif vers la gauche) */
        const float f = play->loco.facing;
        const vec3 from = {feet[0], feet[1] + 0.9F, feet[2]};
        const vec3 to = {feet[0] - (sinf(f) * 0.9F), feet[1] + 0.9F, feet[2] - (cosf(f) * 0.9F)};
        eng_debug_lines_set_overlay(lines, true);
        (void)eng_debug_arrow(lines, from, to, ENG_DEBUG_YELLOW);
        eng_debug_lines_set_overlay(lines, false);
    }
    if (play->combat != NULL) {
        play_combat_draw(play->combat, lines, play);
    }
}

/* ── Arène en volumes ── */

static EngModelMaterial arena_material(float r, float g, float b, float roughness)
{
    return (EngModelMaterial){.base_color = {r, g, b, 1.0F},
                              .image = -1,
                              .metallic = 0.0F,
                              .roughness = roughness,
                              .metallic_roughness_image = -1,
                              .normal_scale = 1.0F,
                              .normal_image = -1,
                              .occlusion_strength = 1.0F,
                              .occlusion_image = -1,
                              .emissive_image = -1};
}

/* Une pièce : boîte [lo ; hi] dans son repère, placée par `place`. */
static bool add_piece(PlayMode *play, EngRenderer *renderer, EngArena *arena,
                      const EngEnvironment *env, const float lo[3], const float hi[3],
                      const EngModelMaterial *mat, mat4 place)
{
    if (play->arena_count >= PLAY_ARENA_PIECES) {
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    EngModel box;
    bool ok = eng_model_make_box(arena, lo, hi, mat, &box);
    EngModelGpu *gpu = &play->arena_models[play->arena_count];
    ok = ok && eng_model_gpu_create(gpu, renderer, arena, &box, 0.0F);
    if (!ok) {
        eng_arena_reset_to(arena, mark);
        return false;
    }
    if (env != NULL) {
        (void)eng_model_gpu_set_environment(gpu, renderer, env);
    }
    glm_mat4_copy(place, play->arena_place[play->arena_count]);
    play->arena_count++;
    return true;
}

bool play_mode_load_arena_visuals(PlayMode *play, EngRenderer *renderer, EngArena *arena,
                                  const EngEnvironment *env)
{
    if (play == NULL || !play->ready || renderer == NULL || arena == NULL || play->on_terrain) {
        return false;
    }
    const EngModelMaterial stone = arena_material(0.42F, 0.40F, 0.38F, 0.85F);
    const EngModelMaterial wood = arena_material(0.45F, 0.30F, 0.18F, 0.7F);
    mat4 id;
    glm_mat4_identity(id);
    const float y0 = PLAY_GROUND_TOP;
    bool ok = true;
    /* marches : mêmes boîtes que la physique (build_arena) */
    const float steps[3][3] = {{10.0F, 12.0F, 0.15F}, {13.0F, 15.0F, 0.30F}, {16.0F, 18.0F, 0.45F}};
    for (int i = 0; i < 3; i++) {
        const float lo[3] = {steps[i][0], y0, 1.0F};
        const float hi[3] = {steps[i][1], y0 + steps[i][2], 7.0F};
        ok = add_piece(play, renderer, arena, env, lo, hi, &stone, id) && ok;
    }
    const float wall_lo[3] = {23.75F, y0, 0.5F};
    const float wall_hi[3] = {24.25F, y0 + 3.0F, 7.5F};
    ok = add_piece(play, renderer, arena, env, wall_lo, wall_hi, &stone, id) && ok;
    const float plat_lo[3] = {-8.0F, y0, 2.5F};
    const float plat_hi[3] = {-5.0F, y0 + 1.2F, 5.5F};
    ok = add_piece(play, renderer, arena, env, plat_lo, plat_hi, &wood, id) && ok;
    /* rampe : de x = -12 (au sol) à x = -18 (2 m) ; dalle de 0,2 m dont le DESSUS suit la pente */
    const float length = sqrtf((6.0F * 6.0F) + (2.0F * 2.0F));
    const float angle = -atanf(2.0F / 6.0F); /* +X local -> (cos, sin) = vers le bas de la pente */
    const float ramp_lo[3] = {-0.5F * length, -0.2F, -1.5F};
    const float ramp_hi[3] = {0.5F * length, 0.0F, 1.5F};
    mat4 ramp;
    glm_translate_make(ramp, (vec3){-15.0F, y0 + 1.0F, 4.0F}); /* milieu du dessus */
    glm_rotate_z(ramp, angle, ramp);
    ok = add_piece(play, renderer, arena, env, ramp_lo, ramp_hi, &wood, ramp) && ok;
    ENG_LOG_INFO("arène : %u pièce(s) dessinée(s)", play->arena_count);
    return ok;
}

void play_mode_unload_visuals(PlayMode *play, EngRenderer *renderer)
{
    if (play == NULL || renderer == NULL) {
        return;
    }
    for (uint32_t i = 0; i < play->arena_count; i++) {
        eng_model_gpu_destroy(&play->arena_models[i], renderer);
    }
    play->arena_count = 0;
}

/* ── Personnages animés (M4) ── */

bool play_mode_attach_avatars(PlayMode *play, AvatarLibrary *lib)
{
    if (play == NULL || !play->ready || lib == NULL || !lib->cpu_ready) {
        return false;
    }
    if (!avatar_init(&play->avatar, lib, AVATAR_KNIGHT, AVATAR_SWORD, AVATAR_SHIELD, 1.0F)) {
        ENG_LOG_WARN("personnages : chevalier impossible, capsule conservée");
        return false;
    }
    play->avatars = lib;
    if (play->combat != NULL && !play_combat_attach_avatars(play->combat, lib, play)) {
        ENG_LOG_WARN("personnages : ennemis impossibles, capsules conservées pour eux");
    }
    return true;
}

void play_mode_update_visuals(PlayMode *play, double alpha, float dt)
{
    if (play == NULL || !play->ready || play->avatars == NULL || !play->avatar.ready) {
        return;
    }
    float feet[3];
    (void)eng_character_interpolated(play->character, alpha, feet);
    float v[3] = {0.0F, 0.0F, 0.0F};
    (void)eng_character_velocity(play->character, v);
    const EngCharacterGround g = eng_character_ground(play->character);
    EngActorDrive drive = {.ground_speed = sqrtf((v[0] * v[0]) + (v[2] * v[2])),
                           .grounded = g == ENG_GROUND_SUPPORTED || g == ENG_GROUND_STEEP,
                           .state = ENG_CS_IDLE,
                           .tick_fraction = (float)alpha};
    if (play->combat != NULL) {
        play_combat_player_drive(play->combat, play->avatars, alpha, &drive);
    }
    avatar_update(&play->avatar, play->avatars, &drive, dt, feet, play->loco.facing);
    if (play->combat != NULL) {
        play_combat_update_visuals(play->combat, play, alpha, dt);
    }
}

void play_mode_draw_models(const PlayMode *play, EngRenderQueue *queue, mat4 view_proj,
                           const EngLighting *light, const EngModelShadow *shadow, bool shadow_pass)
{
    if (play == NULL || !play->ready || play->avatars == NULL) {
        return;
    }
    for (uint32_t i = 0; i < play->arena_count; i++) {
        mat4 place;
        glm_mat4_copy((vec4 *)play->arena_place[i], place);
        if (shadow_pass) {
            (void)eng_model_gpu_draw_depth(&play->arena_models[i], queue,
                                           play->avatars->prop_shadow, place, view_proj);
        } else {
            (void)eng_model_gpu_draw_shadowed(&play->arena_models[i], queue, play->avatars->prop,
                                              place, view_proj, light, shadow);
        }
    }
    if (shadow_pass) {
        avatar_draw_shadow(&play->avatar, play->avatars, queue, view_proj);
    } else {
        avatar_draw(&play->avatar, play->avatars, queue, view_proj, light, shadow);
    }
    if (play->combat != NULL) {
        play_combat_draw_models(play->combat, play->avatars, queue, view_proj, light, shadow,
                                shadow_pass);
    }
}

/* ── Autotest ── */

#define T_DT (1.0F / 60.0F)
#define PI_F 3.14159265F

typedef struct Selftest {
    PlayMode play;
    bool ok;
    int checks;
    float max_y; /* plus haute position des pieds depuis le dernier reset_track */
    int air_ticks;
} Selftest;

static void check(Selftest *t, bool cond, const char *what, float value)
{
    t->checks++;
    if (cond) {
        ENG_LOG_INFO("  ok     %-58s %8.3f", what, (double)value);
    } else {
        ENG_LOG_ERROR("  ÉCHEC  %-58s %8.3f", what, (double)value);
        t->ok = false;
    }
}

static void feet_of(const Selftest *t, float feet[3])
{
    (void)eng_character_position(t->play.character, feet);
}

static bool on_ground(const Selftest *t)
{
    return eng_character_ground(t->play.character) == ENG_GROUND_SUPPORTED;
}

/* `ticks` pas de simulation avec les entrées données. `press_jump` : pression à l'entrée, la
 * touche reste tenue `jump_hold` pas puis est relâchée. */
typedef struct Drive {
    float mx, my;
    bool sprint;
    bool press_jump;
    int jump_hold; /* nombre de pas où Espace reste tenue (0 : jamais) */
} Drive;

static void run_ticks(Selftest *t, const Drive *d, int ticks)
{
    for (int i = 0; i < ticks; i++) {
        EngInputState in;
        memset(&in, 0, sizeof(in));
        in.axis[ENG_AXIS_MOVE] = (EngAxisValue){d->mx, d->my};
        if (d->sprint) {
            in.held |= 1U << ENG_ACTION_SPRINT;
        }
        if (d->jump_hold > 0) {
            if (i < d->jump_hold) {
                in.held |= 1U << ENG_ACTION_JUMP;
                if (i == 0 && d->press_jump) {
                    in.pressed |= 1U << ENG_ACTION_JUMP;
                }
            }
            if (i == d->jump_hold) {
                in.released |= 1U << ENG_ACTION_JUMP;
            }
        }
        play_mode_tick(&t->play, &in, T_DT);
        float feet[3];
        feet_of(t, feet);
        if (feet[1] > t->max_y) {
            t->max_y = feet[1];
        }
        if (!on_ground(t)) {
            t->air_ticks++;
        }
    }
}

static void idle(Selftest *t, int ticks)
{
    const Drive d = {0};
    run_ticks(t, &d, ticks);
}

/* Replace le personnage en (x, z), caméra derrière lui regardant vers `yaw`, immobile. */
static void place(Selftest *t, float x, float z, float yaw)
{
    PlayMode *p = &t->play;
    const float feet[3] = {x, PLAY_GROUND_TOP + 0.05F, z};
    (void)eng_character_teleport(p->character, feet);
    const float pivot[3] = {x, feet[1] + PLAY_PIVOT_HEIGHT, z};
    const float sensitivity = p->orbit.sensitivity;
    (void)eng_orbit_camera_init(&p->orbit, pivot, yaw, -0.25F, ENG_ORBIT_DISTANCE_DEFAULT);
    (void)eng_orbit_camera_set_sensitivity(&p->orbit, sensitivity);
    (void)eng_locomotion_init(&p->loco, yaw);
    (void)eng_jump_init(&p->jump);
    eng_stamina_restore(&p->stamina);
    idle(t, 40); /* se pose */
    t->max_y = feet[1];
    t->air_ticks = 0;
}

bool play_mode_selftest(void)
{
    static Selftest t; /* ~ 1 Ko : statique pour ne pas encombrer la pile */
    memset(&t, 0, sizeof(t));
    t.ok = true;
    if (!play_mode_init(&t.play, NULL, 1.0F, false)) {
        ENG_LOG_ERROR("AUTOTEST JEU : ÉCHEC (initialisation)");
        return false;
    }
    PlayMode *p = &t.play;
    float feet[3];
    const float east = -PI_F / 2.0F; /* lacet qui regarde vers +X */
    const float west = PI_F / 2.0F;
    const float ground = PLAY_GROUND_TOP;

    ENG_LOG_INFO("AUTOTEST JEU : arène, 60 Hz, entrées scriptées");

    ENG_LOG_INFO("1. se pose au spawn");
    place(&t, PLAY_SPAWN_X, PLAY_SPAWN_Z, east);
    feet_of(&t, feet);
    check(&t, on_ground(&t), "debout sur le sol (SUPPORTED)", 1.0F);
    check(&t, fabsf(feet[1] - ground) < 0.03F, "pieds à la hauteur du sol (m)", feet[1]);

    ENG_LOG_INFO("2. marche vers l'avant de la caméra (+X) pendant 1 s");
    run_ticks(&t, &(Drive){.my = 1.0F}, 60);
    feet_of(&t, feet);
    check(&t, eng_locomotion_speed(&p->loco) > 4.2F && eng_locomotion_speed(&p->loco) < 4.7F,
          "vitesse de marche (m/s, cible 4,5)", eng_locomotion_speed(&p->loco));
    check(&t, feet[0] > 7.8F && feet[0] < 8.6F, "distance parcourue : x (m, attendu ~8,2)",
          feet[0]);
    check(&t, fabsf(feet[2] - PLAY_SPAWN_Z) < 0.1F, "trajectoire droite : z (m)", feet[2]);
    idle(&t, 30);
    check(&t, eng_locomotion_speed(&p->loco) < 0.05F, "arrêt net sans entrée (m/s)",
          eng_locomotion_speed(&p->loco));

    ENG_LOG_INFO("3. sprint 1,5 s");
    place(&t, -4.0F, 4.0F, east);
    run_ticks(&t, &(Drive){.my = 1.0F, .sprint = true}, 90);
    check(&t, eng_locomotion_speed(&p->loco) > 6.9F && eng_locomotion_speed(&p->loco) < 7.5F,
          "vitesse de sprint (m/s, cible 7,2)", eng_locomotion_speed(&p->loco));
    check(&t, fabsf(p->stamina.value - 70.0F) < 1.5F, "endurance après 1,5 s (attendu 70)",
          p->stamina.value);

    ENG_LOG_INFO("4. épuisement puis récupération de l'endurance");
    place(&t, 0.0F, 4.0F, east);
    for (int block = 0; block < 11; block++) { /* 11 x 0,5 s, on alterne avant/arrière */
        run_ticks(&t, &(Drive){.my = (block % 2 == 0) ? 1.0F : -1.0F, .sprint = true}, 30);
    }
    check(&t, p->stamina.exhausted && p->stamina.value < 1.0F, "endurance épuisée après 5,5 s",
          p->stamina.value);
    check(&t, !p->sprinting, "plus de sprint tant qu'épuisé", 0.0F);
    run_ticks(&t, &(Drive){.my = 1.0F, .sprint = true}, 40);
    check(&t, eng_locomotion_speed(&p->loco) < 4.7F,
          "touche tenue, épuisé : vitesse de marche (m/s)", eng_locomotion_speed(&p->loco));
    idle(&t, 150);
    check(&t, !p->stamina.exhausted && p->stamina.value >= 25.0F,
          "récupère : reprise à 25 points après 2,5 s de repos", p->stamina.value);

    ENG_LOG_INFO("5. saut complet puis saut court");
    place(&t, PLAY_SPAWN_X, PLAY_SPAWN_Z, east);
    run_ticks(&t, &(Drive){.press_jump = true, .jump_hold = 90}, 100);
    check(&t, p->jumps == 1 && p->landings >= 1, "un saut, un atterrissage", (float)p->jumps);
    check(&t, t.max_y - ground > 1.7F && t.max_y - ground < 1.95F,
          "hauteur du saut (m, théorie 1,83)", t.max_y - ground);
    check(&t, t.air_ticks > 57 && t.air_ticks < 75, "temps en l'air (pas de 1/60 s, ~1,1 s)",
          (float)t.air_ticks);
    check(&t, on_ground(&t), "de nouveau au sol", 1.0F);
    place(&t, PLAY_SPAWN_X, PLAY_SPAWN_Z, east);
    run_ticks(&t, &(Drive){.press_jump = true, .jump_hold = 5}, 100);
    check(&t, t.max_y - ground > 0.2F && t.max_y - ground < 1.2F,
          "saut court (Espace relâchée tôt, m)", t.max_y - ground);

    ENG_LOG_INFO("6. marches de 0,15 / 0,30 / 0,45 m en marchant vers +X");
    place(&t, 8.5F, 4.0F, east);
    run_ticks(&t, &(Drive){.my = 1.0F}, 240);
    feet_of(&t, feet);
    check(&t, t.max_y - ground > 0.27F, "franchit la marche de 0,30 m (hauteur max, m)",
          t.max_y - ground);
    check(&t, feet[0] > 15.3F && feet[0] < 15.85F, "bloqué par la marche de 0,45 m : x (m)",
          feet[0]);

    ENG_LOG_INFO("7. caméra dos au mur");
    place(&t, 22.0F, 4.0F, west);
    idle(&t, 60);
    float eye[3];
    eng_orbit_camera_eye(&p->orbit, eye);
    check(&t, p->orbit.effective > 1.4F && p->orbit.effective < 1.7F,
          "distance caméra réduite de 4 m (attendu ~1,55)", p->orbit.effective);
    check(&t, eye[0] + ENG_ORBIT_COLLISION_RADIUS <= 23.75F + 0.02F,
          "l'oeil + son rayon reste devant le mur (x, m)", eye[0]);
    check(&t, p->orbit.distance == ENG_ORBIT_DISTANCE_DEFAULT, "distance demandée inchangée (m)",
          p->orbit.distance);

    ENG_LOG_INFO("8. rampe (18 degrés) vers -X");
    place(&t, -11.5F, 4.0F, west);
    run_ticks(&t, &(Drive){.my = 1.0F}, 60);
    feet_of(&t, feet);
    check(&t, on_ground(&t), "marche sur la rampe sans décoller (SUPPORTED)", 1.0F);
    /* la rampe monte de 2 m sur 6 m : à l'abscisse x, la surface est à (-12 - x) / 3 */
    check(&t, feet[0] < -14.0F, "avance sur la rampe : x (m)", feet[0]);
    check(&t, fabsf((feet[1] - ground) - ((-12.0F - feet[0]) / 3.0F)) < 0.08F,
          "pieds sur la surface de la rampe (écart, m)",
          (feet[1] - ground) - ((-12.0F - feet[0]) / 3.0F));

    ENG_LOG_INFO("9. plateforme de 1,2 m : élan puis saut");
    place(&t, -9.6F, 4.0F, east);
    run_ticks(&t, &(Drive){.my = 1.0F, .press_jump = true, .jump_hold = 90}, 20);
    run_ticks(&t, &(Drive){.my = 1.0F, .jump_hold = 0}, 40);
    feet_of(&t, feet);
    check(&t, on_ground(&t) && fabsf(feet[1] - (ground + 1.2F)) < 0.05F,
          "se pose sur la plateforme (hauteur, m)", feet[1] - ground);

    play_mode_shutdown(p);
    if (t.ok) {
        ENG_LOG_INFO("AUTOTEST JEU : OK (%d vérifications)", t.checks);
    } else {
        ENG_LOG_ERROR("AUTOTEST JEU : ÉCHEC");
    }
    return t.ok;
}

/* Autotest sur terrain (--play-terrain-selftest PATH) : le personnage se pose sur le relief, y
 * reste en marchant, et la caméra reste au-dessus du sol. */
bool play_mode_terrain_selftest(const char *heightmap_path)
{
    static Selftest t;
    memset(&t, 0, sizeof(t));
    t.ok = true;
    if (!play_mode_init_terrain(&t.play, NULL, 1.0F, false, heightmap_path)) {
        ENG_LOG_ERROR("AUTOTEST TERRAIN : ÉCHEC (initialisation)");
        return false;
    }
    idle(&t, 90);
    float feet[3];
    feet_of(&t, feet);
    float ground = play_mode_ground(&t.play, feet[0], feet[2]);
    check(&t, fabsf(feet[1] - ground) < 0.25F, "posé sur le relief (écart pieds - sol, m)",
          feet[1] - ground);
    check(&t, on_ground(&t), "au sol après 1,5 s", on_ground(&t) ? 1.0F : 0.0F);
    const float start[3] = {feet[0], feet[1], feet[2]};
    float worst = 0.0F;
    for (int k = 0; k < 4; k++) { /* 4 directions, 3 s chacune */
        const Drive d = {(k == 1) ? 1.0F : (k == 3 ? -1.0F : 0.0F),
                         (k == 0) ? 1.0F : (k == 2 ? -1.0F : 0.0F), false, false, 0};
        for (int i = 0; i < 180; i++) {
            run_ticks(&t, &d, 1);
            feet_of(&t, feet);
            ground = play_mode_ground(&t.play, feet[0], feet[2]);
            const float gap = fabsf(feet[1] - ground);
            if (on_ground(&t) && gap > worst) {
                worst = gap;
            }
        }
    }
    feet_of(&t, feet);
    const float moved = hypotf(feet[0] - start[0], feet[2] - start[2]) + 0.0F;
    check(&t, worst < 0.35F, "suit le relief en marchant (pire écart au sol, m)", worst);
    check(&t, feet[1] > ground - 0.5F, "jamais sous le terrain (m)", feet[1] - ground);
    EngCamera cam;
    memset(&cam, 0, sizeof cam);
    play_mode_view(&t.play, &cam);
    const float under = play_mode_ground(&t.play, cam.position[0], cam.position[2]);
    check(&t, cam.position[1] > under, "caméra au-dessus du sol (m)", cam.position[1] - under);
    (void)moved;
    ENG_LOG_INFO("AUTOTEST TERRAIN : %s (%d vérifications)", t.ok ? "OK" : "ÉCHEC", t.checks);
    play_mode_shutdown(&t.play);
    return t.ok;
}
