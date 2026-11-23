#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Boucle à pas de temps fixe (« fix your timestep »).
 *
 * La simulation avance par pas identiques (60 Hz par défaut), quel que soit le rythme de
 * l'affichage : à 30, 60 ou 144 images par seconde, après la même durée réelle, le même nombre
 * de pas a été exécuté, donc le jeu est dans le même état. Le rendu, lui, se dessine à chaque
 * image en interpolant entre deux états de simulation avec `alpha`.
 *
 *     eng_clock_tick(&clock);
 *     uint32_t steps = eng_loop_advance(&loop, clock.dt_counter);
 *     for (uint32_t i = 0; i < steps; i++) { simulate(loop.tick_seconds); }
 *     render(loop.alpha);    // 0 <= alpha < 1 : avancement entre l'avant-dernier et le dernier pas
 *
 * Le temps est accumulé en ENTIERS (ticks du compteur de performance multipliés par la
 * fréquence de simulation) : pas de dérive d'arrondi, résultat déterministe.
 *
 * Protection contre la « spirale de la mort » : une image qui dure plus de max_frame_seconds
 * (jeu arrêté dans le débogueur, fenêtre déplacée, disque lent...) est ramenée à cette durée.
 * Le jeu ne rattrape donc jamais plus de max_frame_seconds de retard : il ralentit au lieu de
 * s'emballer. */

typedef struct EngLoop {
    uint64_t frequency;      /* ticks du compteur par seconde */
    uint32_t tick_hz;        /* pas de simulation par seconde */
    uint64_t max_delta;      /* plafond d'une image, en ticks du compteur */
    uint64_t accumulator;    /* temps non consommé, en ticks_du_compteur * tick_hz */
    uint64_t tick_count;     /* pas de simulation exécutés depuis init */
    uint64_t clamped_frames; /* images dont la durée a été plafonnée */
    double tick_seconds;     /* durée d'un pas : 1 / tick_hz */
    double alpha;            /* fraction de pas restante, pour l'interpolation du rendu */
} EngLoop;

/* false si un paramètre est invalide (fréquence nulle, tick_hz nul, plafond <= 0). */
bool eng_loop_init(EngLoop *loop, uint64_t counter_frequency, uint32_t tick_hz,
                   double max_frame_seconds);

/* Consomme la durée de l'image (en ticks du compteur) et retourne le nombre de pas de
 * simulation à exécuter maintenant. Met à jour tick_count et alpha. */
uint32_t eng_loop_advance(EngLoop *loop, uint64_t frame_delta_counter);
