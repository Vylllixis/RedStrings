#ifndef ENG_SAVE_GAME_H
#define ENG_SAVE_GAME_H

/* Sauvegarde et chargement (M8-05) : format binaire « RSSV » versionné, en sections, avec somme de
 * contrôle et migration des anciennes versions.
 *
 *   en-tête : "RSSV" | version u32 | nombre de sections u32 | FNV-1a 32 du reste u32
 *   section : étiquette 4 octets | taille u32 | contenu
 *
 * Tout est petit-boutiste. Une section inconnue est IGNORÉE (une version plus récente peut ajouter
 * des sections sans casser une ancienne version du jeu) ; une section connue trop courte ou une
 * somme fausse fait refuser le fichier entier (rien n'est appliqué à moitié).
 *
 * Versions :
 *   v1 : PLYR (position, cap, PV, endurance), WRLD (état des cellules).
 *   v2 : PLYR gagne posture, PV max et temps de jeu ; nouvelle section ENMY (ennemis du camp).
 * Un fichier v1 est relu en v2 : les champs absents prennent leur valeur par défaut (posture
 * pleine, PV max 100, temps 0, ennemis « non sauvegardés » = état initial du camp).
 * Section facultative TIME (M8-07, sans changer de version : une section inconnue est ignorée) :
 * heure du cycle jour / nuit (f64). Absente : has_time = false.
 * Section facultative PROG (M8-08) : niveau, XP du niveau, XP totale (u32). Absente :
 * has_progress = false ; incohérente (eng_prog_valid) : fichier refusé.
 * Section facultative EQUI (M8-08) : armes possédées (u32, masque) et arme en main (u32). Absente :
 * has_equipment = false (épée et bouclier) ; incohérente (eng_equip_valid) : fichier refusé.
 * Section facultative META (2026-11-22, gestion des parties) : nom de la partie et lieu de la
 * sauvegarde (longueur u32 puis octets UTF-8, sans zéro final), date de la sauvegarde (secondes
 * depuis 1970, u64). Absente : has_meta = false ; un texte trop long : fichier refusé. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/equipment.h"
#include "engine/progression.h"
#include "engine/world_state.h"

#define ENG_SAVE_VERSION 2U
#define ENG_SAVE_MAX_ENEMIES 16U
#define ENG_SAVE_NAME_MAX 32U  /* octets, zéro final compris */
#define ENG_SAVE_PLACE_MAX 32U /* idem */

typedef struct EngSaveEnemy {
    bool alive;
    float hp;
    float pos[2];
    float yaw;
} EngSaveEnemy;

typedef struct EngSaveGame {
    uint32_t version_read; /* version du fichier relu (0 : neuf) */
    double player_pos[3];  /* monde */
    float player_yaw;
    float hp;
    float hp_max;
    float stamina;
    float posture;
    double play_time;     /* secondes */
    bool has_enemies;     /* false : section absente (fichier v1) */
    bool has_time;        /* section TIME présente */
    double hour;          /* heure du cycle jour / nuit, [0, 24[ */
    bool has_progress;    /* section PROG présente */
    EngProgress progress; /* niveau et XP du joueur */
    bool has_equipment;   /* section EQUI présente */
    EngEquipment equipment;
    uint32_t enemy_count;
    EngSaveEnemy enemies[ENG_SAVE_MAX_ENEMIES];
    bool has_gear;                  /* section GEAR (grande carte) */
    uint32_t gear_tier;             /* palier d'équipement, 0 à 4 (engine/gear.h) */
    uint32_t bosses_beaten;         /* bit i : boss de la zone i vaincu (engine/map_zones.h) */
    bool has_meta;                  /* section META présente */
    char name[ENG_SAVE_NAME_MAX];   /* nom de la partie (UTF-8) */
    char place[ENG_SAVE_PLACE_MAX]; /* lieu de la sauvegarde (« Plaine ») ; vide : inconnu */
    int64_t saved_at;               /* date de la sauvegarde, secondes depuis 1970 (0 : inconnue) */
} EngSaveGame;

void eng_save_defaults(EngSaveGame *s);

/* Écrit la version courante ; retourne la taille, 0 si `capacity` ne suffit pas. `world` peut être
 * NULL (aucune cellule modifiée). */
size_t eng_save_encode(const EngSaveGame *s, const EngWorldState *world, void *out,
                       size_t capacity);

/* Écrit l'ANCIEN format v1 (pour les tests de migration et les outils). */
size_t eng_save_encode_v1(const EngSaveGame *s, const EngWorldState *world, void *out,
                          size_t capacity);

/* Relit v1 ou v2. `world` peut être NULL (section ignorée). Sur erreur : false, message dans `err`,
 * `out` et `world` inchangés. */
bool eng_save_decode(const void *data, size_t size, EngSaveGame *out, EngWorldState *world,
                     char *err, size_t err_capacity);

/* Fichiers : écriture atomique (fichier temporaire puis renommage : une coupure pendant
 * l'enregistrement laisse l'ancienne sauvegarde intacte). */
bool eng_save_write_file(const char *path, const EngSaveGame *s, const EngWorldState *world);
bool eng_save_read_file(const char *path, EngSaveGame *out, EngWorldState *world, char *err,
                        size_t err_capacity);

#endif
