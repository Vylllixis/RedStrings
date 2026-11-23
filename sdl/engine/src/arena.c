#include "engine/arena.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "engine/assert.h"
#include "engine/log.h"

/* Empoisonnement ASan : on déclare les deux fonctions publiques d'ASan nous-mêmes plutôt que
 * d'inclure <sanitizer/asan_interface.h> (absent de certaines installations de clang). */
#if defined(__SANITIZE_ADDRESS__)
#define ENG_HAS_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ENG_HAS_ASAN 1
#endif
#endif

#ifdef ENG_HAS_ASAN
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#define ARENA_POISON(p, n) __asan_poison_memory_region((p), (n))
#define ARENA_UNPOISON(p, n) __asan_unpoison_memory_region((p), (n))
#else
#define ARENA_POISON(p, n) ((void)(p), (void)(n))
#define ARENA_UNPOISON(p, n) ((void)(p), (void)(n))
#endif

static bool is_power_of_two(size_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

bool eng_arena_init(EngArena *arena, const char *name, size_t capacity)
{
    *arena = (EngArena){0};
    arena->base = SDL_malloc(capacity);
    if (arena->base == NULL) {
        ENG_LOG_ERROR("arène \"%s\" : impossible de réserver %zu octets", name, capacity);
        return false;
    }
    arena->capacity = capacity;
    arena->name = name;
    arena->owns_memory = true;
    ARENA_POISON(arena->base, capacity);
    return true;
}

void eng_arena_init_buffer(EngArena *arena, const char *name, void *buffer, size_t capacity)
{
    *arena = (EngArena){0};
    arena->base = buffer;
    arena->capacity = capacity;
    arena->name = name;
    ARENA_POISON(arena->base, capacity);
}

void eng_arena_shutdown(EngArena *arena)
{
    if (arena->base == NULL) {
        return;
    }
    /* Rendre la mémoire « propre » avant de la restituer : sinon ASan garderait ces octets
     * empoisonnés pour la prochaine allocation qui réutilise la zone. */
    ARENA_UNPOISON(arena->base, arena->capacity);
    if (arena->owns_memory) {
        SDL_free(arena->base);
    }
    *arena = (EngArena){0};
}

void *eng_arena_alloc(EngArena *arena, size_t size, size_t align)
{
    ENG_ASSERT_MSG(is_power_of_two(align), "alignement %zu invalide (arène \"%s\")", align,
                   arena->name);
    if (!is_power_of_two(align)) {
        return NULL;
    }

    /* Le remplissage se calcule sur l'adresse réelle, mais le pointeur retourné se dérive de
     * `base` par arithmétique de pointeurs (pas de conversion entier -> pointeur). */
    uintptr_t current = (uintptr_t)arena->base + arena->used;
    size_t padding = (size_t)(((current + (align - 1)) & ~(uintptr_t)(align - 1)) - current);
    size_t remaining = arena->capacity - arena->used;

    if (padding > remaining || size > remaining - padding) {
        ENG_LOG_ERROR("arène \"%s\" pleine : %zu octets demandés, %zu disponibles (capacité %zu)",
                      arena->name, size, remaining > padding ? remaining - padding : 0,
                      arena->capacity);
        return NULL;
    }

    arena->used += padding + size;
    if (arena->used > arena->peak) {
        arena->peak = arena->used;
    }
    uint8_t *result = arena->base + arena->used - size;
    ARENA_UNPOISON(result, size);
    return result;
}

void *eng_arena_alloc_zero(EngArena *arena, size_t size, size_t align)
{
    void *memory = eng_arena_alloc(arena, size, align);
    if (memory != NULL) {
        memset(memory, 0, size);
    }
    return memory;
}

void *eng_arena_alloc_array(EngArena *arena, size_t elem_size, size_t count, size_t align)
{
    if (elem_size != 0 && count > SIZE_MAX / elem_size) {
        ENG_LOG_ERROR("arène \"%s\" : %zu éléments de %zu octets débordent de size_t", arena->name,
                      count, elem_size);
        return NULL;
    }
    return eng_arena_alloc_zero(arena, elem_size * count, align);
}

EngArenaMark eng_arena_mark(const EngArena *arena)
{
    return (EngArenaMark){arena->used};
}

void eng_arena_reset_to(EngArena *arena, EngArenaMark mark)
{
    ENG_ASSERT_MSG(mark.used <= arena->used,
                   "marqueur (%zu) au-delà du curseur (%zu), arène \"%s\"", mark.used, arena->used,
                   arena->name);
    if (mark.used > arena->used) {
        return;
    }
    ARENA_POISON(arena->base + mark.used, arena->used - mark.used);
    arena->used = mark.used;
}

void eng_arena_reset(EngArena *arena)
{
    eng_arena_reset_to(arena, (EngArenaMark){0});
}

size_t eng_arena_remaining(const EngArena *arena)
{
    return arena->capacity - arena->used;
}
