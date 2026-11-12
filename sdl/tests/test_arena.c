#include <stdint.h>

#include "engine/arena.h"
#include "engine/assert.h"
#include "test_util.h"
#include "utest.h"

UTEST(eng_arena, alloc_is_aligned_and_distinct)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 1024));
    uint8_t *a = eng_arena_alloc(&arena, 3, 1);
    uint64_t *b = eng_arena_alloc(&arena, sizeof *b, _Alignof(uint64_t));
    uint8_t *c = eng_arena_alloc(&arena, 1, 64);
    ASSERT_TRUE(a != NULL && b != NULL && c != NULL);
    EXPECT_EQ((uintptr_t)b % _Alignof(uint64_t), (uintptr_t)0);
    EXPECT_EQ((uintptr_t)c % 64, (uintptr_t)0);
    EXPECT_TRUE(a + 3 <= (uint8_t *)b);
    EXPECT_TRUE((uint8_t *)b + sizeof *b <= c);
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, alloc_zero_clears_memory)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 256));
    uint8_t *p = eng_arena_alloc(&arena, 64, 1);
    ASSERT_TRUE(p != NULL);
    memset(p, 0xAB, 64);
    eng_arena_reset(&arena);
    uint8_t *q = eng_arena_alloc_zero(&arena, 64, 1);
    ASSERT_TRUE(q == p);
    for (int i = 0; i < 64; i++) {
        ASSERT_EQ(q[i], 0);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, full_arena_returns_null_and_stays_usable)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 128));
    EXPECT_TRUE(eng_arena_alloc(&arena, 100, 1) != NULL);
    EXPECT_TRUE(eng_arena_alloc(&arena, 100, 1) == NULL);
    EXPECT_TRUE(eng_arena_alloc(&arena, SIZE_MAX, 1) == NULL); /* pas de débordement */
    EXPECT_TRUE(eng_arena_alloc(&arena, 28, 1) != NULL);       /* le reste est utilisable */
    EXPECT_EQ(eng_arena_remaining(&arena), (size_t)0);
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, exact_fit_with_alignment_padding)
{
    uint8_t buffer[64] __attribute__((aligned(64)));
    EngArena arena;
    eng_arena_init_buffer(&arena, "buf", buffer, sizeof buffer);
    EXPECT_TRUE(eng_arena_alloc(&arena, 1, 1) != NULL);
    EXPECT_TRUE(eng_arena_alloc(&arena, 56, 8) != NULL); /* 1 + 7 de remplissage + 56 = 64 */
    EXPECT_EQ(arena.used, (size_t)64);
    EXPECT_TRUE(eng_arena_alloc(&arena, 1, 1) == NULL);
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, array_overflow_is_detected)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 128));
    EXPECT_TRUE(eng_arena_alloc_array(&arena, 16, SIZE_MAX / 8, 8) == NULL);
    EXPECT_EQ(arena.used, (size_t)0);
    int *values = ENG_ARENA_NEW_ARRAY(&arena, int, 10);
    ASSERT_TRUE(values != NULL);
    EXPECT_EQ(values[9], 0);
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, mark_and_reset_to)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 256));
    (void)eng_arena_alloc(&arena, 10, 1);
    EngArenaMark mark = eng_arena_mark(&arena);
    void *first = eng_arena_alloc(&arena, 50, 1);
    EXPECT_EQ(arena.used, (size_t)60);
    eng_arena_reset_to(&arena, mark);
    EXPECT_EQ(arena.used, (size_t)10);
    EXPECT_TRUE(eng_arena_alloc(&arena, 50, 1) == first);
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, peak_survives_reset)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 256));
    (void)eng_arena_alloc(&arena, 100, 1);
    eng_arena_reset(&arena);
    (void)eng_arena_alloc(&arena, 20, 1);
    EXPECT_EQ(arena.peak, (size_t)100);
    EXPECT_EQ(arena.used, (size_t)20);
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, bad_alignment_is_rejected)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 64));
    eng_assert_set_break(false);
    EXPECT_TRUE(eng_arena_alloc(&arena, 8, 3) == NULL);
    EXPECT_TRUE(eng_arena_alloc(&arena, 8, 0) == NULL);
    eng_assert_set_break(true);
    eng_arena_shutdown(&arena);
}

UTEST(eng_arena, shutdown_twice_is_harmless)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 64));
    eng_arena_shutdown(&arena);
    eng_arena_shutdown(&arena);
    EXPECT_TRUE(arena.base == NULL);
}
