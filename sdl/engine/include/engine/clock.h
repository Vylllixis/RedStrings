#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Horloge de la boucle principale : temps écoulé, delta-time (dt) et compteur d'images.
 * Basée sur SDL_GetPerformanceCounter (résolution de l'ordre de la nanoseconde). Les calculs
 * se font sur des compteurs entiers et ne passent en flottant qu'à la fin, pour ne pas perdre
 * de précision quand le jeu tourne depuis longtemps.
 *
 * Utilisation : eng_clock_init() juste avant la boucle, puis eng_clock_tick() UNE fois par
 * image, au même endroit de la boucle. */

typedef struct EngClock {
    uint64_t frequency; /* ticks du compteur par seconde */
    uint64_t start;
    uint64_t last;
    uint64_t frame;      /* nombre d'appels à tick depuis init */
    double dt;           /* durée de la dernière image, en secondes */
    uint64_t dt_counter; /* la même durée en ticks du compteur (exacte, pour eng_loop) */
    double time;         /* secondes écoulées depuis init */
    /* Statistiques sur une fenêtre d'environ 1 s */
    uint64_t window_start;
    uint64_t window_frames;
    double window_max_dt;
} EngClock;

typedef struct EngClockStats {
    double fps;
    double avg_dt_ms;
    double max_dt_ms;
    uint32_t frames;
} EngClockStats;

void eng_clock_init(EngClock *clock);
void eng_clock_tick(EngClock *clock);

/* Variantes à compteur fourni : pour les tests (temps simulé) et pour M0-16. */
void eng_clock_init_at(EngClock *clock, uint64_t frequency, uint64_t counter);
void eng_clock_tick_at(EngClock *clock, uint64_t counter);

/* Vrai environ une fois par seconde, avec les statistiques de la période écoulée. */
bool eng_clock_take_stats(EngClock *clock, EngClockStats *out);

/* Mesure d'une portion de l'image (M1-12 : temps CPU du panneau de debug) : eng_clock_counter()
 * lit le compteur, eng_clock_ms_between() convertit deux lectures en millisecondes avec la
 * fréquence de `clock`. Un intervalle négatif donne 0. */
uint64_t eng_clock_counter(void);
double eng_clock_ms_between(const EngClock *clock, uint64_t from, uint64_t to);
