#include <stdlib.h>

#include "engine/arena.h"
#include "engine/map.h"
#include "utest.h"

UTEST(eng_map, put_get_overwrite)
{
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, NULL, sizeof(int), 0));
    int v = 5;
    ASSERT_TRUE(eng_map_put(&map, 42, &v));
    int *got = eng_map_get(&map, 42);
    ASSERT_TRUE(got != NULL);
    EXPECT_EQ(*got, 5);
    v = 9;
    ASSERT_TRUE(eng_map_put(&map, 42, &v));
    EXPECT_EQ(eng_map_count(&map), (size_t)1); /* remplacement, pas doublon */
    EXPECT_EQ(*(int *)eng_map_get(&map, 42), 9);
    EXPECT_TRUE(eng_map_get(&map, 43) == NULL);
    EXPECT_TRUE(eng_map_contains(&map, 42));
    EXPECT_FALSE(eng_map_contains(&map, 43));
    eng_map_shutdown(&map);
}

UTEST(eng_map, key_zero_is_a_valid_key)
{
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, NULL, sizeof(int), 0));
    int v = 1;
    EXPECT_FALSE(eng_map_contains(&map, 0));
    ASSERT_TRUE(eng_map_put(&map, 0, &v));
    EXPECT_TRUE(eng_map_contains(&map, 0));
    eng_map_shutdown(&map);
}

UTEST(eng_map, grows_and_keeps_everything)
{
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, NULL, sizeof(uint64_t), 0));
    for (uint64_t k = 0; k < 10000; k++) {
        uint64_t v = k * 7;
        ASSERT_TRUE(eng_map_put(&map, k, &v));
    }
    EXPECT_EQ(eng_map_count(&map), (size_t)10000);
    EXPECT_TRUE(map.count * 10 <= map.capacity * 7);
    for (uint64_t k = 0; k < 10000; k++) {
        uint64_t *v = eng_map_get(&map, k);
        ASSERT_TRUE(v != NULL);
        ASSERT_EQ(*v, k * 7);
    }
    EXPECT_TRUE(eng_map_get(&map, 10000) == NULL);
    eng_map_shutdown(&map);
}

UTEST(eng_map, remove_keeps_colliding_entries_reachable)
{
    /* 3000 clés puis suppression d'une sur deux : tout ce qui reste doit rester trouvable,
     * ce qui exerce la suppression par décalage arrière dans des grappes denses. */
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, NULL, sizeof(int), 0));
    for (uint64_t k = 0; k < 3000; k++) {
        int v = (int)k;
        ASSERT_TRUE(eng_map_put(&map, k, &v));
    }
    for (uint64_t k = 0; k < 3000; k += 2) {
        ASSERT_TRUE(eng_map_remove(&map, k));
    }
    EXPECT_EQ(eng_map_count(&map), (size_t)1500);
    for (uint64_t k = 0; k < 3000; k++) {
        int *v = eng_map_get(&map, k);
        if (k % 2 == 0) {
            ASSERT_TRUE(v == NULL);
        } else {
            ASSERT_TRUE(v != NULL);
            ASSERT_EQ(*v, (int)k);
        }
    }
    EXPECT_FALSE(eng_map_remove(&map, 0)); /* déjà supprimée */
    eng_map_shutdown(&map);
}

/* Comparaison avec un modèle naïf sur une longue suite d'opérations pseudo-aléatoires. */
UTEST(eng_map, matches_naive_model_under_random_operations)
{
    enum { KEYS = 200, OPS = 50000 };
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, NULL, sizeof(int), 0));
    int model[KEYS];
    bool present[KEYS] = {false};
    srand(12345);
    for (int op = 0; op < OPS; op++) {
        int k = rand() % KEYS;
        int kind = rand() % 3;
        if (kind == 0) {
            int v = rand();
            ASSERT_TRUE(eng_map_put(&map, (uint64_t)k * 1000003u, &v));
            model[k] = v;
            present[k] = true;
        } else if (kind == 1) {
            bool removed = eng_map_remove(&map, (uint64_t)k * 1000003u);
            ASSERT_EQ(removed, present[k]);
            present[k] = false;
        } else {
            int *v = eng_map_get(&map, (uint64_t)k * 1000003u);
            ASSERT_EQ(v != NULL, present[k]);
            if (v != NULL) {
                ASSERT_EQ(*v, model[k]);
            }
        }
    }
    size_t expected = 0;
    for (int k = 0; k < KEYS; k++) {
        expected += present[k] ? 1u : 0u;
    }
    EXPECT_EQ(eng_map_count(&map), expected);
    eng_map_shutdown(&map);
}

UTEST(eng_map, clear_and_iterate)
{
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, NULL, sizeof(int), 0));
    for (uint64_t k = 1; k <= 20; k++) {
        int v = (int)k;
        (void)eng_map_put(&map, k, &v);
    }
    int sum = 0;
    size_t visited = 0;
    size_t cursor = 0;
    uint64_t key;
    void *value;
    while (eng_map_next(&map, &cursor, &key, &value)) {
        EXPECT_EQ(*(int *)value, (int)key);
        sum += *(int *)value;
        visited++;
    }
    EXPECT_EQ(visited, (size_t)20);
    EXPECT_EQ(sum, 210);
    eng_map_clear(&map);
    EXPECT_EQ(eng_map_count(&map), (size_t)0);
    EXPECT_TRUE(eng_map_get(&map, 5) == NULL);
    cursor = 0;
    EXPECT_FALSE(eng_map_next(&map, &cursor, &key, &value));
    eng_map_shutdown(&map);
}

typedef struct Big {
    char name[24];
    double weight;
} Big;

UTEST(eng_map, large_values_and_string_keys)
{
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, NULL, sizeof(Big), 0));
    Big sword = {"epee", 3.5};
    Big shield = {"bouclier", 7.25};
    ASSERT_TRUE(eng_map_put(&map, eng_hash_string("sword"), &sword));
    ASSERT_TRUE(eng_map_put(&map, eng_hash_string("shield"), &shield));
    Big *got = eng_map_get(&map, eng_hash_string("shield"));
    ASSERT_TRUE(got != NULL);
    EXPECT_STREQ(got->name, "bouclier");
    EXPECT_EQ(got->weight, 7.25);
    EXPECT_TRUE(eng_map_get(&map, eng_hash_string("bow")) == NULL);
    EXPECT_TRUE(eng_hash_string("a") != eng_hash_string("b"));
    EXPECT_EQ(eng_hash_string(""), (uint64_t)0xcbf29ce484222325ULL);
    eng_map_shutdown(&map);
}

UTEST(eng_map, arena_mode_and_presized)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", (size_t)256 * 1024));
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, &arena, sizeof(int), 1000));
    size_t used_after_init = arena.used;
    for (uint64_t k = 0; k < 1000; k++) {
        int v = (int)k;
        ASSERT_TRUE(eng_map_put(&map, k, &v));
    }
    EXPECT_EQ(arena.used, used_after_init); /* dimensionnée d'avance : aucun agrandissement */
    for (uint64_t k = 0; k < 1000; k++) {
        ASSERT_EQ(*(int *)eng_map_get(&map, k), (int)k);
    }
    eng_map_shutdown(&map);
    eng_arena_shutdown(&arena);
}

UTEST(eng_map, arena_full_makes_put_fail_without_corruption)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "tiny", 1024));
    EngMap map;
    ASSERT_TRUE(eng_map_init(&map, &arena, sizeof(int), 0));
    uint64_t inserted = 0;
    for (uint64_t k = 0; k < 1000; k++) {
        int v = (int)k;
        if (!eng_map_put(&map, k, &v)) {
            break;
        }
        inserted++;
    }
    EXPECT_TRUE(inserted > 0 && inserted < 1000);
    for (uint64_t k = 0; k < inserted; k++) {
        ASSERT_TRUE(eng_map_get(&map, k) != NULL);
    }
    eng_map_shutdown(&map);
    eng_arena_shutdown(&arena);
}

UTEST(eng_map, invalid_parameters_are_rejected)
{
    EngMap map;
    EXPECT_FALSE(eng_map_init(&map, NULL, 0, 0));
    EXPECT_FALSE(eng_map_init(&map, NULL, 4, SIZE_MAX));
}
