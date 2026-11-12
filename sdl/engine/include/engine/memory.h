#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

/* Mémoire du moteur : trois arènes aux durées de vie différentes.
 *
 *   permanent : vit toute l'exécution (état du jeu, tables, ressources chargées une fois)
 *   level     : vit le temps d'un niveau/zone ; vidée par eng_memory_reset_level
 *   frame     : mémoire de travail d'UNE image ; vidée automatiquement au début de chaque image
 *
 * Règle : pas de malloc/free dans la boucle de frame. Le compteur d'allocations ci-dessous
 * permet de le vérifier. Il voit les allocations faites par SDL (donc la plupart des
 * allocations de la plateforme) et par le moteur via SDL_malloc ; un appel direct à malloc de la
 * libc appelé directement n'est PAS visible : tools/scripts/lint.sh l'interdit par recherche de
 * texte dans engine/, game/ et launcher/. */

typedef struct EngMemoryConfig {
    size_t permanent_bytes;
    size_t level_bytes;
    size_t frame_bytes;
} EngMemoryConfig;

typedef struct EngMemory {
    EngArena permanent;
    EngArena level;
    EngArena frame;
    uint32_t allocs_at_frame_begin;
    uint64_t frame_alloc_total; /* allocations vues pendant les images, depuis le démarrage */
    uint64_t frame_count;
    bool in_frame;
    bool warned;
} EngMemory;

/* Compteur d'allocations. À installer UNE fois, au tout début de main(). */
void eng_mem_track_install(void);
/* Nombre d'allocations (malloc/calloc/realloc) depuis l'installation. Peut boucler : ne
 * comparer que par soustraction non signée. */
uint32_t eng_mem_alloc_count(void);

bool eng_memory_init(EngMemory *memory, const EngMemoryConfig *config);
void eng_memory_shutdown(EngMemory *memory);

/* À appeler en début et en fin de chaque image. begin vide l'arène « frame » ; end vérifie
 * qu'aucune allocation n'a eu lieu entre les deux. */
void eng_memory_begin_frame(EngMemory *memory);
void eng_memory_end_frame(EngMemory *memory);

void eng_memory_reset_level(EngMemory *memory);

/* Nombre total d'allocations observées pendant les images (objectif : 0). */
uint64_t eng_memory_frame_allocs(const EngMemory *memory);

/* Journalise l'occupation et le pic de chaque arène. */
void eng_memory_log_stats(const EngMemory *memory);
