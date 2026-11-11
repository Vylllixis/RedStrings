#include "engine/assert.h"
#include "test_util.h"
#include "utest.h"

UTEST(eng_assert, true_conditions_do_nothing)
{
    int before = eng_assert_failure_count();
    ENG_ASSERT(1 + 1 == 2);
    ENG_ASSERT_MSG(sizeof(int) >= 2, "taille d'int : %zu", sizeof(int));
    EXPECT_EQ(eng_assert_failure_count(), before);
}

UTEST(eng_assert, enabled_state_follows_ndebug)
{
#ifdef NDEBUG
    EXPECT_EQ(ENG_ASSERTS_ENABLED, 0);
#else
    EXPECT_EQ(ENG_ASSERTS_ENABLED, 1);
#endif
}

UTEST(eng_assert, condition_is_evaluated_only_when_asserts_are_enabled)
{
    int evaluations = 0;
    ENG_ASSERT(++evaluations > 0);
    EXPECT_EQ(evaluations, ENG_ASSERTS_ENABLED ? 1 : 0);
}

#if ENG_ASSERTS_ENABLED
UTEST(eng_assert, failed_assertions_are_logged_with_location_and_counted)
{
    const char *dir = TEST_TMP_DIR "/assert";
    test_remove_log_files(dir);
    ASSERT_TRUE(eng_log_init(dir, ENG_LEVEL_INFO));

    int before = eng_assert_failure_count();
    int value = 41;
    eng_assert_set_break(false); /* sinon le test s'arrêterait dans le débogueur */
    ENG_ASSERT(value == 42);
    ENG_ASSERT_MSG(value == 42, "la valeur vaut %d", value);
    eng_assert_set_break(true);
    eng_log_shutdown();

    EXPECT_EQ(eng_assert_failure_count(), before + 2);

    char *text = test_read_text(dir, "engine.log");
    ASSERT_TRUE(text != NULL);
    EXPECT_TRUE(test_contains(text, "ASSERTION"));
    EXPECT_TRUE(test_contains(text, "value == 42"));       /* l'expression testée */
    EXPECT_TRUE(test_contains(text, "la valeur vaut 41")); /* le message formaté */
    EXPECT_TRUE(test_contains(text, "test_assert.c:"));    /* fichier et ligne */
    SDL_free(text);
}
#endif
