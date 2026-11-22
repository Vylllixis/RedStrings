#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "engine/arena.h"
#include "engine/assert.h"

/* Tableau dynamique générique (macros).
 *
 *     ENG_ARRAY(int) values;
 *     ENG_ARRAY_INIT(values, NULL);            // NULL : tas ; sinon : arène
 *     ENG_ARRAY_PUSH(values, 42);              // true si réussi
 *     int first = values.data[0];
 *     ENG_ARRAY_FREE(values);
 *
 * Les macros évaluent plusieurs fois leur argument `a` : passer un nom de variable ou un champ,
 * pas une expression avec effet de bord.
 *
 * Mode tas   : la croissance utilise SDL_realloc ; ENG_ARRAY_FREE libère.
 * Mode arène : la croissance prend un NOUVEAU bloc dans l'arène et recopie ; l'ancien reste
 *              perdu jusqu'au reset de l'arène. Si la taille finale est connue, appeler
 *              ENG_ARRAY_RESERVE une fois au début.
 * Un pointeur obtenu dans `data` est invalidé par toute insertion (le tableau peut bouger). */

typedef struct EngArrayRaw {
    void *data;
    size_t count;
    size_t capacity;
    EngArena *arena;
} EngArrayRaw;

// NOLINTBEGIN(bugprone-macro-parentheses): T est un nom de type, il ne peut pas être parenthésé
#define ENG_ARRAY(T)                                                                               \
    union {                                                                                        \
        struct {                                                                                   \
            T *data;                                                                               \
            size_t count;                                                                          \
            size_t capacity;                                                                       \
            EngArena *arena;                                                                       \
        };                                                                                         \
        EngArrayRaw raw;                                                                           \
    }
// NOLINTEND(bugprone-macro-parentheses)

bool eng_array_reserve_raw(EngArrayRaw *raw, size_t elem_size, size_t min_capacity);
void eng_array_free_raw(EngArrayRaw *raw);

#define ENG_ARRAY_INIT(a, arena_) ((a).raw = (EngArrayRaw){.arena = (arena_)})
#define ENG_ARRAY_RESERVE(a, n) eng_array_reserve_raw(&(a).raw, sizeof *(a).data, (n))
#define ENG_ARRAY_PUSH(a, value)                                                                   \
    (ENG_ARRAY_RESERVE((a), (a).count + 1) ? ((a).data[(a).count++] = (value), true) : false)
#define ENG_ARRAY_CLEAR(a) ((a).count = 0)
#define ENG_ARRAY_FREE(a) eng_array_free_raw(&(a).raw)

/* Précondition : le tableau n'est pas vide. */
#define ENG_ARRAY_POP(a) ((a).data[--(a).count])

/* Supprime l'élément i en y copiant le dernier : O(1), ne conserve PAS l'ordre. */
#define ENG_ARRAY_REMOVE_SWAP(a, i)                                                                \
    do {                                                                                           \
        ENG_ASSERT_MSG((size_t)(i) < (a).count, "index %zu hors tableau (%zu éléments)",           \
                       (size_t)(i), (a).count);                                                    \
        if ((size_t)(i) < (a).count) {                                                             \
            (a).data[(i)] = (a).data[(a).count - 1];                                               \
            (a).count--;                                                                           \
        }                                                                                          \
    } while (0)
