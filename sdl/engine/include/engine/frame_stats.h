#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Statistiques de durée des images (M1-12) : un historique circulaire des dernières images, et
 * leurs agrégats (moyenne, pire, FPS). Aucune allocation, aucun appel système : le module reçoit
 * des durées déjà mesurées (en millisecondes) et ne dépend donc ni de SDL ni de l'horloge, ce qui
 * le rend testable avec des valeurs exactes.
 *
 * Deux durées par image :
 *   - frame_ms : durée totale de l'image (de début d'image à début d'image), attente de
 *     synchronisation verticale comprise : c'est elle qui donne les FPS ;
 *   - cpu_ms   : temps de travail du processeur dans l'image (simulation, jeu, enregistrement des
 *     commandes), SANS l'attente de la présentation. C'est le budget qu'on peut optimiser.
 * Le temps GPU n'est pas mesuré (SDL_GPU n'offre pas de requêtes de temps) : voir docs/debug-ui.md.
 */

#define ENG_FRAME_STATS_HISTORY 120U /* images conservées (2 s à 60 Hz) */

typedef struct EngFrameStats {
    float
        frame_ms[ENG_FRAME_STATS_HISTORY]; /* tampon circulaire ; ordre chronologique : voir _at */
    float cpu_ms[ENG_FRAME_STATS_HISTORY];
    uint32_t next;  /* prochaine case écrite */
    uint32_t count; /* images valides dans l'historique, au plus ENG_FRAME_STATS_HISTORY */
    uint64_t total; /* images enregistrées depuis le début */
} EngFrameStats;

typedef struct EngFrameSummary {
    uint32_t count;
    float avg_frame_ms;
    float max_frame_ms;
    float avg_cpu_ms;
    float max_cpu_ms;
    float fps; /* 1000 / avg_frame_ms ; 0 si l'historique est vide ou la durée nulle */
} EngFrameSummary;

void eng_frame_stats_init(EngFrameStats *stats);

/* Enregistre une image. Une durée négative, infinie ou NaN (horloge défaillante) est ramenée à 0
 * plutôt que d'empoisonner les moyennes. Retourne false dans ce cas, true sinon. */
bool eng_frame_stats_push(EngFrameStats *stats, float frame_ms, float cpu_ms);

/* Moyennes et maxima sur tout l'historique valide. */
EngFrameSummary eng_frame_stats_summary(const EngFrameStats *stats);

/* i-ème image la plus ancienne (0) à la plus récente (count - 1). Hors limites : 0. */
float eng_frame_stats_frame_at(const EngFrameStats *stats, uint32_t i);
float eng_frame_stats_cpu_at(const EngFrameStats *stats, uint32_t i);

/* Recopie les durées dans l'ordre chronologique (pour un graphique) : retourne le nombre de valeurs
 * écrites, au plus `capacity`, en gardant les plus récentes. */
uint32_t eng_frame_stats_copy_frame(const EngFrameStats *stats, float *out, uint32_t capacity);
uint32_t eng_frame_stats_copy_cpu(const EngFrameStats *stats, float *out, uint32_t capacity);
