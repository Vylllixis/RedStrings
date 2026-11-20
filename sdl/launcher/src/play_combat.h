#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/debug_lines.h"
#include "engine/game_menu.h"
#include "engine/input_state.h"
#include "engine/math.h"
#include "engine/save_game.h"
#include "engine/map_zones.h"
#include "engine/ui_game.h"
#include "avatar.h"

/* Combat dans le mode jouable (raccord M4 + M5 + M6, option --play --combat).
 *
 * Branche sur le personnage du mode jouable : la simulation de combat (M5), un mannequin
 * d'entraînement, un petit camp de 3 ennemis pilotés par l'IA (M6), le verrouillage de cible, les
 * particules et (en option) le son. Le personnage physique (Jolt) reste l'autorité de position du
 * joueur ; la simulation lui renvoie seulement les déplacements de combat (esquive, recul,
 * séparation) et dit quand il ne peut pas marcher (attaque, esquive, réaction).
 *
 * Données : assets/combat/attacks.txt (rechargé à chaud) et assets/combat/enemies.txt. Si les
 * fichiers manquent, des valeurs intégrées identiques sont utilisées.
 *
 * Dessin (fil de fer) : hurtboxes par zone (gris : intouchable), armes actives en rouge, barres de
 * PV (vert) et de posture (jaune), cible verrouillée (cercle magenta), cônes de vision et jetons
 * d'attaque des ennemis, particules d'impact. */

typedef struct PlayCombat PlayCombat;

typedef struct PlayCombatStep {
    bool locked;   /* le joueur ne peut pas marcher ce tick */
    float push[2]; /* déplacement (x, z) imposé par le combat ce tick, en mètres */
    bool face;     /* `facing` doit être appliqué (attaque vers la cible verrouillée) */
    float facing;  /* lacet voulu */
} PlayCombatStep;

/* `audio` : ouvre le périphérique audio (sinon silencieux). `arena_obstacles` : les ennemis
 * contournent les cubes et murs de l'arène de M4 (false sur le terrain : pas d'obstacle).
 * Instance unique (stockage statique). */
/* `boss_arena` : avec l'arène murée et le boss (M6-09 ; terrain, pas le monde ouvert). */
PlayCombat *play_combat_create(bool audio, bool arena_obstacles, bool boss_arena);

/* M7 : décale TOUTE la simulation de (dx, dz) dans le plan (combattants, postes, rondes, chemins,
 * mémoire des ennemis, grille de navigation) : pour poser le camp ailleurs qu'à l'origine, et
 * pour suivre le recentrage de l'origine flottante du monde. */
void play_combat_shift(PlayCombat *c, float dx, float dz);
/* M7 : position (x, z) du poste du premier ennemi du camp ; false sans camp. */
bool play_combat_camp_center(const PlayCombat *c, float out[2]);

/* Grande carte : remplace le camp courant par les ennemis de `zone`, autour de (cx, cz) du repère
 * local : archétypes, PV, dégâts et taille de la zone ; la grille de navigation est reconstruite
 * autour de la zone sur le relief courant. `zone` NULL : aucun ennemi (camp de réveil). Une zone de
 * boss ne réapparaît pas : sa victoire met un butin en attente (play_combat_take_reward). false si
 * un archétype manque ou sur un terrain à arène (M6-09). */
bool play_combat_load_zone(PlayCombat *c, const EngMapZone *zone, int32_t index, float cx,
                           float cz);
/* Un ennemi vivant poursuit ou combat le joueur ? (ne pas changer de zone à ce moment) */
bool play_combat_busy(const PlayCombat *c);
/* Grande carte : les boîtes solides du décor (centre x, z, demi-côtés x, z ; repère local)
 * REMPLACENT les obstacles des ennemis (vue, collision ; le chemin est recalculé au prochain
 * play_combat_load_zone). À appeler avant de charger la zone. */
void play_combat_set_obstacles(PlayCombat *c, float (*boxes)[4], uint32_t count);
/* Le boss de la zone `index` a-t-il été vaincu ? (il ne revient pas ; enregistré dans la
 * sauvegarde, section GEAR) */
bool play_combat_zone_beaten(const PlayCombat *c, int32_t index);
/* Butin des boss : palier d'équipement du joueur (0 à 4, engine/gear.h). Battre le boss d'une zone
 * donne le palier de sa zone s'il est meilleur. */
uint32_t play_combat_gear(const PlayCombat *c);
/* Donne un palier (option --gear, autotest). true s'il a monté. */
bool play_combat_give_gear(PlayCombat *c, uint32_t tier);
/* Mode créatif : le joueur ne perd plus de PV (remis au maximum avant chaque pas de simulation) ;
 * ses attaques et leurs dégâts ne changent pas. */
void play_combat_set_invincible(PlayCombat *c, bool invincible);
void play_combat_destroy(PlayCombat *c);

/* M6-09 : murs de l'arène du boss (terrain seulement), en boîtes au sol : centre x, z et
 * demi-côtés x, z (m). Renvoie leur nombre (0 sans boss, ou si `max` est trop petit). */
uint32_t play_combat_boss_walls(const PlayCombat *c, float out[][4], uint32_t max);
/* M6-09 : un point 4 m devant l'entrée de l'arène (x, 0, z) ; false sans boss. */
bool play_combat_boss_gate(const PlayCombat *c, float out[3]);

/* Un pas fixe. `feet` : pieds du personnage ; `facing` : son lacet ; `move_dir` : direction de
 * marche voulue (x, z), nulle si immobile ; `camera_yaw` : pour le verrouillage. */
void play_combat_tick(PlayCombat *c, const EngInputState *input, const float feet[3], float facing,
                      const float move_dir[2], float camera_yaw, double now_seconds,
                      PlayCombatStep *out);

struct PlayMode;

/* Dessin de debug, posé sur le sol du mode jouable (relief en mode terrain ; la simulation est
 * plane, à y = 0). */
void play_combat_draw(const PlayCombat *c, EngDebugLines *lines, const struct PlayMode *play);

/* Pilote automatique (--combat-autoplay, démonstration sans manette) : écrit dans `in` les
 * entrées d'un joueur qui verrouille l'ennemi vivant le plus proche, s'en approche et frappe
 * (relatives à la caméra `camera_yaw`). */
void play_combat_autopilot(PlayCombat *c, const float feet[3], float camera_yaw, uint32_t tick,
                           EngInputState *in);

/* Terrain (M6-03) : la grille de navigation des ennemis bloque les cellules plus raides que
 * `max_slope_deg` (relief donné par `height`). Retourne le nombre de cellules bloquées. */
uint32_t play_combat_set_relief(PlayCombat *c, float (*height)(void *user, float x, float z),
                                void *user, float max_slope_deg);

/* Nouvelle partie : joueur à neuf, camp et mannequin remis à zéro. */
void play_combat_reset(PlayCombat *c);
/* Sauvegarde (M8-05) : PV, endurance, posture du joueur et état des ennemis du camp. */
void play_combat_snapshot(const PlayCombat *c, EngSaveGame *s);
void play_combat_restore(PlayCombat *c, const EngSaveGame *s);
/* Volumes (général et par bus). */
void play_combat_apply_options(PlayCombat *c, const EngGameOptions *o);
/* Interface (M8-03) : barres, cible verrouillée, nombres de dégâts projetés à l'écran. */
void play_combat_fill_hud(const PlayCombat *c, const struct PlayMode *play, mat4 view_proj,
                          int width, int height, EngHud *hud);

/* ── Personnages animés (M4) ── */

/* Un squelette par combattant (mannequin, grunt, brute, archer) ; false si la bibliothèque n'est
 * pas prête. Dès lors, à chaque tick, la simulation reçoit des hurtboxes qui suivent les os
 * (assets/characters/hurtboxes.txt, M5-05) et la lame tenue en main (M5-06), calculées sur une
 * pose avancée au rythme des ticks (déterministe, indépendante de l'affichage). */
bool play_combat_attach_avatars(PlayCombat *c, AvatarLibrary *lib, const struct PlayMode *play);
/* Complète `d` (état de combat, attaque et son clip, esquive) pour le joueur. */
void play_combat_player_drive(const PlayCombat *c, const AvatarLibrary *lib, double alpha,
                              EngActorDrive *d);
/* Anime les combattants autres que le joueur (une fois par image). */
void play_combat_update_visuals(PlayCombat *c, const struct PlayMode *play, double alpha, float dt);
/* Grande carte : boîtes du cheval ou du dragon du boss spécial (boîte unité -> monde, couleur
 * EngCreatureColor), calculées par play_combat_update_visuals. Renvoie leur nombre. */
uint32_t play_combat_creature(const PlayCombat *c, const mat4 **places, const uint8_t **colors);
/* La cinématique du cavalier est en cours ; `focus` (peut être NULL) : où il est (x, z). */
bool play_combat_cinematic(const PlayCombat *c, float focus[2]);
void play_combat_draw_models(const PlayCombat *c, const AvatarLibrary *lib, EngRenderQueue *queue,
                             mat4 view_proj, const EngLighting *light, const EngModelShadow *shadow,
                             bool shadow_pass);

/* Débogage (F1) : état de chaque combattant (état, attaque, phase et tick, PV, posture,
 * endurance) dans `buf` ; retourne la longueur écrite. */
size_t play_combat_debug_text(const PlayCombat *c, char *buf, size_t cap);
/* M6-10 : sélectionne l'ennemi touché par le rayon (origine et direction dans le MONDE ; `ground`
 * : hauteur du sol sous le joueur, la simulation étant plane à y = 0). Retourne l'agent ou -1. */
int32_t play_combat_pick(PlayCombat *c, const float origin[3], const float dir[3], float ground);
/* Fiche de l'ennemi sélectionné (état, cible, jeton, chemin, perception) ; 0 sans sélection. */
size_t play_combat_ai_text(const PlayCombat *c, char *buf, size_t cap);

/* Mesures pour l'autotest et le journal. */
typedef struct PlayCombatStats {
    uint32_t tick;
    float dummy_damage; /* dégâts reçus par le mannequin */
    uint32_t enemy_kills;
    uint32_t player_hits_taken;
    uint32_t max_attackers; /* maximum observé d'ennemis attaquant en même temps */
    uint32_t enemies_engaged;
    float player_hp;
    int32_t lock_target; /* -1 si aucune */
    uint32_t attacks_version;
    uint32_t audio_callbacks;
    uint32_t level; /* progression (M8-08) */
    uint32_t total_xp;
} PlayCombatStats;

void play_combat_stats(const PlayCombat *c, PlayCombatStats *out);

/* M8-08 : met en main l'arme `weapon` (EngWeaponId) ; `give` : la donne d'abord (option de test
 * --weapon). false si elle n'est pas possédée. */
bool play_combat_equip(PlayCombat *c, uint32_t weapon, bool give);

/* M8-01 : poids de la nuit [0 ; 1] pour l'ambiance (oiseaux le jour, grillons la nuit). */
void play_combat_set_night(PlayCombat *c, float night);

/* M8-08 : objets que le chevalier tient selon l'arme en main (espadon : sans bouclier). */
void play_combat_player_props(const PlayCombat *c, AvatarProp *right, AvatarProp *left);
/* M8-08 : arme en main (EngWeaponId) et armes possédées (masque). */
uint32_t play_combat_weapon(const PlayCombat *c, uint32_t *owned);

/* Autotest sans fenêtre (option --combat-selftest) : nécessite Jolt. Journalise
 * « AUTOTEST COMBAT : OK » ou la première erreur. `rig` (--combat-rig-selftest) : avec les
 * personnages KayKit (partie CPU) : hurtboxes sur les os et lame animée (M5-05, M5-06). */
bool play_combat_selftest(bool rig);
/* Grande carte : les boss spéciaux (cavalier, dragon) sans fenêtre (--boss-selftest). */
bool play_combat_bosses_selftest(void);

/* Autotest M6-03 (option --combat-terrain-selftest FICHIER.hmap) : le camp sur un terrain en
 * relief ; le joueur garde près du camp. Vérifie que les ennemis l'atteignent, ne se tiennent
 * jamais sur une cellule trop raide (grille + relief) et qu'il n'y a jamais plus de 2 attaquants.
 * Journalise « AUTOTEST CAMP TERRAIN : OK ». */
bool play_combat_terrain_selftest(const char *heightmap_path);
