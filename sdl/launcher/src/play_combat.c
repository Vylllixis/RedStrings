#include "play_combat.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/ambience.h"
#include "engine/equipment.h"
#include "engine/ai_camp.h"
#include "engine/ai_debug.h"
#include "engine/arena.h"
#include "engine/attack_data.h"
#include "engine/audio_device.h"
#include "engine/combat_audio.h"
#include "engine/damage_numbers.h"
#include "engine/files.h"
#include "engine/hurtbox.h"
#include "engine/pose.h"
#include "engine/lockon.h"
#include "engine/log.h"
#include "engine/mixer.h"
#include "engine/progression.h"
#include "engine/training_dummy.h"
#include "engine/vfx.h"
#include "play_mode.h"

#define PLAYER 0U
#define DUMMY 1U
#define FIRST_ENEMY 2U
#define FIGHTERS 5U
#define RESPAWN_TICKS 180U
#define CAMP_RESPAWN_TICKS 600U
#define MAX_PUSH 0.6F
#define DUMMY_X 4.0F
#define DUMMY_Z (-1.0F)

/* Valeurs intégrées : copie de assets/combat/ (utilisées si les fichiers manquent). */
static const char DEFAULT_ATTACKS[] =
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n next light2\n anim slash_h 1.0 0.075\n "
    " hitbox hand_r 0 0 0.3 0 0 1.4 0.15\nend\n"
    "attack light2\n startup 7\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n next light3\n anim slash_d 1.0 0.26\n "
    " hitbox hand_r 0 0 0.3 0 0 1.4 0.15\nend\n"
    "attack light3\n startup 10\n active 5\n recovery 20\n damage 14\n posture 14\n stamina 14\n"
    " hitstop 4\n anim chop 1.0 0.375\n "
    " hitbox hand_r 0 0 0.3 0 0 1.5 0.18\nend\n"
    "attack heavy\n startup 20\n active 5\n recovery 24\n damage 25\n posture 30\n stamina 25\n"
    " hitstop 6\n anim stab 1.0 0\n "
    " hitbox hand_r 0 0 0.3 0 0 1.6 0.2\nend\n"
    "attack gs_light1\n startup 16\n active 5\n recovery 24\n damage 18\n posture 16\n stamina 16\n"
    " hitstop 4\n anim slash_2h 1.4 0\n "
    " hitbox hand_r 0 0 0.3 0 0 1.8 0.18\nend\n"
    "attack gs_heavy\n startup 26\n active 6\n recovery 32\n damage 40\n posture 45\n stamina 30\n"
    " hitstop 8\n anim chop_2h 1.54 0\n "
    " hitbox hand_r 0 0 0.3 0 0 2.0 0.22\nend\n"
    "attack g_slash\n startup 18\n active 4\n recovery 18\n damage 6\n posture 8\n stamina 5\n"
    " hitstop 3\n cancel_from 6\n next g_slash2\n anim slash_d 1.0 0.075\n "
    " hitbox hand_r 0 0 0.3 0 0 1.5 0.15\nend\n"
    "attack g_slash2\n startup 18\n active 4\n recovery 20\n damage 6\n posture 8\n stamina 5\n"
    " hitstop 3\n anim slash_h 0.75 0\n "
    " hitbox hand_r 0 0 0.3 0 0 1.5 0.15\nend\n"
    "attack g_lunge\n startup 24\n active 6\n recovery 30\n damage 10\n posture 14\n stamina 5\n"
    " hitstop 4\n anim stab 0.9 0\n "
    " hitbox hand_r 0 0 0.3 0 0 2.0 0.15\nend\n"
    "attack b_swing\n startup 28\n active 5\n recovery 30\n damage 14\n posture 20\n stamina 5\n"
    " hitstop 5\n anim slash_2h 0.8 0\n "
    " hitbox hand_r 0 0 0.3 0 0 1.8 0.2\nend\n"
    "attack b_slam\n startup 40\n active 6\n recovery 40\n damage 22\n posture 30\n stamina 5\n"
    " hitstop 6\n anim chop_2h 1.0 0\n "
    " hitbox hand_r 0 0 0.3 0 0 2.2 0.25\nend\n"
    "attack a_stab\n startup 24\n active 4\n recovery 40\n damage 8\n posture 8\n stamina 5\n"
    " hitstop 3\n anim shoot 1.0 -0.29\n "
    " hitbox hand_r 0 0 0.5 0 0 5.5 0.12\nend\n"
    "attack a_pierce\n startup 36\n active 5\n recovery 50\n damage 12\n posture 12\n stamina 5\n"
    " hitstop 4\n anim shoot 1.0 -0.49\n "
    " hitbox hand_r 0 0 0.5 0 0 6.5 0.12\nend\n";

static const char DEFAULT_ENEMIES[] =
    "archetype grunt\n hp 50\n speed 1.15\n sight 16\n fov 120\n hearing 12\n standoff 4\n"
    " light g_slash\n heavy g_lunge\n plan light 0 1.9 50 3 2\n plan heavy 0.8 2.4 140 1 1\nend\n"
    "archetype brute\n hp 120\n speed 0.7\n radius 0.5\n sight 14\n fov 100\n hearing 10\n"
    " standoff 4.5\n light b_swing\n heavy b_slam\n plan light 0 2.2 80 2 1\n"
    " plan heavy 0 2.6 180 1 1\nend\n"
    "archetype archer\n hp 40\n speed 0.9\n sight 20\n fov 110\n hearing 12\n standoff 7\n"
    " flee_below 0.3\n light a_stab\n heavy a_pierce\n plan light 3 6 90 3 1\n"
    " plan heavy 4 7 200 1 1\nend\n";

struct PlayCombat {
    EngArena scratch;
    EngAttackFile file;
    EngAttackSet attacks; /* copie utilisée par la simulation */
    uint32_t attacks_version;
    EngArchSet arch;
    EngAiWorld world;
    EngNavGrid nav;
    EngCombatSim sim;
    EngAiGroup group;
    EngCamp camp;
    EngTrainingDummy dummy;
    EngParticles particles;
    EngDamageNumbers numbers;
    EngMixer mixer;
    EngCombatAudio audio;
    EngAudioDevice *device;
    int32_t lock; /* indice de combattant verrouillé, -1 */
    uint32_t dead_ticks;
    uint32_t cleared_ticks;
    bool ready;
    bool file_ok;
    bool was_dead[FIGHTERS];
    bool has_avatar[FIGHTERS];
    bool rig_ready[FIGHTERS]; /* M5-05 / M5-06 : pose au rythme des ticks (ci-dessous) */
    EngProgress progress;     /* M8-08 */
    uint32_t levelup_ticks;   /* message « Niveau N » encore affiché */
    EngEquipment equip;       /* M8-08 : armes possédées, arme en main */
    uint32_t weapon_ticks;    /* message « Espadon obtenu » / « Arme : ... » encore affiché */
    uint32_t weapon_switches; /* changements d'arme depuis le début (journal, autotest) */
    PlayCombatStats stats;
    AvatarLibrary *avatars;  /* M4 : NULL tant que les personnages ne sont pas branchés */
    Avatar avatar[FIGHTERS]; /* le joueur (0) est dans PlayMode */
    /* M5-05 / M5-06 : pose au rythme des ticks pour les hurtboxes et la lame */
    EngHurtboxSet hurt;
    EngActorAnim rig_anim[FIGHTERS];
    mat4 rig_world[FIGHTERS][AVATAR_MAX_JOINTS];
    AvatarProp rig_prop[FIGHTERS];
    float rig_scale[FIGHTERS];
    EngPose reach_pose;             /* mesure des portées (play_combat_attach_avatars) */
    EngWeaponTrail trail[FIGHTERS]; /* M5-20 : traînée de la lame pendant une attaque */
    int32_t ai_selected;            /* M6-10 : agent choisi au clic, -1 */
};

/* applique les statistiques du niveau au joueur ; `heal` : PV et endurance au maximum */
/* M8-08 : l'arme en main donne au joueur ses attaques (et, pour le rig, la lame à suivre). */
static void apply_weapon(PlayCombat *c)
{
    const EngWeaponDef *w = eng_weapon_def(c->equip.equipped);
    if (w == NULL) {
        return;
    }
    EngFighter *pf = &c->sim.fighters[PLAYER];
    EngFighterParams p = pf->params;
    (void)memset(p.light_name, 0, sizeof p.light_name);
    (void)memset(p.heavy_name, 0, sizeof p.heavy_name);
    (void)strncpy(p.light_name, w->light, sizeof p.light_name - 1U);
    (void)strncpy(p.heavy_name, w->heavy, sizeof p.heavy_name - 1U);
    (void)eng_fighter_set_params(pf, &p);
    c->rig_prop[PLAYER] =
        c->equip.equipped == ENG_WEAPON_GREATSWORD ? AVATAR_GREATSWORD : AVATAR_SWORD;
}

static void apply_progress(PlayCombat *c, bool heal)
{
    EngFighter *pf = &c->sim.fighters[PLAYER];
    const EngProgressStats st = eng_prog_stats(&c->progress);
    pf->params.max_stamina = st.stamina_max;
    pf->params.damage_scale = st.damage_scale;
    pf->vitals.max_hp = st.hp_max;
    if (heal) {
        pf->vitals.hp = st.hp_max;
        pf->stamina = st.stamina_max;
    } else {
        pf->vitals.hp = pf->vitals.hp > st.hp_max ? st.hp_max : pf->vitals.hp;
        pf->stamina = pf->stamina > st.stamina_max ? st.stamina_max : pf->stamina;
    }
}

/* M6-03 : une région praticable plus petite (cellules de la grille, 0,5 m : 16 m²) est bloquée. */
#define PLAY_NAV_MIN_REGION 64U

static PlayCombat g_combat; /* ~1 Mo : instance unique, hors pile */
/* M8-01 : ambiance et musique (tampons synthétisés au chargement, ~10 Mo) ; poids de la nuit donné
 * par le lanceur à chaque image */
static EngArena g_ambience_arena;
static EngAmbience g_ambience;
static float g_night;

/* Fichier de données : d'abord les sources du dépôt (rechargement à chaud pendant le
 * développement), sinon à côté de l'exécutable (version distribuée). */
static bool data_path(const char *relative, char *out, size_t cap)
{
#ifdef OW_ASSET_SOURCE_DIR
    (void)snprintf(out, cap, "%s/%s", OW_ASSET_SOURCE_DIR, relative);
    SDL_PathInfo info;
    if (SDL_GetPathInfo(out, &info)) {
        return true;
    }
#endif
    char rel[256];
    (void)snprintf(rel, sizeof rel, "assets/%s", relative);
    return eng_path_resolve(out, cap, rel);
}

static bool read_text(EngArena *arena, const char *relative, const char **out)
{
    char path[1024];
    if (!data_path(relative, path, sizeof path)) {
        return false;
    }
    EngFileData data;
    if (!eng_file_read_all(arena, path, &data)) {
        return false;
    }
    *out = (const char *)data.data;
    return true;
}

static void attack_path(char *out, size_t cap)
{
    if (!data_path("combat/attacks.txt", out, cap)) {
        out[0] = '\0';
    }
}

static void build_world(EngAiWorld *w)
{
    eng_aiw_init(w);
    /* les cubes de la démonstration (1 m) sauf le central, comme play_mode.c */
    for (int gz = -3; gz <= 3; gz++) {
        for (int gx = -3; gx <= 3; gx++) {
            if (gx != 0 || gz != 0) {
                (void)eng_aiw_add_box(w, (float)gx * 8.0F, (float)gz * 8.0F, 0.5F, 0.5F);
            }
        }
    }
    (void)eng_aiw_add_box(w, 24.0F, 4.0F, 0.25F, 3.5F); /* mur */
    (void)eng_aiw_add_box(w, -6.5F, 4.0F, 1.5F, 1.5F);  /* plateforme */
    (void)eng_aiw_add_box(w, 14.0F, 4.0F, 4.0F, 3.0F);  /* marches : les ennemis les évitent */
}

static void spawn_all(PlayCombat *c)
{
    for (uint32_t i = 0; i < c->group.count; i++) {
        (void)eng_aig_reset_agent(&c->group, &c->sim, i);
        (void)eng_sim_place(&c->sim, c->group.agents[i].fighter, c->camp.m[i].pos[0],
                            c->camp.m[i].pos[1], c->camp.m[i].yaw);
    }
}

PlayCombat *play_combat_create(bool audio, bool arena_obstacles)
{
    PlayCombat *c = &g_combat;
    if (c->ready) {
        return NULL;
    }
    memset(c, 0, sizeof *c);
    eng_prog_init(&c->progress);
    eng_equip_init(&c->equip); /* M8-08 : épée et bouclier (attaques appliquées plus bas) */
    c->lock = -1;
    c->ai_selected = -1;
    if (!eng_arena_init(&c->scratch, "combat", (size_t)1 << 20)) {
        return NULL;
    }
    char err[256];
    char path[1024];
    attack_path(path, sizeof path);
    c->file_ok =
        path[0] != '\0' && eng_attackfile_open(&c->file, &c->scratch, path) && c->file.valid;
    if (c->file_ok) {
        c->attacks = c->file.set;
        c->attacks_version = c->file.version;
        ENG_LOG_INFO("combat : attaques lues dans %s (rechargées à chaud)", path);
    } else if (!eng_attackset_parse(DEFAULT_ATTACKS, &c->attacks, err, sizeof err)) {
        ENG_LOG_ERROR("combat : attaques intégrées invalides : %s", err);
        play_combat_destroy(c);
        return NULL;
    } else {
        ENG_LOG_WARN("combat : %s introuvable ou invalide, attaques intégrées", path);
    }
    eng_arena_reset(&c->scratch);
    const char *enemies = NULL;
    if (!read_text(&c->scratch, "combat/enemies.txt", &enemies) ||
        !eng_arch_parse(enemies, &c->arch, err, sizeof err)) {
        if (enemies != NULL) {
            ENG_LOG_WARN("combat : enemies.txt invalide (%s), archétypes intégrés", err);
        }
        if (!eng_arch_parse(DEFAULT_ENEMIES, &c->arch, err, sizeof err)) {
            ENG_LOG_ERROR("combat : archétypes intégrés invalides : %s", err);
            play_combat_destroy(c);
            return NULL;
        }
    }
    if (!eng_arch_validate(&c->arch, &c->attacks, err, sizeof err)) {
        ENG_LOG_ERROR("combat : %s", err);
        play_combat_destroy(c);
        return NULL;
    }
    if (arena_obstacles) {
        build_world(&c->world);
    } else {
        eng_aiw_init(&c->world);
    }
    const float mn[2] = {-30.0F, -30.0F};
    const float mx[2] = {30.0F, 30.0F};
    if (!eng_nav_build(&c->nav, &c->world, mn, mx, 0.5F, 0.55F) ||
        !eng_sim_init(&c->sim, &c->attacks, FIGHTERS)) {
        play_combat_destroy(c);
        return NULL;
    }
    c->sim.world = &c->world;
    c->sim.world_ignore = 1U << PLAYER; /* le joueur a sa propre physique (Jolt) */
    c->sim.radius[PLAYER] = 0.4F;
    c->sim.radius[DUMMY] = 0.4F;
    (void)eng_sim_place(&c->sim, PLAYER, PLAY_SPAWN_X, PLAY_SPAWN_Z, -1.5707963F);
    (void)eng_sim_place(&c->sim, DUMMY, DUMMY_X, DUMMY_Z,
                        0.0F); /* entre les cubes, devant le départ */
    eng_dummy_init(&c->dummy, DUMMY, ENG_DUMMY_STATIC, 90);
    c->sim.fighters[DUMMY].vitals.max_hp = 1000.0F; /* se remet à neuf après 3 s sans coup */
    c->sim.fighters[DUMMY].vitals.hp = 1000.0F;
    eng_aig_init(&c->group, &c->arch, &c->world, &c->nav, PLAYER);
    eng_camp_init(&c->camp);
    const float face = 2.3561945F; /* tourné vers le départ (+X, +Z) */
    const float patrol[2][2] = {{-12.0F, -12.0F}, {-12.0F, -4.0F}};
    bool ok =
        eng_camp_add(&c->camp, eng_arch_find(&c->arch, "grunt"), -12.0F, -12.0F, face, patrol, 2) &&
        eng_camp_add(&c->camp, eng_arch_find(&c->arch, "brute"), -14.0F, -10.0F, face, NULL, 0) &&
        eng_camp_add(&c->camp, eng_arch_find(&c->arch, "archer"), -18.0F, -12.0F, face, NULL, 0) &&
        eng_camp_spawn(&c->camp, &c->group, &c->sim, FIRST_ENEMY);
    if (!ok) {
        ENG_LOG_ERROR(
            "combat : création du camp impossible (archétypes grunt/brute/archer requis)");
        play_combat_destroy(c);
        return NULL;
    }
    eng_particles_init(&c->particles, 12345U);
    eng_dmgnum_init(&c->numbers);
    if (audio && eng_mixer_init(&c->mixer, 48000U) && eng_combat_audio_init(&c->audio, &c->mixer)) {
        c->device = eng_audio_device_open(&c->mixer);
        if (c->device == NULL) {
            ENG_LOG_WARN("combat : périphérique audio indisponible, combat silencieux");
        } else if (eng_arena_init(&g_ambience_arena, "ambiance", (size_t)16 << 20) &&
                   eng_ambience_init(&g_ambience, &g_ambience_arena, c->mixer.sample_rate)) {
            ENG_LOG_INFO("combat : ambiance (vent, oiseaux / grillons) et musique synthétisées");
        }
    }
    c->ready = true;
    ENG_LOG_INFO("combat : clic gauche = légère, clic droit = lourde, Ctrl = esquive, E maintenu = "
                 "garde (pression au bon moment = parade), A (AZERTY) / Q (QWERTY) = verrouillage "
                 "; mannequin en (4, -1), "
                 "camp de 3 ennemis vers (-14, -11)");
    return c;
}

void play_combat_destroy(PlayCombat *c)
{
    if (c == NULL) {
        return;
    }
    eng_audio_device_close(c->device); /* le mixeur ne lit plus les tampons de l'ambiance */
    c->device = NULL;
    g_ambience.ready = false;
    eng_arena_shutdown(&g_ambience_arena);
    eng_arena_shutdown(&c->scratch);
    c->ready = false;
}

static void poll_attacks(PlayCombat *c, double now)
{
    if (!c->file_ok) {
        return;
    }
    eng_arena_reset(&c->scratch);
    if (!eng_attackfile_poll(&c->file, &c->scratch, now)) {
        return;
    }
    char err[256];
    if (!eng_arch_validate(&c->arch, &c->file.set, err, sizeof err)) {
        ENG_LOG_WARN("combat : nouvelle version refusée (%s), l'ancienne reste", err);
        return;
    }
    for (uint32_t i = 0; i < c->sim.count; i++) { /* une attaque en cours garde un indice valide */
        if (c->sim.fighters[i].fsm.state == ENG_CS_ATTACK) {
            ENG_LOG_INFO("combat : rechargement différé (attaque en cours)");
            return;
        }
    }
    c->attacks = c->file.set;
    c->attacks_version = c->file.version;
    ENG_LOG_INFO("combat : attaques rechargées (version %u)", c->attacks_version);
}

static void rig_tick(PlayCombat *c);

void play_combat_tick(PlayCombat *c, const EngInputState *input, const float feet[3], float facing,
                      const float move_dir[2], float camera_yaw, double now_seconds,
                      PlayCombatStep *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof *out);
    }
    if (c == NULL || !c->ready || input == NULL || feet == NULL || move_dir == NULL ||
        out == NULL) {
        return;
    }
    if (c->sim.tick % 30U == 0U) {
        poll_attacks(c, now_seconds);
    }
    if (c->device != NULL && g_ambience.ready) { /* M8-01 : boucles relancées sans trou */
        eng_audio_device_lock(c->device);
        eng_ambience_update(&g_ambience, &c->mixer, g_night, c->mixer.sample_rate / 10U);
        eng_audio_device_unlock(c->device);
    }
    EngFighter *pf = &c->sim.fighters[PLAYER];
    /* le joueur : position du personnage physique */
    (void)eng_sim_place(&c->sim, PLAYER, feet[0], feet[2], facing);
    const float player_xz[2] = {feet[0], feet[2]};

    /* verrouillage */
    EngLockCandidate cand[FIGHTERS];
    for (uint32_t i = 0; i < FIGHTERS; i++) {
        cand[i].pos[0] = c->sim.bodies[i].pos[0];
        cand[i].pos[1] = c->sim.bodies[i].pos[1];
        cand[i].alive = !c->sim.fighters[i].vitals.dead;
    }
    if (eng_action_pressed(input, ENG_ACTION_SWITCH_WEAPON)) { /* M8-08 */
        const EngCombatState ps = c->sim.fighters[PLAYER].fsm.state;
        if ((ps == ENG_CS_IDLE || ps == ENG_CS_MOVE || ps == ENG_CS_BLOCK) &&
            (c->equip.owned & (c->equip.owned - 1U)) != 0U) { /* au moins deux armes */
            (void)eng_equip_next(&c->equip);
            apply_weapon(c);
            c->weapon_ticks = 120U;
            c->weapon_switches++;
            ENG_LOG_INFO("combat : arme en main : %s", eng_weapon_def(c->equip.equipped)->label);
        } else {
            ENG_LOG_DEBUG("combat : changement d'arme refusé (état %s, armes 0x%x)",
                          eng_combat_state_name(ps), (unsigned)c->equip.owned);
        }
    }
    if (eng_action_pressed(input, ENG_ACTION_LOCK_ON)) {
        c->lock = c->lock >= 0
                      ? -1
                      : eng_lockon_acquire(player_xz, camera_yaw, cand, FIGHTERS, (int32_t)PLAYER);
    }
    if (c->lock >= 0 && !eng_lockon_valid(player_xz, cand, FIGHTERS, c->lock)) {
        c->lock = -1;
    }

    EngFighterInput in[FIGHTERS];
    memset(in, 0, sizeof in);
    if (!pf->vitals.dead) {
        in[PLAYER].light = eng_action_pressed(input, ENG_ACTION_ATTACK);
        in[PLAYER].heavy = eng_action_pressed(input, ENG_ACTION_HEAVY_ATTACK);
        in[PLAYER].dodge = eng_action_pressed(input, ENG_ACTION_DODGE);
        in[PLAYER].block_pressed = eng_action_pressed(input, ENG_ACTION_INTERACT);
        in[PLAYER].block_held = eng_action_held(input, ENG_ACTION_INTERACT);
        const float len = sqrtf(move_dir[0] * move_dir[0] + move_dir[1] * move_dir[1]);
        if (len > 0.1F) {
            in[PLAYER].move_dir[0] = move_dir[0] / len;
            in[PLAYER].move_dir[1] = move_dir[1] / len;
        }
        /* la simulation ne déplace pas le joueur (moving = false) : c'est le personnage physique */
        if ((in[PLAYER].light || in[PLAYER].heavy) && c->lock >= 0) {
            const float dx = c->sim.bodies[c->lock].pos[0] - player_xz[0];
            const float dz = c->sim.bodies[c->lock].pos[1] - player_xz[1];
            if (dx * dx + dz * dz > 1e-6F) {
                out->face = true;
                out->facing = atan2f(-dx, -dz);
                c->sim.bodies[PLAYER].yaw = out->facing;
                c->sim.bodies[PLAYER].prev_yaw = out->facing;
                pf->yaw = out->facing;
            }
        }
    }
    eng_dummy_tick(&c->dummy, &c->sim, &in[DUMMY]);
    const bool moving = (move_dir[0] * move_dir[0] + move_dir[1] * move_dir[1]) > 0.01F;
    const bool attacking = pf->fsm.state == ENG_CS_ATTACK;
    eng_aig_tick(&c->group, &c->sim, attacking ? 12.0F : (moving ? 8.0F : 0.0F), in);

    const float before[2] = {c->sim.bodies[PLAYER].pos[0], c->sim.bodies[PLAYER].pos[1]};
    rig_tick(c); /* M5-05 / M5-06 : hurtboxes sur les os, lame en main */
    eng_sim_step(&c->sim, in);
    /* le mannequin est un poteau : ni recul ni poussée */
    (void)eng_sim_place(&c->sim, DUMMY, DUMMY_X, DUMMY_Z, c->sim.bodies[DUMMY].yaw);
    out->push[0] = c->sim.bodies[PLAYER].pos[0] - before[0];
    out->push[1] = c->sim.bodies[PLAYER].pos[1] - before[1];
    const float push_len = hypotf(out->push[0], out->push[1]);
    if (push_len > MAX_PUSH) { /* recul d'un coup lourd au plus ; jamais un saut de téléportation */
        out->push[0] *= MAX_PUSH / push_len;
        out->push[1] *= MAX_PUSH / push_len;
    }
    const EngCombatState st = pf->fsm.state;
    out->locked = st == ENG_CS_ATTACK || st == ENG_CS_DODGE || st == ENG_CS_HITSTUN ||
                  st == ENG_CS_STAGGER || st == ENG_CS_DEAD || st == ENG_CS_PARRY;

    /* retours : particules, son, mesures */
    for (uint32_t k = 0; k < c->sim.event_count; k++) {
        const EngSimEvent *e = &c->sim.events[k];
        (void)eng_vfx_on_event(&c->particles, e);
        (void)eng_dmgnum_spawn(&c->numbers, e);
        if (c->device != NULL) {
            eng_audio_device_lock(c->device);
            (void)eng_combat_audio_on_event(&c->audio, e, player_xz, facing);
            eng_audio_device_unlock(c->device);
        }
        if (e->defender == PLAYER && e->kind != ENG_HITKIND_IGNORED) {
            c->stats.player_hits_taken++;
        }
    }
    eng_particles_update(&c->particles);
    eng_dmgnum_update(&c->numbers);
    for (uint32_t i = FIRST_ENEMY; i < FIGHTERS; i++) {
        const bool dead = c->sim.fighters[i].vitals.dead;
        if (dead && !c->was_dead[i]) {
            c->stats.enemy_kills++;
            const uint32_t xp = eng_prog_kill_xp(c->sim.fighters[i].vitals.max_hp);
            if (eng_prog_add_xp(&c->progress, xp) > 0U) {
                apply_progress(c, true); /* montée de niveau : soins complets */
                c->levelup_ticks = 180U;
                ENG_LOG_INFO("progression : niveau %u (%u XP au total)", c->progress.level,
                             c->progress.total_xp);
            }
        }
        c->was_dead[i] = dead;
    }
    uint32_t engaged = 0;
    for (uint32_t i = 0; i < c->group.count; i++) {
        engaged += c->group.agents[i].state == ENG_AI_ENGAGE ? 1U : 0U;
    }
    const uint32_t att = eng_aig_attackers(&c->group, &c->sim);
    if (att > c->stats.max_attackers) {
        c->stats.max_attackers = att;
    }
    if (engaged > c->stats.enemies_engaged) {
        c->stats.enemies_engaged = engaged;
    }

    /* mort du joueur : renaissance et camp remis à neuf ; camp vaincu : réapparition */
    if (pf->vitals.dead) {
        if (++c->dead_ticks >= RESPAWN_TICKS) {
            const EngFighterParams p = pf->params;
            eng_fighter_init(pf, &p);
            apply_progress(c, true); /* la renaissance garde le niveau */
            c->dead_ticks = 0;
            spawn_all(c);
            ENG_LOG_INFO("combat : le joueur renaît, le camp est remis à neuf");
        }
    }
    if (eng_camp_cleared(&c->group, &c->sim)) {
        if (eng_equip_give(&c->equip, ENG_WEAPON_GREATSWORD)) { /* M8-08 : récompense */
            c->weapon_ticks = 300U;
            ENG_LOG_INFO("combat : camp vaincu : espadon obtenu (R pour changer d'arme)");
        }
        if (++c->cleared_ticks >= CAMP_RESPAWN_TICKS) {
            c->cleared_ticks = 0;
            spawn_all(c);
            ENG_LOG_INFO("combat : camp vaincu, il réapparaît");
        }
    } else {
        c->cleared_ticks = 0;
    }
    if (c->levelup_ticks > 0U) {
        c->levelup_ticks--;
    }
    if (c->weapon_ticks > 0U) {
        c->weapon_ticks--;
    }
    c->stats.level = c->progress.level;
    c->stats.total_xp = c->progress.total_xp;
    c->stats.tick = c->sim.tick;
    c->stats.dummy_damage = c->dummy.total_damage;
    c->stats.player_hp = pf->vitals.hp;
    c->stats.lock_target = c->lock;
    c->stats.attacks_version = c->attacks_version;
    c->stats.audio_callbacks = c->device != NULL ? eng_audio_device_callbacks(c->device) : 0U;
}

static void bar(EngDebugLines *lines, const float p[2], float y, float frac, float width,
                EngDebugColor col)
{
    frac = frac < 0.0F ? 0.0F : (frac > 1.0F ? 1.0F : frac);
    const vec3 a = {p[0] - width * 0.5F, y, p[1]};
    const vec3 b = {p[0] + width * 0.5F, y, p[1]};
    const vec3 m = {a[0] + width * frac, y, p[1]};
    (void)eng_debug_line(lines, a, b, ENG_DEBUG_GRAY);
    if (frac > 0.0F) {
        const vec3 a2 = {a[0], y + 0.03F, p[1]};
        const vec3 m2 = {m[0], y + 0.03F, p[1]};
        (void)eng_debug_line(lines, a2, m2, col);
    }
}

static void shift(EngDebugLines *lines, uint32_t depth_from, uint32_t overlay_from, float dy)
{
    for (uint32_t i = depth_from; i < lines->depth_count; i++) {
        lines->depth[i].position[1] += dy;
    }
    for (uint32_t i = overlay_from; i < lines->overlay_count; i++) {
        lines->overlay[i].position[1] += dy;
    }
}

void play_combat_draw(const PlayCombat *c, EngDebugLines *lines, const PlayMode *play)
{
    if (c == NULL || !c->ready || lines == NULL) {
        return;
    }
    for (uint32_t i = 0; i < c->sim.count; i++) {
        const EngFighter *f = &c->sim.fighters[i];
        const EngSimBody *b = &c->sim.bodies[i];
        const uint32_t d0 = lines->depth_count;
        const uint32_t o0 = lines->overlay_count;
        if (f->vitals.dead) {
            const vec3 a = {b->pos[0] - 0.5F, 0.05F, b->pos[1]};
            const vec3 e = {b->pos[0] + 0.5F, 0.05F, b->pos[1]};
            (void)eng_debug_line(lines, a, e, ENG_DEBUG_GRAY);
        } else {
            const bool modeled = i == PLAYER ? play->avatars != NULL : c->has_avatar[i];
            /* F3 avec les modèles : capsules PAR-DESSUS (sinon cachées dans les corps opaques) */
            eng_debug_lines_set_overlay(lines, modeled);
            if (i != PLAYER && (!c->has_avatar[i] || play->show_debug)) {
                /* le joueur a déjà sa capsule physique ; avec les modèles animés : F3 seulement */
                EngCapsule box[ENG_HURTBOX_MAX];
                EngHitZone zones[ENG_HURTBOX_MAX];
                uint32_t nb = ENG_SIM_HURTBOXES;
                if (c->sim.rig[i].count > 0U) { /* hurtboxes sur les os (M5-05) */
                    nb = c->sim.rig[i].count;
                    memcpy(box, c->sim.rig[i].cur, sizeof(EngCapsule) * nb);
                    memcpy(zones, c->sim.rig[i].zones, sizeof(EngHitZone) * nb);
                } else {
                    eng_sim_hurtboxes(b, false, box, zones);
                }
                const bool touchable = eng_fighter_hittable(f);
                for (uint32_t k = 0; k < nb; k++) {
                    (void)eng_capsule_debug_draw(
                        lines, &box[k], touchable ? eng_zone_color(zones[k]) : ENG_DEBUG_GRAY);
                }
            }
            if (f->fsm.state == ENG_CS_ATTACK && (!modeled || play->show_debug)) {
                /* arme : orange pendant l'anticipation d'un ennemi, rouge quand elle peut toucher
                 */
                const bool active = eng_fighter_hitboxes_active(f, c->sim.attacks);
                if (active || i >= FIRST_ENEMY) {
                    EngCapsule w[ENG_ATTACK_MAX_HITBOXES];
                    uint32_t nw = 0;
                    if (c->sim.weapon[i].count > 0U) { /* la lame animée (M5-06) */
                        nw = c->sim.weapon[i].count;
                        memcpy(w, c->sim.weapon[i].cur, sizeof(EngCapsule) * nw);
                    } else {
                        nw = eng_sim_weapons(b, false, eng_fighter_attack(f, c->sim.attacks), w);
                    }
                    for (uint32_t k = 0; k < nw; k++) {
                        (void)eng_capsule_debug_draw(
                            lines, &w[k], active ? ENG_DEBUG_RED : ENG_DEBUG_RGB(255, 150, 30));
                    }
                }
            }
            eng_debug_lines_set_overlay(lines, false);
            (void)eng_trail_draw_debug(&c->trail[i], lines); /* M5-20 */
            bar(lines, b->pos, 2.15F, f->vitals.hp / f->vitals.max_hp, 0.9F, ENG_DEBUG_GREEN);
            bar(lines, b->pos, 2.05F, f->posture / f->params.max_posture, 0.9F, ENG_DEBUG_YELLOW);
            if ((int32_t)i == c->lock) {
                const vec3 ctr = {b->pos[0], 0.05F, b->pos[1]};
                const vec3 u = {0.7F, 0, 0};
                const vec3 v = {0, 0, 0.7F};
                (void)eng_debug_circle(lines, ctr, u, v, ENG_DEBUG_MAGENTA, 24);
            }
        }
        shift(lines, d0, o0, play_mode_ground(play, b->pos[0], b->pos[1]));
    }
    /* cônes de l'IA et particules : posés à la hauteur du sol sous le joueur (approximation) */
    const uint32_t d0 = lines->depth_count;
    const uint32_t o0 = lines->overlay_count;
    if (c->avatars == NULL || play->show_debug || c->ai_selected >= 0) {
        /* cônes de vision : debug (F3) ; l'ennemi sélectionné au clic en blanc, avec son chemin */
        (void)eng_aidbg_draw(&c->group, &c->sim, c->ai_selected, lines);
    }
    (void)eng_particles_draw_debug(&c->particles, lines);
    shift(lines, d0, o0,
          play_mode_ground(play, c->sim.bodies[PLAYER].pos[0], c->sim.bodies[PLAYER].pos[1]));
}

/* ── Personnages animés (M4) ── */

static float smooth01(float u)
{
    u = u < 0.0F ? 0.0F : (u > 1.0F ? 1.0F : u);
    return u * u * (3.0F - (2.0F * u)); /* même courbe que l'esquive du combattant (fighter.c) */
}

static void fighter_drive(const PlayCombat *c, uint32_t i, const AvatarLibrary *lib, double alpha,
                          EngActorDrive *d)
{
    const EngFighter *f = &c->sim.fighters[i];
    const EngSimBody *b = &c->sim.bodies[i];
    d->tick_fraction = c->sim.hitstop > 0U ? 0.0F : (float)alpha; /* gel d'impact : figé */
    d->state = f->vitals.dead ? ENG_CS_DEAD : f->fsm.state;
    d->state_ticks = f->fsm.ticks;
    if (d->state == ENG_CS_ATTACK) {
        const EngAttack *atk = eng_fighter_attack(f, c->sim.attacks);
        d->state_ticks = f->attack_tick;
        d->attack_clip = atk != NULL && atk->anim[0] != '\0' ? avatar_clip(lib, atk->anim) : NULL;
        d->attack_speed = atk != NULL ? atk->anim_speed : 1.0F;
        d->attack_offset = atk != NULL ? atk->anim_offset : 0.0F;
        d->attack_serial = f->attack_serial;
    } else if (d->state == ENG_CS_DODGE) {
        const float total = (float)f->params.dodge_total;
        d->dodge_progress = smooth01(((float)f->state_tick + d->tick_fraction) / total);
        /* monde -> repère du rig (avant +Z, gauche +X) */
        const float yaw = b->yaw;
        const float fwd = (f->dodge_dir[0] * -sinf(yaw)) + (f->dodge_dir[1] * -cosf(yaw));
        const float right = (f->dodge_dir[0] * cosf(yaw)) + (f->dodge_dir[1] * -sinf(yaw));
        d->dodge_dir[0] = -right;
        d->dodge_dir[1] = fwd;
    }
}

void play_combat_player_drive(const PlayCombat *c, const AvatarLibrary *lib, double alpha,
                              EngActorDrive *d)
{
    if (c == NULL || !c->ready || d == NULL) {
        return;
    }
    fighter_drive(c, PLAYER, lib, alpha, d);
}

/* Portée réelle d'une attaque (M4) : plus grande avancée de la lame devant le corps (lacet 0 :
 * avant = -Z) pendant ses ticks actifs, clip calé comme en jeu ; 0 si rien à mesurer (pas de clip,
 * arme sans tranchant). */
static float blade_reach(PlayCombat *c, const AvatarLibrary *lib, const EngAttack *atk,
                         AvatarProp prop, float scale)
{
    const EngAnimClip *clip = atk != NULL ? avatar_clip(lib, atk->anim) : NULL;
    if (clip == NULL || c->reach_pose.joint_count == 0U) {
        return 0.0F;
    }
    static mat4 world[AVATAR_MAX_JOINTS];
    const float feet[3] = {0.0F, 0.0F, 0.0F};
    mat4 model;
    avatar_model_matrix(feet, 0.0F, scale, model);
    float best = 0.0F;
    for (uint32_t k = atk->startup; k < atk->startup + atk->active; k++) {
        const float t = atk->anim_offset + ((float)k / 60.0F * atk->anim_speed);
        EngCapsule blade;
        if (eng_pose_sample(&c->reach_pose, &lib->skeleton, clip, t, ENG_ANIM_WRAP_CLAMP) &&
            eng_pose_world(&c->reach_pose, &lib->skeleton, world) &&
            avatar_blade_world(lib, prop, world, model, scale, &blade)) {
            best = fmaxf(best, fmaxf(-blade.a[2], -blade.b[2]) + blade.radius);
        }
    }
    return best;
}

/* Les plans d'attaque de l'IA (portée maximale) sont bornés par la portée réelle de la lame, plus
 * le rayon d'un torse (0,2 m) : sans cela, un ennemi frapperait dans le vide (données écrites pour
 * l'ancienne hitbox « corps » de 1,5 m). */
static void fit_plans_to_reach(PlayCombat *c, const AvatarLibrary *lib, uint32_t arch,
                               AvatarProp prop, float scale)
{
    EngArchetype *a = &c->arch.a[arch];
    for (uint32_t p = 0; p < a->plan_count; p++) {
        EngArchPlan *plan = &a->plans[p];
        const uint32_t idx = eng_attackset_find(&c->attacks, plan->heavy ? a->heavy : a->light);
        const EngAttack *atk = idx != ENG_ATTACK_NONE ? &c->attacks.attacks[idx] : NULL;
        const float reach = blade_reach(c, lib, atk, prop, scale);
        if (reach <= 0.0F) {
            continue;
        }
        const float limit = fmaxf(plan->range_min + 0.1F, reach + 0.2F);
        ENG_LOG_INFO("combat : %s %s (%s) : lame à %.2f m, portée du plan %.2f -> %.2f m", a->name,
                     plan->heavy ? "lourde" : "légère", atk->name, (double)reach,
                     (double)plan->range_max, (double)fminf(plan->range_max, limit));
        plan->range_max = fminf(plan->range_max, limit);
    }
}

bool play_combat_attach_avatars(PlayCombat *c, AvatarLibrary *lib, const PlayMode *play)
{
    (void)play;
    if (c == NULL || !c->ready || lib == NULL || !lib->cpu_ready) {
        return false;
    }
    c->avatars = lib;
    bool ok = true;
    /* hurtboxes du rig (source du dépôt d'abord : rechargées à chaque partie) */
    const char *text = NULL;
    const EngArenaMark mark = eng_arena_mark(&c->scratch);
    char err[160] = "fichier absent";
    eng_hurtboxset_init(&c->hurt);
    const bool hurt_ok = read_text(&c->scratch, "characters/hurtboxes.txt", &text) &&
                         eng_hurtboxset_parse(text, &lib->skeleton, &c->hurt, err, sizeof err);
    eng_arena_reset_to(&c->scratch, mark);
    if (!hurt_ok) {
        ENG_LOG_WARN("combat : hurtboxes du rig illisibles (%s) : corps simplifié", err);
    }
    for (uint32_t i = DUMMY; i < c->sim.count && i < FIGHTERS; i++) {
        AvatarBody body = AVATAR_SKELETON_MINION;
        AvatarProp right = AVATAR_SKELETON_BLADE;
        AvatarProp left = AVATAR_PROP_NONE;
        float scale = 1.0F;
        if (i == DUMMY) {
            right = AVATAR_PROP_NONE; /* squelette d'entraînement, mains nues */
        } else {
            const uint32_t k = i - FIRST_ENEMY;
            const uint32_t arch = k < c->camp.count ? c->camp.m[k].arch : UINT32_MAX;
            const char *name = arch < c->arch.count ? c->arch.a[arch].name : "";
            if (strcmp(name, "brute") == 0) {
                body = AVATAR_SKELETON_WARRIOR;
                right = AVATAR_SKELETON_AXE;
                scale = 1.2F;
            } else if (strcmp(name, "archer") == 0) {
                body = AVATAR_SKELETON_ROGUE;
                right = AVATAR_SKELETON_CROSSBOW;
            } else {
                left = AVATAR_SKELETON_SHIELD;
            }
        }
        c->has_avatar[i] = avatar_init(&c->avatar[i], lib, body, right, left, scale);
        ok = ok && c->has_avatar[i];
        c->rig_prop[i] = right;
        c->rig_scale[i] = scale;
        if (i >= FIRST_ENEMY && c->reach_pose.joint_count == 0U) {
            (void)eng_pose_init(&lib->arena, &lib->skeleton, &c->reach_pose);
        }
        if (i >= FIRST_ENEMY && i - FIRST_ENEMY < c->camp.count) {
            fit_plans_to_reach(c, lib, c->camp.m[i - FIRST_ENEMY].arch, right, scale);
        }
    }
    if (c->reach_pose.joint_count > 0U) {
        const uint32_t light = eng_attackset_find(&c->attacks, "light1");
        ENG_LOG_INFO("combat : portée de l'épée du joueur (light1) : %.2f m",
                     (double)blade_reach(
                         c, lib, light != ENG_ATTACK_NONE ? &c->attacks.attacks[light] : NULL,
                         AVATAR_SWORD, 1.0F));
    }
    for (uint32_t i = 0; i < FIGHTERS; i++) {
        (void)eng_trail_init(&c->trail[i], 10U); /* 10 ticks de rémanence */
    }
    c->rig_prop[PLAYER] = /* le chevalier (PlayMode), arme en main (M8-08) */
        c->equip.equipped == ENG_WEAPON_GREATSWORD ? AVATAR_GREATSWORD : AVATAR_SWORD;
    c->rig_scale[PLAYER] = 1.0F;
    for (uint32_t i = 0; hurt_ok && i < c->sim.count && i < FIGHTERS; i++) {
        c->rig_ready[i] =
            (i == PLAYER || c->has_avatar[i]) &&
            eng_actor_anim_init(&c->rig_anim[i], &lib->arena, &lib->skeleton, &lib->set);
    }
    return ok;
}

/* Avant chaque pas de simulation : pose au rythme des ticks (dt = 1/60, sans interpolation), puis
 * hurtboxes et lame dans le plan de la simulation (pieds à y = 0, comme son corps simplifié et les
 * hitboxes du repère du corps). */
static void rig_tick(PlayCombat *c)
{
    if (c->avatars == NULL) {
        return;
    }
    const AvatarLibrary *lib = c->avatars;
    for (uint32_t i = 0; i < c->sim.count && i < FIGHTERS; i++) {
        if (!c->rig_ready[i]) {
            continue;
        }
        const EngSimBody *b = &c->sim.bodies[i];
        const float dx = b->pos[0] - b->prev_pos[0];
        const float dz = b->pos[1] - b->prev_pos[1];
        EngActorDrive d = {.ground_speed = sqrtf((dx * dx) + (dz * dz)) * 60.0F, .grounded = true};
        fighter_drive(c, i, lib, 0.0, &d);
        if (d.state == ENG_CS_ATTACK) {
            d.state_ticks += 1U; /* le tick que la simulation va jouer */
        }
        d.ground_speed /= AVATAR_SCALE * c->rig_scale[i];
        (void)eng_actor_anim_update(&c->rig_anim[i], &d, 1.0F / 60.0F);
        if (!eng_pose_world(&c->rig_anim[i].pose, &lib->skeleton, c->rig_world[i])) {
            continue;
        }
        const float feet[3] = {b->pos[0], 0.0F, b->pos[1]};
        mat4 model;
        avatar_model_matrix(feet, b->yaw, c->rig_scale[i], model);
        EngCapsule caps[ENG_HURTBOX_MAX];
        EngHitZone zones[ENG_HURTBOX_MAX];
        if (eng_hurtboxset_world(&c->hurt, c->rig_world[i], lib->skeleton.joint_count, model,
                                 caps)) {
            for (uint32_t k = 0; k < c->hurt.count; k++) {
                zones[k] = c->hurt.boxes[k].zone;
            }
            (void)eng_sim_set_rig(&c->sim, i, caps, zones, c->hurt.count);
        }
        EngCapsule blade;
        if (avatar_blade_world(lib, c->rig_prop[i], c->rig_world[i], model, c->rig_scale[i],
                               &blade)) {
            (void)eng_sim_set_weapon(&c->sim, i, &blade, 1);
            /* traînée : la lame laisse sa trace pendant toute l'attaque, puis s'efface */
            if (d.state == ENG_CS_ATTACK) {
                (void)eng_trail_push(&c->trail[i], blade.a, blade.b);
            }
        } else {
            (void)eng_sim_set_weapon(&c->sim, i, NULL, 0);
        }
        eng_trail_update(&c->trail[i]);
    }
}

static float lerp_angle(float a, float b, float t)
{
    float d = fmodf(b - a + 3.14159265F, 6.28318531F);
    if (d < 0.0F) {
        d += 6.28318531F;
    }
    return a + ((d - 3.14159265F) * t);
}

/* Télégraphe (M6) : l'arme d'un ennemi s'allume en ORANGE pendant l'anticipation, de plus en plus
 * fort jusqu'au coup, puis ROUGE tant qu'elle peut toucher. Valeurs HDR : le bloom les fait
 * rayonner. Le joueur et le mannequin ne s'allument pas. */
static void telegraph_glow(const PlayCombat *c, uint32_t i, float glow[3])
{
    glow[0] = 0.0F;
    glow[1] = 0.0F;
    glow[2] = 0.0F;
    const EngFighter *f = &c->sim.fighters[i];
    if (i < FIRST_ENEMY || f->vitals.dead || f->fsm.state != ENG_CS_ATTACK) {
        return;
    }
    const EngAttack *a = eng_fighter_attack(f, c->sim.attacks);
    if (a == NULL) {
        return;
    }
    const EngAttackPhase ph = eng_attack_phase(a, f->attack_tick);
    if (ph == ENG_ATTACK_STARTUP) {
        const float u = a->startup > 0U ? (float)f->attack_tick / (float)a->startup : 1.0F;
        const float k = 0.5F + (1.5F * (u > 1.0F ? 1.0F : u)); /* monte jusqu'au coup */
        glow[0] = 1.0F * k; /* orange : assez peu intense pour que le tonemapping garde la teinte */
        glow[1] = 0.28F * k;
        glow[2] = 0.02F * k;
    } else if (ph == ENG_ATTACK_ACTIVE) {
        glow[0] = 3.0F; /* rouge franc */
        glow[1] = 0.08F;
        glow[2] = 0.04F;
    }
}

void play_combat_update_visuals(PlayCombat *c, const PlayMode *play, double alpha, float dt)
{
    if (c == NULL || !c->ready || c->avatars == NULL) {
        return;
    }
    const float t = (float)alpha;
    for (uint32_t i = DUMMY; i < c->sim.count && i < FIGHTERS; i++) {
        if (!c->has_avatar[i]) {
            continue;
        }
        const EngSimBody *b = &c->sim.bodies[i];
        const float x = b->prev_pos[0] + ((b->pos[0] - b->prev_pos[0]) * t);
        const float z = b->prev_pos[1] + ((b->pos[1] - b->prev_pos[1]) * t);
        const float feet[3] = {x, play_mode_ground(play, x, z), z};
        const float dx = b->pos[0] - b->prev_pos[0];
        const float dz = b->pos[1] - b->prev_pos[1];
        EngActorDrive d = {.ground_speed = sqrtf((dx * dx) + (dz * dz)) * 60.0F, .grounded = true};
        fighter_drive(c, i, c->avatars, alpha, &d);
        avatar_update(&c->avatar[i], c->avatars, &d, dt, feet, lerp_angle(b->prev_yaw, b->yaw, t));
        telegraph_glow(c, i, c->avatar[i].glow);
    }
}

void play_combat_draw_models(const PlayCombat *c, const AvatarLibrary *lib, EngRenderQueue *queue,
                             mat4 view_proj, const EngLighting *light, const EngModelShadow *shadow,
                             bool shadow_pass)
{
    if (c == NULL || !c->ready || lib == NULL) {
        return;
    }
    for (uint32_t i = DUMMY; i < c->sim.count && i < FIGHTERS; i++) {
        if (!c->has_avatar[i]) {
            continue;
        }
        if (shadow_pass) {
            avatar_draw_shadow(&c->avatar[i], lib, queue, view_proj);
        } else {
            avatar_draw(&c->avatar[i], lib, queue, view_proj, light, shadow);
        }
    }
}

uint32_t play_combat_set_relief(PlayCombat *c, float (*height)(void *user, float x, float z),
                                void *user, float max_slope_deg)
{
    if (c == NULL || !c->ready) {
        return 0;
    }
    const uint32_t n = eng_nav_block_steep(&c->nav, height, user, max_slope_deg);
    const uint32_t islands = eng_nav_remove_islands(&c->nav, PLAY_NAV_MIN_REGION);
    c->sim.nav = &c->nav; /* les ennemis ne montent jamais sur une pente bloquée */
    /* postes et rondes posés sur une pente : déplacés sur le praticable le plus proche */
    uint32_t moved = 0;
    for (uint32_t i = 0; i < c->group.count && i < c->camp.count; i++) {
        EngAiAgent *a = &c->group.agents[i];
        EngCampMember *m = &c->camp.m[i];
        moved += eng_nav_walkable(&c->nav, m->pos) ? 0U : 1U;
        (void)eng_nav_nearest_walkable(&c->nav, m->pos, m->pos);
        a->home[0] = m->pos[0];
        a->home[1] = m->pos[1];
        for (uint32_t k = 0; k < m->patrol_n && k < ENG_AI_PATROL_MAX; k++) {
            (void)eng_nav_nearest_walkable(&c->nav, m->patrol[k], m->patrol[k]);
        }
        for (uint32_t k = 0; k < a->patrol_n && k < ENG_AI_PATROL_MAX; k++) {
            (void)eng_nav_nearest_walkable(&c->nav, a->patrol[k], a->patrol[k]);
        }
    }
    spawn_all(c);
    ENG_LOG_INFO("combat : navigation sur le relief, %u cellule(s) trop raide(s) (> %.0f°), %u en "
                 "îlots isolés, %u poste(s) d'ennemi déplacé(s) hors pente",
                 n, (double)max_slope_deg, islands, moved);
    return n;
}

size_t play_combat_debug_text(const PlayCombat *c, char *buf, size_t cap)
{
    if (c == NULL || !c->ready || buf == NULL || cap == 0U) {
        return 0;
    }
    static const char *const names[FIGHTERS] = {"joueur", "mannequin", "ennemi 1", "ennemi 2",
                                                "ennemi 3"};
    static const char *const phases[4] = {"anticipation", "ACTIF", "récupération", "fini"};
    size_t n = 0;
    buf[0] = '\0';
    n += (size_t)snprintf(buf + n, cap - n, "tick %u%s\n", (unsigned)c->sim.tick,
                          c->sim.hitstop > 0U ? "  (gel d'impact)" : "");
    for (uint32_t i = 0; i < c->sim.count && i < FIGHTERS && n < cap; i++) {
        const EngFighter *f = &c->sim.fighters[i];
        char what[96] = "";
        if (f->fsm.state == ENG_CS_ATTACK) {
            const EngAttack *a = eng_fighter_attack(f, c->sim.attacks);
            const EngAttackPhase ph =
                a != NULL ? eng_attack_phase(a, f->attack_tick) : ENG_ATTACK_DONE;
            (void)snprintf(what, sizeof what, " %s t%u %s", a != NULL ? a->name : "?",
                           (unsigned)f->attack_tick, phases[(uint32_t)ph < 4U ? ph : 3U]);
        } else {
            (void)snprintf(what, sizeof what, " t%u", (unsigned)f->fsm.ticks);
        }
        const int k = snprintf(
            buf + n, cap - n, "%s : %s%s\n    PV %.0f/%.0f  posture %.0f  endurance %.0f\n",
            names[i], eng_combat_state_name(f->vitals.dead ? ENG_CS_DEAD : f->fsm.state), what,
            (double)f->vitals.hp, (double)f->vitals.max_hp, (double)f->posture, (double)f->stamina);
        if (k < 0) {
            break;
        }
        n += (size_t)k;
    }
    return n < cap ? n : cap - 1U;
}

int32_t play_combat_pick(PlayCombat *c, const float origin[3], const float dir[3], float ground)
{
    if (c == NULL || !c->ready || origin == NULL || dir == NULL) {
        return -1;
    }
    const float o[3] = {origin[0], origin[1] - ground,
                        origin[2]}; /* monde -> plan de la simulation */
    const int32_t picked = eng_aidbg_pick(&c->group, &c->sim, o, dir, 200.0F);
    if (picked != c->ai_selected) {
        if (picked >= 0) {
            const EngAiAgent *a = &c->group.agents[picked];
            ENG_LOG_INFO("IA : ennemi %u sélectionné (%s)",
                         (unsigned)(a->fighter - FIRST_ENEMY + 1U),
                         a->arch < c->arch.count ? c->arch.a[a->arch].name : "?");
        } else {
            ENG_LOG_INFO("IA : sélection effacée");
        }
    }
    c->ai_selected = picked;
    return c->ai_selected;
}

size_t play_combat_ai_text(const PlayCombat *c, char *buf, size_t cap)
{
    if (c == NULL || !c->ready || buf == NULL || cap == 0U || c->ai_selected < 0) {
        return 0;
    }
    EngAiDebugInfo info;
    if (!eng_aidbg_info(&c->group, &c->sim, (uint32_t)c->ai_selected, &info)) {
        buf[0] = '\0';
        return 0;
    }
    return eng_aidbg_format(&info, buf, cap);
}

void play_combat_reset(PlayCombat *c)
{
    if (c == NULL || !c->ready) {
        return;
    }
    EngFighter *pf = &c->sim.fighters[PLAYER];
    const EngFighterParams p = pf->params;
    const float max_hp = pf->vitals.max_hp;
    eng_fighter_init(pf, &p);
    pf->vitals.max_hp = max_hp;
    pf->vitals.hp = max_hp;
    EngFighter *df = &c->sim.fighters[DUMMY];
    const EngFighterParams dp = df->params;
    eng_fighter_init(df, &dp);
    df->vitals.max_hp = 1000.0F;
    df->vitals.hp = 1000.0F;
    spawn_all(c);
    c->lock = -1;
    c->dead_ticks = 0;
    c->cleared_ticks = 0;
    eng_dmgnum_init(&c->numbers);
    eng_particles_init(&c->particles, 12345U);
    memset(c->was_dead, 0, sizeof c->was_dead);
    eng_prog_init(&c->progress); /* nouvelle partie : niveau 1 */
    c->levelup_ticks = 0;
    eng_equip_init(&c->equip); /* M8-08 : épée et bouclier */
    c->weapon_ticks = 0;
    apply_weapon(c);
    apply_progress(c, true);
}

void play_combat_snapshot(const PlayCombat *c, EngSaveGame *s)
{
    if (c == NULL || s == NULL || !c->ready) {
        return;
    }
    const EngFighter *pf = &c->sim.fighters[PLAYER];
    s->hp = pf->vitals.hp;
    s->hp_max = pf->vitals.max_hp;
    s->stamina = pf->stamina;
    s->posture = pf->posture;
    s->has_progress = true;
    s->progress = c->progress;
    s->has_equipment = true; /* M8-08 */
    s->equipment = c->equip;
    s->has_enemies = true;
    s->enemy_count = c->group.count < ENG_SAVE_MAX_ENEMIES ? c->group.count : ENG_SAVE_MAX_ENEMIES;
    for (uint32_t i = 0; i < s->enemy_count; i++) {
        const uint32_t f = c->group.agents[i].fighter;
        s->enemies[i].alive = !c->sim.fighters[f].vitals.dead;
        s->enemies[i].hp = c->sim.fighters[f].vitals.hp;
        s->enemies[i].pos[0] = c->sim.bodies[f].pos[0];
        s->enemies[i].pos[1] = c->sim.bodies[f].pos[1];
        s->enemies[i].yaw = c->sim.bodies[f].yaw;
    }
}

void play_combat_restore(PlayCombat *c, const EngSaveGame *s)
{
    if (c == NULL || s == NULL || !c->ready) {
        return;
    }
    play_combat_reset(c);
    EngFighter *pf = &c->sim.fighters[PLAYER];
    if (s->has_progress && eng_prog_valid(&s->progress)) {
        c->progress = s->progress;
        apply_progress(c, false);
    }
    if (s->has_equipment && eng_equip_valid(&s->equipment)) { /* M8-08 */
        c->equip = s->equipment;
        apply_weapon(c);
    }
    pf->vitals.max_hp = s->hp_max;
    pf->vitals.hp = s->hp > s->hp_max ? s->hp_max : s->hp;
    pf->stamina = s->stamina;
    pf->posture = s->posture;
    if (!s->has_enemies) {
        return; /* sauvegarde v1 : le camp repart de son état initial */
    }
    for (uint32_t i = 0; i < s->enemy_count && i < c->group.count; i++) {
        const uint32_t f = c->group.agents[i].fighter;
        EngFighter *ef = &c->sim.fighters[f];
        (void)eng_sim_place(&c->sim, f, s->enemies[i].pos[0], s->enemies[i].pos[1],
                            s->enemies[i].yaw);
        if (s->enemies[i].alive) {
            ef->vitals.hp = s->enemies[i].hp > 0.0F ? s->enemies[i].hp : 1.0F;
        } else {
            ef->vitals.hp = 0.0F;
            ef->vitals.dead = true;
            ef->fsm.state = ENG_CS_DEAD;
            c->was_dead[f] = true;
        }
    }
}

void play_combat_apply_options(PlayCombat *c, const EngGameOptions *o)
{
    if (c == NULL || o == NULL || !c->ready) {
        return;
    }
    if (c->device != NULL) {
        eng_audio_device_lock(c->device);
    }
    c->mixer.master = o->master;
    for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
        (void)eng_mixer_set_bus(&c->mixer, (EngAudioBus)b, o->volume[b]);
    }
    if (c->device != NULL) {
        eng_audio_device_unlock(c->device);
    }
}

static bool project(mat4 view_proj, const float p[3], int w, int h, float *sx, float *sy)
{
    vec4 v = {p[0], p[1], p[2], 1.0F};
    vec4 clip;
    glm_mat4_mulv(view_proj, v, clip);
    if (clip[3] <= 0.05F) {
        return false;
    }
    const float nx = clip[0] / clip[3];
    const float ny = clip[1] / clip[3];
    if (nx < -1.2F || nx > 1.2F || ny < -1.2F || ny > 1.2F) {
        return false;
    }
    *sx = (nx * 0.5F + 0.5F) * (float)w;
    *sy = (0.5F - ny * 0.5F) * (float)h;
    return true;
}

void play_combat_fill_hud(const PlayCombat *c, const PlayMode *play, mat4 view_proj, int w, int h,
                          EngHud *hud)
{
    if (c == NULL || hud == NULL || !c->ready) {
        return;
    }
    const EngFighter *pf = &c->sim.fighters[PLAYER];
    hud->hp = pf->vitals.hp;
    hud->hp_max = pf->vitals.max_hp;
    hud->stamina = pf->stamina;
    hud->stamina_max = pf->params.max_stamina;
    hud->posture = pf->posture;
    hud->posture_max = pf->params.max_posture;
    hud->level = c->progress.level;
    (void)snprintf(hud->weapon, sizeof hud->weapon, "%s", eng_weapon_def(c->equip.equipped)->label);
    {
        const uint32_t need = eng_prog_need(c->progress.level);
        hud->xp_frac = need > 0U ? (float)c->progress.xp / (float)need : 1.0F;
    }
    hud->has_target = c->lock >= 0;
    if (hud->has_target) {
        const EngFighter *tf = &c->sim.fighters[c->lock];
        const char *name = "Mannequin";
        for (uint32_t i = 0; i < c->group.count; i++) {
            if (c->group.agents[i].fighter == (uint32_t)c->lock) {
                name = c->arch.a[c->group.agents[i].arch].name;
            }
        }
        (void)snprintf(hud->target_name, sizeof hud->target_name, "%s", name);
        hud->target_hp = tf->vitals.hp;
        hud->target_hp_max = tf->vitals.max_hp;
        hud->target_posture = tf->posture;
        hud->target_posture_max = tf->params.max_posture;
    }
    hud->number_count = 0;
    for (uint32_t i = 0; i < ENG_DMGNUM_MAX && hud->number_count < ENG_HUD_NUMBERS; i++) {
        const EngDamageNumber *d = &c->numbers.items[i];
        if (!d->active) {
            continue;
        }
        const float ground = play_mode_ground(play, d->pos[0], d->pos[2]);
        const float p[3] = {d->pos[0], d->pos[1] + ground + 0.4F, d->pos[2]};
        EngHudNumber *n = &hud->numbers[hud->number_count];
        if (!project(view_proj, p, w, h, &n->x, &n->y) ||
            !eng_dmgnum_text(d, n->text, sizeof n->text)) {
            continue;
        }
        n->alpha = eng_dmgnum_alpha(d);
        n->to_player = false;
        hud->number_count++;
    }
    if (pf->vitals.dead) {
        (void)snprintf(hud->message, sizeof hud->message, "%s",
                       "Tu es tombé... renaissance dans un instant");
    } else if (c->levelup_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "Niveau %u !", c->progress.level);
    } else if (c->weapon_ticks > 200U) {
        (void)snprintf(hud->message, sizeof hud->message, "%s",
                       "Espadon obtenu ! (R : changer d'arme)");
    } else if (c->weapon_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "Arme : %s",
                       eng_weapon_def(c->equip.equipped)->label);
    } else if (eng_camp_cleared(&c->group, &c->sim)) {
        (void)snprintf(hud->message, sizeof hud->message, "%s", "Camp vaincu !");
    } else {
        hud->message[0] = '\0';
    }
}

bool play_combat_equip(PlayCombat *c, uint32_t weapon, bool give)
{
    if (c == NULL || !c->ready) {
        return false;
    }
    if (give) {
        (void)eng_equip_give(&c->equip, weapon);
    }
    if (!eng_equip_select(&c->equip, weapon)) {
        return false;
    }
    apply_weapon(c);
    return true;
}

void play_combat_set_night(PlayCombat *c, float night)
{
    (void)c;
    g_night = isfinite(night) ? night : 0.0F;
}

void play_combat_player_props(const PlayCombat *c, AvatarProp *right, AvatarProp *left)
{
    const bool great = c != NULL && c->ready && c->equip.equipped == ENG_WEAPON_GREATSWORD;
    *right = great ? AVATAR_GREATSWORD : AVATAR_SWORD;
    *left = great ? AVATAR_PROP_NONE : AVATAR_SHIELD;
}

uint32_t play_combat_weapon(const PlayCombat *c, uint32_t *owned)
{
    if (owned != NULL) {
        *owned = c != NULL ? c->equip.owned : 0U;
    }
    return c != NULL ? c->equip.equipped : 0U;
}

void play_combat_stats(const PlayCombat *c, PlayCombatStats *out)
{
    if (c != NULL && out != NULL) {
        *out = c->stats;
    }
}

void play_combat_autopilot(PlayCombat *c, const float feet[3], float camera_yaw, uint32_t tick,
                           EngInputState *in)
{
    if (c == NULL || !c->ready || feet == NULL || in == NULL) {
        return;
    }
    memset(in, 0, sizeof *in);
    if (c->lock < 0 || c->sim.fighters[c->lock].vitals.dead || c->lock == (int32_t)DUMMY) {
        float best = 1e9F;
        c->lock = -1;
        for (uint32_t k = FIRST_ENEMY; k < FIGHTERS; k++) {
            const float d =
                hypotf(c->sim.bodies[k].pos[0] - feet[0], c->sim.bodies[k].pos[1] - feet[2]);
            if (!c->sim.fighters[k].vitals.dead && d < best) {
                best = d;
                c->lock = (int32_t)k;
            }
        }
    }
    if (c->lock < 0) {
        return;
    }
    const float dx = c->sim.bodies[c->lock].pos[0] - feet[0];
    const float dz = c->sim.bodies[c->lock].pos[1] - feet[2];
    const float d = hypotf(dx, dz);
    if (d > 1.4F) {
        const float fx = -sinf(camera_yaw);
        const float fz = -cosf(camera_yaw);
        const float rx = cosf(camera_yaw);
        const float rz = -sinf(camera_yaw);
        in->axis[ENG_AXIS_MOVE] = (EngAxisValue){(dx * rx + dz * rz) / d, (dx * fx + dz * fz) / d};
    } else if ((tick % 24U) == 0U) {
        in->pressed |= 1U << ENG_ACTION_ATTACK;
    }
    if (c->sim.fighters[PLAYER].stamina < 20.0F && (tick % 90U) == 0U) {
        in->pressed |= 1U << ENG_ACTION_DODGE; /* recule quand il est à bout de souffle */
    }
    in->held |= in->pressed;
}

/* ── Autotest ── */

typedef struct CombatTest {
    PlayMode play;
    bool ok;
    int checks;
} CombatTest;

static void check(CombatTest *t, bool cond, const char *what, float value)
{
    t->checks++;
    if (cond) {
        ENG_LOG_INFO("  ok     %-58s %8.3f", what, (double)value);
    } else {
        ENG_LOG_ERROR("  ÉCHEC  %-58s %8.3f", what, (double)value);
        t->ok = false;
    }
}

static void step(CombatTest *t, uint32_t pressed, uint32_t held, float mx, float my)
{
    EngInputState in;
    memset(&in, 0, sizeof in);
    in.pressed = pressed;
    in.held = held | pressed;
    in.axis[ENG_AXIS_MOVE] = (EngAxisValue){mx, my};
    play_mode_tick(&t->play, &in, 1.0F / 60.0F);
}

static void put(CombatTest *t, float x, float z, float yaw)
{
    const float feet[3] = {x, PLAY_GROUND_TOP + 0.05F, z};
    (void)eng_character_teleport(t->play.character, feet);
    const float pivot[3] = {x, feet[1] + PLAY_PIVOT_HEIGHT, z};
    (void)eng_orbit_camera_init(&t->play.orbit, pivot, yaw, -0.25F, ENG_ORBIT_DISTANCE_DEFAULT);
    (void)eng_locomotion_init(&t->play.loco, yaw);
    for (int i = 0; i < 30; i++) {
        step(t, 0, 0, 0.0F, 0.0F);
    }
}

bool play_combat_terrain_selftest(const char *heightmap_path)
{
    static CombatTest t;
    memset(&t, 0, sizeof t);
    t.ok = true;
    if (heightmap_path == NULL ||
        !play_mode_init_terrain(&t.play, NULL, 1.0F, false, heightmap_path) ||
        !play_mode_enable_combat(&t.play, false)) {
        ENG_LOG_ERROR("AUTOTEST CAMP TERRAIN : ÉCHEC (initialisation ; Jolt et le terrain requis)");
        play_mode_shutdown(&t.play);
        return false;
    }
    PlayCombat *c = t.play.combat;
    c->sim.fighters[PLAYER].vitals.max_hp = 5000.0F;
    c->sim.fighters[PLAYER].vitals.hp = 5000.0F;
    uint32_t steep_cells = 0;
    for (uint32_t i = 0; i < c->nav.w * c->nav.h; i++) {
        steep_cells += c->nav.blocked[i] != 0U ? 1U : 0U;
    }
    /* joueur posé sur le relief à environ 6 m du camp, en garde */
    float spot[2] = {-9.0F, -5.0F}; /* sur le praticable le plus proche : jamais dans une pente */
    (void)eng_nav_nearest_walkable(&c->nav, spot, spot);
    float feet[3] = {spot[0], 0.0F, spot[1]};
    feet[1] = play_mode_ground(&t.play, feet[0], feet[2]) + 0.05F;
    play_mode_teleport(&t.play, feet, 2.3561945F - 3.14159265F);
    const uint32_t block = 1U << ENG_ACTION_INTERACT;
    uint32_t in_steep = 0;
    bool was_walkable[FIGHTERS] = {false};
    uint32_t start_steep = 0;
    for (uint32_t k = FIRST_ENEMY; k < FIGHTERS; k++) {
        start_steep += eng_nav_walkable(&c->nav, c->sim.bodies[k].pos) ? 0U : 1U;
    }
    uint32_t closest_tick = 0;
    float closest = 1e9F;
    for (int i = 0; i < 1800; i++) {
        step(&t, i == 0 ? block : 0U, block, 0.0F, 0.0F);
        for (uint32_t k = FIRST_ENEMY; k < FIGHTERS; k++) {
            if (c->sim.fighters[k].vitals.dead) {
                continue;
            }
            const float *p = c->sim.bodies[k].pos;
            if (eng_nav_walkable(&c->nav, p)) {
                was_walkable[k] = true;
            } else if (was_walkable[k]) { /* revenu sur une pente après l'avoir quittée */
                in_steep++;
            }
            const float d =
                hypotf(p[0] - c->sim.bodies[PLAYER].pos[0], p[1] - c->sim.bodies[PLAYER].pos[1]);
            if (d < closest) {
                closest = d;
                closest_tick = (uint32_t)i;
            }
        }
    }
    PlayCombatStats s;
    play_combat_stats(c, &s);
    float route[64][2];
    const float *pp = c->sim.bodies[PLAYER].pos;
    const uint32_t route_n = eng_nav_path(&c->nav, c->camp.m[0].pos, pp, route, 64);
    const bool reachable = route_n > 0U && hypotf(route[route_n - 1U][0] - pp[0],
                                                  route[route_n - 1U][1] - pp[1]) < 0.75F;
    ENG_LOG_INFO("autotest : joueur en (%.1f, %.1f) sur une cellule %s ; chemin depuis le camp : "
                 "%u point(s), %s",
                 (double)pp[0], (double)pp[1],
                 eng_nav_walkable(&c->nav, pp) ? "praticable" : "BLOQUÉE", route_n,
                 reachable ? "jusqu'au joueur" : "s'arrête avant le joueur");
    ENG_LOG_INFO("autotest : %u cellule(s) raide(s) dans la grille, %u ennemi(s) placé(s) sur une "
                 "pente au départ, ennemi le plus proche à %.2f m (tick %u)",
                 steep_cells, start_steep, (double)closest, closest_tick);
    check(&t, s.enemies_engaged >= 1U, "camp sur relief : ennemis engagés",
          (float)s.enemies_engaged);
    check(&t, closest < 2.5F, "camp sur relief : un ennemi atteint le joueur (m)", closest);
    check(&t, in_steep == 0U, "camp sur relief : ticks revenus sur une pente quittée",
          (float)in_steep);
    check(&t, start_steep == 0U, "camp sur relief : ennemis placés sur une pente au départ",
          (float)start_steep);
    check(&t, s.max_attackers <= 2U, "camp sur relief : attaquants simultanés (<= 2)",
          (float)s.max_attackers);
    check(&t, s.player_hits_taken >= 1U, "camp sur relief : coups reçus par le joueur",
          (float)s.player_hits_taken);
    ENG_LOG_INFO("AUTOTEST CAMP TERRAIN : %s (%d mesures)", t.ok ? "OK" : "ÉCHEC", t.checks);
    play_mode_shutdown(&t.play);
    return t.ok;
}

bool play_combat_selftest(bool rig)
{
    static CombatTest t;
    static AvatarLibrary lib;
    memset(&t, 0, sizeof t);
    t.ok = true;
    if (!play_mode_init(&t.play, NULL, 1.0F, false) || !play_mode_enable_combat(&t.play, false)) {
        ENG_LOG_ERROR("AUTOTEST COMBAT : ÉCHEC (initialisation ; Jolt requis)");
        play_mode_shutdown(&t.play);
        return false;
    }
    PlayCombat *c = t.play.combat;
    if (rig) { /* M4 / M5-05 / M5-06 : personnages KayKit (partie CPU), hurtboxes et lame animées */
        if (!avatar_library_load_cpu(&lib) || !play_mode_attach_avatars(&t.play, &lib) ||
            c->hurt.count == 0U) {
            ENG_LOG_ERROR("AUTOTEST COMBAT : ÉCHEC (personnages cuits introuvables)");
            play_mode_shutdown(&t.play);
            avatar_library_shutdown(&lib, NULL);
            return false;
        }
        ENG_LOG_INFO("autotest : rig animé, %u hurtboxes par combattant, lame en main",
                     c->hurt.count);
    }
    c->sim.fighters[PLAYER].vitals.max_hp = 2000.0F;
    c->sim.fighters[PLAYER].vitals.hp = 2000.0F;
    const uint32_t atk = 1U << ENG_ACTION_ATTACK;
    PlayCombatStats s;

    /* 1. mannequin : 1,3 m devant, attaques répétées */
    put(&t, 4.0F, 0.3F, 0.0F);
    for (int i = 0; i < 300; i++) {
        step(&t, (i % 30) == 0 ? atk : 0U, 0U, 0.0F, 0.0F);
    }
    play_combat_stats(c, &s);
    check(&t, s.dummy_damage >= 50.0F, "mannequin : dégâts reçus (10 coups légers)",
          s.dummy_damage);
    check(&t, c->dummy.hits_taken >= 5U, "mannequin : coups comptés", (float)c->dummy.hits_taken);

    /* 1 bis. équipement (M8-08) : l'espadon donné, R le met en main ; ses coups touchent ; R remet
     * l'épée */
    (void)eng_equip_give(&c->equip, ENG_WEAPON_GREATSWORD);
    for (int i = 0; i < 60; i++) { /* fin de l'attaque en cours */
        step(&t, 0U, 0U, 0.0F, 0.0F);
    }
    step(&t, 1U << ENG_ACTION_SWITCH_WEAPON, 0U, 0.0F, 0.0F);
    check(&t, c->equip.equipped == ENG_WEAPON_GREATSWORD, "équipement : R met l'espadon en main",
          (float)c->equip.equipped);
    check(&t, strcmp(c->sim.fighters[PLAYER].params.light_name, "gs_light1") == 0,
          "équipement : attaques de l'espadon", 0.0F);
    const uint32_t hits_before = c->dummy.hits_taken;
    for (int i = 0; i < 240; i++) {
        step(&t, (i % 60) == 0 ? atk : 0U, 0U, 0.0F, 0.0F);
    }
    check(&t, c->dummy.hits_taken > hits_before, "équipement : l'espadon touche le mannequin",
          (float)(c->dummy.hits_taken - hits_before));
    step(&t, 1U << ENG_ACTION_SWITCH_WEAPON, 0U, 0.0F, 0.0F);
    check(&t, c->equip.equipped == ENG_WEAPON_SWORD, "équipement : R remet l'épée",
          (float)c->equip.equipped);

    /* 2. verrouillage sur le mannequin (caméra vers -Z) */
    step(&t, 1U << ENG_ACTION_LOCK_ON, 0U, 0.0F, 0.0F);
    play_combat_stats(c, &s);
    check(&t, s.lock_target == (int32_t)DUMMY, "verrouillage : cible = mannequin",
          (float)s.lock_target);
    step(&t, 1U << ENG_ACTION_LOCK_ON, 0U, 0.0F, 0.0F);
    play_combat_stats(c, &s);
    check(&t, s.lock_target == -1, "verrouillage : relâché", (float)s.lock_target);

    /* 3. esquive : sans direction = pas en arrière, déplace le personnage */
    put(&t, 4.0F, 3.0F, 0.0F);
    for (int i = 0; i < 240; i++) { /* l'endurance revient (1 s d'attente puis 25/s) */
        step(&t, 0U, 0U, 0.0F, 0.0F);
    }
    float before[3];
    (void)eng_character_position(t.play.character, before);
    step(&t, 1U << ENG_ACTION_DODGE, 0U, 0.0F, 0.0F);
    bool dodging = false;
    for (int i = 0; i < 30; i++) {
        dodging = dodging || c->sim.fighters[PLAYER].fsm.state == ENG_CS_DODGE;
        step(&t, 0U, 0U, 0.0F, 0.0F);
    }
    float after[3];
    (void)eng_character_position(t.play.character, after);
    const float moved = hypotf(after[0] - before[0], after[2] - before[2]);
    check(&t, dodging, "esquive : état Dodge atteint", dodging ? 1.0F : 0.0F);
    check(&t, moved > 1.5F && after[2] > before[2], "esquive : recul du personnage (m)", moved);

    /* 4. camp : le joueur s'approche et garde ; les ennemis viennent, 2 attaquants au plus */
    put(&t, -9.0F, -5.0F, 2.3561945F - 3.14159265F);
    const uint32_t block = 1U << ENG_ACTION_INTERACT;
    step(&t, block, 0U, 0.0F, 0.0F);
    for (int i = 0; i < 1500; i++) {
        step(&t, 0U, block, 0.0F, 0.0F);
    }
    play_combat_stats(c, &s);
    check(&t, s.enemies_engaged >= 1U, "camp : ennemis engagés (maximum simultané)",
          (float)s.enemies_engaged);
    check(&t, s.max_attackers <= 2U, "camp : attaquants simultanés (<= 2)", (float)s.max_attackers);
    check(&t, s.player_hits_taken >= 1U, "camp : coups reçus par le joueur",
          (float)s.player_hits_taken);

    /* 5. combat : verrouillage + attaques jusqu'à au moins une victoire */
    for (int i = 0; i < 6000 && s.enemy_kills == 0U; i++) {
        uint32_t pressed = 0U;
        if (c->lock < 0 || c->sim.fighters[c->lock].vitals.dead || c->lock == (int32_t)DUMMY) {
            float best = 1e9F; /* l'automate vise l'ennemi vivant le plus proche */
            c->lock = -1;
            for (uint32_t k = FIRST_ENEMY; k < FIGHTERS; k++) {
                const float d = hypotf(c->sim.bodies[k].pos[0] - c->sim.bodies[PLAYER].pos[0],
                                       c->sim.bodies[k].pos[1] - c->sim.bodies[PLAYER].pos[1]);
                if (!c->sim.fighters[k].vitals.dead && d < best) {
                    best = d;
                    c->lock = (int32_t)k;
                }
            }
        }
        float mx = 0.0F;
        float my = 0.0F;
        if (c->lock >= 0) {
            const float dx = c->sim.bodies[c->lock].pos[0] - c->sim.bodies[PLAYER].pos[0];
            const float dz = c->sim.bodies[c->lock].pos[1] - c->sim.bodies[PLAYER].pos[1];
            const float d = hypotf(dx, dz);
            if (d > 1.4F) { /* avance vers la cible (axe de déplacement relatif à la caméra) */
                const float cy = t.play.orbit.yaw;
                const float fx = -sinf(cy);
                const float fz = -cosf(cy);
                const float rx = cosf(cy);
                const float rz = -sinf(cy);
                my = (dx * fx + dz * fz) / d;
                mx = (dx * rx + dz * rz) / d;
            } else if ((i % 24) == 0) {
                pressed |= atk;
            }
        }
        step(&t, pressed, 0U, mx, my);
        play_combat_stats(c, &s);
    }
    check(&t, s.enemy_kills >= 1U, "combat : ennemis vaincus", (float)s.enemy_kills);
    check(&t, s.total_xp >= 10U * s.enemy_kills, "progression : XP gagnée sur les ennemis",
          (float)s.total_xp);
    check(&t, s.max_attackers <= 2U, "combat : attaquants simultanés (<= 2)",
          (float)s.max_attackers);
    check(&t, s.attacks_version >= 1U || !c->file_ok, "données : version des attaques chargée",
          (float)s.attacks_version);
    if (rig) {
        check(&t, c->sim.rig[PLAYER].count == c->hurt.count && c->sim.rig[FIRST_ENEMY].count > 0U,
              "rig : hurtboxes sur les os données à la simulation",
              (float)c->sim.rig[FIRST_ENEMY].count);
    }
    ENG_LOG_INFO("AUTOTEST COMBAT : %s (%d mesures, %u ticks%s)", t.ok ? "OK" : "ÉCHEC", t.checks,
                 s.tick, rig ? ", rig animé" : "");
    play_mode_shutdown(&t.play);
    if (rig) {
        avatar_library_shutdown(&lib, NULL);
    }
    return t.ok;
}
