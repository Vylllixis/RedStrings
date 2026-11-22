#include <SDL3/SDL.h>
#include <string.h>

#include "engine/asset_cook.h"
#include "utest.h"

/* Cuisson à la volée (M2-06) : une source modifiée lance sa commande, sans bloquer, une à la fois.
 * La commande des tests est `cmake -E copy <source> <cible>` (OW_CMAKE_COMMAND) : elle existe
 * partout où CMake existe. */

#define SRC_A "asset_cook_a.src"
#define DST_A "asset_cook_a.out"
#define SRC_B "asset_cook_b.src"
#define DST_B "asset_cook_b.out"

static const char *const COPY_A[] = {OW_CMAKE_COMMAND, "-E", "copy", SRC_A, DST_A, NULL};
static const char *const COPY_B[] = {OW_CMAKE_COMMAND, "-E", "copy", SRC_B, DST_B, NULL};
static const char *const COPY_MISSING[] = {OW_CMAKE_COMMAND,        "-E",  "copy",
                                           "asset_cook_absent.src", DST_A, NULL};

static double now_seconds(void)
{
    return (double)SDL_GetTicks() / 1000.0;
}

static bool write_text(const char *path, const char *text)
{
    return SDL_SaveFile(path, text, strlen(text));
}

static bool file_is(const char *path, const char *text)
{
    size_t size = 0;
    char *data = SDL_LoadFile(path, &size);
    const bool same = data != NULL && size == strlen(text) && memcmp(data, text, size) == 0;
    SDL_free(data);
    return same;
}

static void clean(void)
{
    SDL_RemovePath(SRC_A);
    SDL_RemovePath(DST_A);
    SDL_RemovePath(SRC_B);
    SDL_RemovePath(DST_B);
}

/* Appelle eng_asset_cook_poll jusqu'à ce que `cook` ne soit plus occupé, ou 10 s. */
static bool settle(EngAssetCook *cook)
{
    const double start = now_seconds();
    while (now_seconds() - start < 10.0) {
        (void)eng_asset_cook_poll(cook, now_seconds());
        if (!eng_asset_cook_busy(cook)) {
            return true;
        }
        SDL_Delay(5);
    }
    return false;
}

UTEST(eng_asset_cook, a_modified_source_runs_its_command)
{
    clean();
    ASSERT_TRUE(write_text(SRC_A, "version 1"));
    const EngCookRule rules[] = {{.name = "a", .source = SRC_A, .argv = COPY_A}};
    EngAssetCook cook;
    ASSERT_TRUE(eng_asset_cook_init(&cook, rules, 1, 0.02));

    /* rien n'a changé : rien ne tourne */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(eng_asset_cook_poll(&cook, now_seconds()), (uint32_t)0);
        SDL_Delay(5);
    }
    EXPECT_FALSE(eng_asset_cook_busy(&cook));
    EXPECT_EQ(cook.succeeded, (uint32_t)0);

    ASSERT_TRUE(write_text(SRC_A, "version 2, plus longue"));
    const double start = now_seconds();
    while (cook.succeeded == 0 && now_seconds() - start < 10.0) {
        (void)eng_asset_cook_poll(&cook, now_seconds());
        SDL_Delay(5);
    }
    EXPECT_EQ(cook.succeeded, (uint32_t)1);
    EXPECT_EQ(cook.failed, (uint32_t)0);
    ASSERT_TRUE(settle(&cook));
    EXPECT_TRUE(file_is(DST_A, "version 2, plus longue"));
    eng_asset_cook_shutdown(&cook);
    clean();
}

UTEST(eng_asset_cook, a_failing_command_is_counted_and_does_not_stop_the_next)
{
    clean();
    ASSERT_TRUE(write_text(SRC_A, "x"));
    const EngCookRule rules[] = {{.name = "absente", .source = SRC_A, .argv = COPY_MISSING}};
    EngAssetCook cook;
    ASSERT_TRUE(eng_asset_cook_init(&cook, rules, 1, 0.02));
    ASSERT_TRUE(write_text(SRC_A, "xy"));
    const double start = now_seconds();
    while (cook.failed == 0 && now_seconds() - start < 10.0) {
        (void)eng_asset_cook_poll(&cook, now_seconds());
        SDL_Delay(5);
    }
    EXPECT_EQ(cook.failed, (uint32_t)1);
    EXPECT_EQ(cook.succeeded, (uint32_t)0);
    EXPECT_TRUE(settle(&cook));
    eng_asset_cook_shutdown(&cook);
    clean();
}

UTEST(eng_asset_cook, two_rules_run_one_at_a_time_and_both_complete)
{
    clean();
    ASSERT_TRUE(write_text(SRC_A, "a1"));
    ASSERT_TRUE(write_text(SRC_B, "b1"));
    const EngCookRule rules[] = {{.name = "a", .source = SRC_A, .argv = COPY_A},
                                 {.name = "b", .source = SRC_B, .argv = COPY_B}};
    EngAssetCook cook;
    ASSERT_TRUE(eng_asset_cook_init(&cook, rules, 2, 0.02));
    ASSERT_TRUE(write_text(SRC_A, "a2 changed"));
    ASSERT_TRUE(write_text(SRC_B, "b2 changed"));
    const double start = now_seconds();
    while (cook.succeeded < 2 && now_seconds() - start < 15.0) {
        (void)eng_asset_cook_poll(&cook, now_seconds());
        SDL_Delay(5);
    }
    EXPECT_EQ(cook.succeeded, (uint32_t)2);
    ASSERT_TRUE(settle(&cook));
    EXPECT_TRUE(file_is(DST_A, "a2 changed"));
    EXPECT_TRUE(file_is(DST_B, "b2 changed"));
    eng_asset_cook_shutdown(&cook);
    clean();
}

UTEST(eng_asset_cook, a_change_during_a_cook_is_not_lost)
{
    clean();
    ASSERT_TRUE(write_text(SRC_A, "v1"));
    const EngCookRule rules[] = {{.name = "a", .source = SRC_A, .argv = COPY_A}};
    EngAssetCook cook;
    ASSERT_TRUE(eng_asset_cook_init(&cook, rules, 1, 0.02));
    ASSERT_TRUE(write_text(SRC_A, "v2 first change"));
    double start = now_seconds();
    while (!eng_asset_cook_busy(&cook) && cook.succeeded == 0 && now_seconds() - start < 10.0) {
        (void)eng_asset_cook_poll(&cook, now_seconds());
        SDL_Delay(2);
    }
    ASSERT_TRUE(write_text(SRC_A, "v3 second change, longer"));
    start = now_seconds();
    while (!file_is(DST_A, "v3 second change, longer") && now_seconds() - start < 15.0) {
        (void)eng_asset_cook_poll(&cook, now_seconds());
        SDL_Delay(5);
    }
    EXPECT_TRUE(file_is(DST_A, "v3 second change, longer"));
    ASSERT_TRUE(settle(&cook));
    eng_asset_cook_shutdown(&cook);
    clean();
}

UTEST(eng_asset_cook, invalid_rules_are_refused)
{
    EngAssetCook cook;
    const EngCookRule no_name = {.source = SRC_A, .argv = COPY_A};
    const EngCookRule no_source = {.name = "n", .argv = COPY_A};
    const EngCookRule no_command = {.name = "n", .source = SRC_A};
    const char *const empty[] = {NULL};
    const EngCookRule empty_command = {.name = "n", .source = SRC_A, .argv = empty};
    EXPECT_FALSE(eng_asset_cook_init(&cook, &no_name, 1, 0.0));
    EXPECT_FALSE(eng_asset_cook_init(&cook, &no_source, 1, 0.0));
    EXPECT_FALSE(eng_asset_cook_init(&cook, &no_command, 1, 0.0));
    EXPECT_FALSE(eng_asset_cook_init(&cook, &empty_command, 1, 0.0));
    EXPECT_FALSE(eng_asset_cook_init(&cook, &no_name, ENG_COOK_MAX_RULES + 1U, 0.0));
    const EngCookRule ok = {.name = "n", .source = SRC_A, .argv = COPY_A};
    EXPECT_TRUE(eng_asset_cook_init(&cook, &ok, 1, 0.0));
    EXPECT_FALSE(eng_asset_cook_busy(&cook));
    eng_asset_cook_shutdown(&cook);
    EXPECT_TRUE(eng_asset_cook_init(&cook, NULL, 0, 0.0)); /* aucune règle : valide, inerte */
    EXPECT_EQ(eng_asset_cook_poll(&cook, 1.0), (uint32_t)0);
    eng_asset_cook_shutdown(&cook);
}

UTEST(eng_asset_cook, shutdown_during_a_cook_leaves_nothing_running)
{
    clean();
    ASSERT_TRUE(write_text(SRC_A, "v1"));
    const EngCookRule rules[] = {{.name = "a", .source = SRC_A, .argv = COPY_A}};
    EngAssetCook cook;
    ASSERT_TRUE(eng_asset_cook_init(&cook, rules, 1, 0.02));
    ASSERT_TRUE(write_text(SRC_A, "v2 changed"));
    const double start = now_seconds();
    while (cook.process == NULL && cook.succeeded == 0 && now_seconds() - start < 10.0) {
        (void)eng_asset_cook_poll(&cook, now_seconds());
        SDL_Delay(2);
    }
    eng_asset_cook_shutdown(&cook); /* tourne encore, ou déjà fini : dans les deux cas, propre */
    EXPECT_TRUE(cook.process == NULL);
    EXPECT_EQ(cook.rule_count, (uint32_t)0);
    clean();
}

/* Le nombre maximal de règles est accepté, et les boucles n'en lisent pas une de trop. */
UTEST(eng_asset_cook, the_maximum_number_of_rules_is_accepted)
{
    clean();
    ASSERT_TRUE(write_text(SRC_A, "v1"));
    EngCookRule rules[ENG_COOK_MAX_RULES];
    for (uint32_t i = 0; i < ENG_COOK_MAX_RULES; i++) {
        rules[i] = (EngCookRule){.name = "a", .source = SRC_A, .argv = COPY_A};
    }
    EngAssetCook cook;
    ASSERT_TRUE(eng_asset_cook_init(&cook, rules, ENG_COOK_MAX_RULES, 0.02));
    EXPECT_EQ(cook.rule_count, ENG_COOK_MAX_RULES);
    EXPECT_FALSE(eng_asset_cook_busy(&cook));
    EXPECT_EQ(eng_asset_cook_poll(&cook, now_seconds()), (uint32_t)0);
    ASSERT_TRUE(write_text(SRC_A, "v2 changed"));
    const double start = now_seconds();
    while (!file_is(DST_A, "v2 changed") && now_seconds() - start < 15.0) {
        (void)eng_asset_cook_poll(&cook, now_seconds());
        SDL_Delay(5);
    }
    EXPECT_TRUE(file_is(DST_A, "v2 changed"));
    eng_asset_cook_shutdown(&cook);
    clean();
}
