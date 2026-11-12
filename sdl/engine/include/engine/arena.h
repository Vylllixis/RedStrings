#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Arène linéaire : on réserve un gros bloc une fois, on l'utilise en avançant un curseur,
 * et on libère TOUT d'un coup (reset) ou jusqu'à un repère (marqueur). Pas de libération
 * individuelle.
 *
 * Sous ASan, la zone non allouée est « empoisonnée » : lire ou écrire après un reset, ou
 * déborder d'une allocation, est détecté immédiatement. */

typedef struct EngArena {
    uint8_t *base;
    size_t capacity;
    size_t used;
    size_t peak;      /* plus haut niveau d'occupation atteint (survit aux reset) */
    const char *name; /* pour les messages ; doit rester valide (littéral) */
    bool owns_memory;
} EngArena;

typedef struct EngArenaMark {
    size_t used;
} EngArenaMark;

/* Réserve `capacity` octets. false si la mémoire n'est pas disponible. */
bool eng_arena_init(EngArena *arena, const char *name, size_t capacity);

/* Utilise un tampon fourni par l'appelant (non libéré par l'arène). */
void eng_arena_init_buffer(EngArena *arena, const char *name, void *buffer, size_t capacity);

/* Libère la mémoire réservée par eng_arena_init. Sans effet sur une arène déjà arrêtée. */
void eng_arena_shutdown(EngArena *arena);

/* `align` doit être une puissance de deux. Retournent NULL (et journalisent) si l'arène est
 * pleine, si `align` est invalide ou si count * elem_size déborde. */
void *eng_arena_alloc(EngArena *arena, size_t size, size_t align);
void *eng_arena_alloc_zero(EngArena *arena, size_t size, size_t align);
void *eng_arena_alloc_array(EngArena *arena, size_t elem_size, size_t count, size_t align);

#define ENG_ARENA_NEW(arena, Type)                                                                 \
    ((Type *)eng_arena_alloc_zero((arena), sizeof(Type), _Alignof(Type)))
#define ENG_ARENA_NEW_ARRAY(arena, Type, count)                                                    \
    ((Type *)eng_arena_alloc_array((arena), sizeof(Type), (count), _Alignof(Type)))

EngArenaMark eng_arena_mark(const EngArena *arena);
/* Libère tout ce qui a été alloué depuis le marqueur. */
void eng_arena_reset_to(EngArena *arena, EngArenaMark mark);
void eng_arena_reset(EngArena *arena);

size_t eng_arena_remaining(const EngArena *arena);
