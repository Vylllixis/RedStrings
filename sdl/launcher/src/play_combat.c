#include "play_combat.h"

#include "engine/gear.h"
#include "play_boss.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/ambience.h"
#include "engine/equipment.h"
#include "engine/ai_avoid.h"
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
/* Combattants : joueur, mannequin, puis 5 ennemis (une meute de la grande carte en compte cinq ;
 * un camp, trois : les autres sont rangés hors de la scène). Avec l'arène murée du boss (terrain
 * seulement) : joueur, mannequin, 3 ennemis du camp, boss. */
#define FIGHTERS_MAX 7U
#define ARENA_FIGHTERS 6U
#define BOSS 5U      /* M6-09 : indice du boss dans la simulation (s'il y en a un) */
#define BOSS_X 16.0F /* centre de l'arène murée du boss */
#define BOSS_Z (-16.0F)
#define BOSS_HALF 7.0F /* demi-côté de l'aire (m) */
#define BOSS_GATE 4.0F /* largeur de l'entrée, au nord (+Z, côté du départ) */
#define BOSS_WALLS 5U
#define BOSS_NAME "Seigneur des os"
#define RESPAWN_TICKS 180U
#define IMP_SCALE 0.62F /* un osselet (meute) : 1,1 m de haut */
#define CAMP_RESPAWN_TICKS 600U
#define PACK_RESPAWN_TICKS 360U /* une meute de petits monstres revient 6 s après sa défaite */
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
    uint32_t zone_ticks; /* nom de la zone encore affiché */
    EngMixer mixer;
    EngCombatAudio audio;
    EngAudioDevice *device;
    int32_t lock;        /* indice de combattant verrouillé, -1 */
    uint32_t boss_agent; /* M6-09 : indice du boss dans le groupe d'IA */
    /* grande carte : zone chargée (play_combat_load_zone) ; PV, dégâts et taille de ses ennemis */
    uint32_t gear_tier;    /* butin des boss : palier d'équipement 0 à 4 (engine/gear.h) */
    uint32_t zones_beaten; /* bit i : boss de la zone i vaincu (ne revient pas) */
    uint32_t gear_ticks;   /* message « Butin : ... » encore affiché */
    int32_t zone_index;    /* zone chargée, -1 : aucune */
    char zone_title[32];   /* zone de boss : nom du boss */
    float zone_hp;
    float zone_damage;
    float zone_size;
    uint32_t zone_reward;
    uint32_t flight_msg_ticks; /* message « le dragon prend son vol / se pose » encore affiché */
    uint32_t respawns;         /* retours d'un camp ou d'une meute vaincus, depuis le lancement */
    char zone_name[64];
    float (*relief_fn)(void *user, float x, float z); /* gardé pour recharger une zone */
    void *relief_user;
    float relief_slope;
    uint32_t dead_ticks;
    uint32_t cleared_ticks;
    bool ready;
    bool file_ok;
    bool has_boss;       /* M6-09 : arène et boss (terrain seulement) */
    bool boss_defeated;  /* tué : ne renaît plus (déduit de la sauvegarde au chargement) */
    bool boss_reachable; /* l'entrée mène au boss sur ce relief (play_combat_set_relief) */
    bool invincible; /* mode créatif : le joueur ne perd pas de PV (ses dégâts sont inchangés) */
    bool zone_boss; /* grande carte : la zone chargée est une zone de boss (pas de réapparition) */
    bool roam;      /* grande carte : sans cible, le pilote automatique suit le chemin (+X) */
    bool zone_reward_pending; /* le boss de la zone vient d'être vaincu : butin à donner */
    bool zone_pack;           /* la zone chargée est une meute de petits monstres (revient vite) */
    bool flight_msg_landing;  /* message du dragon : il se pose (sinon : il décolle) */
    bool hidden[FIGHTERS_MAX]; /* combattant sans rôle dans la zone : ni dessiné, ni ciblé */
    bool was_dead[FIGHTERS_MAX];
    bool has_avatar[FIGHTERS_MAX];
    bool rig_ready[FIGHTERS_MAX]; /* M5-05 / M5-06 : pose au rythme des ticks (ci-dessous) */
    EngProgress progress;         /* M8-08 */
    uint32_t levelup_ticks;       /* message « Niveau N » encore affiché */
    EngEquipment equip;           /* M8-08 : armes possédées, arme en main */
    uint32_t weapon_ticks;    /* message « Espadon obtenu » / « Arme : ... » encore affiché */
    uint32_t weapon_switches; /* changements d'arme depuis le début (journal, autotest) */
    PlayCombatStats stats;
    AvatarLibrary *avatars;      /* M4 : NULL tant que les personnages ne sont pas branchés */
    Avatar avatar[FIGHTERS_MAX]; /* le joueur (0) est dans PlayMode */
    /* M5-05 / M5-06 : pose au rythme des ticks pour les hurtboxes et la lame */
    EngHurtboxSet hurt;
    uint32_t boss_ticks; /* message « Seigneur des os vaincu » encore affiché */
    EngActorAnim rig_anim[FIGHTERS_MAX];
    mat4 rig_world[FIGHTERS_MAX][AVATAR_MAX_JOINTS];
    AvatarProp rig_prop[FIGHTERS_MAX];
    float rig_scale[FIGHTERS_MAX];
    EngPose reach_pose;                 /* mesure des portées (play_combat_attach_avatars) */
    EngWeaponTrail trail[FIGHTERS_MAX]; /* M5-20 : traînée de la lame pendant une attaque */
    int32_t ai_selected;                /* M6-10 : agent choisi au clic, -1 */
    /* grande carte : boss spécial de la zone (cavalier, dragon) et ses boîtes à dessiner */
    uint32_t lava_ticks;     /* ticks passés dans la lave (une brûlure par PLAY_LAVA_PERIOD) */
    uint32_t lava_msg_ticks; /* message « La lave brûle » encore affiché */
    uint32_t lava_burns;     /* brûlures de lave depuis le début (journal, autotest) */
    bool in_lava;            /* le joueur a les pieds dans la lave (play_combat_set_lava) */
    /* carte du monde : troncs et rochers que les ennemis évitent (play_combat_set_trees) */
    EngAvoidCircle trees[PLAY_AVOID_MAX];
    uint32_t tree_count;
    EngAvoidState avoid[FIGHTERS_MAX];
    uint32_t avoid_steers; /* directions corrigées depuis le début (autotest) */
    EngBossKind zone_boss_kind;
    uint32_t creature_count;
    uint32_t cinematic_ticks; /* message de la cinématique encore affiché */
    uint8_t creature_color[ENG_CREATURE_MAX_PARTS];
    PlayBoss boss;
    mat4 creature_place[ENG_CREATURE_MAX_PARTS];
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
    /* butin des boss : l'arme multiplie les dégâts, l'armure réduit ceux reçus, l'écu la garde */
    pf->params.damage_scale = st.damage_scale * eng_gear_damage(c->gear_tier);
    pf->params.armor_scale = eng_gear_armor(c->gear_tier);
    pf->params.block_multiplier = eng_gear_block(c->gear_tier);
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

/* M6-09 : murs de l'arène du boss, en boîtes de l'IA (centre x, z, demi-côtés x, z) : 1 m
 * d'épaisseur hors de l'aire, une entrée de BOSS_GATE m au nord (+Z, côté du départ). */
static uint32_t boss_wall_boxes(float out[BOSS_WALLS][4])
{
    const float h = BOSS_HALF;
    const float t = 0.5F;                                      /* demi-épaisseur */
    const float side = 0.5F * (h + 1.0F - (0.5F * BOSS_GATE)); /* demi-longueur d'un pan nord */
    const float boxes[BOSS_WALLS][4] = {
        {BOSS_X, BOSS_Z - h - t, h + 1.0F, t},               /* sud */
        {BOSS_X - h - t, BOSS_Z, t, h},                      /* ouest */
        {BOSS_X + h + t, BOSS_Z, t, h},                      /* est */
        {BOSS_X - h - 1.0F + side, BOSS_Z + h + t, side, t}, /* nord, à gauche de l'entrée */
        {BOSS_X + (0.5F * BOSS_GATE) + side, BOSS_Z + h + t, side, t}, /* nord, à droite */
    };
    memcpy(out, boxes, sizeof boxes);
    return BOSS_WALLS;
}

uint32_t play_combat_boss_walls(const PlayCombat *c, float out[][4], uint32_t max)
{
    if (c == NULL || !c->ready || !c->has_boss || out == NULL || max < BOSS_WALLS) {
        return 0;
    }
    float boxes[BOSS_WALLS][4];
    const uint32_t n = boss_wall_boxes(boxes);
    memcpy(out, boxes, sizeof boxes);
    return n;
}

bool play_combat_boss_gate(const PlayCombat *c, float out[3])
{
    if (c == NULL || !c->ready || !c->has_boss || out == NULL) {
        return false;
    }
    out[0] = BOSS_X;
    out[1] = 0.0F;
    out[2] = BOSS_Z + BOSS_HALF + 4.0F;
    return true;
}

/* Nom affiché d'un archétype (HUD) ; le nom de fichier sinon. */
static const char *display_name(const char *arch)
{
    static const char *const names[][2] = {{"grunt", "Soldat squelette"},
                                           {"brute", "Brute squelette"},
                                           {"archer", "Archer squelette"},
                                           {"imp", "Osselet"},
                                           {"boss", BOSS_NAME}};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (strcmp(arch, names[i][0]) == 0) {
            return names[i][1];
        }
    }
    return arch;
}

/* Archétype du combattant `fighter` (groupe d'IA), UINT32_MAX s'il n'y est pas. */
static uint32_t fighter_arch(const PlayCombat *c, uint32_t fighter)
{
    for (uint32_t i = 0; i < c->group.count; i++) {
        if (c->group.agents[i].fighter == fighter) {
            return c->group.agents[i].arch;
        }
    }
    return UINT32_MAX;
}

/* Combattant sans rôle (place libre d'un camp de moins de cinq ennemis) : mort, caché, loin de la
 * scène ; jamais compté comme une victoire. */
static void park_fighter(PlayCombat *c, uint32_t f, float x, float z)
{
    c->hidden[f] = true;
    c->was_dead[f] = true;
    c->sim.fighters[f].vitals.hp = 0.0F;
    c->sim.fighters[f].vitals.dead = true;
    c->sim.fighters[f].fsm.state = ENG_CS_DEAD;
    (void)eng_sim_place(&c->sim, f, x, z - 4000.0F, 0.0F);
}

/* M6-09 : le boss à neuf, chez lui, tourné vers l'entrée. */
static void spawn_boss(PlayCombat *c)
{
    if (!c->has_boss) {
        return;
    }
    (void)eng_aig_reset_agent(&c->group, &c->sim, c->boss_agent);
    const EngAiAgent *a = &c->group.agents[c->boss_agent];
    (void)eng_sim_place(&c->sim, BOSS, a->home[0], a->home[1], 3.14159265F);
    c->was_dead[BOSS] = false;
}

static void spawn_all(PlayCombat *c)
{
    for (uint32_t i = 0; i < c->camp.count && i < c->group.count; i++) {
        (void)eng_aig_reset_agent(&c->group, &c->sim, i);
        const uint32_t f = c->group.agents[i].fighter;
        (void)eng_sim_place(&c->sim, f, c->camp.m[i].pos[0], c->camp.m[i].pos[1], c->camp.m[i].yaw);
        if (c->zone_hp > 0.0F) { /* grande carte : PV, dégâts et carrure de la zone */
            c->sim.fighters[f].vitals.max_hp = c->zone_hp;
            c->sim.fighters[f].vitals.hp = c->zone_hp;
            c->sim.fighters[f].params.damage_scale = c->zone_damage;
            c->sim.radius[f] *= c->zone_size > 0.0F ? c->zone_size : 1.0F;
        }
    }
    play_boss_clear(&c->boss);
    c->cinematic_ticks = 0;
    c->sim.world_ignore = 1U << PLAYER; /* le dragon d'une zone précédente enjambait le décor */
    if (c->zone_boss && c->zone_boss_kind != ENG_BOSSKIND_NONE && c->group.count > 0U) {
        play_boss_begin(&c->boss, c->zone_boss_kind, c->group.agents[0].fighter, &c->sim,
                        c->zone_damage);
    }
}

void play_combat_shift(PlayCombat *c, float dx, float dz)
{
    if (c == NULL || !c->ready || !isfinite(dx) || !isfinite(dz)) {
        return;
    }
    const float d[2] = {dx, dz};
    for (int a = 0; a < 2; a++) {
        for (uint32_t i = 0; i < c->sim.count; i++) {
            c->sim.bodies[i].pos[a] += d[a];
            c->sim.bodies[i].prev_pos[a] += d[a];
        }
        for (uint32_t i = 0; i < c->group.count; i++) {
            EngAiAgent *ag = &c->group.agents[i];
            ag->home[a] += d[a];
            ag->goal[a] += d[a];
            ag->ring[a] += d[a];
            ag->perc.last_known[a] += d[a];
            for (uint32_t k = 0; k < ENG_AI_PATROL_MAX; k++) {
                ag->patrol[k][a] += d[a];
            }
            for (uint32_t k = 0; k < ENG_AI_PATH_MAX; k++) {
                ag->path[k][a] += d[a];
            }
        }
        for (uint32_t i = 0; i < c->camp.count; i++) {
            c->camp.m[i].pos[a] += d[a];
            for (uint32_t k = 0; k < ENG_AI_PATROL_MAX; k++) {
                c->camp.m[i].patrol[k][a] += d[a];
            }
        }
        for (uint32_t i = 0; i < c->world.count; i++) {
            c->world.obs[i].c[a] += d[a];
        }
        for (uint32_t i = 0; i < FIGHTERS_MAX; i++) {
            c->avoid[i].from[a] += d[a];
        }
        c->nav.origin[a] += d[a];
    }
    for (uint32_t i = 0; i < c->tree_count; i++) {
        c->trees[i].x += dx;
        c->trees[i].z += dz;
    }
    play_boss_shift(&c->boss, dx, dz);
    for (uint32_t i = 0; i < FIGHTERS_MAX; i++) {
        (void)eng_trail_init(&c->trail[i], 10U); /* pas de traînée à travers le décalage */
    }
    eng_particles_init(&c->particles, 12345U);
    eng_dmgnum_init(&c->numbers);
}

static void enemy_look(const PlayCombat *c, uint32_t fighter, AvatarBody *body, AvatarProp *right,
                       AvatarProp *left, float *scale);
static void fit_plans_to_reach(PlayCombat *c, const AvatarLibrary *lib, uint32_t arch,
                               AvatarProp prop, float scale);

bool play_combat_load_zone(PlayCombat *c, const EngMapZone *zone, int32_t index, float cx, float cz)
{
    if (c == NULL || !c->ready || c->has_boss || !isfinite(cx) || !isfinite(cz)) {
        return false;
    }
    const uint32_t slots = c->sim.count - FIRST_ENEMY;
    const bool beaten = zone != NULL && zone->kind == ENG_MAPZONE_BOSS &&
                        play_combat_zone_beaten(c, index); /* un boss vaincu ne revient pas */
    const uint32_t count =
        (zone == NULL || beaten) ? 0U : (zone->enemy_count < slots ? zone->enemy_count : slots);
    c->zone_index = zone != NULL ? index : -1;
    (void)snprintf(c->zone_title, sizeof c->zone_title, "%s",
                   zone != NULL && zone->boss_title != NULL ? zone->boss_title : "");
    eng_aig_init(&c->group, &c->arch, &c->world, &c->nav, PLAYER);
    eng_camp_init(&c->camp);
    memset(c->avoid, 0,
           sizeof c->avoid); /* aucun ennemi ne fuit plus un arbre de l'ancienne zone */
    c->zone_hp = zone != NULL ? zone->hp : 0.0F;
    c->zone_damage = zone != NULL ? zone->damage_scale : 1.0F;
    c->zone_size = zone != NULL ? zone->size_scale : 1.0F;
    c->zone_boss = zone != NULL && zone->kind == ENG_MAPZONE_BOSS && count > 0U;
    c->zone_pack = zone != NULL && zone->kind == ENG_MAPZONE_PACK;
    c->zone_reward = zone != NULL ? zone->reward : 0U;
    c->zone_boss_kind = c->zone_boss ? zone->boss_kind : ENG_BOSSKIND_NONE;
    play_boss_clear(&c->boss);
    c->creature_count = 0;
    c->zone_ticks = zone != NULL ? 240U : 0U;
    c->roam = true;
    (void)snprintf(c->zone_name, sizeof c->zone_name, "%s", zone != NULL ? zone->name : "");
    const float face = 1.5707963F; /* tournés vers -X : d'où vient le joueur */
    bool ok = true;
    for (uint32_t i = 0; i < count && ok; i++) {
        uint32_t arch = eng_arch_find(&c->arch, zone->arch[i]);
        if (arch == UINT32_MAX && c->zone_boss) { /* données anciennes : boss ordinaire */
            arch = eng_arch_find(&c->arch, "boss");
            c->zone_boss_kind = ENG_BOSSKIND_NONE;
        }
        const float angle = 6.2831853F * (float)i / (float)count;
        /* une meute est plus serrée, et ses membres ne sont pas tous à la même distance */
        const float spread = c->zone_pack ? 3.0F + (1.5F * (float)(i % 2U)) : 5.0F;
        const float ring = count > 1U ? spread : 0.0F;
        const float px = cx + (ring * cosf(angle));
        const float pz = cz + (ring * sinf(angle));
        const float patrol[2][2] = {{px, pz}, {px, pz + 8.0F}};
        const bool walks = i == 0U && count > 1U; /* le premier fait sa ronde */
        ok = arch != UINT32_MAX &&
             eng_camp_add(&c->camp, arch, px, pz, face, walks ? patrol : NULL, walks ? 2U : 0U);
    }
    ok = ok && (count == 0U || eng_camp_spawn(&c->camp, &c->group, &c->sim, FIRST_ENEMY));
    for (uint32_t f = FIRST_ENEMY; f < c->sim.count; f++) {
        const bool used = ok && f - FIRST_ENEMY < c->camp.count;
        c->hidden[f] = false;
        c->was_dead[f] = false;
        if (!used) { /* sans rôle dans cette zone : mort, hors de la scène */
            park_fighter(c, f, cx, cz);
        }
    }
    const float mn[2] = {cx - 30.0F, cz - 30.0F};
    const float mx[2] = {cx + 30.0F, cz + 30.0F};
    ok = ok && eng_nav_build(&c->nav, &c->world, mn, mx, 0.5F, 0.55F);
    if (ok && c->relief_fn != NULL) {
        (void)play_combat_set_relief(c, c->relief_fn, c->relief_user, c->relief_slope);
    } else if (ok) {
        spawn_all(c);
    }
    for (uint32_t f = FIRST_ENEMY; ok && c->avatars != NULL && f < c->sim.count; f++) {
        if (c->hidden[f] || !c->has_avatar[f]) {
            continue;
        }
        float scale = 1.0F;
        enemy_look(c, f, &c->avatar[f].body, &c->avatar[f].right, &c->avatar[f].left, &scale);
        c->avatar[f].scale = scale;
        c->rig_prop[f] = c->avatar[f].right;
        c->rig_scale[f] = scale;
        if (c->zone_boss_kind != ENG_BOSSKIND_DRAGON) { /* le dragon frappe avec son corps */
            fit_plans_to_reach(c, c->avatars, fighter_arch(c, f), c->avatar[f].right, scale);
        }
    }
    c->lock = -1;
    c->cleared_ticks = 0;
    return ok;
}

void play_combat_set_obstacles(PlayCombat *c, float (*boxes)[4], uint32_t count)
{
    if (c == NULL || !c->ready) {
        return;
    }
    eng_aiw_init(&c->world);
    uint32_t refused = 0;
    for (uint32_t i = 0; boxes != NULL && i < count; i++) {
        refused += eng_aiw_add_box(&c->world, boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3])
                       ? 0U
                       : 1U;
    }
    if (refused > 0U) {
        ENG_LOG_WARN("combat : %u obstacle(s) du décor refusé(s) (au plus %u)", refused,
                     (unsigned)ENG_AI_MAX_OBSTACLES);
    }
}

bool play_combat_busy(const PlayCombat *c)
{
    if (c == NULL || !c->ready) {
        return false;
    }
    for (uint32_t i = 0; i < c->group.count; i++) {
        const EngAiAgent *a = &c->group.agents[i];
        if (!c->sim.fighters[a->fighter].vitals.dead &&
            (a->state == ENG_AI_CHASE || a->state == ENG_AI_ENGAGE)) {
            return true;
        }
    }
    return false;
}

bool play_combat_zone_beaten(const PlayCombat *c, int32_t index)
{
    return c != NULL && c->ready && index >= 0 && index < 32 &&
           (c->zones_beaten & (1U << (uint32_t)index)) != 0U;
}

uint32_t play_combat_gear(const PlayCombat *c)
{
    return c != NULL && c->ready ? c->gear_tier : 0U;
}

bool play_combat_give_gear(PlayCombat *c, uint32_t tier)
{
    if (c == NULL || !c->ready || !eng_gear_give(&c->gear_tier, tier)) {
        return false;
    }
    apply_progress(c, false);
    c->gear_ticks = 420U;
    return true;
}

void play_combat_set_invincible(PlayCombat *c, bool invincible)
{
    if (c != NULL && c->ready) {
        c->invincible = invincible;
    }
}

bool play_combat_camp_center(const PlayCombat *c, float out[2])
{
    if (c == NULL || !c->ready || c->camp.count == 0U || out == NULL) {
        return false;
    }
    out[0] = c->camp.m[0].pos[0];
    out[1] = c->camp.m[0].pos[1];
    return true;
}

PlayCombat *play_combat_create(bool audio, bool arena_obstacles, bool boss_arena)
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
    /* M6-09 : sur un terrain, l'arène murée du boss (s'il est décrit dans enemies.txt) */
    const uint32_t boss_arch =
        (arena_obstacles || !boss_arena) ? UINT32_MAX : eng_arch_find(&c->arch, "boss");
    if (boss_arch != UINT32_MAX) {
        float walls[BOSS_WALLS][4];
        const uint32_t n = boss_wall_boxes(walls);
        bool walls_ok = true;
        for (uint32_t i = 0; i < n; i++) {
            walls_ok =
                eng_aiw_add_box(&c->world, walls[i][0], walls[i][1], walls[i][2], walls[i][3]) &&
                walls_ok;
        }
        c->has_boss = walls_ok;
        if (!walls_ok) {
            ENG_LOG_WARN("combat : murs de l'arène refusés par l'IA, pas de boss");
        }
    }
    const float mn[2] = {-30.0F, -30.0F};
    const float mx[2] = {30.0F, 30.0F};
    if (!eng_nav_build(&c->nav, &c->world, mn, mx, 0.5F, 0.55F) ||
        !eng_sim_init(&c->sim, &c->attacks, c->has_boss ? ARENA_FIGHTERS : FIGHTERS_MAX)) {
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
    if (ok && c->has_boss) { /* M6-09 : le boss, au centre de son arène, tourné vers l'entrée */
        ok = eng_aig_add(&c->group, &c->sim, BOSS, boss_arch, BOSS_X, BOSS_Z, 3.14159265F, NULL, 0);
        c->boss_agent = c->group.count - 1U;
    }
    if (!ok) {
        ENG_LOG_ERROR(
            "combat : création du camp impossible (archétypes grunt/brute/archer requis)");
        play_combat_destroy(c);
        return NULL;
    }
    for (uint32_t f = FIRST_ENEMY + c->camp.count; f < c->sim.count; f++) {
        if (!(c->has_boss && f == BOSS)) { /* places libres : une meute en prend cinq */
            park_fighter(c, f, 0.0F, 0.0F);
        }
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
                 "camp de 3 ennemis vers (-14, -11)%s",
                 c->has_boss ? " ; boss dans l'arène murée vers (16, -16), entrée au nord" : "");
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

/* une brûlure (flammes du dragon, lave) : étincelles, nombre de dégâts, compteur de coups reçus */
static void burn_feedback(PlayCombat *c, float dealt, const float feet[3], uint32_t attacker)
{
    const EngSimEvent burn = {.tick = c->sim.tick,
                              .attacker = attacker,
                              .defender = PLAYER,
                              .kind = ENG_HITKIND_DAMAGE,
                              .zone = ENG_ZONE_TORSO,
                              .dealt = dealt,
                              .killed = c->sim.fighters[PLAYER].vitals.dead,
                              .point = {feet[0], feet[1] + 1.0F, feet[2]}};
    (void)eng_vfx_on_event(&c->particles, &burn);
    (void)eng_dmgnum_spawn(&c->numbers, &burn);
    c->stats.player_hits_taken++;
}

/* carte du monde : la lave brûle le joueur qui y marche */
static void lava_tick(PlayCombat *c, const float feet[3])
{
    EngFighter *pf = &c->sim.fighters[PLAYER];
    if (!c->in_lava || pf->vitals.dead) {
        c->lava_ticks = 0;
        return;
    }
    if (c->lava_ticks++ % PLAY_LAVA_PERIOD != 0U || c->invincible ||
        pf->fsm.state == ENG_CS_DODGE) {
        return; /* entre deux brûlures ; mode créatif ; en pleine esquive */
    }
    const float burn = fmaxf(4.0F, pf->vitals.max_hp * PLAY_LAVA_BURN);
    const EngDamageResult r = eng_damage_apply(&pf->vitals, burn, ENG_ZONE_TORSO, pf->tick);
    if (r.ignored) {
        return;
    }
    if (r.killed) {
        (void)eng_fsm_request(&pf->fsm, ENG_CS_DEAD);
        ENG_LOG_INFO("combat : le joueur est mort dans la lave");
    }
    if (c->lava_burns++ == 0U) {
        ENG_LOG_INFO("combat : la lave brûle le joueur (%.0f PV par brûlure)", (double)r.dealt);
    }
    c->lava_msg_ticks = 90U;
    burn_feedback(c, r.dealt, feet, PLAYER);
}

void play_combat_set_trees(PlayCombat *c, float (*circles)[3], uint32_t count)
{
    if (c == NULL || !c->ready) {
        return;
    }
    c->tree_count = 0;
    for (uint32_t i = 0; circles != NULL && i < count && c->tree_count < PLAY_AVOID_MAX; i++) {
        if (isfinite(circles[i][0]) && isfinite(circles[i][1]) && circles[i][2] > 0.0F) {
            c->trees[c->tree_count++] =
                (EngAvoidCircle){.x = circles[i][0], .z = circles[i][1], .r = circles[i][2]};
        }
    }
}

uint32_t play_combat_tree_count(const PlayCombat *c)
{
    return c != NULL && c->ready ? c->tree_count : 0U;
}

static float body_radius(const PlayCombat *c, uint32_t fighter)
{
    return c->sim.radius[fighter] > 0.0F ? c->sim.radius[fighter] : 0.3F;
}

/* Les ennemis évitent les troncs et les rochers (idée de Théo, engine/ai_avoid.h) : la direction
 * voulue par leur IA est corrigée à moins de 1 m d'un obstacle ; au contact, ils s'en éloignent. */
static void avoid_trees(PlayCombat *c, EngFighterInput *in)
{
    for (uint32_t i = 0; c->tree_count > 0U && i < c->group.count; i++) {
        const uint32_t f = c->group.agents[i].fighter;
        if (f >= c->sim.count || c->sim.fighters[f].vitals.dead || c->hidden[f]) {
            continue;
        }
        float dir[2] = {in[f].moving ? in[f].move_dir[0] : 0.0F,
                        in[f].moving ? in[f].move_dir[1] : 0.0F};
        if (eng_avoid_steer(&c->avoid[f], c->sim.bodies[f].pos, body_radius(c, f), c->trees,
                            c->tree_count, dir)) {
            in[f].moving = true;
            in[f].move_dir[0] = dir[0];
            in[f].move_dir[1] = dir[1];
            c->avoid_steers++;
        }
    }
}

/* après le pas : aucun ennemi ne reste dans un tronc (recul d'un coup, poussée d'un voisin) */
static void keep_out_of_trees(PlayCombat *c)
{
    for (uint32_t i = 0; c->tree_count > 0U && i < c->group.count; i++) {
        const uint32_t f = c->group.agents[i].fighter;
        if (f < c->sim.count && !c->sim.fighters[f].vitals.dead && !c->hidden[f]) {
            (void)eng_avoid_push_out(c->sim.bodies[f].pos, body_radius(c, f), c->trees,
                                     c->tree_count);
        }
    }
}

void play_combat_set_lava(PlayCombat *c, bool in_lava)
{
    if (c != NULL && c->ready) {
        c->in_lava = in_lava;
    }
}

bool play_combat_in_lava(const PlayCombat *c)
{
    return c != NULL && c->ready && c->in_lava;
}

bool play_combat_hunted(const PlayCombat *c)
{
    return c != NULL && c->ready && play_boss_hunting(&c->boss, &c->sim, PLAYER);
}

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
    if (c->invincible && !pf->vitals.dead) { /* mode créatif : PV pleins avant chaque pas */
        pf->vitals.hp = pf->vitals.max_hp;
    }
    /* le joueur : position du personnage physique */
    (void)eng_sim_place(&c->sim, PLAYER, feet[0], feet[2], facing);
    const float player_xz[2] = {feet[0], feet[2]};

    /* verrouillage */
    EngLockCandidate cand[FIGHTERS_MAX];
    for (uint32_t i = 0; i < c->sim.count; i++) {
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
        c->lock = c->lock >= 0 ? -1
                               : eng_lockon_acquire(player_xz, camera_yaw, cand, c->sim.count,
                                                    (int32_t)PLAYER);
    }
    if (c->lock >= 0 && !eng_lockon_valid(player_xz, cand, c->sim.count, c->lock)) {
        c->lock = -1;
    }

    EngFighterInput in[FIGHTERS_MAX];
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
    avoid_trees(c, in);

    /* grande carte : boss spécial : cinématique (tout le monde figé), souffle (dragon figé) */
    const bool cinematic = play_boss_before_step(&c->boss, &c->sim, in, PLAYER);
    const float before[2] = {c->sim.bodies[PLAYER].pos[0], c->sim.bodies[PLAYER].pos[1]};
    rig_tick(c); /* M5-05 / M5-06 : hurtboxes sur les os, lame en main */
    play_boss_rig(&c->boss, &c->sim);
    eng_sim_step(&c->sim, in);
    keep_out_of_trees(c);
    PlayBossEvent boss_event;
    const bool boss_engaged =
        c->zone_boss && c->group.count > 0U &&
        (c->group.agents[0].state == ENG_AI_CHASE || c->group.agents[0].state == ENG_AI_ENGAGE);
    play_boss_after_step(&c->boss, &c->sim, PLAYER, boss_engaged, c->invincible, &boss_event);
    if (boss_event.horse_died) {
        c->cinematic_ticks = ENG_RIDER_FALL_TICKS;
        c->lock = -1;
        ENG_LOG_INFO("combat : le cheval du %s est mort : il tombe, se relève et combat à pied",
                     c->zone_title);
    }
    if (boss_event.burn_dealt > 0.0F) { /* flammes du dragon : comme une touche */
        burn_feedback(c, boss_event.burn_dealt, feet, c->boss.fighter);
    }
    if (boss_event.took_off) {
        c->flight_msg_ticks = 180U;
        c->flight_msg_landing = false;
        ENG_LOG_INFO("combat : le %s décolle", c->zone_title);
    }
    if (boss_event.landed) {
        c->flight_msg_ticks = 150U;
        c->flight_msg_landing = true;
        if (boss_event.shock_dealt > 0.0F) { /* onde de choc : comme une touche */
            burn_feedback(c, boss_event.shock_dealt, feet, c->boss.fighter);
        }
        ENG_LOG_INFO("combat : le %s se pose%s", c->zone_title,
                     boss_event.shock_dealt > 0.0F ? " : son onde de choc touche le joueur" : "");
    }
    lava_tick(c, feet);
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
                  st == ENG_CS_STAGGER || st == ENG_CS_DEAD || st == ENG_CS_PARRY || cinematic;

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
    for (uint32_t i = FIRST_ENEMY; i < c->sim.count; i++) {
        const bool dead = c->sim.fighters[i].vitals.dead;
        if (dead && !c->was_dead[i]) {
            c->stats.enemy_kills++;
            if (c->has_boss && i == BOSS && !c->boss_defeated) { /* M6-09 */
                c->boss_defeated = true;
                c->boss_ticks = 300U;
                ENG_LOG_INFO("combat : %s vaincu", BOSS_NAME);
            }
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
            if (!c->boss_defeated) {
                spawn_boss(c); /* M6-09 : le boss aussi, s'il vit encore */
            }
            out->respawned = true;
            ENG_LOG_INFO("combat : le joueur renaît, le camp est remis à neuf");
        }
    }
    if (eng_camp_cleared_range(&c->group, &c->sim, 0U, c->camp.count)) {
        if (eng_equip_give(&c->equip, ENG_WEAPON_GREATSWORD)) { /* M8-08 : récompense */
            c->weapon_ticks = 300U;
            ENG_LOG_INFO("combat : camp vaincu : espadon obtenu (R pour changer d'arme)");
        }
        if (c->zone_boss) { /* grande carte : un boss de zone ne réapparaît pas, il donne son butin
                             */
            if (c->cleared_ticks == 0U) {
                c->boss_ticks = 240U;
                if (c->zone_index >= 0) {
                    c->zones_beaten |= 1U << (uint32_t)c->zone_index;
                }
                const bool better = eng_gear_give(&c->gear_tier, c->zone_reward);
                if (better) {
                    apply_progress(c, false);
                    c->gear_ticks = 420U;
                }
                ENG_LOG_INFO("combat : %s vaincu (zone « %s ») : butin de palier %u%s",
                             c->zone_title, c->zone_name, c->zone_reward,
                             better ? " (équipement amélioré)" : " (déjà mieux équipé)");
            }
            c->cleared_ticks = 1U;
        } else if (++c->cleared_ticks >= (c->zone_pack ? PACK_RESPAWN_TICKS : CAMP_RESPAWN_TICKS)) {
            c->cleared_ticks = 0;
            spawn_all(c);
            c->respawns++;
            ENG_LOG_INFO(c->zone_pack ? "combat : meute vaincue, elle revient"
                                      : "combat : camp vaincu, il réapparaît");
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
    if (c->boss_ticks > 0U) {
        c->boss_ticks--;
    }
    if (c->zone_ticks > 0U) {
        c->zone_ticks--;
    }
    if (c->gear_ticks > 0U) {
        c->gear_ticks--;
    }
    if (c->cinematic_ticks > 0U) {
        c->cinematic_ticks--;
    }
    if (c->lava_msg_ticks > 0U) {
        c->lava_msg_ticks--;
    }
    if (c->flight_msg_ticks > 0U) {
        c->flight_msg_ticks--;
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

/* Apparence d'un ennemi selon son archétype (corps, armes, taille), fois la taille de la zone. */
static void enemy_look(const PlayCombat *c, uint32_t fighter, AvatarBody *body, AvatarProp *right,
                       AvatarProp *left, float *scale)
{
    *body = AVATAR_SKELETON_MINION;
    *right = AVATAR_SKELETON_BLADE;
    *left = AVATAR_PROP_NONE;
    *scale = 1.0F;
    const uint32_t arch = fighter_arch(c, fighter);
    const char *name = arch < c->arch.count ? c->arch.a[arch].name : "";
    if (strcmp(name, "boss") == 0) { /* M6-09 : guerrier squelette géant à l'espadon */
        *body = c->zone_boss_kind == ENG_BOSSKIND_RIDER ? AVATAR_KNIGHT : AVATAR_SKELETON_WARRIOR;
        *right = AVATAR_GREATSWORD;
        *scale = 1.45F;
    } else if (strcmp(name, "brute") == 0) {
        *body = AVATAR_SKELETON_WARRIOR;
        *right = AVATAR_SKELETON_AXE;
        *scale = 1.2F;
    } else if (strcmp(name, "archer") == 0) {
        *body = AVATAR_SKELETON_ROGUE;
        *right = AVATAR_SKELETON_CROSSBOW;
    } else if (strcmp(name, "imp") == 0) { /* meute : petit squelette, un sur deux encapuchonné */
        *body = (fighter % 2U) == 0U ? AVATAR_SKELETON_MINION : AVATAR_SKELETON_ROGUE;
        *scale = IMP_SCALE;
    } else {
        *left = AVATAR_SKELETON_SHIELD;
    }
    if (c->zone_size > 0.0F) {
        *scale *= c->zone_size;
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
    for (uint32_t i = DUMMY; i < c->sim.count && i < FIGHTERS_MAX; i++) {
        AvatarBody body = AVATAR_SKELETON_MINION;
        AvatarProp right = AVATAR_SKELETON_BLADE;
        AvatarProp left = AVATAR_PROP_NONE;
        float scale = 1.0F;
        if (i == DUMMY) {
            right = AVATAR_PROP_NONE; /* squelette d'entraînement, mains nues */
        } else {
            enemy_look(c, i, &body, &right, &left, &scale);
        }
        c->has_avatar[i] = avatar_init(&c->avatar[i], lib, body, right, left, scale);
        ok = ok && c->has_avatar[i];
        c->rig_prop[i] = right;
        c->rig_scale[i] = scale;
        if (i >= FIRST_ENEMY && c->reach_pose.joint_count == 0U) {
            (void)eng_pose_init(&lib->arena, &lib->skeleton, &c->reach_pose);
        }
        if (i >= FIRST_ENEMY && fighter_arch(c, i) != UINT32_MAX) {
            fit_plans_to_reach(c, lib, fighter_arch(c, i), right, scale);
        }
    }
    if (c->reach_pose.joint_count > 0U) {
        const uint32_t light = eng_attackset_find(&c->attacks, "light1");
        ENG_LOG_INFO("combat : portée de l'épée du joueur (light1) : %.2f m",
                     (double)blade_reach(
                         c, lib, light != ENG_ATTACK_NONE ? &c->attacks.attacks[light] : NULL,
                         AVATAR_SWORD, 1.0F));
    }
    for (uint32_t i = 0; i < FIGHTERS_MAX; i++) {
        (void)eng_trail_init(&c->trail[i], 10U); /* 10 ticks de rémanence */
    }
    c->rig_prop[PLAYER] = /* le chevalier (PlayMode), arme en main (M8-08) */
        c->equip.equipped == ENG_WEAPON_GREATSWORD ? AVATAR_GREATSWORD : AVATAR_SWORD;
    c->rig_scale[PLAYER] = 1.0F;
    for (uint32_t i = 0; hurt_ok && i < c->sim.count && i < FIGHTERS_MAX; i++) {
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
    for (uint32_t i = 0; i < c->sim.count && i < FIGHTERS_MAX; i++) {
        if (!c->rig_ready[i] || play_boss_hides_fighter(&c->boss, i)) {
            continue; /* le dragon : hurtboxes et armes de play_boss_rig */
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
    for (uint32_t i = DUMMY; i < c->sim.count && i < FIGHTERS_MAX; i++) {
        if (!c->has_avatar[i]) {
            continue;
        }
        const EngSimBody *b = &c->sim.bodies[i];
        const float x = b->prev_pos[0] + ((b->pos[0] - b->prev_pos[0]) * t);
        const float z = b->prev_pos[1] + ((b->pos[1] - b->prev_pos[1]) * t);
        /* le cavalier est soulevé par sa monture (et ne marche pas dessus) */
        const float lift = play_boss_rider_lift(&c->boss, i);
        const float feet[3] = {x, play_mode_ground(play, x, z) + lift, z};
        const float dx = b->pos[0] - b->prev_pos[0];
        const float dz = b->pos[1] - b->prev_pos[1];
        EngActorDrive d = {.ground_speed =
                               lift > 0.0F ? 0.0F : sqrtf((dx * dx) + (dz * dz)) * 60.0F,
                           .grounded = true};
        fighter_drive(c, i, c->avatars, alpha, &d);
        if (play_boss_rider_down(&c->boss, i)) { /* cinématique : il tombe, puis se relève */
            d.state = ENG_CS_DEAD;
            d.state_ticks = c->boss.rider.ticks - 30U;
        }
        avatar_update(&c->avatar[i], c->avatars, &d, dt, feet, lerp_angle(b->prev_yaw, b->yaw, t));
        telegraph_glow(c, i, c->avatar[i].glow);
    }
    /* boîtes du cheval ou du dragon */
    c->creature_count = 0;
    float xz[2];
    float yaw = 0.0F;
    if (play_boss_anchor(&c->boss, &c->sim, t, xz, &yaw)) {
        float feet[3] = {xz[0], play_mode_ground(play, xz[0], xz[1]), xz[1]};
        const float altitude = play_boss_altitude(&c->boss);
        if (altitude > 0.0F) {
            /* dragon en vol : son altitude se compte depuis le sol de son antre, mais il garde
             * au moins 20 m au-dessus du relief qu'il survole (le bord du cratère) */
            const float lair = play_mode_ground(play, c->boss.home[0], c->boss.home[1]) + altitude;
            const float over = feet[1] + (altitude < 20.0F ? altitude : 20.0F);
            feet[1] = lair > over ? lair : over;
        }
        c->creature_count = play_boss_boxes(&c->boss, &c->sim, feet, yaw, c->creature_place,
                                            c->creature_color, ENG_CREATURE_MAX_PARTS);
    }
}

uint32_t play_combat_creature(const PlayCombat *c, const mat4 **places, const uint8_t **colors)
{
    if (c == NULL || !c->ready || places == NULL || colors == NULL) {
        return 0U;
    }
    *places = c->creature_place;
    *colors = c->creature_color;
    return c->creature_count;
}

bool play_combat_boss_point(const PlayCombat *c, float xz[2], float *altitude)
{
    if (c == NULL || !c->ready || xz == NULL || altitude == NULL ||
        c->boss.kind == ENG_BOSSKIND_NONE || c->boss.fighter >= c->sim.count) {
        return false;
    }
    xz[0] = c->sim.bodies[c->boss.fighter].pos[0];
    xz[1] = c->sim.bodies[c->boss.fighter].pos[1];
    *altitude = play_boss_altitude(&c->boss);
    return true;
}

bool play_combat_cinematic(const PlayCombat *c, float focus[2])
{
    if (c == NULL || !c->ready || !play_boss_cinematic(&c->boss)) {
        return false;
    }
    if (focus != NULL) {
        focus[0] = c->sim.bodies[c->boss.fighter].pos[0];
        focus[1] = c->sim.bodies[c->boss.fighter].pos[1];
    }
    return true;
}

void play_combat_draw_models(const PlayCombat *c, const AvatarLibrary *lib, EngRenderQueue *queue,
                             mat4 view_proj, const EngLighting *light, const EngModelShadow *shadow,
                             bool shadow_pass)
{
    if (c == NULL || !c->ready || lib == NULL) {
        return;
    }
    for (uint32_t i = DUMMY; i < c->sim.count && i < FIGHTERS_MAX; i++) {
        if (!c->has_avatar[i] || c->hidden[i] || play_boss_hides_fighter(&c->boss, i)) {
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
    c->relief_fn = height; /* gardé : une zone de la grande carte se recharge sur le même relief */
    c->relief_user = user;
    c->relief_slope = max_slope_deg;
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
    if (c->has_boss) { /* M6-09 : le centre de l'arène, sur le praticable le plus proche */
        EngAiAgent *a = &c->group.agents[c->boss_agent];
        moved += eng_nav_walkable(&c->nav, a->home) ? 0U : 1U;
        (void)eng_nav_nearest_walkable(&c->nav, a->home, a->home);
    }
    spawn_all(c);
    spawn_boss(c);
    if (c->has_boss) { /* M6-09 : l'entrée de l'arène mène-t-elle au boss sur ce relief ? */
        float gate[2] = {BOSS_X, BOSS_Z + BOSS_HALF + 3.0F};
        (void)eng_nav_nearest_walkable(&c->nav, gate, gate);
        const float *home = c->group.agents[c->boss_agent].home;
        float route[64][2];
        const uint32_t steps = eng_nav_path(&c->nav, gate, home, route, 64);
        c->boss_reachable = steps > 0U && hypotf(route[steps - 1U][0] - home[0],
                                                 route[steps - 1U][1] - home[1]) < 0.75F;
        if (!c->boss_reachable) {
            ENG_LOG_WARN("combat : arène du boss inaccessible sur ce relief (pente à l'entrée)");
        }
    }
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
    static const char *const names[FIGHTERS_MAX] = {"joueur",   "mannequin", "ennemi 1", "ennemi 2",
                                                    "ennemi 3", "ennemi 4",  "ennemi 5"};
    static const char *const phases[4] = {"anticipation", "ACTIF", "récupération", "fini"};
    size_t n = 0;
    buf[0] = '\0';
    n += (size_t)snprintf(buf + n, cap - n, "tick %u%s\n", (unsigned)c->sim.tick,
                          c->sim.hitstop > 0U ? "  (gel d'impact)" : "");
    for (uint32_t i = 0; i < c->sim.count && i < FIGHTERS_MAX && n < cap; i++) {
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
    for (uint32_t f = FIRST_ENEMY; f < c->sim.count; f++) {
        /* Monde ouvert : un combattant sans rôle dans la zone est rangé « mort » hors de la scène.
         * Ce n'est pas un ennemi vaincu : sans ceci, une nouvelle partie lancée depuis le menu
         * offrait 105 XP et le niveau 2 dès la première seconde. */
        c->was_dead[f] = c->hidden[f];
    }
    eng_prog_init(&c->progress); /* nouvelle partie : niveau 1 */
    c->levelup_ticks = 0;
    eng_equip_init(&c->equip); /* M8-08 : épée et bouclier */
    c->weapon_ticks = 0;
    apply_weapon(c);
    apply_progress(c, true);
    c->gear_tier = 0; /* grande carte : équipement de départ, tous les boss vivants */
    c->zones_beaten = 0;
    c->gear_ticks = 0;
    apply_progress(c, true);
    c->boss_defeated = false; /* M6-09 */
    c->boss_ticks = 0;
    spawn_boss(c);
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
    s->has_gear = true; /* grande carte : palier de butin, boss vaincus */
    s->gear_tier = c->gear_tier;
    s->bosses_beaten = c->zones_beaten;
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
    if (s->has_gear) { /* grande carte */
        c->gear_tier = eng_gear_clamp(s->gear_tier);
        c->zones_beaten = s->bosses_beaten;
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
    /* M6-09 : un boss mort dans la sauvegarde reste vaincu (sauvegardes sans lui : il vit) */
    c->boss_defeated = c->has_boss && c->sim.fighters[BOSS].vitals.dead;
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
    char mode[48] = ""; /* mode créatif : annoncé avec la vitesse de vol (réglée à la molette) */
    if (c->invincible) {
        const float speed = play_mode_creative_speed(play);
        (void)snprintf(mode, sizeof mode,
                       speed < 10.0F ? "  ·  MODE CRÉATIF  ·  %.1f m/s"
                                     : "  ·  MODE CRÉATIF  ·  %.0f m/s",
                       (double)speed);
    }
    (void)snprintf(hud->weapon, sizeof hud->weapon, "%s%s%s%s",
                   eng_weapon_def(c->equip.equipped)->label, c->gear_tier > 0U ? " " : "",
                   c->gear_tier > 0U ? eng_gear_tier_name(c->gear_tier) : "", mode);
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
                name = c->zone_boss ? c->zone_title
                                    : display_name(c->arch.a[c->group.agents[i].arch].name);
            }
        }
        (void)snprintf(hud->target_name, sizeof hud->target_name, "%s", name);
        hud->target_hp = tf->vitals.hp;
        hud->target_hp_max = tf->vitals.max_hp;
        hud->target_posture = tf->posture;
        hud->target_posture_max = tf->params.max_posture;
    }
    hud->has_boss = false; /* M6-09 : barre du boss tant qu'il combat (ou qu'il est verrouillé) */
    if (c->has_boss && !c->sim.fighters[BOSS].vitals.dead) {
        const EngAiState st = c->group.agents[c->boss_agent].state;
        if (st == ENG_AI_CHASE || st == ENG_AI_ENGAGE || c->lock == (int32_t)BOSS) {
            hud->has_boss = true;
            (void)snprintf(hud->boss_name, sizeof hud->boss_name, "%s", BOSS_NAME);
            hud->boss_hp = c->sim.fighters[BOSS].vitals.hp;
            hud->boss_hp_max = c->sim.fighters[BOSS].vitals.max_hp;
        }
    }
    if (c->zone_boss && c->group.count > 0U) { /* grande carte : barre du boss de la zone */
        const uint32_t bf = c->group.agents[0].fighter;
        const EngAiState st = c->group.agents[0].state;
        if (!c->sim.fighters[bf].vitals.dead &&
            (st == ENG_AI_CHASE || st == ENG_AI_ENGAGE || c->lock == (int32_t)bf ||
             play_boss_hunting(&c->boss, &c->sim, PLAYER))) {
            hud->has_boss = true;
            (void)snprintf(hud->boss_name, sizeof hud->boss_name, "%.19s%s", c->zone_title,
                           c->boss.kind == ENG_BOSSKIND_RIDER &&
                                   c->boss.rider.phase == ENG_RIDER_MOUNTED
                               ? ", à cheval"
                               : "");
            hud->boss_hp = c->sim.fighters[bf].vitals.hp;
            hud->boss_hp_max = c->sim.fighters[bf].vitals.max_hp;
        }
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
    hud->letterbox = play_boss_cinematic(&c->boss) ? 1.0F : 0.0F;
    if (pf->vitals.dead) {
        (void)snprintf(hud->message, sizeof hud->message, "%s",
                       "Tu es tombé... renaissance dans un instant");
    } else if (c->lava_msg_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "%s", "La lave brûle : sors de là !");
    } else if (play_boss_diving(&c->boss) && play_boss_hunting(&c->boss, &c->sim, PLAYER)) {
        (void)snprintf(hud->message, sizeof hud->message,
                       "Le %s fond sur toi : écarte-toi de son axe !", c->zone_title);
    } else if (c->flight_msg_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message,
                       c->flight_msg_landing ? "Le %s se pose" : "Le %s prend son vol",
                       c->zone_title);
    } else if (c->cinematic_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "Le cheval s'effondre : le %s se relève",
                       c->zone_title);
    } else if (c->boss_ticks > 0U && c->zone_name[0] != '\0') {
        (void)snprintf(hud->message, sizeof hud->message, "%s vaincu !", c->zone_title);
    } else if (c->gear_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "Butin : %s, %s, %s",
                       eng_gear_name(c->gear_tier, ENG_GEAR_WEAPON),
                       eng_gear_name(c->gear_tier, ENG_GEAR_ARMOR),
                       eng_gear_name(c->gear_tier, ENG_GEAR_SHIELD));
    } else if (c->boss_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "%s vaincu !", BOSS_NAME);
    } else if (c->zone_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "%s", c->zone_name);
    } else if (c->levelup_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "Niveau %u !", c->progress.level);
    } else if (c->weapon_ticks > 200U) {
        (void)snprintf(hud->message, sizeof hud->message, "%s",
                       "Espadon obtenu ! (R : changer d'arme)");
    } else if (c->weapon_ticks > 0U) {
        (void)snprintf(hud->message, sizeof hud->message, "Arme : %s",
                       eng_weapon_def(c->equip.equipped)->label);
    } else if (!c->zone_boss && eng_camp_cleared_range(&c->group, &c->sim, 0U, c->camp.count)) {
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
        for (uint32_t k = FIRST_ENEMY; k < c->sim.count; k++) {
            /* M6-09 : le boss compte 40 m de plus : le camp d'abord, tant qu'il vit */
            const float d =
                hypotf(c->sim.bodies[k].pos[0] - feet[0], c->sim.bodies[k].pos[1] - feet[2]) +
                (c->has_boss && k == BOSS ? 40.0F : 0.0F);
            if (!c->sim.fighters[k].vitals.dead && d < best) {
                best = d;
                c->lock = (int32_t)k;
            }
        }
    }
    if (c->lock < 0) {
        if (c->roam) { /* grande carte : personne à combattre, on suit le chemin vers +X */
            in->axis[ENG_AXIS_MOVE] = (EngAxisValue){cosf(camera_yaw), -sinf(camera_yaw)};
        }
        return;
    }
    float dx = c->sim.bodies[c->lock].pos[0] - feet[0];
    float dz = c->sim.bodies[c->lock].pos[1] - feet[2];
    const bool outside = fabsf(feet[0] - BOSS_X) > BOSS_HALF || fabsf(feet[2] - BOSS_Z) > BOSS_HALF;
    const bool target_inside = fabsf(c->sim.bodies[c->lock].pos[0] - BOSS_X) < BOSS_HALF &&
                               fabsf(c->sim.bodies[c->lock].pos[1] - BOSS_Z) < BOSS_HALF;
    if (c->has_boss && outside && target_inside) {
        /* M6-09 : cible dans l'arène, joueur dehors : passer par l'entrée (2 m devant elle,
         * puis 2 m dedans) plutôt que buter contre le mur */
        const float ahead = feet[2] > BOSS_Z + BOSS_HALF + 1.5F ? 2.5F : -2.0F;
        dx = BOSS_X - feet[0];
        dz = (BOSS_Z + BOSS_HALF + ahead) - feet[2];
        if (fabsf(dx) > 0.6F || ahead < 0.0F) {
            const float l = hypotf(dx, dz);
            if (l > 1e-3F) {
                const float fx = -sinf(camera_yaw);
                const float fz = -cosf(camera_yaw);
                const float rx = cosf(camera_yaw);
                const float rz = -sinf(camera_yaw);
                in->axis[ENG_AXIS_MOVE] =
                    (EngAxisValue){(dx * rx + dz * rz) / l, (dx * fx + dz * fz) / l};
            }
            return;
        }
        dx = c->sim.bodies[c->lock].pos[0] - feet[0];
        dz = c->sim.bodies[c->lock].pos[1] - feet[2];
    }
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

/* un tick où la molette tourne de `notches` crans */
static void wheel(CombatTest *t, float notches)
{
    EngInputState in;
    memset(&in, 0, sizeof in);
    in.wheel = notches;
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

/* M6-09 : le boss sur le terrain : chez lui dans l'arène, joignable par l'entrée, pas à travers les
 * murs ; combattu par un joueur automate (PV énormes) : enchaînement scripté vu, phase 2 vue,
 * vaincu, et il ne renaît pas avec le joueur. */
static bool boss_selftest(CombatTest *t)
{
    PlayCombat *c = t->play.combat;
    check(t, c->has_boss, "boss présent sur un terrain", c->has_boss ? 1.0F : 0.0F);
    if (!c->has_boss) {
        return false;
    }
    const float *bp = c->sim.bodies[BOSS].pos;
    const bool home = fabsf(bp[0] - BOSS_X) < BOSS_HALF && fabsf(bp[1] - BOSS_Z) < BOSS_HALF;
    check(t, home, "boss dans son arène (m du centre)", hypotf(bp[0] - BOSS_X, bp[1] - BOSS_Z));
    float route[64][2];
    float gate[2] = {BOSS_X, BOSS_Z + BOSS_HALF + 3.0F};
    (void)eng_nav_nearest_walkable(&c->nav, gate, gate); /* devant l'entrée, hors pente */
    const uint32_t via_gate = eng_nav_path(&c->nav, gate, bp, route, 64);
    const bool reach = via_gate > 0U && hypotf(route[via_gate - 1U][0] - bp[0],
                                               route[via_gate - 1U][1] - bp[1]) < 0.75F;
    /* sur un relief trop raide, l'entrée peut être en pente : le jeu doit alors le signaler */
    check(t, reach == c->boss_reachable, "accessibilité de l'arène bien signalée (points)",
          (float)via_gate);
    float back[2] = {BOSS_X, BOSS_Z - BOSS_HALF - 3.0F}; /* derrière le mur sud */
    (void)eng_nav_nearest_walkable(&c->nav, back, back);
    const uint32_t around = eng_nav_path(&c->nav, back, bp, route, 64);
    float length = 0.0F;
    float prev[2] = {back[0], back[1]};
    for (uint32_t k = 0; k < around; k++) {
        length += hypotf(route[k][0] - prev[0], route[k][1] - prev[1]);
        prev[0] = route[k][0];
        prev[1] = route[k][1];
    }
    check(t, around == 0U || length > 2.0F * (BOSS_HALF + 3.0F),
          "derrière le mur : on fait le tour (longueur du chemin, m)", length);
    /* murs dans Jolt : un rayon vertical au-dessus du mur sud touche son sommet (3 m au-dessus du
     * sol) ; au-dessus de l'entrée, il touche le sol */
    const float down[3] = {0.0F, -1.0F, 0.0F};
    const float wall_x = BOSS_X;
    const float wall_z = BOSS_Z - BOSS_HALF - 0.5F;
    const float wall_ground = play_mode_ground(&t->play, wall_x, wall_z);
    const float top[3] = {wall_x, wall_ground + 30.0F, wall_z};
    EngRayHit hit;
    const bool wall_hit =
        eng_physics_raycast(t->play.world, top, down, 60.0F, 1U << ENG_LAYER_STATIC, &hit);
    check(t, wall_hit && hit.point[1] > wall_ground + 2.0F,
          "mur sud dans la physique (m au-dessus du sol)",
          wall_hit ? hit.point[1] - wall_ground : -1.0F);
    const float gate_z = BOSS_Z + BOSS_HALF + 0.5F;
    const float gate_ground = play_mode_ground(&t->play, BOSS_X, gate_z);
    const float above_gate[3] = {BOSS_X, gate_ground + 30.0F, gate_z};
    const bool gate_hit =
        eng_physics_raycast(t->play.world, above_gate, down, 60.0F, 1U << ENG_LAYER_STATIC, &hit);
    check(t, gate_hit && hit.point[1] < gate_ground + 0.5F, "entrée ouverte (m au-dessus du sol)",
          gate_hit ? hit.point[1] - gate_ground : -1.0F);
    /* le camp vaincu donne l'espadon même si le boss vit (eng_camp_cleared_range) */
    for (uint32_t i = 0; i < c->camp.count; i++) {
        EngFighter *f = &c->sim.fighters[c->group.agents[i].fighter];
        f->vitals.hp = 0.0F;
        f->vitals.dead = true;
        f->fsm.state = ENG_CS_DEAD;
    }
    step(t, 0U, 0U, 0.0F, 0.0F);
    check(t, eng_equip_owns(&c->equip, ENG_WEAPON_GREATSWORD),
          "camp vaincu, boss vivant : espadon obtenu", 1.0F);
    /* combat : le joueur entre, verrouille le boss et le frappe dès qu'il est à portée */
    c->sim.fighters[PLAYER].vitals.max_hp = 100000.0F;
    c->sim.fighters[PLAYER].vitals.hp = 100000.0F;
    float feet[3] = {BOSS_X, 0.0F, BOSS_Z + 3.0F};
    feet[1] = play_mode_ground(&t->play, feet[0], feet[2]) + 0.05F;
    play_mode_teleport(&t->play, feet, 0.0F); /* regarde -Z : vers le boss */
    step(t, 1U << ENG_ACTION_LOCK_ON, 0U, 0.0F, 0.0F);
    char last[3][32] = {{0}};
    uint32_t starts[3] = {0};
    bool pattern = false;
    bool phase2 = false;
    uint32_t ticks = 0;
    for (; ticks < 15000U && !c->sim.fighters[BOSS].vitals.dead; ticks++) {
        float pf[3];
        (void)eng_character_position(t->play.character, pf);
        const float dx = c->sim.bodies[BOSS].pos[0] - pf[0];
        const float dz = c->sim.bodies[BOSS].pos[1] - pf[2];
        const float l = hypotf(dx, dz);
        const float yaw = t->play.orbit.yaw;
        float mx = 0.0F;
        float my = 0.0F;
        uint32_t press = 0U;
        if (l > 2.0F) {
            mx = ((dx * cosf(yaw)) - (dz * sinf(yaw))) / l;
            my = ((-dx * sinf(yaw)) - (dz * cosf(yaw))) / l;
        } else if (ticks % 12U == 0U) {
            press = 1U << ENG_ACTION_ATTACK;
        }
        if (c->lock != (int32_t)BOSS && ticks % 30U == 0U) {
            press |= 1U << ENG_ACTION_LOCK_ON;
        }
        step(t, press, 0U, mx, my);
        if ((c->sim.fighters[BOSS].events & ENG_FEV_ATTACK_START) != 0U) {
            const EngAttack *a = eng_fighter_attack(&c->sim.fighters[BOSS], c->sim.attacks);
            memmove(last[0], last[1], sizeof last[0] * 2U);
            memmove(&starts[0], &starts[1], sizeof starts[0] * 2U);
            (void)snprintf(last[2], sizeof last[2], "%s", a != NULL ? a->name : "?");
            starts[2] = c->sim.tick;
            phase2 = phase2 || strcmp(last[2], "boss_l2") == 0 || strcmp(last[2], "boss_h2") == 0;
            /* léger, léger, lourd en moins de 4 s : l'enchaînement scripté (LLH) */
            const bool l1 = strncmp(last[0], "boss_l", 6) == 0;
            const bool l2 = strncmp(last[1], "boss_l", 6) == 0;
            const bool h3 = strncmp(last[2], "boss_h", 6) == 0;
            pattern = pattern || (l1 && l2 && h3 && starts[2] - starts[0] < 240U);
        }
    }
    check(t, c->sim.fighters[BOSS].vitals.dead, "boss vaincu (ticks)", (float)ticks);
    check(t, pattern, "enchaînement léger, léger, lourd vu", pattern ? 1.0F : 0.0F);
    check(t, phase2, "attaques de la phase 2 vues", phase2 ? 1.0F : 0.0F);
    check(t, c->boss_defeated, "boss marqué vaincu", c->boss_defeated ? 1.0F : 0.0F);
    /* le joueur meurt : il renaît, le boss reste mort */
    c->sim.fighters[PLAYER].vitals.hp = 0.0F;
    c->sim.fighters[PLAYER].vitals.dead = true;
    for (uint32_t i = 0; i < RESPAWN_TICKS + 30U; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, !c->sim.fighters[PLAYER].vitals.dead, "le joueur renaît", 1.0F);
    check(t, c->sim.fighters[BOSS].vitals.dead, "le boss vaincu ne renaît pas", 1.0F);
    return t->ok;
}

/* Grande carte : les deux boss spéciaux, sans fenêtre. Le cavalier : à cheval (plus rapide, plus
 * large, soulevé), cheval mort -> cinématique (PV tenus, personne ne bouge), puis à pied, vaincu ->
 * butin ; un coup énorme ne traverse pas le cheval. Le dragon : il souffle (brûlures, sauf en mode
 * créatif), il frappe au corps à corps, l'épée du joueur l'atteint, vaincu -> butin final. */
static void kill_fighter(PlayCombat *c, uint32_t f)
{
    EngFighter *fighter = &c->sim.fighters[f];
    (void)eng_damage_apply(&fighter->vitals, 1.0e6F, ENG_ZONE_TORSO, fighter->tick + 1000U);
    (void)eng_fsm_request(&fighter->fsm, ENG_CS_DEAD);
}

static void rider_selftest(CombatTest *t, const EngMapZone *zone)
{
    PlayCombat *c = t->play.combat;
    put(t, 12.0F, -2.0F, 0.0F);
    check(t, play_combat_load_zone(c, zone, 2, 12.0F, -12.0F), "cavalier : zone chargée", 1.0F);
    const uint32_t bf = c->group.agents[0].fighter;
    const float max_hp = c->sim.fighters[bf].vitals.max_hp;
    check(t, c->boss.kind == ENG_BOSSKIND_RIDER && c->boss.rider.phase == ENG_RIDER_MOUNTED,
          "cavalier : à cheval au départ", (float)c->boss.kind);
    check(t, fabsf(c->sim.speed_scale[bf] - (c->boss.base_speed * ENG_RIDER_SPEED)) < 1e-5F,
          "cavalier : plus rapide à cheval (x)", c->sim.speed_scale[bf] / c->boss.base_speed);
    check(t, c->sim.radius[bf] >= 1.0F, "cavalier : plus large à cheval (m)", c->sim.radius[bf]);
    check(t, play_boss_rider_lift(&c->boss, bf) > 1.5F, "cavalier : soulevé par la monture (m)",
          play_boss_rider_lift(&c->boss, bf));
    static mat4 places[ENG_CREATURE_MAX_PARTS];
    uint8_t colors[ENG_CREATURE_MAX_PARTS];
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    check(t, play_boss_boxes(&c->boss, &c->sim, origin, 0.0F, places, colors, 40U) >= 12U,
          "cavalier : boîtes du cheval", 0.0F);
    /* le cheval meurt : cinématique */
    c->sim.fighters[bf].vitals.hp = max_hp * 0.55F;
    step(t, 0U, 0U, 0.0F, 0.0F);
    check(t, play_boss_cinematic(&c->boss) && c->cinematic_ticks > 0U,
          "cavalier : cheval mort, cinématique", (float)c->cinematic_ticks);
    check(t, fabsf(c->sim.fighters[bf].vitals.hp - (max_hp * 0.6F)) < 0.01F,
          "cavalier : il lui reste 60 % des PV", c->sim.fighters[bf].vitals.hp);
    const float boss0[2] = {c->sim.bodies[bf].pos[0], c->sim.bodies[bf].pos[1]};
    float feet0[3];
    float feet1[3];
    (void)eng_character_position(t->play.character, feet0);
    const float player_hp = c->sim.fighters[PLAYER].vitals.hp;
    bool down = false;
    bool swung = false;
    for (int i = 0; i < 200; i++) { /* le joueur pousse en avant et frappe : rien ne se passe */
        step(t, (i % 20) == 0 ? (1U << ENG_ACTION_ATTACK) : 0U, 0U, 0.0F, 1.0F);
        c->sim.fighters[bf].vitals.hp -= 3.0F; /* et un coup qui passerait quand même */
        down = down || play_boss_rider_down(&c->boss, bf);
        swung = swung || c->sim.fighters[PLAYER].fsm.state == ENG_CS_ATTACK;
    }
    (void)eng_character_position(t->play.character, feet1);
    check(t, play_boss_cinematic(&c->boss), "cavalier : cinématique encore en cours", 0.0F);
    check(t,
          !eng_vitals_hittable(&c->sim.fighters[PLAYER].vitals, c->sim.fighters[PLAYER].tick) &&
              !eng_vitals_hittable(&c->sim.fighters[bf].vitals, c->sim.fighters[bf].tick),
          "cavalier : joueur et boss intouchables pendant la cinématique", 0.0F);
    check(t, down, "cavalier : le chevalier est tombé", down ? 1.0F : 0.0F);
    check(t, !swung, "cavalier : le joueur ne frappe pas pendant la cinématique",
          swung ? 1.0F : 0.0F);
    check(t,
          hypotf(c->sim.bodies[bf].pos[0] - boss0[0], c->sim.bodies[bf].pos[1] - boss0[1]) < 0.05F,
          "cavalier : immobile pendant la cinématique (m)",
          hypotf(c->sim.bodies[bf].pos[0] - boss0[0], c->sim.bodies[bf].pos[1] - boss0[1]));
    check(t, hypotf(feet1[0] - feet0[0], feet1[2] - feet0[2]) < 0.3F,
          "cavalier : le joueur est figé (m)", hypotf(feet1[0] - feet0[0], feet1[2] - feet0[2]));
    check(t, c->sim.fighters[PLAYER].vitals.hp >= player_hp, "cavalier : le joueur ne perd rien",
          c->sim.fighters[PLAYER].vitals.hp);
    for (int i = 0; i < 60; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, c->boss.rider.phase == ENG_RIDER_ON_FOOT && !play_boss_cinematic(&c->boss),
          "cavalier : à pied après la cinématique", (float)c->boss.rider.phase);
    check(t, fabsf(c->sim.fighters[bf].vitals.hp - (max_hp * 0.6F)) < 20.0F,
          "cavalier : PV tenus pendant la cinématique", c->sim.fighters[bf].vitals.hp);
    check(t,
          fabsf(c->sim.speed_scale[bf] - c->boss.base_speed) < 1e-5F &&
              fabsf(c->sim.radius[bf] - c->boss.base_radius) < 1e-5F &&
              play_boss_rider_lift(&c->boss, bf) == 0.0F,
          "cavalier : vitesse et carrure à pied", c->sim.radius[bf]);
    kill_fighter(c, bf);
    for (int i = 0; i < 5; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, play_combat_zone_beaten(c, 2) && c->gear_tier == 1U,
          "cavalier : vaincu, butin de palier 1", (float)c->gear_tier);
    /* un coup énorme pendant qu'il est à cheval ne tue que le cheval */
    c->zones_beaten = 0;
    (void)play_combat_load_zone(c, zone, 2, 12.0F, -12.0F);
    kill_fighter(c, bf);
    step(t, 0U, 0U, 0.0F, 0.0F);
    check(t,
          !c->sim.fighters[bf].vitals.dead && play_boss_cinematic(&c->boss) &&
              fabsf(c->sim.fighters[bf].vitals.hp - (max_hp * 0.6F)) < 0.01F,
          "cavalier : un coup énorme ne traverse pas le cheval", c->sim.fighters[bf].vitals.hp);
    check(t, !play_combat_zone_beaten(c, 2), "cavalier : pas encore vaincu", 0.0F);
}

/* où est, dans le plan de la simulation, le point (x, z) du modèle du dragon (échelle du jeu) */
static void dragon_spot(const PlayCombat *c, uint32_t bf, float model_x, float model_z,
                        float out[2])
{
    const EngSimBody *b = &c->sim.bodies[bf];
    const float lx = model_x * PLAY_DRAGON_SCALE;
    const float lz = model_z * PLAY_DRAGON_SCALE;
    out[0] = b->pos[0] + (lx * cosf(b->yaw)) + (lz * sinf(b->yaw));
    out[1] = b->pos[1] - (lx * sinf(b->yaw)) + (lz * cosf(b->yaw));
}

/* avance de `ticks` pas en relevant le plus grand pivotement du dragon en un pas (rad) */
static float dragon_wait(CombatTest *t, uint32_t bf, int ticks, uint32_t pressed)
{
    PlayCombat *c = t->play.combat;
    float worst = 0.0F;
    for (int i = 0; i < ticks; i++) {
        const float yaw = c->sim.bodies[bf].yaw;
        step(t, (i % 30) == 0 ? pressed : 0U, 0U, 0.0F, 0.0F);
        const float turn = fabsf(remainderf(c->sim.bodies[bf].yaw - yaw, 6.2831853F));
        worst = turn > worst ? turn : worst;
    }
    return worst;
}

static bool dragon_struck(const PlayCombat *c, uint32_t bf)
{
    for (uint32_t k = 0; k < c->sim.event_count; k++) {
        const EngSimEvent *e = &c->sim.events[k];
        if (e->attacker == bf && e->defender == PLAYER && e->kind == ENG_HITKIND_DAMAGE) {
            return true;
        }
    }
    return false;
}

/* Le Dragon de lave, dix fois plus grand qu'avant, et qui vole (demande de Théo, 2026-11-22).
 * L'arène d'essai (56 m) est trop petite pour lui : le joueur est en vol créatif (posé au niveau
 * du sol, sans tomber), mais vulnérable (`invincible` remis à faux). */
static void dragon_ground_selftest(CombatTest *t, const EngMapZone *zone, uint32_t *fighter)
{
    PlayCombat *c = t->play.combat;
    PlayMode *p = &t->play;
    c->zones_beaten = 0;
    /* le « dernier lieu sûr » (où l'on renaît) n'est plus relevé dans le domaine du dragon :
     * mort sous ses flammes, le joueur renaît là où il était avant d'y entrer */
    (void)play_combat_load_zone(c, NULL, -1, 12.0F, -14.0F);
    put(t, 3.0F, 14.0F, 0.0F);
    for (int i = 0; i < 35; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    const bool safe_before = p->safe_valid && fabsf(p->safe[0] - 3.0F) < 0.2F &&
                             fabsf(p->safe[2] - 14.0F) < 0.2F && !play_combat_hunted(c);
    const bool loaded = play_combat_load_zone(c, zone, 11, 12.0F, -14.0F);
    check(t, loaded, "dragon : zone chargée", 1.0F);
    const uint32_t bf = c->group.agents[0].fighter;
    *fighter = bf;
    EngFighter *pf = &c->sim.fighters[PLAYER];
    pf->vitals.max_hp = 20000.0F;
    pf->vitals.hp = 20000.0F;
    const float inside[3] = {-10.0F, PLAY_GROUND_TOP + 0.05F, 10.0F};
    (void)eng_character_teleport(p->character, inside);
    for (int i = 0; i < 70; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t,
          safe_before && play_combat_hunted(c) && fabsf(p->safe[0] - 3.0F) < 0.2F &&
              fabsf(p->safe[2] - 14.0F) < 0.2F,
          "dragon : dans son domaine, le lieu sûr n'est plus relevé (x)", p->safe[0]);
    play_mode_set_creative(&t->play, true);
    put(t, 12.0F, 200.0F, 0.0F);
    (void)play_combat_load_zone(c, zone, 11, 12.0F, -14.0F); /* le dragon à neuf */
    c->invincible = false;
    pf->vitals.max_hp = 20000.0F;
    pf->vitals.hp = 20000.0F;
    check(t, c->boss.kind == ENG_BOSSKIND_DRAGON && play_boss_hides_fighter(&c->boss, bf),
          "dragon : boss de la zone du volcan", (float)c->boss.kind);
    check(t, fabsf(c->sim.radius[bf] - 2.0F) < 1e-5F && (c->sim.world_ignore & (1U << bf)) != 0U,
          "dragon : on passe sous son ventre, il enjambe le décor (rayon, m)", c->sim.radius[bf]);
    /* dix fois plus grand : tête à plus de 40 m, 50 m devant lui ; plus de 100 m d'envergure */
    static mat4 places[ENG_CREATURE_MAX_PARTS];
    uint8_t colors[ENG_CREATURE_MAX_PARTS];
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    step(t, 0U, 0U, 0.0F, 0.0F);
    const uint32_t boxes = play_boss_boxes(&c->boss, &c->sim, origin, 0.0F, places, colors, 48U);
    float top = 0.0F;
    float front = 0.0F;
    float span = 0.0F;
    for (uint32_t i = 0; i < boxes; i++) {
        top = fmaxf(top, places[i][3][1]);
        front = fminf(front, places[i][3][2]);
        span = fmaxf(span, fabsf(places[i][3][0]));
    }
    check(t, boxes >= 30U && top > 40.0F && front < -45.0F && span > 40.0F,
          "dragon : dix fois plus grand : tête à plus de 40 m (m)", top);
    const float *home = c->boss.home;
    /* 1. souffle au sol : il voit le joueur à 110 m devant lui, charge (sans brûler), puis brûle
     * une bande ; hors de la bande, plus rien */
    put(t, home[0] - 110.0F, home[1], 0.0F);
    int waited = 0;
    float worst = 0.0F;
    for (; waited < 900 && c->boss.flame.state != ENG_FLAME_BURN; waited++) {
        worst = fmaxf(worst, dragon_wait(t, bf, 1, 0U));
        c->boss.flight.ticks = 0; /* pas de décollage pendant les essais au sol */
    }
    check(t, c->boss.flame.state == ENG_FLAME_BURN, "dragon : il souffle (ticks d'attente)",
          (float)waited);
    check(t, c->boss.burns == 0U, "dragon : la charge ne brûle pas", (float)c->boss.burns);
    c->invincible = true; /* mode créatif : les flammes ne retirent rien */
    (void)dragon_wait(t, bf, 14, 0U);
    check(t, c->boss.burns == 0U && pf->vitals.hp >= 20000.0F, "dragon : mode créatif, pas brûlé",
          pf->vitals.hp);
    c->invincible = false;
    const float hp0 = pf->vitals.hp;
    (void)dragon_wait(t, bf, 60, 0U);
    const uint32_t burns = c->boss.burns;
    check(t, burns >= 4U, "dragon : brûlures dans la bande de feu", (float)burns);
    check(t,
          fabsf((hp0 - pf->vitals.hp) -
                ((float)burns * 8.0F * zone->damage_scale * eng_gear_armor(c->gear_tier))) < 1.0F,
          "dragon : PV retirés par les flammes", hp0 - pf->vitals.hp);
    put(t, home[0] - 110.0F, home[1] + 40.0F, 0.0F); /* 40 m sur le côté : hors de la bande */
    check(t, c->boss.burns <= burns + 1U, "dragon : hors de la bande, plus de brûlure",
          (float)(c->boss.burns - burns));
    /* 2. pendant qu'il souffle il ne bouge pas, tête au sol : c'est le moment de le frapper. Le
     * joueur revient devant lui ; dès qu'il charge, il file sous sa tête, puis à une patte. */
    for (int i = 0; i < 600 && c->boss.flame.state != ENG_FLAME_IDLE; i++) {
        (void)dragon_wait(t, bf, 1, 0U);
        c->boss.flight.ticks = 0;
    }
    put(t, home[0] - 110.0F, home[1], 0.0F);
    for (waited = 0; waited < 900 && c->boss.flame.state != ENG_FLAME_BURN; waited++) {
        worst = fmaxf(worst, dragon_wait(t, bf, 1, 0U));
        c->boss.flight.ticks = 0;
    }
    float spot[2];
    dragon_spot(c, bf, 0.0F, -4.4F, spot); /* sous sa tête, derrière sa gueule : hors du jet */
    float feet[3] = {spot[0], PLAY_GROUND_TOP + 0.05F, spot[1]};
    (void)eng_character_teleport(t->play.character, feet);
    float hp = c->sim.fighters[bf].vitals.hp;
    const float hp_player = pf->vitals.hp;
    for (int i = 0; i < 45 && c->sim.fighters[bf].vitals.hp >= hp; i++) {
        step(t, (i % 15) == 0 ? (1U << ENG_ACTION_ATTACK) : 0U, 0U, 0.0F, 0.0F);
    }
    check(t, c->boss.flame.state == ENG_FLAME_BURN && c->sim.fighters[bf].vitals.hp < hp,
          "dragon : il souffle, tête au sol : l'épée l'atteint (PV)",
          hp - c->sim.fighters[bf].vitals.hp);
    check(t, fabsf(pf->vitals.hp - hp_player) < 1e-3F,
          "dragon : sous sa tête, derrière la gueule : pas brûlé (PV)", hp_player - pf->vitals.hp);
    /* ses pattes : posé dessus, le joueur est repoussé à 5 m ; face à elle, l'épée l'entame */
    float paw[2];
    dragon_spot(c, bf, 0.8F, 1.4F, paw); /* patte arrière droite */
    feet[0] = paw[0];
    feet[2] = paw[1];
    (void)eng_character_teleport(t->play.character, feet);
    for (int i = 0; i < 25; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    (void)eng_character_position(t->play.character, feet);
    const float out_of_leg = hypotf(feet[0] - paw[0], feet[2] - paw[1]);
    check(t, out_of_leg > 4.8F && out_of_leg < 5.3F, "dragon : une patte repousse le joueur (m)",
          out_of_leg);
    const float face = atan2f(-(paw[0] - feet[0]), -(paw[1] - feet[2]));
    (void)eng_orbit_camera_init(&t->play.orbit, feet, face, -0.25F, ENG_ORBIT_DISTANCE_DEFAULT);
    (void)eng_locomotion_init(&t->play.loco, face);
    hp = c->sim.fighters[bf].vitals.hp;
    for (int i = 0; i < 60 && c->sim.fighters[bf].vitals.hp >= hp; i++) {
        step(t, (i % 15) == 0 ? (1U << ENG_ACTION_ATTACK) : 0U, 0U, 0.0F, 0.0F);
    }
    check(t, eng_flame_busy(&c->boss.flame) && c->sim.fighters[bf].vitals.hp < hp,
          "dragon : l'épée du joueur entame une patte (PV)", hp - c->sim.fighters[bf].vitals.hp);
    /* au centre, sous son ventre (13 m plus haut), l'épée ne touche rien */
    const EngSimBody *body = &c->sim.bodies[bf];
    feet[0] = body->pos[0];
    feet[2] = body->pos[1];
    (void)eng_character_teleport(t->play.character, feet);
    hp = c->sim.fighters[bf].vitals.hp;
    for (int i = 0; i < 30; i++) {
        step(t, (i % 15) == 0 ? (1U << ENG_ACTION_ATTACK) : 0U, 0U, 0.0F, 0.0F);
    }
    check(t, fabsf(c->sim.fighters[bf].vitals.hp - hp) < 1e-3F,
          "dragon : sous son ventre, l'épée ne touche rien (PV)",
          hp - c->sim.fighters[bf].vitals.hp);
    /* 3. morsure : sa tête s'abat là où se tient le joueur : à ses pieds (31 m devant son centre)
     * comme au bout de son cou (70 m) */
    for (int i = 0; i < 500 && eng_flame_busy(&c->boss.flame); i++) {
        (void)dragon_wait(t, bf, 1, 0U);
    }
    bool struck = false;
    static const float bite_at[2] = {-2.4F, -5.4F};
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < 400 && c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK; i++) {
            (void)dragon_wait(t, bf, 1, 0U);
            c->boss.flight.ticks = 0;
        }
        dragon_spot(c, bf, 0.0F, bite_at[k], spot);
        put(t, spot[0], spot[1], 0.0F);
        struck = false;
        for (int i = 0; i < 1500 && !struck; i++) {
            worst = fmaxf(worst, dragon_wait(t, bf, 1, 0U));
            c->boss.flight.ticks = 0;
            struck = dragon_struck(c, bf);
        }
        check(t, struck && c->boss.bite > 0.6F,
              k == 0 ? "dragon : sa morsure touche le joueur à ses pieds (31 m)"
                     : "dragon : sa morsure touche le joueur au bout de son cou (70 m)",
              c->boss.lunge);
    }
    /* pendant qu'il arme sa morsure, il suit le joueur : celui-ci se déplace de 15 m (12 degrés)
     * au début de l'anticipation, la tête tombe quand même sur lui */
    for (int i = 0; i < 400 && c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK; i++) {
        (void)dragon_wait(t, bf, 1, 0U);
        c->boss.flight.ticks = 0;
    }
    dragon_spot(c, bf, 0.0F, -5.3F, spot);
    put(t, spot[0], spot[1], 0.0F);
    bool winding = false; /* la morsure vient de commencer */
    for (int i = 0; i < 1500 && !winding; i++) {
        (void)dragon_wait(t, bf, 1, 0U);
        c->boss.flight.ticks = 0;
        const EngFighter *df = &c->sim.fighters[bf];
        const EngAttack *atk =
            df->fsm.state == ENG_CS_ATTACK ? eng_fighter_attack(df, c->sim.attacks) : NULL;
        winding = atk != NULL && strcmp(atk->name, "dragon_bite") == 0 && df->attack_tick < 6U;
    }
    dragon_spot(c, bf, 1.15F, -5.2F, spot);
    feet[0] = spot[0];
    feet[2] = spot[1];
    (void)eng_character_teleport(t->play.character, feet);
    struck = false;
    for (int i = 0; i < 100 && !struck; i++) {
        (void)dragon_wait(t, bf, 1, 0U);
        c->boss.flight.ticks = 0;
        struck = dragon_struck(c, bf);
    }
    check(t, winding && struck, "dragon : il suit le joueur pendant qu'il arme sa morsure", 0.0F);
    /* le coup parti, il ne corrige plus : le joueur qui, lorsque la tête commence à tomber,
     * s'écarte de 25 m sur le côté, ou court 22 m vers lui (sous son cou), n'est pas touché */
    static const float escape[2][2] = {{1.95F, -4.0F}, {0.0F, -2.3F}};
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < 400 && c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK; i++) {
            (void)dragon_wait(t, bf, 1, 0U);
            c->boss.flight.ticks = 0;
        }
        dragon_spot(c, bf, 0.0F, -4.0F, spot);
        put(t, spot[0], spot[1], 0.0F);
        bool falling = false; /* la morsure entre dans sa fenêtre active */
        for (int i = 0; i < 1500 && !falling; i++) {
            (void)dragon_wait(t, bf, 1, 0U);
            c->boss.flight.ticks = 0;
            const EngFighter *df = &c->sim.fighters[bf];
            const EngAttack *atk =
                df->fsm.state == ENG_CS_ATTACK ? eng_fighter_attack(df, c->sim.attacks) : NULL;
            falling = atk != NULL && strcmp(atk->name, "dragon_bite") == 0 &&
                      df->attack_tick + 1U >= atk->startup;
        }
        dragon_spot(c, bf, escape[k][0], escape[k][1], spot);
        feet[0] = spot[0];
        feet[2] = spot[1];
        (void)eng_character_teleport(t->play.character, feet);
        struck = false;
        for (int i = 0; i < 90; i++) {
            (void)dragon_wait(t, bf, 1, 0U);
            c->boss.flight.ticks = 0;
            struck = struck || dragon_struck(c, bf);
        }
        check(t, falling && !struck,
              k == 0 ? "dragon : morsure partie, le joueur qui s'écarte l'évite"
                     : "dragon : morsure partie, le joueur qui court sous son cou l'évite",
              c->boss.lunge);
    }
    /* 4. piétinement : près de ses pattes avant, il se cabre et retombe */
    for (int i = 0; i < 400 && c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK; i++) {
        (void)dragon_wait(t, bf, 1, 0U);
        c->boss.flight.ticks = 0;
    }
    dragon_spot(c, bf, 0.0F, -1.8F, spot);
    put(t, spot[0], spot[1], 0.0F);
    struck = false;
    float reared = 0.0F;
    for (int i = 0; i < 1500 && !struck; i++) {
        worst = fmaxf(worst, dragon_wait(t, bf, 1, 0U));
        c->boss.flight.ticks = 0;
        reared = fmaxf(reared, c->boss.rear);
        struck = dragon_struck(c, bf);
    }
    check(t, struck && reared > 0.6F, "dragon : il se cabre et piétine le joueur (cabré)", reared);
    /* 5. il pivote lentement : le joueur sur son flanc, à 60 m ; il finit par lui faire face,
     * sans jamais dépasser 0,45 rad/s (un quart de tour demande plus de 3 s) */
    for (int i = 0; i < 400 && c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK; i++) {
        (void)dragon_wait(t, bf, 1, 0U);
        c->boss.flight.ticks = 0;
    }
    dragon_spot(c, bf, 4.6F, 0.0F, spot); /* 60 m sur sa droite */
    feet[0] = spot[0];
    feet[2] = spot[1];
    (void)eng_character_teleport(t->play.character, feet);
    float off_axis[2] = {0.0F, 0.0F}; /* écart entre son cap et le joueur, après 1 s puis 6 s */
    for (int i = 0; i < 360; i++) {
        worst = fmaxf(worst, dragon_wait(t, bf, 1, 0U));
        c->boss.flight.ticks = 0;
        if (i == 59 || i == 359) {
            const EngSimBody *d = &c->sim.bodies[bf];
            const float to_player = atan2f(-(spot[0] - d->pos[0]), -(spot[1] - d->pos[1]));
            off_axis[i == 59 ? 0 : 1] = fabsf(remainderf(to_player - d->yaw, 6.2831853F));
        }
    }
    check(t, off_axis[0] > 0.8F, "dragon : pas de demi-tour sur place (écart après 1 s, rad)",
          off_axis[0]);
    check(t, off_axis[1] < 0.35F, "dragon : il finit par faire face au joueur (écart, rad)",
          off_axis[1]);
    check(t, worst > 0.005F && worst <= (0.45F / 60.0F) + 1e-4F,
          "dragon : au sol, il pivote lentement (rad/pas)", worst);
}

/* Le vol : il décolle après 35 s de combat, fond deux fois sur le joueur en brûlant une bande de
 * sol (qui reste sur son axe est brûlé ; qui s'en écarte ne l'est pas), puis revient se poser
 * dans une onde de choc. Tué en vol, il tombe. */
static void dragon_flight_selftest(CombatTest *t, uint32_t bf)
{
    PlayCombat *c = t->play.combat;
    EngFighter *pf = &c->sim.fighters[PLAYER];
    const float *home = c->boss.home;
    put(t, home[0] + 40.0F, home[1] + 30.0F, 0.0F);
    /* blessé à plus de la moitié, il fait deux passages de feu par vol (un seul en pleine forme) */
    c->sim.fighters[bf].vitals.hp = 0.4F * c->sim.fighters[bf].vitals.max_hp;
    c->boss.flight.ticks = ENG_DRAGON_GROUND_FIGHT_TICKS; /* le combat au sol a assez duré */
    int waited = 0;
    for (; waited < 600 && !play_boss_airborne(&c->boss); waited++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, play_boss_airborne(&c->boss) && c->boss.flights == 1U,
          "dragon : il décolle (ticks d'attente)", (float)waited);
    for (int i = 0; i < 200; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, play_boss_altitude(&c->boss) > 60.0F && c->sim.radius[bf] < 1e-6F,
          "dragon : en vol, à plus de 60 m (altitude, m)", play_boss_altitude(&c->boss));
    const float hp_air = c->sim.fighters[bf].vitals.hp;
    /* premier passage : le joueur ne bouge pas : le feu passe sur lui */
    uint32_t burns = c->boss.burns;
    bool fired = false;
    bool struck_in_air = false; /* en vol, il ne mord ni ne piétine : son IA est muette */
    for (int i = 0; i < 3000 && c->boss.flight.passes < 1U; i++) {
        step(t, (i % 30) == 0 ? (1U << ENG_ACTION_ATTACK) : 0U, 0U, 0.0F, 0.0F);
        fired = fired || c->boss.flight.flame;
        struck_in_air = struck_in_air || c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK;
    }
    check(t, fired && c->boss.flight.passes == 1U, "dragon : premier passage de feu", 1.0F);
    check(t, c->boss.burns >= burns + 2U, "dragon : sur son axe, le joueur est brûlé (brûlures)",
          (float)(c->boss.burns - burns));
    check(t, fabsf(c->sim.fighters[bf].vitals.hp - hp_air) < 1e-3F,
          "dragon : en vol, l'épée ne l'atteint pas (PV)", hp_air - c->sim.fighters[bf].vitals.hp);
    /* second passage : dès qu'il a figé son axe, le joueur s'en écarte de 30 m : pas brûlé */
    burns = c->boss.burns;
    bool dodged = false;
    for (int i = 0; i < 3000 && c->boss.flight.passes < 2U; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
        struck_in_air = struck_in_air || c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK;
        if (c->boss.flight.aim_locked && !dodged) {
            const float yaw = c->boss.flight.yaw;
            const float feet[3] = {c->boss.flight.aim[0] - (30.0F * cosf(yaw)),
                                   PLAY_GROUND_TOP + 0.05F,
                                   c->boss.flight.aim[1] + (30.0F * sinf(yaw))};
            (void)eng_character_teleport(t->play.character, feet);
            dodged = true;
        }
    }
    check(t, dodged && c->boss.flight.passes == 2U && c->boss.burns == burns,
          "dragon : écarté de son axe, le joueur n'est pas brûlé", (float)(c->boss.burns - burns));
    /* atterrissage : il revient au centre de l'antre ; son onde de choc touche le joueur proche */
    put(t, home[0] + 12.0F, home[1], 0.0F);
    const float hp0 = pf->vitals.hp;
    for (waited = 0; waited < 3000 && play_boss_airborne(&c->boss); waited++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
        struck_in_air = struck_in_air || (play_boss_airborne(&c->boss) &&
                                          c->sim.fighters[bf].fsm.state == ENG_CS_ATTACK);
    }
    check(t, !struck_in_air, "dragon : de tout son vol, il ne mord ni ne piétine", 0.0F);
    const EngSimBody *body = &c->sim.bodies[bf];
    check(t,
          !play_boss_airborne(&c->boss) &&
              hypotf(body->pos[0] - home[0], body->pos[1] - home[1]) < 4.0F,
          "dragon : il se pose au centre de son antre (ticks)", (float)waited);
    check(t, fabsf(c->sim.radius[bf] - 2.0F) < 1e-5F && play_boss_altitude(&c->boss) < 1e-4F,
          "dragon : posé (altitude, m)", play_boss_altitude(&c->boss));
    check(t, hp0 - pf->vitals.hp > 30.0F, "dragon : onde de choc de l'atterrissage (PV)",
          hp0 - pf->vitals.hp);
    /* remis en pleine forme, il redécolle pour un vol à UN passage de feu, et se repose */
    c->sim.fighters[bf].vitals.hp = c->sim.fighters[bf].vitals.max_hp;
    put(t, home[0] + 40.0F, home[1] + 30.0F, 0.0F);
    c->boss.flight.ticks = ENG_DRAGON_GROUND_FIGHT_TICKS;
    uint32_t passes = 0;
    bool flew = false;
    for (waited = 0; waited < 4000 && (!flew || play_boss_airborne(&c->boss)); waited++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
        flew = flew || play_boss_airborne(&c->boss);
        passes = c->boss.flight.passes > passes ? c->boss.flight.passes : passes;
    }
    check(t, flew && !play_boss_airborne(&c->boss) && passes == 1U && c->boss.flights == 2U,
          "dragon : en pleine forme, un seul passage de feu par vol (passages)", (float)passes);
    /* il redécolle ; tué en vol, il tombe : vaincu, butin de palier 4 */
    c->boss.flight.ticks = ENG_DRAGON_GROUND_FIGHT_TICKS;
    for (waited = 0; waited < 1200 && play_boss_altitude(&c->boss) < 50.0F; waited++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, play_boss_altitude(&c->boss) >= 50.0F && c->boss.flights == 3U,
          "dragon : il redécolle (ticks d'attente)", (float)waited);
    kill_fighter(c, bf);
    for (waited = 0; waited < 300 && play_boss_altitude(&c->boss) > 0.0F; waited++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, play_boss_altitude(&c->boss) < 1e-4F && waited < 150,
          "dragon : tué en vol, il tombe (ticks de chute)", (float)waited);
    for (int i = 0; i < 5; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, play_combat_zone_beaten(c, 11) && c->gear_tier == 4U,
          "dragon : vaincu, butin de palier 4", (float)c->gear_tier);
    check(t, c->boss.flame.state == ENG_FLAME_IDLE && !c->boss.flight.flame,
          "dragon : mort, plus de flammes", (float)c->boss.flame.state);
    play_mode_set_creative(&t->play, false);
    put(t, 4.0F, 0.3F, 0.0F);
}

static void dragon_selftest(CombatTest *t, const EngMapZone *zone)
{
    uint32_t bf = FIRST_ENEMY;
    dragon_ground_selftest(t, zone, &bf);
    dragon_flight_selftest(t, bf);
}

/* Grande carte : le décor dans le mode jouable : une boîte solide arrête le joueur et devient un
 * obstacle des ennemis ; elle suit le recentrage de l'origine ; retirée, elle ne bloque plus. */
static float walk_east(CombatTest *t, int ticks)
{
    for (int i = 0; i < ticks; i++) {
        step(t, 0U, 0U, 0.0F, 1.0F);
    }
    float feet[3] = {0.0F, 0.0F, 0.0F};
    (void)eng_character_position(t->play.character, feet);
    return feet[0];
}

static void decor_selftest(CombatTest *t)
{
    PlayMode *p = &t->play;
    PlayCombat *c = p->combat;
    (void)play_combat_load_zone(c, NULL, -1, 0.0F, 0.0F); /* plus d'ennemis */
    const float east = -1.5707963F;                       /* lacet qui regarde +X */
    put(t, 3.0F, 4.0F, east);
    check(t, walk_east(t, 120) > 6.5F, "décor : sans décor, le chemin est libre (x)", 0.0F);
    const EngDecorBox boxes[2] = {
        {.x = 0.0F, .z = 0.0F, .half = {0.5F, 1.0F, 3.0F}, .solid = true},
        {.x = -2.0F, .z = 1.0F, .half = {0.3F, 0.3F, 0.3F}, .lift = 0.1F, .yaw = 0.5F},
    };
    put(t, 3.0F, 4.0F, east);
    check(t, play_mode_add_decor(p, boxes, 2U, 6.0F, 4.0F) == 2U && p->decor_count == 2U,
          "décor : deux boîtes posées", (float)p->decor_count);
    check(t,
          c->world.count == 1U && c->world.obs[0].box && fabsf(c->world.obs[0].c[0] - 6.0F) < 1e-4F,
          "décor : la boîte solide est un obstacle des ennemis", (float)c->world.count);
    check(t,
          fabsf(p->decor_center[0][1] - (PLAY_GROUND_TOP + 0.75F)) < 1e-4F &&
              fabsf(p->decor_half[0][1] - 1.25F) < 1e-4F,
          "décor : enfoncée de 0,5 m dans le sol (centre y)", p->decor_center[0][1]);
    check(t, fabsf(p->decor_center[1][1] - (PLAY_GROUND_TOP + 0.4F)) < 1e-4F,
          "décor : la boîte de décor suit le sol (centre y)", p->decor_center[1][1]);
    const float stopped = walk_east(t, 120);
    check(t, stopped < 5.6F, "décor : le mur arrête le joueur (x)", stopped);
    const float shift[3] = {0.0F, 0.0F, 10.0F}; /* recentrage : le mur part 10 m plus loin */
    play_decor_shift(p, shift);
    check(t, fabsf(p->decor_center[0][2] - 14.0F) < 1e-4F, "décor : décalé avec l'origine (z)",
          p->decor_center[0][2]);
    check(t, walk_east(t, 120) > 6.5F, "décor : le corps physique a suivi (x)", 0.0F);
    play_mode_clear_decor(p);
    check(t, p->decor_count == 0U && c->world.count == 0U, "décor : retiré", (float)p->decor_count);
    put(t, 3.0F, 14.0F, east);
    check(t, walk_east(t, 120) > 6.5F, "décor : retiré, il ne bloque plus (x)", 0.0F);
}

/* Carte du monde : dans l'eau jusqu'à la taille (pieds à plus de 0,35 m sous la surface), le
 * joueur marche à 60 % de sa vitesse ; au sec, à sa vitesse normale. */
static void wade_selftest(CombatTest *t)
{
    PlayMode *p = &t->play;
    const float east = -1.5707963F;
    put(t, 3.0F, 18.0F, east);
    const float dry = walk_east(t, 120) - 3.0F;
    put(t, 3.0F, 18.0F, east);
    play_mode_set_water_level(p, PLAY_GROUND_TOP + 1.0F); /* de l'eau jusqu'à la taille */
    const float wet = walk_east(t, 120) - 3.0F;
    check(t, p->wading, "gué : le joueur est dans l'eau", p->wading ? 1.0F : 0.0F);
    check(t, dry > 4.0F && wet > 0.5F * dry && wet < 0.7F * dry,
          "gué : il avance à 60 % de sa vitesse (rapport)", wet / dry);
    play_mode_set_water_level(p, PLAY_GROUND_TOP + 0.2F); /* de l'eau aux chevilles : rien */
    put(t, 3.0F, 18.0F, east);
    const float shallow = walk_east(t, 120) - 3.0F;
    check(t, !p->wading && shallow > 0.95F * dry, "gué : eau aux chevilles, vitesse normale",
          shallow / dry);
    play_mode_set_water_level(p, -INFINITY);
}

/* Carte du monde : la lave brûle (6 % des PV toutes les 20 ticks), sauf en mode créatif ; mort dans
 * la lave, le joueur renaît au dernier lieu sûr. */
static void lava_selftest(CombatTest *t)
{
    PlayMode *p = &t->play;
    PlayCombat *c = p->combat;
    EngFighter *pf = &c->sim.fighters[PLAYER];
    put(t, 3.0F, 14.0F, 0.0F); /* 30 ticks au sol, hors de la lave : c'est le lieu sûr */
    pf->vitals.max_hp = 200.0F;
    pf->vitals.hp = 200.0F;
    check(t, p->safe_valid && fabsf(p->safe[0] - 3.0F) < 0.2F && fabsf(p->safe[2] - 14.0F) < 0.2F,
          "lave : lieu sûr relevé (x)", p->safe[0]);
    play_combat_set_lava(c, true);
    for (int i = 0; i < 61; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, c->lava_burns == 4U && fabsf(pf->vitals.hp - (200.0F - (4.0F * 12.0F))) < 0.01F,
          "lave : 4 brûlures de 6 % en 61 ticks (PV)", pf->vitals.hp);
    c->invincible = true; /* mode créatif */
    const float hp_creative = pf->vitals.hp;
    for (int i = 0; i < 45; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, pf->vitals.hp >= hp_creative, "lave : rien en mode créatif (PV)", pf->vitals.hp);
    c->invincible = false;
    play_combat_set_lava(c, false);
    pf->vitals.hp = 200.0F;
    for (int i = 0; i < 45; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, pf->vitals.hp == 200.0F, "lave : hors de la lave, plus de brûlure (PV)",
          pf->vitals.hp);
    /* il marche dans la lave 6 m plus loin et y meurt : il renaît au lieu sûr */
    const float lava_spot[3] = {9.0F, PLAY_GROUND_TOP + 0.05F, 14.0F};
    (void)eng_character_teleport(p->character, lava_spot);
    play_combat_set_lava(c, true);
    int ticks = 0;
    for (; ticks < 900 && !pf->vitals.dead; ticks++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, pf->vitals.dead, "lave : le joueur y meurt (ticks)", (float)ticks);
    for (int i = 0; i < (int)RESPAWN_TICKS + 10 && pf->vitals.dead; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    play_combat_set_lava(c, false); /* le lanceur le constate à l'image suivante */
    step(t, 0U, 0U, 0.0F, 0.0F);
    float feet[3] = {0.0F, 0.0F, 0.0F};
    (void)eng_character_position(p->character, feet);
    check(t, !pf->vitals.dead && hypotf(feet[0] - 3.0F, feet[2] - 14.0F) < 0.5F,
          "lave : il renaît au dernier lieu sûr (écart, m)",
          hypotf(feet[0] - 3.0F, feet[2] - 14.0F));
}

/* Carte du monde : les ennemis évitent les arbres (engine/ai_avoid.h). Un tronc est posé sur le
 * chemin du premier ennemi vers le joueur : il le contourne sans jamais y entrer et rejoint le
 * joueur ; posé de force dans le tronc, il en sort aussitôt et s'en éloigne d'au moins 1 m. */
static void trees_selftest(CombatTest *t, const EngMapZone *zone)
{
    PlayMode *p = &t->play;
    PlayCombat *c = p->combat;
    c->zones_beaten = 0;
    c->invincible = true;
    put(t, 30.0F, -12.0F, 0.0F);
    check(t, play_combat_load_zone(c, zone, 0, 12.0F, -12.0F), "arbres : zone chargée", 1.0F);
    /* le premier ennemi est posé en (19, -12), tourné vers le joueur en (30, -12) : un tronc à
     * mi-chemin, deux autres de part et d'autre un peu plus loin */
    float circles[3][3] = {{24.5F, -12.0F, 0.5F}, {27.0F, -9.8F, 0.4F}, {27.0F, -14.2F, 0.4F}};
    play_combat_set_trees(c, circles, 3U);
    check(t, play_combat_tree_count(c) == 3U, "arbres : trois troncs donnés au combat", 3.0F);
    const uint32_t f = c->group.agents[0].fighter;
    const float r = body_radius(c, f);
    (void)eng_sim_place(&c->sim, f, 19.0F, -12.0F, -1.5707963F);
    const uint32_t steers = c->avoid_steers;
    float closest = INFINITY; /* écart le plus faible de tous les ennemis à un tronc */
    float nearest = INFINITY; /* distance la plus faible du premier ennemi au joueur */
    for (int i = 0; i < 900; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
        for (uint32_t k = 0; k < c->group.count; k++) {
            const uint32_t e = c->group.agents[k].fighter;
            closest = fminf(closest, eng_avoid_gap(c->sim.bodies[e].pos, body_radius(c, e),
                                                   c->trees, c->tree_count, NULL));
        }
        nearest = fminf(nearest, hypotf(c->sim.bodies[f].pos[0] - c->sim.bodies[PLAYER].pos[0],
                                        c->sim.bodies[f].pos[1] - c->sim.bodies[PLAYER].pos[1]));
    }
    check(t, c->avoid_steers > steers, "arbres : direction corrigée près d'un tronc (fois)",
          (float)(c->avoid_steers - steers));
    check(t, closest >= -1e-3F && closest < 1.0F,
          "arbres : passés à moins de 1 m, jamais dans un tronc (m)", closest);
    check(t, nearest < 5.5F, "arbres : le premier ennemi a rejoint le joueur (m)", nearest);
    /* posé de force dans le tronc : il en sort au tick suivant, puis s'éloigne d'au moins 1 m */
    (void)eng_sim_place(&c->sim, f, 24.6F, -12.0F, 0.0F);
    step(t, 0U, 0U, 0.0F, 0.0F);
    float gap = eng_avoid_gap(c->sim.bodies[f].pos, r, c->trees, c->tree_count, NULL);
    check(t, gap >= -1e-3F && c->avoid[f].fleeing, "arbres : sorti du tronc, il s'en éloigne (m)",
          gap);
    int ticks = 0;
    for (; ticks < 300 && c->avoid[f].fleeing; ticks++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    gap = hypotf(c->sim.bodies[f].pos[0] - 24.5F, c->sim.bodies[f].pos[1] + 12.0F) - (0.5F + r);
    check(t, !c->avoid[f].fleeing && gap >= 0.95F,
          "arbres : éloigné d'au moins 1 m après le contact (m)", gap);
    /* sans arbres : plus rien n'est corrigé */
    play_combat_set_trees(c, NULL, 0U);
    const uint32_t after = c->avoid_steers;
    for (int i = 0; i < 60; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, play_combat_tree_count(c) == 0U && c->avoid_steers == after,
          "arbres : liste vidée, plus de correction", (float)(c->avoid_steers - after));
    c->invincible = false;
    (void)play_combat_load_zone(c, NULL, -1, 12.0F, -12.0F);
}

/* Nouvelle partie dans le monde ouvert, loin de toute zone : les combattants rangés hors de la
 * scène ne comptent pas comme des ennemis vaincus. */
/* nombre d'ennemis de la zone encore debout */
static uint32_t standing(const PlayCombat *c)
{
    uint32_t n = 0;
    for (uint32_t f = FIRST_ENEMY; f < c->sim.count; f++) {
        n += (!c->hidden[f] && !c->sim.fighters[f].vitals.dead) ? 1U : 0U;
    }
    return n;
}

/* Grande carte : une meute de petits monstres (demande de Théo, 2026-11-22) : cinq osselets
 * fragiles ; chaque victoire rapporte de l'XP ; la meute vaincue revient 6 s plus tard (un camp :
 * 10 s), autant de fois qu'on veut : de quoi monter de niveau. */
static void pack_selftest(CombatTest *t, const EngMapZone *camp)
{
    PlayCombat *c = t->play.combat;
    EngMapZone pack;
    check(t, eng_mapzones_pack(ENG_BIOME_PLAIN, 0.0, 0.0, &pack), "meute : description", 1.0F);
    put(t, 12.0F, -2.0F, 0.0F);
    check(t, play_combat_load_zone(c, &pack, (int32_t)ENG_MAPZONE_COUNT + 5, 12.0F, -14.0F),
          "meute : zone chargée", 1.0F);
    check(t, standing(c) == 5U && c->group.count == 5U && c->sim.count == FIGHTERS_MAX,
          "meute : cinq osselets debout", (float)standing(c));
    check(t, c->sim.world_ignore == (1U << PLAYER),
          "meute après le dragon : plus personne n'enjambe le décor (masque)",
          (float)c->sim.world_ignore);
    const EngFighter *first = &c->sim.fighters[FIRST_ENEMY];
    check(t, fabsf(first->vitals.max_hp - 12.0F) < 1e-4F && fabsf(first->vitals.hp - 12.0F) < 1e-4F,
          "meute : 12 PV par osselet en plaine (deux coups)", first->vitals.max_hp);
    check(t, c->zone_pack && !c->zone_boss && c->boss.kind == ENG_BOSSKIND_NONE,
          "meute : ni boss ni butin", (float)c->zone_reward);
    check(t,
          strcmp(display_name(c->arch.a[c->group.agents[0].arch].name), "Osselet") == 0 &&
              c->sim.radius[FIRST_ENEMY] < 0.35F,
          "meute : des osselets, plus petits qu'un soldat (rayon, m)", c->sim.radius[FIRST_ENEMY]);
    const uint32_t xp0 = c->progress.total_xp;
    const uint32_t kills0 = c->stats.enemy_kills;
    const uint32_t respawns0 = c->respawns;
    for (uint32_t f = FIRST_ENEMY; f < FIRST_ENEMY + 4U; f++) {
        kill_fighter(c, f);
    }
    for (int i = 0; i < 700; i++) { /* un osselet encore debout : personne ne revient */
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, standing(c) == 1U && c->respawns == respawns0,
          "meute : tant qu'il en reste un, les vaincus ne reviennent pas", (float)standing(c));
    check(t, c->progress.total_xp - xp0 == 40U && c->stats.enemy_kills - kills0 == 4U,
          "meute : 10 XP par osselet vaincu (XP pour quatre)", (float)(c->progress.total_xp - xp0));
    kill_fighter(c, FIRST_ENEMY + 4U);
    for (int i = 0; i < 355; i++) { /* 5,9 s : des ticks comptés ici, pas la constante du jeu */
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, standing(c) == 0U && c->respawns == respawns0,
          "meute vaincue : pas encore revenue après 5,9 s", (float)standing(c));
    for (int i = 0; i < 10; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, standing(c) == 5U && c->respawns == respawns0 + 1U,
          "meute vaincue : les cinq reviennent après 6 s", (float)standing(c));
    check(t, fabsf(c->sim.fighters[FIRST_ENEMY + 4U].vitals.hp - 12.0F) < 1e-4F,
          "meute revenue : PV pleins", c->sim.fighters[FIRST_ENEMY + 4U].vitals.hp);
    for (uint32_t f = FIRST_ENEMY; f < FIRST_ENEMY + 5U; f++) { /* on recommence : encore de l'XP */
        kill_fighter(c, f);
    }
    step(t, 0U, 0U, 0.0F, 0.0F);
    check(t, c->progress.total_xp - xp0 == 100U && c->stats.enemy_kills - kills0 == 10U,
          "meute : deux meutes vaincues, 100 XP (de quoi passer un niveau)",
          (float)(c->progress.total_xp - xp0));
    /* un camp ordinaire (trois ennemis) : les deux places en trop sont rangées, sans XP ; il
     * revient après 10 s, pas 6 */
    const uint32_t xp1 = c->progress.total_xp;
    const bool camp_loaded = play_combat_load_zone(c, camp, 0, 12.0F, -14.0F);
    check(t, camp_loaded && standing(c) == 3U,
          "camp après une meute : trois ennemis, deux places rangées", (float)standing(c));
    for (int i = 0; i < 30; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, c->progress.total_xp == xp1 && !c->zone_pack,
          "camp après une meute : aucune XP offerte par les places rangées",
          (float)(c->progress.total_xp - xp1));
    for (uint32_t f = FIRST_ENEMY; f < FIRST_ENEMY + 3U; f++) {
        kill_fighter(c, f);
    }
    for (int i = 0; i < 590; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, standing(c) == 0U, "camp vaincu : pas revenu après 9,8 s", (float)standing(c));
    for (int i = 0; i < 20; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, standing(c) == 3U, "camp vaincu : revenu après 10 s", (float)standing(c));
}

static void new_game_selftest(CombatTest *t)
{
    PlayCombat *c = t->play.combat;
    (void)play_combat_load_zone(c, NULL, -1, 12.0F, -12.0F);
    const uint32_t kills = c->stats.enemy_kills;
    play_combat_reset(c);
    for (int i = 0; i < 30; i++) {
        step(t, 0U, 0U, 0.0F, 0.0F);
    }
    check(t, c->progress.level == 1U && c->progress.total_xp == 0U,
          "nouvelle partie hors zone : niveau 1, aucune XP offerte (XP)",
          (float)c->progress.total_xp);
    check(t, c->stats.enemy_kills == kills, "nouvelle partie hors zone : aucun ennemi « vaincu »",
          (float)(c->stats.enemy_kills - kills));
}

bool play_combat_bosses_selftest(void)
{
    static CombatTest t;
    memset(&t, 0, sizeof t);
    t.ok = true;
    if (!play_mode_init(&t.play, NULL, 1.0F, false) || !play_mode_enable_combat(&t.play, false)) {
        ENG_LOG_ERROR("AUTOTEST BOSS : ÉCHEC (initialisation ; Jolt requis)");
        play_mode_shutdown(&t.play);
        return false;
    }
    PlayCombat *c = t.play.combat;
    static EngMapZone zones[ENG_MAPZONE_COUNT];
    const EngBiomeMap map = eng_biome_map_default();
    if (!eng_mapzones_build(&map, zones)) {
        play_mode_shutdown(&t.play);
        return false;
    }
    c->world.count = 0; /* pas de cubes : les zones sont en terrain libre */
    c->sim.fighters[PLAYER].vitals.max_hp = 20000.0F;
    c->sim.fighters[PLAYER].vitals.hp = 20000.0F;
    rider_selftest(&t, &zones[2]);
    dragon_selftest(&t, &zones[11]);
    decor_selftest(&t);
    trees_selftest(&t, &zones[0]);
    wade_selftest(&t);
    lava_selftest(&t);
    pack_selftest(&t, &zones[0]);
    new_game_selftest(&t);
    const uint32_t ticks = c->sim.tick;
    play_mode_shutdown(&t.play);
    if (t.ok) {
        ENG_LOG_INFO("AUTOTEST BOSS : OK (%d mesures, %u ticks)", t.checks, ticks);
    } else {
        ENG_LOG_ERROR("AUTOTEST BOSS : ÉCHEC");
    }
    return t.ok;
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
    bool was_walkable[FIGHTERS_MAX] = {false};
    uint32_t start_steep = 0;
    for (uint32_t k = FIRST_ENEMY; k < c->sim.count; k++) {
        start_steep += eng_nav_walkable(&c->nav, c->sim.bodies[k].pos) ? 0U : 1U;
    }
    uint32_t closest_tick = 0;
    float closest = 1e9F;
    for (int i = 0; i < 1800; i++) {
        step(&t, i == 0 ? block : 0U, block, 0.0F, 0.0F);
        for (uint32_t k = FIRST_ENEMY; k < c->sim.count; k++) {
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
    (void)boss_selftest(&t); /* M6-09 : après le camp (ses mesures sont prises) */
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

    /* 1 ter. mode créatif (choisi à la création de la partie) : invincible, vol libre, dégâts du
     * joueur inchangés */
    for (int i = 0; i < 60; i++) {
        step(&t, 0U, 0U, 0.0F, 0.0F);
    }
    float dealt = c->dummy.total_damage;
    for (int i = 0; i < 60; i++) {
        step(&t, i == 0 ? atk : 0U, 0U, 0.0F, 0.0F);
    }
    const float normal_hit = c->dummy.total_damage - dealt;
    play_mode_set_creative(&t.play, true);
    step(&t, 0U, 0U, 0.0F, 0.0F);
    check(&t, t.play.creative && c->invincible, "créatif : activé avec la partie",
          t.play.creative ? 1.0F : 0.0F);
    static EngHud creative_hud; /* le HUD l'annonce en entier (« MODE CR » était coupé avant) */
    mat4 no_view = GLM_MAT4_IDENTITY_INIT;
    play_combat_fill_hud(c, &t.play, no_view, 1280, 720, &creative_hud);
    check(&t,
          strstr(creative_hud.weapon, "MODE CR\xC3\x89"
                                      "ATIF") != NULL,
          "créatif : « MODE CRÉATIF » entier dans le HUD (octets)",
          (float)strlen(creative_hud.weapon));
    dealt = c->dummy.total_damage;
    for (int i = 0; i < 60; i++) {
        step(&t, i == 0 ? atk : 0U, 0U, 0.0F, 0.0F);
    }
    const float creative_hit = c->dummy.total_damage - dealt;
    check(&t, normal_hit > 0.0F && fabsf(creative_hit - normal_hit) < 0.01F,
          "créatif : mêmes dégâts qu'en mode normal (dégâts d'un coup)", creative_hit);
    c->sim.fighters[PLAYER].vitals.hp = 1.0F; /* presque mort : le mode créatif remet les PV */
    step(&t, 0U, 0U, 0.0F, 0.0F);
    check(&t,
          fabsf(c->sim.fighters[PLAYER].vitals.hp - c->sim.fighters[PLAYER].vitals.max_hp) < 0.01F,
          "créatif : invincible (PV remis au maximum)", c->sim.fighters[PLAYER].vitals.hp);
    float low[3];
    float high[3];
    float later[3];
    (void)eng_character_position(t.play.character, low);
    for (int i = 0; i < 60; i++) { /* Espace tenue : monte */
        step(&t, 0U, 1U << ENG_ACTION_JUMP, 0.0F, 0.0F);
    }
    (void)eng_character_position(t.play.character, high);
    check(&t, high[1] - low[1] > 24.5F && high[1] - low[1] < 25.5F,
          "créatif : Espace fait monter de 25 m en 1 s (vitesse par défaut)", high[1] - low[1]);
    for (int i = 0; i < 60; i++) { /* rien : reste en l'air */
        step(&t, 0U, 0U, 0.0F, 0.0F);
    }
    (void)eng_character_position(t.play.character, later);
    check(&t, fabsf(later[1] - high[1]) < 0.05F, "créatif : ne retombe pas (écart en 1 s, m)",
          later[1] - high[1]);
    /* vol horizontal, loin de tout combattant (aucune poussée) : à fond de stick, la vitesse
     * réglée (25 m/s par défaut) ; le déplacement suit la vitesse du personnage, multipliée par
     * vitesse de vol / vitesse de course */
    put(&t, 20.0F, 20.0F, 0.0F);
    float from[3];
    float to[3];
    float walked = 0.0F;
    (void)eng_character_position(t.play.character, from);
    for (int i = 0; i < 60; i++) {
        step(&t, 0U, 0U, 0.0F, 1.0F);
        float v[3];
        eng_locomotion_velocity(&t.play.loco, v);
        walked += hypotf(v[0], v[2]) / 60.0F;
    }
    (void)eng_character_position(t.play.character, to);
    const float flown = hypotf(to[0] - from[0], to[2] - from[2]);
    const float run_speed = t.play.loco.params.max_speed;
    check(&t,
          walked > 2.0F &&
              fabsf(flown - (walked * PLAY_CREATIVE_SPEED_DEFAULT / run_speed)) < 0.03F * flown,
          "créatif : vol à 25 m/s à fond de stick (m en 1 s)", flown);
    /* la molette règle la vitesse de vol, de 0,1 à 1000 m/s ; elle ne recule plus la caméra */
    const float zoom = t.play.orbit.distance;
    wheel(&t, 3.0F);
    check(&t, fabsf(play_mode_creative_speed(&t.play) - (25.0F * 1.25F * 1.25F * 1.25F)) < 0.01F,
          "créatif : trois crans de molette : vitesse x 1,25 trois fois (m/s)",
          play_mode_creative_speed(&t.play));
    check(&t, fabsf(t.play.orbit.distance - zoom) < 1e-5F,
          "créatif : la molette ne recule plus la caméra (m)", t.play.orbit.distance);
    static EngHud speed_hud;
    play_combat_fill_hud(c, &t.play, no_view, 1280, 720, &speed_hud);
    check(&t, strstr(speed_hud.weapon, "49 m/s") != NULL, "créatif : la vitesse est dans le HUD",
          (float)strlen(speed_hud.weapon));
    wheel(&t, 100.0F);
    check(&t, fabsf(play_mode_creative_speed(&t.play) - PLAY_CREATIVE_SPEED_MAX) < 1e-3F,
          "créatif : vitesse plafonnée à 1000 m/s", play_mode_creative_speed(&t.play));
    (void)eng_character_position(t.play.character, from);
    walked = 0.0F;
    for (int i = 0; i < 60; i++) {
        step(&t, 0U, 0U, 0.0F, 1.0F);
        float v[3];
        eng_locomotion_velocity(&t.play.loco, v);
        walked += hypotf(v[0], v[2]) / 60.0F;
    }
    (void)eng_character_position(t.play.character, to);
    const float fast = hypotf(to[0] - from[0], to[2] - from[2]);
    check(&t, fast > 700.0F && fabsf(fast - (walked * 1000.0F / run_speed)) < 0.03F * fast,
          "créatif : à 1000 m/s, près d'un kilomètre en 1 s (m)", fast);
    wheel(&t, -200.0F);
    check(&t, fabsf(play_mode_creative_speed(&t.play) - PLAY_CREATIVE_SPEED_MIN) < 1e-6F,
          "créatif : vitesse plancher à 0,1 m/s", play_mode_creative_speed(&t.play));
    (void)eng_character_position(t.play.character, from);
    for (int i = 0; i < 60; i++) {
        step(&t, 0U, 1U << ENG_ACTION_JUMP, 0.0F, 0.0F);
    }
    (void)eng_character_position(t.play.character, to);
    check(&t, fabsf((to[1] - from[1]) - 0.1F) < 0.01F,
          "créatif : à 0,1 m/s, 10 cm de montée en 1 s", to[1] - from[1]);
    play_mode_set_creative_speed(&t.play, NAN); /* valeur absurde : ignorée */
    check(&t, fabsf(play_mode_creative_speed(&t.play) - PLAY_CREATIVE_SPEED_MIN) < 1e-6F,
          "créatif : une vitesse non finie est ignorée", play_mode_creative_speed(&t.play));
    play_mode_set_creative_speed(&t.play, PLAY_CREATIVE_SPEED_DEFAULT);
    /* retour au-dessus du point de départ, en l'air : repasser en aventure fait retomber */
    put(&t, low[0], low[2], 0.0F);
    const float aloft[3] = {low[0], high[1], low[2]};
    (void)eng_character_teleport(t.play.character, aloft);
    play_mode_set_creative(&t.play, false);
    check(&t, !t.play.creative && !c->invincible,
          "retour en aventure : plus de vol ni d'invincibilité", t.play.creative ? 1.0F : 0.0F);
    const float zoom_before = t.play.orbit.distance; /* en aventure, la molette recule la caméra */
    wheel(&t, -2.0F);
    check(&t,
          t.play.orbit.distance > zoom_before + 0.05F &&
              fabsf(play_mode_creative_speed(&t.play) - PLAY_CREATIVE_SPEED_DEFAULT) < 1e-5F,
          "aventure : la molette règle la caméra, pas la vitesse de vol (m)",
          t.play.orbit.distance);
    wheel(&t, 2.0F);
    for (int i = 0; i < 180; i++) {
        step(&t, 0U, 0U, 0.0F, 0.0F);
    }
    (void)eng_character_position(t.play.character, later);
    check(&t, later[1] < low[1] + 0.2F, "créatif coupé : retombe au sol (m au-dessus du départ)",
          later[1] - low[1]);
    c->sim.fighters[PLAYER].vitals.hp = c->sim.fighters[PLAYER].vitals.max_hp;
    put(&t, 4.0F, 0.3F, 0.0F);

    /* 1 quater. butin des boss : le palier 2 multiplie les dégâts du joueur par 1,8, réduit ceux
     * qu'il reçoit (x 0,65) et ce que sa garde laisse passer (0,1) ; puis retour au départ */
    for (int i = 0; i < 60; i++) {
        step(&t, 0U, 0U, 0.0F, 0.0F);
    }
    check(&t, play_combat_give_gear(c, 2U) && play_combat_gear(c) == 2U, "butin : palier 2 donné",
          (float)play_combat_gear(c));
    check(&t, !play_combat_give_gear(c, 1U), "butin : un palier plus bas ne remplace pas", 0.0F);
    dealt = c->dummy.total_damage;
    for (int i = 0; i < 60; i++) {
        step(&t, i == 0 ? atk : 0U, 0U, 0.0F, 0.0F);
    }
    check(&t, fabsf((c->dummy.total_damage - dealt) - (normal_hit * 1.8F)) < 0.05F,
          "butin : dégâts du joueur x 1,8 (dégâts d'un coup)", c->dummy.total_damage - dealt);
    check(&t,
          fabsf(c->sim.fighters[PLAYER].params.armor_scale - 0.65F) < 1e-6F &&
              fabsf(c->sim.fighters[PLAYER].params.block_multiplier - 0.1F) < 1e-6F,
          "butin : armure x 0,65 et garde 0,1", c->sim.fighters[PLAYER].params.armor_scale);
    c->gear_tier = 0;
    apply_progress(c, false);

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
            for (uint32_t k = FIRST_ENEMY; k < c->sim.count; k++) {
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
