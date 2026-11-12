#include "engine/array.h"
#include "engine/arena.h"
#include "engine/assert.h"
#include "utest.h"

UTEST(eng_array, push_grows_and_keeps_values)
{
    ENG_ARRAY(int) values;
    ENG_ARRAY_INIT(values, NULL);
    for (int i = 0; i < 1000; i++) {
        ASSERT_TRUE(ENG_ARRAY_PUSH(values, i * 3));
    }
    EXPECT_EQ(values.count, (size_t)1000);
    EXPECT_TRUE(values.capacity >= 1000);
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ(values.data[i], i * 3);
    }
    ENG_ARRAY_FREE(values);
    EXPECT_TRUE(values.data == NULL);
    EXPECT_EQ(values.count, (size_t)0);
}

UTEST(eng_array, pop_and_clear)
{
    ENG_ARRAY(int) values;
    ENG_ARRAY_INIT(values, NULL);
    (void)ENG_ARRAY_PUSH(values, 1);
    (void)ENG_ARRAY_PUSH(values, 2);
    EXPECT_EQ(ENG_ARRAY_POP(values), 2);
    EXPECT_EQ(values.count, (size_t)1);
    size_t capacity = values.capacity;
    ENG_ARRAY_CLEAR(values);
    EXPECT_EQ(values.count, (size_t)0);
    EXPECT_EQ(values.capacity, capacity); /* clear garde la mémoire */
    ENG_ARRAY_FREE(values);
}

UTEST(eng_array, remove_swap)
{
    ENG_ARRAY(int) values;
    ENG_ARRAY_INIT(values, NULL);
    for (int i = 0; i < 5; i++) {
        (void)ENG_ARRAY_PUSH(values, i);
    }
    ENG_ARRAY_REMOVE_SWAP(values, 1); /* 0 4 2 3 */
    EXPECT_EQ(values.count, (size_t)4);
    EXPECT_EQ(values.data[1], 4);
    ENG_ARRAY_REMOVE_SWAP(values, 3); /* dernier élément */
    EXPECT_EQ(values.count, (size_t)3);
    ENG_ARRAY_FREE(values);
}

UTEST(eng_array, remove_swap_out_of_range_is_reported_and_ignored)
{
    ENG_ARRAY(int) values;
    ENG_ARRAY_INIT(values, NULL);
    (void)ENG_ARRAY_PUSH(values, 7);
    eng_assert_set_break(false);
    ENG_ARRAY_REMOVE_SWAP(values, 5);
    eng_assert_set_break(true);
    EXPECT_EQ(values.count, (size_t)1);
    ENG_ARRAY_FREE(values);
}

typedef struct Vec3 {
    float x, y, z;
} Vec3;

UTEST(eng_array, struct_elements)
{
    ENG_ARRAY(Vec3) points;
    ENG_ARRAY_INIT(points, NULL);
    for (int i = 0; i < 50; i++) {
        ASSERT_TRUE(ENG_ARRAY_PUSH(points, ((Vec3){(float)i, 2.0F, 3.0F})));
    }
    EXPECT_EQ(points.data[49].x, 49.0F);
    EXPECT_EQ(points.data[0].z, 3.0F);
    ENG_ARRAY_FREE(points);
}

UTEST(eng_array, arena_mode_copies_on_growth)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", (size_t)64 * 1024));
    ENG_ARRAY(int) values;
    ENG_ARRAY_INIT(values, &arena);
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(ENG_ARRAY_PUSH(values, i));
    }
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(values.data[i], i);
    }
    EXPECT_TRUE(arena.used >= 100 * sizeof(int));
    ENG_ARRAY_FREE(values); /* ne libère rien : mémoire d'arène */
    eng_arena_shutdown(&arena);
}

UTEST(eng_array, arena_full_makes_push_fail_without_corruption)
{
    uint8_t buffer[256] __attribute__((aligned(16)));
    EngArena arena;
    eng_arena_init_buffer(&arena, "small", buffer, sizeof buffer);
    ENG_ARRAY(int) values;
    ENG_ARRAY_INIT(values, &arena);
    int pushed = 0;
    while (ENG_ARRAY_PUSH(values, pushed)) {
        pushed++;
    }
    EXPECT_TRUE(pushed > 0);
    EXPECT_EQ(values.count, (size_t)pushed);
    for (int i = 0; i < pushed; i++) {
        ASSERT_EQ(values.data[i], i);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_array, reserve_avoids_regrowth)
{
    ENG_ARRAY(int) values;
    ENG_ARRAY_INIT(values, NULL);
    ASSERT_TRUE(ENG_ARRAY_RESERVE(values, 500));
    int *before = values.data;
    for (int i = 0; i < 500; i++) {
        (void)ENG_ARRAY_PUSH(values, i);
    }
    EXPECT_TRUE(values.data == before);
    ENG_ARRAY_FREE(values);
}
