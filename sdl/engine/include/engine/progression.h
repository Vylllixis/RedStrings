#ifndef ENG_PROGRESSION_H
#define ENG_PROGRESSION_H

/* Progression du joueur (M8-08) : expérience, niveaux, statistiques dérivées.
 *
 *   XP pour passer du niveau L au niveau L+1 : need(L) = 100 + 50 (L - 1)   (100, 150, 200...)
 *   XP d'un ennemi tué : max(10, arrondi(PV max / 2))  (sbire 60 PV -> 30 XP)
 *   au niveau L : PV max = 100 + 10 (L - 1), endurance max = 100 + 5 (L - 1),
 *                 dégâts infligés x (1 + 0,05 (L - 1))
 * Niveau maximal ENG_PROG_MAX_LEVEL : l'XP au-delà est comptée dans `total_xp` mais ne fait plus
 * monter. Fonctions pures, sans état global (tests/test_progression.c). */

#include <stdbool.h>
#include <stdint.h>

#define ENG_PROG_MAX_LEVEL 20U

typedef struct EngProgress {
    uint32_t level;    /* [1, ENG_PROG_MAX_LEVEL] */
    uint32_t xp;       /* XP acquise dans le niveau courant, < need(level) (0 au niveau max) */
    uint32_t total_xp; /* depuis le début de la partie */
} EngProgress;

typedef struct EngProgressStats {
    float hp_max;
    float stamina_max;
    float damage_scale;
} EngProgressStats;

void eng_prog_init(EngProgress *p);

/* XP nécessaire pour quitter le niveau `level` (0 au niveau max ou hors bornes). */
uint32_t eng_prog_need(uint32_t level);

/* Ajoute de l'XP ; retourne le nombre de niveaux gagnés. */
uint32_t eng_prog_add_xp(EngProgress *p, uint32_t amount);

/* XP rapportée par un ennemi de ce PV max (valeur non finie ou négative : 10). */
uint32_t eng_prog_kill_xp(float enemy_hp_max);

EngProgressStats eng_prog_stats(const EngProgress *p);

/* Cohérent (niveau dans les bornes, xp < need, total >= xp) : sert à refuser une sauvegarde
 * abîmée. */
bool eng_prog_valid(const EngProgress *p);

#endif
