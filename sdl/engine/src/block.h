#pragma once

/* Usage interne : bloc de mémoire zéro-initialisé, pris dans une arène si on en fournit une,
 * sur le tas (SDL_calloc) sinon. Les structures de données (tableau, pool, table) s'en servent
 * pour accepter les deux modes. */

#include <SDL3/SDL.h>

#include "engine/arena.h"

#define ENG_BLOCK_ALIGN 16

static inline size_t eng_align_up(size_t value, size_t align)
{
    return (value + (align - 1)) & ~(align - 1);
}

static inline void *eng_block_alloc(EngArena *arena, size_t bytes)
{
    if (arena != NULL) {
        return eng_arena_alloc_zero(arena, bytes, ENG_BLOCK_ALIGN);
    }
    return SDL_calloc(1, bytes);
}

/* Sans effet pour un bloc d'arène : il disparaît avec le reset de l'arène. */
static inline void eng_block_free(EngArena *arena, void *block)
{
    if (arena == NULL) {
        SDL_free(block);
    }
}
