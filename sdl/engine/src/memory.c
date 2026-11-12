#include "engine/memory.h"

#include <SDL3/SDL.h>

#include "engine/assert.h"
#include "engine/log.h"

/* ── Compteur d'allocations : on s'intercale devant l'allocateur de SDL ── */

static SDL_malloc_func g_orig_malloc;
static SDL_calloc_func g_orig_calloc;
static SDL_realloc_func g_orig_realloc;
static SDL_free_func g_orig_free;
static SDL_AtomicU32 g_alloc_count;
static bool g_tracking;

static void *SDLCALL track_malloc(size_t size)
{
    SDL_AddAtomicU32(&g_alloc_count, 1);
    return g_orig_malloc(size);
}

static void *SDLCALL track_calloc(size_t count, size_t size)
{
    SDL_AddAtomicU32(&g_alloc_count, 1);
    return g_orig_calloc(count, size);
}

static void *SDLCALL track_realloc(void *pointer, size_t size)
{
    SDL_AddAtomicU32(&g_alloc_count, 1);
    return g_orig_realloc(pointer, size);
}

static void SDLCALL track_free(void *pointer)
{
    g_orig_free(pointer);
}

void eng_mem_track_install(void)
{
    if (g_tracking) {
        return; /* idempotent : sinon on prendrait nos propres fonctions pour les originales */
    }
    SDL_GetOriginalMemoryFunctions(&g_orig_malloc, &g_orig_calloc, &g_orig_realloc, &g_orig_free);
    if (!SDL_SetMemoryFunctions(track_malloc, track_calloc, track_realloc, track_free)) {
        ENG_LOG_ERROR("compteur d'allocations non installé : %s", SDL_GetError());
        return;
    }
    g_tracking = true;
}

uint32_t eng_mem_alloc_count(void)
{
    return SDL_GetAtomicU32(&g_alloc_count);
}

/* ── Les trois arènes ── */

bool eng_memory_init(EngMemory *memory, const EngMemoryConfig *config)
{
    *memory = (EngMemory){0};
    if (!eng_arena_init(&memory->permanent, "permanent", config->permanent_bytes) ||
        !eng_arena_init(&memory->level, "level", config->level_bytes) ||
        !eng_arena_init(&memory->frame, "frame", config->frame_bytes)) {
        eng_memory_shutdown(memory);
        return false;
    }
    return true;
}

void eng_memory_shutdown(EngMemory *memory)
{
    eng_arena_shutdown(&memory->frame);
    eng_arena_shutdown(&memory->level);
    eng_arena_shutdown(&memory->permanent);
}

void eng_memory_begin_frame(EngMemory *memory)
{
    ENG_ASSERT_MSG(!memory->in_frame, "eng_memory_begin_frame appelé deux fois sans end_frame");
    eng_arena_reset(&memory->frame);
    memory->allocs_at_frame_begin = eng_mem_alloc_count();
    memory->in_frame = true;
}

void eng_memory_end_frame(EngMemory *memory)
{
    ENG_ASSERT_MSG(memory->in_frame, "eng_memory_end_frame appelé sans begin_frame");
    uint32_t allocations = eng_mem_alloc_count() - memory->allocs_at_frame_begin;
    if (allocations != 0) {
        memory->frame_alloc_total += allocations;
        if (!memory->warned) { /* une seule alerte : le total est rappelé à l'arrêt */
            memory->warned = true;
            ENG_LOG_WARN("%u allocation(s) pendant l'image %llu (première occurrence)", allocations,
                         (unsigned long long)memory->frame_count);
        }
    }
    memory->frame_count++;
    memory->in_frame = false;
}

void eng_memory_reset_level(EngMemory *memory)
{
    eng_arena_reset(&memory->level);
}

uint64_t eng_memory_frame_allocs(const EngMemory *memory)
{
    return memory->frame_alloc_total;
}

static void log_arena(const EngArena *arena)
{
    ENG_LOG_INFO("  arène %-10s : %9zu utilisés, pic %9zu, capacité %9zu", arena->name, arena->used,
                 arena->peak, arena->capacity);
}

void eng_memory_log_stats(const EngMemory *memory)
{
    ENG_LOG_INFO("mémoire :");
    log_arena(&memory->permanent);
    log_arena(&memory->level);
    log_arena(&memory->frame);
    ENG_LOG_INFO("allocations dans la boucle de frame : %llu (sur %llu images)",
                 (unsigned long long)memory->frame_alloc_total,
                 (unsigned long long)memory->frame_count);
}
