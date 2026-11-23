#pragma once

#include <stdbool.h>

/* Benchmark rejouable de la traversée du monde ouvert (M7-14, --world-bench [km]) : streaming des
 * cellules, origine flottante, fenêtre de navigation de l'IA et file d'uploads, à 60 images/s, sans
 * fenêtre. Mesure le temps CPU du thread principal par image (moyenne, p99, pire), la mémoire, les
 * rechargements. Journalise un rapport et l'écrit dans `report_path` (NULL : journal seulement).
 * Retourne false si une image dépasse 25 ms ou si un trou apparaît sous le joueur. */
bool world_bench_run(double kilometres, double speed_mps, const char *report_path);
