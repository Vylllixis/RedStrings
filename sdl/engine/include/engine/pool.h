#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

/* Pool d'objets à handles générationnels.
 *
 * Un handle = (index de case, génération). Quand un objet est libéré, la génération de sa case
 * augmente : tout ancien handle vers cette case devient « périmé » et eng_pool_get renvoie NULL
 * au lieu de lire un autre objet qui aurait pris la place. C'est ce qui permet au jeu de garder
 * des références (cible d'un ennemi, ressource...) sans pointeurs qui pendent.
 *
 * La génération 0 n'est jamais utilisée : ENG_HANDLE_NULL (tout à zéro) n'est jamais valide.
 * La capacité est fixe : eng_pool_alloc renvoie NULL quand le pool est plein.
 * Les pointeurs rendus par alloc/get restent valides jusqu'à la libération de l'objet. */

typedef struct EngHandle {
    uint32_t index;
    uint32_t generation;
} EngHandle;

#define ENG_HANDLE_NULL ((EngHandle){0, 0})

typedef struct EngPool {
    uint8_t *items;
    uint32_t *generations;
    uint32_t *next_free;
    uint8_t *alive;
    size_t item_size;
    size_t item_stride;
    uint32_t capacity;
    uint32_t count;
    uint32_t free_head;
    EngArena *arena;
} EngPool;

/* arena == NULL : mémoire sur le tas (libérée par eng_pool_shutdown). */
bool eng_pool_init(EngPool *pool, EngArena *arena, size_t item_size, uint32_t capacity);
void eng_pool_shutdown(EngPool *pool);

/* Réserve un objet (mis à zéro) et son handle. NULL si le pool est plein. */
void *eng_pool_alloc(EngPool *pool, EngHandle *out_handle);

/* NULL si le handle est nul, hors limites ou périmé. */
void *eng_pool_get(const EngPool *pool, EngHandle handle);
bool eng_pool_is_valid(const EngPool *pool, EngHandle handle);

/* false (et avertissement dans le journal) si le handle est invalide ou déjà libéré. */
bool eng_pool_free(EngPool *pool, EngHandle handle);

uint32_t eng_pool_count(const EngPool *pool);

/* Itération : EngHandle h; uint32_t cursor = 0; while (eng_pool_next(&pool, &cursor, &h)) {...}
 * Libérer l'objet courant pendant le parcours est permis. */
bool eng_pool_next(const EngPool *pool, uint32_t *cursor, EngHandle *out_handle);
