#include <SDL3/SDL.h>
#include <string.h>

#include "engine/assert.h"
#include "engine/game_host.h"
#include "engine/memory.h"
#include "test_util.h"
#include "utest.h"

/* État du faux jeu, tel que défini dans test_game_lib.c (première partie commune aux variantes). */
typedef struct TestStateView {
    long accumulated;
    int init_calls;
    int reload_calls;
    int unload_calls;
    void *engine_memory;
} TestStateView;

#define LIVE_DIR TEST_TMP_DIR "/host"
#define LIVE_LIB LIVE_DIR "/game" TEST_LIB_EXT

typedef struct Fixture {
    EngMemory memory;
    EngServices services;
    EngGameHost host;
    double now;
} Fixture;

static bool install(const char *variant)
{
    char source[512];
    SDL_snprintf(source, sizeof source, "%s/%s%s", TEST_LIB_DIR, variant, TEST_LIB_EXT);
    SDL_Delay(30); /* garantit un horodatage de fichier différent (granularité des systèmes) */
    return SDL_CopyFile(source, LIVE_LIB);
}

static bool fixture_start(Fixture *f, const char *variant)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(LIVE_DIR);
    if (!install(variant)) {
        return false;
    }
    const EngMemoryConfig config = {
        .permanent_bytes = 65536, .level_bytes = 4096, .frame_bytes = 4096};
    if (!eng_memory_init(&f->memory, &config)) {
        return false;
    }
    eng_game_services_init(&f->services, &f->memory);
    f->now = 0.0;
    return eng_game_host_init(&f->host, LIVE_LIB, &f->services, &f->memory.permanent, 0.0);
}

static void fixture_stop(Fixture *f)
{
    eng_game_host_shutdown(&f->host);
    eng_memory_shutdown(&f->memory);
    (void)SDL_RemovePath(LIVE_LIB);
    (void)SDL_RemovePath(LIVE_DIR);
}

static bool poll(Fixture *f)
{
    f->now += 1.0; /* chaque appel dépasse l'intervalle de contrôle (0) */
    return eng_game_host_poll(&f->host, f->now);
}

static const EngInputState NO_INPUT = {0};

static void step(Fixture *f, int count)
{
    for (int i = 0; i < count; i++) {
        f->host.api->update(f->host.state, &NO_INPUT, 1.0 / 60.0);
    }
}

static TestStateView *state_of(Fixture *f)
{
    return f->host.state;
}

UTEST(eng_game_host, loads_the_game_and_runs_it)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    EXPECT_EQ(state_of(&f)->init_calls, 1);
    step(&f, 3);
    EXPECT_EQ(state_of(&f)->accumulated, 3L);
    EngDebugSquare square;
    f.host.api->render(f.host.state, 0.5, &square);
    EXPECT_EQ(square.r, 1); /* variante A */
    EXPECT_EQ(square.size, 0.5);
    /* La mémoire demandée par le jeu au moteur est bien dans l'arène du moteur. */
    EXPECT_TRUE(state_of(&f)->engine_memory != NULL);
    EXPECT_TRUE(f.memory.permanent.used >= f.host.state_size + 128);
    fixture_stop(&f);
}

/* Critères M0-17 : le code modifié est actif sans relancer, et l'état est conservé. */
UTEST(eng_game_host, modified_code_takes_effect_and_state_is_kept)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    step(&f, 3); /* +1 par pas : accumulated = 3 */
    void *state_before = f.host.state;
    ASSERT_TRUE(install("variant_b")); /* « on recompile » : le fichier surveillé change */

    EXPECT_FALSE(poll(&f)); /* 1re observation : on attend que le fichier soit stable */
    ASSERT_TRUE(poll(&f));  /* 2e : stable, rechargement */

    EXPECT_EQ(f.host.reload_count, (uint32_t)1);
    EXPECT_TRUE(f.host.state == state_before); /* même bloc mémoire */
    EXPECT_EQ(state_of(&f)->accumulated, 3L);  /* état conservé */
    EXPECT_EQ(state_of(&f)->init_calls, 1);    /* init n'est PAS rappelé */
    EXPECT_EQ(state_of(&f)->unload_calls, 1);
    EXPECT_EQ(state_of(&f)->reload_calls, 1);
    step(&f, 1);
    EXPECT_EQ(state_of(&f)->accumulated, 103L); /* nouveau code : +100 par pas */
    EngDebugSquare square;
    f.host.api->render(f.host.state, 0.0, &square);
    EXPECT_EQ(square.r, 2);
    fixture_stop(&f);
}

UTEST(eng_game_host, no_reload_when_the_file_is_unchanged)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(poll(&f));
    }
    EXPECT_EQ(f.host.reload_count, (uint32_t)0);
    fixture_stop(&f);
}

UTEST(eng_game_host, poll_interval_is_respected)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    f.host.poll_interval = 100.0;
    ASSERT_TRUE(install("variant_b"));
    (void)eng_game_host_poll(&f.host, 1.0); /* contrôle effectué : change vu, en attente */
    EXPECT_FALSE(eng_game_host_poll(&f.host, 2.0)); /* trop tôt : ignoré */
    EXPECT_FALSE(eng_game_host_poll(&f.host, 50.0));
    EXPECT_TRUE(eng_game_host_poll(&f.host, 101.5)); /* intervalle écoulé : confirmé, rechargé */
    fixture_stop(&f);
}

UTEST(eng_game_host, a_file_still_being_written_waits_for_stability)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    ASSERT_TRUE(install("variant_b"));
    EXPECT_FALSE(poll(&f));            /* vu */
    ASSERT_TRUE(install("variant_a")); /* le fichier change encore (nouvelle écriture) */
    EXPECT_FALSE(poll(&f));            /* version différente de la précédente : on réattend */
    EXPECT_EQ(f.host.reload_count, (uint32_t)0);
    EXPECT_TRUE(poll(&f)); /* enfin stable */
    fixture_stop(&f);
}

UTEST(eng_game_host, state_layout_change_is_refused_and_old_code_keeps_running)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    step(&f, 2);
    ASSERT_TRUE(install("variant_big")); /* la structure d'état a changé de taille */
    EXPECT_FALSE(poll(&f));
    EXPECT_FALSE(poll(&f)); /* refusé */
    EXPECT_EQ(f.host.refused_count, (uint32_t)1);
    EXPECT_EQ(f.host.reload_count, (uint32_t)0);
    step(&f, 1); /* l'ancien code tourne toujours, état intact */
    EXPECT_EQ(state_of(&f)->accumulated, 3L);
    /* La même version n'est pas réessayée en boucle. */
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(poll(&f));
    }
    EXPECT_EQ(f.host.refused_count, (uint32_t)1);
    /* Une nouvelle compilation compatible est de nouveau acceptée. */
    ASSERT_TRUE(install("variant_b"));
    EXPECT_FALSE(poll(&f));
    EXPECT_TRUE(poll(&f));
    EXPECT_EQ(state_of(&f)->accumulated, 3L);
    fixture_stop(&f);
}

UTEST(eng_game_host, a_broken_library_is_refused_and_a_good_one_recovers)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    step(&f, 1);
    SDL_Delay(30);
    const char junk[] = "ceci n'est pas une bibliotheque dynamique";
    ASSERT_TRUE(SDL_SaveFile(LIVE_LIB, junk, sizeof junk));
    EXPECT_FALSE(poll(&f));
    EXPECT_FALSE(poll(&f));
    EXPECT_EQ(f.host.refused_count, (uint32_t)1);
    step(&f, 1); /* le jeu continue avec l'ancien code */
    EXPECT_EQ(state_of(&f)->accumulated, 2L);

    ASSERT_TRUE(install("variant_b"));
    EXPECT_FALSE(poll(&f));
    EXPECT_TRUE(poll(&f));
    EXPECT_EQ(state_of(&f)->accumulated, 2L);
    fixture_stop(&f);
}

UTEST(eng_game_host, missing_file_is_ignored)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    /* On ne peut pas supprimer le fichier surveillé et continuer à le contrôler sans risque de
     * course sous Windows ; on vérifie surtout qu'un chemin absent n'est pas une erreur. */
    f.host.source_path[0] = 'X'; /* chemin devenu inexistant */
    EXPECT_FALSE(poll(&f));
    EXPECT_FALSE(poll(&f));
    f.host.source_path[0] = LIVE_LIB[0];
    fixture_stop(&f);
}

UTEST(eng_game_host, init_fails_cleanly_on_missing_or_invalid_library)
{
    EngMemory memory;
    const EngMemoryConfig config = {
        .permanent_bytes = 4096, .level_bytes = 4096, .frame_bytes = 4096};
    ASSERT_TRUE(eng_memory_init(&memory, &config));
    EngServices services;
    eng_game_services_init(&services, &memory);
    EngGameHost host;
    EXPECT_FALSE(eng_game_host_init(&host, TEST_TMP_DIR "/absent/game" TEST_LIB_EXT, &services,
                                    &memory.permanent, 0.0));
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    ASSERT_TRUE(SDL_SaveFile(TEST_TMP_DIR "/faux" TEST_LIB_EXT, "xx", 2));
    EXPECT_FALSE(eng_game_host_init(&host, TEST_TMP_DIR "/faux" TEST_LIB_EXT, &services,
                                    &memory.permanent, 0.0));
    (void)SDL_RemovePath(TEST_TMP_DIR "/faux" TEST_LIB_EXT);
    eng_memory_shutdown(&memory);
}

UTEST(eng_game_host, hot_copies_are_cleaned_up)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    ASSERT_TRUE(install("variant_b"));
    (void)poll(&f);
    ASSERT_TRUE(poll(&f));
    int count = 0;
    char **files = SDL_GlobDirectory(f.host.hot_dir, NULL, 0, &count);
    ASSERT_TRUE(files != NULL);
    /* Une seule copie : celle du code actif. Sauf si dlclose ne décharge pas (Tracy, Linux) :
     * l'ancienne est alors gardée jusqu'à l'arrêt. */
    EXPECT_EQ(count, ENG_HOST_DLCLOSE_IS_NOOP ? 2 : 1);
    SDL_free((void *)files);
    fixture_stop(&f);
    EXPECT_FALSE(
        SDL_GetPathInfo(LIVE_DIR "/hot", NULL)); /* dossier supprimé avec la dernière copie */
}

UTEST(eng_game_host, game_logs_and_assertions_reach_the_engine)
{
    Fixture f;
    ASSERT_TRUE(fixture_start(&f, "variant_a"));
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    test_remove_log_files(TEST_TMP_DIR);
    ASSERT_TRUE(eng_log_init(TEST_TMP_DIR, ENG_LEVEL_INFO));
    f.host.api->reload(f.host.state, &f.services); /* le jeu relie ses services... */
    ENG_LOG_INFO("marqueur-moteur");
#if ENG_ASSERTS_ENABLED
    eng_assert_set_break(false);
    int before = eng_assert_failure_count();
    f.host.api->update(f.host.state, &NO_INPUT, -1.0); /* ENG_ASSERT(dt >= 0) échoue dans le JEU */
    EXPECT_EQ(eng_assert_failure_count() - before, 1);
    eng_assert_set_break(true);
#endif
    eng_log_shutdown();
    char *log = test_read_text(TEST_TMP_DIR, "engine.log");
    EXPECT_TRUE(test_contains(log, "marqueur-moteur"));
#if ENG_ASSERTS_ENABLED
    EXPECT_TRUE(test_contains(log, "ASSERTION"));
    EXPECT_TRUE(test_contains(log, "dt >= 0.0"));
#endif
    SDL_free(log);
    test_remove_log_files(TEST_TMP_DIR);
    fixture_stop(&f);
}
