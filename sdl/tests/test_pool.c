#include "engine/arena.h"
#include "engine/pool.h"
#include "utest.h"

typedef struct Enemy {
    int hp;
    float x;
} Enemy;

UTEST(eng_pool, alloc_get_free)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 4));
    EngHandle h;
    Enemy *e = eng_pool_alloc(&pool, &h);
    ASSERT_TRUE(e != NULL);
    EXPECT_EQ(e->hp, 0); /* objet mis à zéro */
    e->hp = 10;
    EXPECT_TRUE(eng_pool_get(&pool, h) == e);
    EXPECT_EQ(eng_pool_count(&pool), (uint32_t)1);
    EXPECT_TRUE(eng_pool_free(&pool, h));
    EXPECT_EQ(eng_pool_count(&pool), (uint32_t)0);
    eng_pool_shutdown(&pool);
}

/* Critère d'acceptation : un handle périmé est détecté. */
UTEST(eng_pool, stale_handle_is_detected_after_slot_reuse)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 4));
    EngHandle old_handle;
    Enemy *first = eng_pool_alloc(&pool, &old_handle);
    first->hp = 111;
    ASSERT_TRUE(eng_pool_free(&pool, old_handle));
    EXPECT_TRUE(eng_pool_get(&pool, old_handle) == NULL); /* périmé dès la libération */

    EngHandle new_handle;
    Enemy *second = eng_pool_alloc(&pool, &new_handle);
    ASSERT_TRUE(second != NULL);
    EXPECT_EQ(new_handle.index, old_handle.index); /* la case est réutilisée... */
    EXPECT_TRUE(new_handle.generation != old_handle.generation);
    EXPECT_TRUE(eng_pool_get(&pool, old_handle) == NULL); /* ...mais l'ancien handle reste refusé */
    EXPECT_TRUE(eng_pool_get(&pool, new_handle) == second);
    EXPECT_FALSE(eng_pool_is_valid(&pool, old_handle));
    eng_pool_shutdown(&pool);
}

UTEST(eng_pool, double_free_is_refused)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 4));
    EngHandle h;
    (void)eng_pool_alloc(&pool, &h);
    EXPECT_TRUE(eng_pool_free(&pool, h));
    EXPECT_FALSE(eng_pool_free(&pool, h));
    EXPECT_EQ(eng_pool_count(&pool),
              (uint32_t)0); /* le compteur n'a pas été décrémenté deux fois */
    eng_pool_shutdown(&pool);
}

UTEST(eng_pool, null_and_out_of_range_handles_are_invalid)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 4));
    (void)eng_pool_alloc(&pool, &(EngHandle){0});
    EXPECT_TRUE(eng_pool_get(&pool, ENG_HANDLE_NULL) == NULL);
    EXPECT_TRUE(eng_pool_get(&pool, (EngHandle){99, 1}) == NULL);
    EXPECT_FALSE(eng_pool_free(&pool, ENG_HANDLE_NULL));
    EXPECT_FALSE(eng_pool_free(&pool, (EngHandle){99, 1}));
    eng_pool_shutdown(&pool);
}

UTEST(eng_pool, full_pool_returns_null_then_recovers)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 3));
    EngHandle h[3];
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_pool_alloc(&pool, &h[i]) != NULL);
    }
    EXPECT_TRUE(eng_pool_alloc(&pool, NULL) == NULL);
    ASSERT_TRUE(eng_pool_free(&pool, h[1]));
    EXPECT_TRUE(eng_pool_alloc(&pool, NULL) != NULL);
    eng_pool_shutdown(&pool);
}

UTEST(eng_pool, objects_do_not_overlap)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 100));
    EngHandle h[100];
    for (int i = 0; i < 100; i++) {
        Enemy *e = eng_pool_alloc(&pool, &h[i]);
        ASSERT_TRUE(e != NULL);
        e->hp = i;
    }
    for (int i = 0; i < 100; i++) {
        Enemy *e = eng_pool_get(&pool, h[i]);
        ASSERT_TRUE(e != NULL);
        ASSERT_EQ(e->hp, i);
    }
    eng_pool_shutdown(&pool);
}

UTEST(eng_pool, iteration_visits_only_live_objects)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 8));
    EngHandle h[5];
    for (int i = 0; i < 5; i++) {
        Enemy *e = eng_pool_alloc(&pool, &h[i]);
        e->hp = i;
    }
    (void)eng_pool_free(&pool, h[1]);
    (void)eng_pool_free(&pool, h[3]);

    int sum = 0;
    int visited = 0;
    uint32_t cursor = 0;
    EngHandle it;
    while (eng_pool_next(&pool, &cursor, &it)) {
        sum += ((Enemy *)eng_pool_get(&pool, it))->hp;
        visited++;
    }
    EXPECT_EQ(visited, 3);
    EXPECT_EQ(sum, 0 + 2 + 4);
    eng_pool_shutdown(&pool);
}

UTEST(eng_pool, arena_mode)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", (size_t)16 * 1024));
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, &arena, sizeof(Enemy), 10));
    EngHandle h;
    ASSERT_TRUE(eng_pool_alloc(&pool, &h) != NULL);
    EXPECT_TRUE(eng_pool_get(&pool, h) != NULL);
    eng_pool_shutdown(&pool); /* ne libère rien : mémoire d'arène */
    eng_arena_shutdown(&arena);
}

UTEST(eng_pool, invalid_parameters_are_rejected)
{
    EngPool pool;
    EXPECT_FALSE(eng_pool_init(&pool, NULL, 0, 4));
    EXPECT_FALSE(eng_pool_init(&pool, NULL, 8, 0));
    EXPECT_FALSE(eng_pool_init(&pool, NULL, SIZE_MAX / 2, 1000));
}

UTEST(eng_pool, generation_wraps_without_reaching_zero)
{
    EngPool pool;
    ASSERT_TRUE(eng_pool_init(&pool, NULL, sizeof(Enemy), 1));
    pool.generations[0] = UINT32_MAX; /* on force la fin de la plage */
    EngHandle h;
    ASSERT_TRUE(eng_pool_alloc(&pool, &h) != NULL);
    EXPECT_EQ(h.generation, UINT32_MAX);
    ASSERT_TRUE(eng_pool_free(&pool, h));
    EXPECT_EQ(pool.generations[0], (uint32_t)1); /* saute 0 */
    EXPECT_TRUE(eng_pool_get(&pool, h) == NULL);
    eng_pool_shutdown(&pool);
}
