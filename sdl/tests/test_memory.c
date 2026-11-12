#include <SDL3/SDL.h>

#include "engine/assert.h"
#include "engine/memory.h"
#include "test_util.h"
#include "utest.h"

static const EngMemoryConfig CONFIG = {
    .permanent_bytes = 4096, .level_bytes = 4096, .frame_bytes = 1024};

UTEST(eng_memory, counter_sees_sdl_allocations)
{
    eng_mem_track_install();
    eng_mem_track_install(); /* idempotent */
    uint32_t before = eng_mem_alloc_count();
    void *p = SDL_malloc(16);
    void *q = SDL_calloc(1, 16);
    void *grown = SDL_realloc(p, 32);
    p = grown != NULL ? grown : p;
    EXPECT_EQ(eng_mem_alloc_count() - before, (uint32_t)3);
    SDL_free(p);
    SDL_free(q);
    EXPECT_EQ(eng_mem_alloc_count() - before, (uint32_t)3); /* free ne compte pas */
}

UTEST(eng_memory, frame_arena_is_cleared_each_frame)
{
    EngMemory memory;
    ASSERT_TRUE(eng_memory_init(&memory, &CONFIG));
    eng_memory_begin_frame(&memory);
    void *first = eng_arena_alloc(&memory.frame, 100, 1);
    eng_memory_end_frame(&memory);
    eng_memory_begin_frame(&memory);
    EXPECT_EQ(memory.frame.used, (size_t)0);
    EXPECT_TRUE(eng_arena_alloc(&memory.frame, 100, 1) == first);
    eng_memory_end_frame(&memory);
    EXPECT_EQ(eng_memory_frame_allocs(&memory), (uint64_t)0);
    eng_memory_shutdown(&memory);
}

UTEST(eng_memory, allocation_in_frame_is_counted)
{
    eng_mem_track_install();
    EngMemory memory;
    ASSERT_TRUE(eng_memory_init(&memory, &CONFIG));
    eng_memory_begin_frame(&memory);
    SDL_free(SDL_malloc(8));
    SDL_free(SDL_malloc(8));
    eng_memory_end_frame(&memory);
    EXPECT_EQ(eng_memory_frame_allocs(&memory), (uint64_t)2);
    eng_memory_begin_frame(&memory);
    eng_memory_end_frame(&memory); /* image propre : le total ne bouge pas */
    EXPECT_EQ(eng_memory_frame_allocs(&memory), (uint64_t)2);
    eng_memory_shutdown(&memory);
}

UTEST(eng_memory, level_reset_keeps_permanent)
{
    EngMemory memory;
    ASSERT_TRUE(eng_memory_init(&memory, &CONFIG));
    (void)eng_arena_alloc(&memory.permanent, 64, 1);
    (void)eng_arena_alloc(&memory.level, 64, 1);
    eng_memory_reset_level(&memory);
    EXPECT_EQ(memory.permanent.used, (size_t)64);
    EXPECT_EQ(memory.level.used, (size_t)0);
    eng_memory_shutdown(&memory);
}

UTEST(eng_memory, double_begin_is_reported)
{
    EngMemory memory;
    ASSERT_TRUE(eng_memory_init(&memory, &CONFIG));
    eng_assert_set_break(false);
    uint32_t failures = eng_assert_failure_count();
    eng_memory_begin_frame(&memory);
    eng_memory_begin_frame(&memory);
    eng_memory_end_frame(&memory);
    eng_assert_set_break(true);
#if ENG_ASSERTS_ENABLED
    EXPECT_EQ(eng_assert_failure_count() - failures, (uint32_t)1);
#else
    (void)failures;
#endif
    eng_memory_shutdown(&memory);
}
