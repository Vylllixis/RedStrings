#include "avatar.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"
#include "engine/mesh_file.h"
#include "engine/pose.h"
#include "engine/skin_file.h"

#define CHAR_DIR "assets/cooked/characters/"
#define PROP_DIR "assets/cooked/props/"
#define LIB_ARENA_BYTES ((size_t)16 << 20)
#define CPU_ARENA_BYTES ((size_t)48 << 20)
#define PI_F 3.14159265F

/* Mêmes noms que OW_CHARACTER_CLIPS (tools/CMakeLists.txt). */
static const char *const CLIP_NAMES[AVATAR_CLIP_COUNT] = {
    "idle",        "walk",     "run",       "walk_back",  "strafe_left",   "strafe_right",
    "jump_start",  "jump_air", "jump_land", "dodge_back", "dodge_forward", "dodge_left",
    "dodge_right", "slash_h",  "slash_d",   "chop",       "stab",          "idle_2h",
    "slash_2h",    "chop_2h",  "spin_2h",   "stab_2h",    "block",         "block_raise",
    "block_hit",   "parry",    "hit_a",     "hit_b",      "death",         "death_b",
    "cheer",       "shoot",    "spell"};

static const char *const BODY_NAMES[AVATAR_BODY_COUNT] = {
    "knight", "skeleton_minion", "skeleton_warrior", "skeleton_rogue", "skeleton_mage"};

static const char *const PROP_NAMES[AVATAR_PROP_COUNT] = {"sword_1handed",
                                                          "sword_2handed",
                                                          "axe_2handed",
                                                          "shield_round",
                                                          "Skeleton_Blade",
                                                          "Skeleton_Axe",
                                                          "Skeleton_Shield_Small_A",
                                                          "Skeleton_Crossbow",
                                                          "Skeleton_Staff"};

/* Les armes à gauche (boucliers) n'ont pas le même décalage que celles de droite (voir le fichier
 * du chevalier : 1H_Sword sous handslot.r, Round_Shield sous handslot.l). */
static bool prop_is_left(AvatarProp p)
{
    return p == AVATAR_SHIELD || p == AVATAR_SKELETON_SHIELD;
}

/* Vitesses au sol des clips de marche et de course, mesurées sur le pied d'appui (unité du rig) :
 * docs/characters.md. */
#define WALK_SPEED_RIG 0.84F
#define RUN_SPEED_RIG 3.55F
#define DODGE_DISTANCE_M 3.0F /* EngFighterParams.dodge_distance par défaut */

/* Chemin relatif à l'exécutable (assets/cooked/...) -> chemin complet. */
static bool resolve(const char *relative, char *out, size_t cap)
{
    if (!eng_path_resolve(out, cap, relative)) {
        ENG_LOG_ERROR("personnages : chemin trop long : %s", relative);
        return false;
    }
    return true;
}

static uint32_t find_joint(const EngSkeleton *s, const char *name)
{
    for (uint32_t i = 0; i < s->joint_count; i++) {
        if (s->joints[i].name != NULL && strcmp(s->joints[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

const EngAnimClip *avatar_clip(const AvatarLibrary *lib, const char *name)
{
    if (lib == NULL || !lib->cpu_ready || name == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < AVATAR_CLIP_COUNT; i++) {
        if (lib->clip_ok[i] && strcmp(CLIP_NAMES[i], name) == 0) {
            return &lib->clips[i];
        }
    }
    return NULL;
}

bool avatar_library_load_cpu(AvatarLibrary *lib)
{
    if (lib == NULL) {
        return false;
    }
    memset(lib, 0, sizeof *lib);
    EngArena scratch;
    if (!eng_arena_init(&lib->arena, "avatars", LIB_ARENA_BYTES)) {
        return false;
    }
    if (!eng_arena_init(&scratch, "avatars-lecture", CPU_ARENA_BYTES)) {
        eng_arena_shutdown(&lib->arena);
        return false;
    }
    char path[512];
    bool ok = resolve(CHAR_DIR "knight.skel", path, sizeof path) &&
              eng_skeleton_load(&lib->arena, &scratch, path, &lib->skeleton);
    if (!ok) {
        ENG_LOG_WARN("personnages : squelette illisible : %s", path);
    }
    if (ok && lib->skeleton.joint_count > AVATAR_MAX_JOINTS) {
        ENG_LOG_ERROR("personnages : %u articulations (au plus %u)", lib->skeleton.joint_count,
                      AVATAR_MAX_JOINTS);
        ok = false;
    }
    uint32_t loaded = 0;
    for (uint32_t i = 0; ok && i < AVATAR_CLIP_COUNT; i++) {
        char rel[256];
        (void)snprintf(rel, sizeof rel, CHAR_DIR "knight/%s.anim", CLIP_NAMES[i]);
        lib->clip_ok[i] = resolve(rel, path, sizeof path) &&
                          eng_anim_load(&lib->arena, &scratch, path, &lib->clips[i]);
        if (!lib->clip_ok[i]) {
            ENG_LOG_WARN("personnages : clip illisible : %s", rel);
        }
        loaded += lib->clip_ok[i] ? 1U : 0U;
    }
    eng_arena_shutdown(&scratch);
    if (ok) {
        lib->joint_root = find_joint(&lib->skeleton, "root");
        lib->joint_hand_r = find_joint(&lib->skeleton, "handslot.r");
        lib->joint_hand_l = find_joint(&lib->skeleton, "handslot.l");
        ok = lib->joint_root != UINT32_MAX && lib->joint_hand_r != UINT32_MAX &&
             lib->joint_hand_l != UINT32_MAX;
        if (!ok) {
            ENG_LOG_ERROR("personnages : articulations root / handslot.r / handslot.l absentes");
        }
    }
    if (!ok) {
        eng_arena_shutdown(&lib->arena);
        memset(lib, 0, sizeof *lib);
        ENG_LOG_WARN("personnages : fichiers cuits absents ou invalides (%s), capsules à la place",
                     CHAR_DIR);
        return false;
    }
    lib->cpu_ready = true;
    EngActorSet *s = &lib->set;
    s->clips[ENG_ACTOR_IDLE] = avatar_clip(lib, "idle");
    s->clips[ENG_ACTOR_WALK] = avatar_clip(lib, "walk");
    s->clips[ENG_ACTOR_RUN] = avatar_clip(lib, "run");
    s->clips[ENG_ACTOR_JUMP_START] = avatar_clip(lib, "jump_start");
    s->clips[ENG_ACTOR_JUMP_AIR] = avatar_clip(lib, "jump_air");
    s->clips[ENG_ACTOR_JUMP_LAND] = avatar_clip(lib, "jump_land");
    s->clips[ENG_ACTOR_ROLL_POSE] = avatar_clip(lib, "jump_start");
    s->clips[ENG_ACTOR_BLOCK] = avatar_clip(lib, "block");
    s->clips[ENG_ACTOR_PARRY] = avatar_clip(lib, "parry");
    s->clips[ENG_ACTOR_HIT] = avatar_clip(lib, "hit_a");
    s->clips[ENG_ACTOR_STAGGER] = avatar_clip(lib, "hit_b");
    s->clips[ENG_ACTOR_DEATH] = avatar_clip(lib, "death");
    s->walk_speed = WALK_SPEED_RIG;
    s->run_speed = RUN_SPEED_RIG;
    s->roll_pose_time = 0.3F; /* accroupi, juste avant l'impulsion */
    s->roll_radius = DODGE_DISTANCE_M / (2.0F * PI_F) / AVATAR_SCALE;
    s->root_joint = lib->joint_root;
    s->fade = 0.12F;
    s->speed_smooth = 0.1F;
    s->land_time = 0.3F;
    ENG_LOG_INFO("personnages : squelette de %u articulations, %u/%u clips",
                 lib->skeleton.joint_count, (unsigned)loaded, (unsigned)AVATAR_CLIP_COUNT);
    return true;
}

/* Charge un .mesh (+ .skin si `skinned`) cuit et crée son modèle GPU. */
static bool load_model(AvatarLibrary *lib, EngRenderer *renderer, const EngEnvironment *env,
                       const char *relative, bool skinned, EngModelGpu *out)
{
    char path[512];
    if (!resolve(relative, path, sizeof path)) {
        return false;
    }
    EngArena cpu;
    EngArena scratch;
    if (!eng_arena_init(&cpu, "avatar-mesh", CPU_ARENA_BYTES)) {
        return false;
    }
    if (!eng_arena_init(&scratch, "avatar-mesh-lecture", CPU_ARENA_BYTES)) {
        eng_arena_shutdown(&cpu);
        return false;
    }
    bool ok = false;
    EngModel model;
    if (eng_mesh_load(&cpu, &scratch, path, &model)) {
        if (skinned) {
            char skin_path[512];
            (void)snprintf(skin_path, sizeof skin_path, "%s", path);
            char *dot = strrchr(skin_path, '.');
            if (dot != NULL) {
                (void)snprintf(dot, sizeof skin_path - (size_t)(dot - skin_path), ".skin");
            }
            EngSkinVertex *skin = NULL;
            uint32_t joints = 0;
            if (eng_skin_load(&cpu, &scratch, skin_path, model.vertex_count, &skin, &joints)) {
                if (joints != lib->skeleton.joint_count) {
                    ENG_LOG_ERROR("%s : %u articulations, le squelette partagé en a %u", skin_path,
                                  joints, lib->skeleton.joint_count);
                } else {
                    ok = eng_model_gpu_create_skinned(out, renderer, &lib->arena, &model, skin,
                                                      8.0F);
                }
            }
        } else {
            ok = eng_model_gpu_create(out, renderer, &lib->arena, &model, 8.0F);
        }
        eng_model_free(&model);
    }
    if (ok && env != NULL && !eng_model_gpu_set_environment(out, renderer, env)) {
        ENG_LOG_WARN("%s : environnement refusé", path);
    }
    eng_arena_shutdown(&scratch);
    eng_arena_shutdown(&cpu);
    return ok;
}

bool avatar_library_load_gpu(AvatarLibrary *lib, EngRenderer *renderer, const EngEnvironment *env)
{
    if (lib == NULL || renderer == NULL || !lib->cpu_ready) {
        return false;
    }
    EngPipelineDesc desc;
    eng_model_skinned_pipeline_desc(&desc);
    lib->skinned = eng_renderer_create_pipeline(renderer, &desc);
    eng_model_skinned_shadow_pipeline_desc(&desc);
    lib->skinned_shadow = eng_renderer_create_pipeline(renderer, &desc);
    eng_model_pipeline_desc(&desc);
    desc.name = "avatar-prop";
    lib->prop = eng_renderer_create_pipeline(renderer, &desc);
    eng_model_shadow_pipeline_desc(&desc);
    desc.name = "avatar-prop-shadow";
    lib->prop_shadow = eng_renderer_create_pipeline(renderer, &desc);
    if (!eng_renderer_pipeline_is_valid(renderer, lib->skinned) ||
        !eng_renderer_pipeline_is_valid(renderer, lib->prop)) {
        ENG_LOG_ERROR("personnages : pipelines impossibles");
        return false;
    }
    uint32_t bodies = 0;
    for (uint32_t b = 0; b < AVATAR_BODY_COUNT; b++) {
        char path[256];
        (void)snprintf(path, sizeof path, CHAR_DIR "%s.mesh", BODY_NAMES[b]);
        lib->body_ok[b] = load_model(lib, renderer, env, path, true, &lib->bodies[b]);
        bodies += lib->body_ok[b] ? 1U : 0U;
    }
    uint32_t props = 0;
    for (uint32_t p = 0; p < AVATAR_PROP_COUNT; p++) {
        char path[256];
        (void)snprintf(path, sizeof path, PROP_DIR "%s.mesh", PROP_NAMES[p]);
        lib->prop_ok[p] = load_model(lib, renderer, env, path, false, &lib->props[p]);
        props += lib->prop_ok[p] ? 1U : 0U;
    }
    lib->gpu_ready = lib->body_ok[AVATAR_KNIGHT];
    ENG_LOG_INFO("personnages : %u/%u corps, %u/%u armes sur le GPU", (unsigned)bodies,
                 (unsigned)AVATAR_BODY_COUNT, (unsigned)props, (unsigned)AVATAR_PROP_COUNT);
    return lib->gpu_ready;
}

void avatar_library_shutdown(AvatarLibrary *lib, EngRenderer *renderer)
{
    if (lib == NULL) {
        return;
    }
    if (renderer != NULL) {
        for (uint32_t b = 0; b < AVATAR_BODY_COUNT; b++) {
            if (lib->body_ok[b]) {
                eng_model_gpu_destroy(&lib->bodies[b], renderer);
            }
        }
        for (uint32_t p = 0; p < AVATAR_PROP_COUNT; p++) {
            if (lib->prop_ok[p]) {
                eng_model_gpu_destroy(&lib->props[p], renderer);
            }
        }
        const EngPipelineHandle pipelines[4] = {lib->skinned, lib->skinned_shadow, lib->prop,
                                                lib->prop_shadow};
        for (int i = 0; i < 4; i++) {
            if (eng_renderer_pipeline_is_valid(renderer, pipelines[i])) {
                (void)eng_renderer_destroy_pipeline(renderer, pipelines[i]);
            }
        }
    }
    if (lib->cpu_ready) {
        eng_arena_shutdown(&lib->arena);
    }
    memset(lib, 0, sizeof *lib);
}

bool avatar_init(Avatar *av, AvatarLibrary *lib, AvatarBody body, AvatarProp right, AvatarProp left,
                 float scale)
{
    if (av == NULL || lib == NULL || !lib->cpu_ready || (uint32_t)body >= AVATAR_BODY_COUNT ||
        !isfinite(scale) || scale <= 0.0F) {
        return false;
    }
    memset(av, 0, sizeof *av);
    const uint32_t n = lib->skeleton.joint_count;
    av->joint_world = ENG_ARENA_NEW_ARRAY(&lib->arena, mat4, n);
    av->palette = ENG_ARENA_NEW_ARRAY(&lib->arena, mat4, n);
    if (av->joint_world == NULL || av->palette == NULL ||
        !eng_actor_anim_init(&av->anim, &lib->arena, &lib->skeleton, &lib->set)) {
        return false;
    }
    av->body = body;
    av->right = right;
    av->left = left;
    av->scale = scale;
    glm_mat4_identity(av->model);
    av->ready = true;
    return true;
}

void avatar_update(Avatar *av, const AvatarLibrary *lib, const EngActorDrive *drive, float dt,
                   const float feet[3], float yaw)
{
    if (av == NULL || !av->ready || lib == NULL || drive == NULL || feet == NULL) {
        return;
    }
    const float s = AVATAR_SCALE * av->scale;
    EngActorDrive d = *drive;
    d.ground_speed = drive->ground_speed / s; /* vitesses du monde -> unité du rig */
    (void)eng_actor_anim_update(&av->anim, &d, dt);
    avatar_model_matrix(feet, yaw, av->scale, av->model);
    if (eng_pose_world(&av->anim.pose, &lib->skeleton, av->joint_world)) {
        (void)eng_pose_skin_matrices(&lib->skeleton, av->joint_world, av->palette);
    }
}

void avatar_model_matrix(const float feet[3], float yaw, float scale, mat4 out)
{
    glm_translate_make(out, (vec3){feet[0], feet[1], feet[2]});
    glm_rotate_y(out, yaw + PI_F, out); /* KayKit regarde vers +Z */
    glm_scale_uni(out, AVATAR_SCALE * scale);
}

bool avatar_prop_matrix(const AvatarLibrary *lib, AvatarProp prop, mat4 *joint_world, mat4 model,
                        mat4 out)
{
    if (lib == NULL || joint_world == NULL || (int)prop < 0 ||
        (uint32_t)prop >= AVATAR_PROP_COUNT) {
        return false;
    }
    const uint32_t joint = prop_is_left(prop) ? lib->joint_hand_l : lib->joint_hand_r;
    mat4 slot;
    glm_mat4_mul(model, joint_world[joint], slot);
    mat4 offset;
    if (prop_is_left(prop)) {
        glm_translate_make(offset, (vec3){0.0F, 0.017F, 0.156F});
    } else {
        glm_translate_make(offset, (vec3){0.0F, 0.0333F, 0.0F});
        glm_rotate_y(offset, PI_F, offset);
    }
    glm_mat4_mul(slot, offset, out);
    return true;
}

/* Lames (repère de l'arme, unité du rig) : du haut de la garde à la pointe, mesurées sur les
 * boîtes des maillages KayKit (assets/props/). Pas de tranchant : bouclier, arbalète. */
typedef struct Blade {
    float a[3];
    float b[3];
    float radius;
} Blade;

static bool blade_of(AvatarProp p, Blade *out)
{
    switch (p) {
    case AVATAR_SWORD:
        *out = (Blade){{0.0F, 0.25F, 0.0F}, {0.0F, 1.40F, 0.0F}, 0.10F};
        return true;
    case AVATAR_GREATSWORD:
        *out = (Blade){{0.0F, 0.35F, 0.0F}, {0.0F, 1.95F, 0.0F}, 0.12F};
        return true;
    case AVATAR_AXE_2H:
        *out = (Blade){{0.0F, 0.60F, 0.0F}, {0.0F, 1.28F, 0.0F}, 0.30F};
        return true;
    case AVATAR_SKELETON_BLADE:
        *out = (Blade){{0.0F, 0.20F, 0.0F}, {0.0F, 1.16F, 0.0F}, 0.10F};
        return true;
    case AVATAR_SKELETON_AXE:
        *out = (Blade){{0.0F, 0.45F, 0.0F}, {0.0F, 1.00F, 0.0F}, 0.28F};
        return true;
    case AVATAR_SKELETON_STAFF:
        *out = (Blade){{0.0F, 0.60F, 0.0F}, {0.0F, 1.25F, 0.0F}, 0.15F};
        return true;
    default:
        return false;
    }
}

bool avatar_blade_world(const AvatarLibrary *lib, AvatarProp prop, mat4 *joint_world, mat4 model,
                        float scale, EngCapsule *out)
{
    Blade blade;
    mat4 world;
    if (out == NULL || !blade_of(prop, &blade) ||
        !avatar_prop_matrix(lib, prop, joint_world, model, world)) {
        return false;
    }
    glm_mat4_mulv3(world, blade.a, 1.0F, out->a);
    glm_mat4_mulv3(world, blade.b, 1.0F, out->b);
    out->radius = blade.radius * AVATAR_SCALE * scale;
    return true;
}

/* Socket d'arme : os handslot + décalage du fichier KayKit. */
bool avatar_prop_world(const Avatar *av, const AvatarLibrary *lib, bool right, mat4 out)
{
    if (av == NULL || lib == NULL || !av->ready) {
        return false;
    }
    mat4 model;
    memcpy(model, av->model, sizeof model);
    return avatar_prop_matrix(lib, right ? av->right : av->left, av->joint_world, model, out);
}

static void draw_props(const Avatar *av, const AvatarLibrary *lib, EngRenderQueue *queue,
                       mat4 view_proj, const EngLighting *light, const EngModelShadow *shadow,
                       bool shadow_pass)
{
    for (int side = 0; side < 2; side++) {
        const AvatarProp p = side == 0 ? av->right : av->left;
        if ((int)p < 0 || (uint32_t)p >= AVATAR_PROP_COUNT || !lib->prop_ok[p]) {
            continue;
        }
        mat4 world;
        if (!avatar_prop_world(av, lib, side == 0, world)) {
            continue;
        }
        if (shadow_pass) {
            (void)eng_model_gpu_draw_depth(&lib->props[p], queue, lib->prop_shadow, world,
                                           view_proj);
        } else {
            (void)eng_model_gpu_draw_shadowed(&lib->props[p], queue, lib->prop, world, view_proj,
                                              light, shadow);
        }
    }
}

void avatar_draw(const Avatar *av, const AvatarLibrary *lib, EngRenderQueue *queue, mat4 view_proj,
                 const EngLighting *light, const EngModelShadow *shadow)
{
    if (av == NULL || !av->ready || lib == NULL || !lib->gpu_ready || queue == NULL ||
        !lib->body_ok[av->body]) {
        return;
    }
    const EngModelDrawOptions options = {.skin_matrices = (const float *)av->palette,
                                         .skin_joint_count = lib->skeleton.joint_count};
    mat4 model;
    memcpy(model, av->model, sizeof model);
    (void)eng_model_gpu_draw_ex(&lib->bodies[av->body], queue, lib->skinned, model, view_proj,
                                light, shadow, &options);
    draw_props(av, lib, queue, view_proj, light, shadow, false);
}

void avatar_draw_shadow(const Avatar *av, const AvatarLibrary *lib, EngRenderQueue *queue,
                        mat4 light_view_proj)
{
    if (av == NULL || !av->ready || lib == NULL || !lib->gpu_ready || queue == NULL ||
        !lib->body_ok[av->body]) {
        return;
    }
    mat4 model;
    memcpy(model, av->model, sizeof model);
    (void)eng_model_gpu_draw_depth_skinned(&lib->bodies[av->body], queue, lib->skinned_shadow,
                                           model, light_view_proj, (const float *)av->palette,
                                           lib->skeleton.joint_count);
    draw_props(av, lib, queue, light_view_proj, NULL, NULL, true);
}
