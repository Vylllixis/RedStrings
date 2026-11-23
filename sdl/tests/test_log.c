#include "engine/log.h"
#include "test_util.h"
#include "utest.h"

UTEST(eng_log, parse_accepts_names_in_any_case)
{
    const char *names[] = {"trace", "DEBUG", "Info", "wArN", "error"};
    const EngLogLevel expected[] = {ENG_LEVEL_TRACE, ENG_LEVEL_DEBUG, ENG_LEVEL_INFO,
                                    ENG_LEVEL_WARN, ENG_LEVEL_ERROR};
    for (int i = 0; i < 5; i++) {
        EngLogLevel level = ENG_LEVEL_TRACE;
        ASSERT_TRUE(eng_log_parse_level(names[i], &level));
        EXPECT_EQ((int)level, (int)expected[i]);
    }
}

UTEST(eng_log, parse_rejects_unknown_names_and_keeps_output)
{
    const char *bad[] = {"", "bruit", "inf", "info ", "warning"};
    for (int i = 0; i < 5; i++) {
        EngLogLevel level = ENG_LEVEL_ERROR;
        EXPECT_FALSE(eng_log_parse_level(bad[i], &level));
        EXPECT_EQ((int)level, (int)ENG_LEVEL_ERROR);
    }
}

UTEST(eng_log, set_level_is_visible_through_get_level)
{
    eng_log_set_level(ENG_LEVEL_DEBUG);
    EXPECT_EQ((int)eng_log_get_level(), (int)ENG_LEVEL_DEBUG);
    eng_log_set_level(ENG_LEVEL_ERROR);
    EXPECT_EQ((int)eng_log_get_level(), (int)ENG_LEVEL_ERROR);
}

UTEST(eng_log, file_gets_level_location_and_formatted_message)
{
    const char *dir = TEST_TMP_DIR "/format";
    test_remove_log_files(dir);

    ASSERT_TRUE(eng_log_init(dir, ENG_LEVEL_INFO));
    ENG_LOG_INFO("bonjour %d", 42);
    eng_log_shutdown();

    char *text = test_read_text(dir, "engine.log");
    ASSERT_TRUE(text != NULL);
    EXPECT_TRUE(test_contains(text, "INFO "));
    EXPECT_TRUE(test_contains(text, "test_log.c:")); /* nom du fichier, sans le chemin complet */
    EXPECT_TRUE(test_contains(text, "bonjour 42"));
    SDL_free(text);
}

UTEST(eng_log, messages_below_the_level_are_dropped)
{
    const char *dir = TEST_TMP_DIR "/filter";
    test_remove_log_files(dir);

    ASSERT_TRUE(eng_log_init(dir, ENG_LEVEL_WARN));
    ENG_LOG_INFO("message-info");
    ENG_LOG_WARN("message-warn");
    ENG_LOG_ERROR("message-error");
    eng_log_shutdown();

    char *text = test_read_text(dir, "engine.log");
    ASSERT_TRUE(text != NULL);
    EXPECT_FALSE(test_contains(text, "message-info"));
    EXPECT_TRUE(test_contains(text, "message-warn"));
    EXPECT_TRUE(test_contains(text, "message-error"));
    SDL_free(text);
}

UTEST(eng_log, each_launch_rotates_and_keeps_one_previous_file)
{
    const char *dir = TEST_TMP_DIR "/rotate";
    test_remove_log_files(dir);

    for (int run = 1; run <= 3; run++) {
        ASSERT_TRUE(eng_log_init(dir, ENG_LEVEL_INFO));
        ENG_LOG_INFO("lancement-numero-%d", run);
        eng_log_shutdown();
    }

    char *current = test_read_text(dir, "engine.log");
    char *previous = test_read_text(dir, "engine.1.log");
    ASSERT_TRUE(current != NULL);
    ASSERT_TRUE(previous != NULL);
    EXPECT_TRUE(test_contains(current, "lancement-numero-3"));
    EXPECT_FALSE(test_contains(current, "lancement-numero-2"));
    EXPECT_TRUE(test_contains(previous, "lancement-numero-2"));
    EXPECT_FALSE(test_contains(previous, "lancement-numero-1")); /* une seule génération */
    SDL_free(current);
    SDL_free(previous);
}

UTEST(eng_log, very_long_message_is_truncated_on_one_line)
{
    const char *dir = TEST_TMP_DIR "/long";
    test_remove_log_files(dir);

    char big[3000];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';

    ASSERT_TRUE(eng_log_init(dir, ENG_LEVEL_INFO));
    ENG_LOG_INFO("%s", big);
    eng_log_shutdown();

    char *text = test_read_text(dir, "engine.log");
    ASSERT_TRUE(text != NULL);
    EXPECT_TRUE(test_contains(text, "...\n"));
    EXPECT_LT(strlen(text), 1500u); /* ni débordement ni message complet */
    SDL_free(text);
}

UTEST(eng_log, init_fails_cleanly_when_the_directory_is_a_file)
{
    ASSERT_TRUE(SDL_CreateDirectory(TEST_TMP_DIR));
    SDL_IOStream *file = SDL_IOFromFile(TEST_TMP_DIR "/blocker", "w");
    ASSERT_TRUE(file != NULL);
    SDL_CloseIO(file);

    EXPECT_FALSE(eng_log_init(TEST_TMP_DIR "/blocker", ENG_LEVEL_INFO));
    ENG_LOG_INFO("la console continue de fonctionner");
    eng_log_shutdown();
}

UTEST(eng_log, env_variable_sets_level_and_invalid_value_is_ignored)
{
    SDL_Environment *env = SDL_GetEnvironment();
    ASSERT_TRUE(env != NULL);

    ASSERT_TRUE(SDL_SetEnvironmentVariable(env, "ENG_LOG_LEVEL", "debug", true));
    EXPECT_EQ((int)eng_log_env_level(ENG_LEVEL_ERROR), (int)ENG_LEVEL_DEBUG);

    ASSERT_TRUE(SDL_SetEnvironmentVariable(env, "ENG_LOG_LEVEL", "bruit", true));
    EXPECT_EQ((int)eng_log_env_level(ENG_LEVEL_ERROR), (int)ENG_LEVEL_ERROR);

    ASSERT_TRUE(SDL_UnsetEnvironmentVariable(env, "ENG_LOG_LEVEL"));
    EXPECT_EQ((int)eng_log_env_level(ENG_LEVEL_WARN), (int)ENG_LEVEL_WARN);
}
