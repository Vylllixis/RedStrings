#include <SDL3/SDL.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/files.h"
#include "test_util.h"
#include "utest.h"

#define DIR TEST_TMP_DIR "/files"

static void prepare(void)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
}

static void write_file(const char *path, const void *data, size_t size)
{
    (void)SDL_SaveFile(path, data, size);
}

UTEST(eng_files, base_path_is_the_executable_directory)
{
    char base[512];
    ASSERT_TRUE(eng_path_base(base, sizeof base));
    size_t length = strlen(base);
    ASSERT_TRUE(length > 1);
    EXPECT_TRUE(base[length - 1] == '/' || base[length - 1] == '\\');
    EXPECT_TRUE(eng_path_is_absolute(base));
    /* Indépendant du dossier courant : c'est le dossier du binaire de test. */
    EXPECT_TRUE(strstr(base, "tests") != NULL);
}

UTEST(eng_files, resolve_joins_relative_and_keeps_absolute)
{
    char base[512];
    char out[512];
    ASSERT_TRUE(eng_path_base(base, sizeof base));
    ASSERT_TRUE(eng_path_resolve(out, sizeof out, "config.ini"));
    char expected[600];
    SDL_snprintf(expected, sizeof expected, "%sconfig.ini", base);
    EXPECT_STREQ(out, expected);

    ASSERT_TRUE(eng_path_resolve(out, sizeof out, "/etc/hosts"));
    EXPECT_STREQ(out, "/etc/hosts");
    ASSERT_TRUE(eng_path_resolve(out, sizeof out, "C:\\jeu\\a.txt"));
    EXPECT_STREQ(out, "C:\\jeu\\a.txt");
}

UTEST(eng_files, resolve_refuses_a_result_that_does_not_fit)
{
    char tiny[8];
    EXPECT_FALSE(eng_path_resolve(tiny, sizeof tiny, "un_nom_de_fichier_assez_long.txt"));
    EXPECT_FALSE(eng_path_base(tiny, sizeof tiny));
}

UTEST(eng_files, absolute_path_detection)
{
    EXPECT_TRUE(eng_path_is_absolute("/usr/bin"));
    EXPECT_TRUE(eng_path_is_absolute("\\\\serveur\\partage"));
    EXPECT_TRUE(eng_path_is_absolute("c:/dossier"));
    EXPECT_FALSE(eng_path_is_absolute("assets/a.png"));
    EXPECT_FALSE(eng_path_is_absolute("./a"));
    EXPECT_FALSE(eng_path_is_absolute("C:fichier")); /* relatif au lecteur : pas absolu */
    EXPECT_FALSE(eng_path_is_absolute(""));
}

UTEST(eng_files, read_all_returns_the_exact_content_nul_terminated)
{
    prepare();
    write_file(DIR "/texte.txt", "resolution = 1280x720\n", 22);
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 4096));
    EngFileData file;
    ASSERT_TRUE(eng_file_read_all(&arena, DIR "/texte.txt", &file));
    EXPECT_EQ(file.size, (size_t)22);
    EXPECT_EQ(memcmp(file.data, "resolution = 1280x720\n", 22), 0);
    EXPECT_EQ(file.data[file.size], 0); /* utilisable comme chaîne C */
    EXPECT_STREQ((const char *)file.data, "resolution = 1280x720\n");
    eng_arena_shutdown(&arena);
    (void)SDL_RemovePath(DIR "/texte.txt");
}

UTEST(eng_files, read_all_keeps_binary_data_with_embedded_zeros)
{
    prepare();
    const uint8_t bytes[] = {0, 1, 0, 255, 0, 0, 7};
    write_file(DIR "/binaire.bin", bytes, sizeof bytes);
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 4096));
    EngFileData file;
    ASSERT_TRUE(eng_file_read_all(&arena, DIR "/binaire.bin", &file));
    ASSERT_EQ(file.size, sizeof bytes);
    EXPECT_EQ(memcmp(file.data, bytes, sizeof bytes), 0);
    eng_arena_shutdown(&arena);
    (void)SDL_RemovePath(DIR "/binaire.bin");
}

UTEST(eng_files, read_all_handles_empty_and_large_files)
{
    prepare();
    write_file(DIR "/vide.txt", "", 0);
    size_t big_size = (size_t)1 << 20;
    uint8_t *big = SDL_malloc(big_size);
    ASSERT_TRUE(big != NULL);
    for (size_t i = 0; i < big_size; i++) {
        big[i] = (uint8_t)(i * 31u);
    }
    write_file(DIR "/gros.bin", big, big_size);

    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", (size_t)2 << 20));
    EngFileData file;
    ASSERT_TRUE(eng_file_read_all(&arena, DIR "/vide.txt", &file));
    EXPECT_EQ(file.size, (size_t)0);
    EXPECT_TRUE(file.data != NULL);
    EXPECT_EQ(file.data[0], 0);
    ASSERT_TRUE(eng_file_read_all(&arena, DIR "/gros.bin", &file));
    ASSERT_EQ(file.size, big_size);
    EXPECT_EQ(memcmp(file.data, big, big_size), 0);

    SDL_free(big);
    eng_arena_shutdown(&arena);
    (void)SDL_RemovePath(DIR "/vide.txt");
    (void)SDL_RemovePath(DIR "/gros.bin");
}

UTEST(eng_files, read_all_failures_leave_the_arena_untouched)
{
    prepare();
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 256));
    EngFileData file = {(const uint8_t *)"x", 99};

    /* Fichier absent */
    EXPECT_FALSE(eng_file_read_all(&arena, DIR "/n_existe_pas.txt", &file));
    EXPECT_TRUE(file.data == NULL && file.size == 0); /* sortie remise à zéro */
    EXPECT_EQ(arena.used, (size_t)0);
    /* Un dossier n'est pas un fichier */
    EXPECT_FALSE(eng_file_read_all(&arena, DIR, &file));
    EXPECT_EQ(arena.used, (size_t)0);
    /* Arène trop petite */
    uint8_t content[1000];
    memset(content, 'a', sizeof content);
    write_file(DIR "/grand.txt", content, sizeof content);
    EXPECT_FALSE(eng_file_read_all(&arena, DIR "/grand.txt", &file));
    EXPECT_EQ(arena.used, (size_t)0);
    EXPECT_FALSE(eng_file_exists(DIR "/n_existe_pas.txt"));
    EXPECT_TRUE(eng_file_exists(DIR "/grand.txt"));
    EXPECT_FALSE(eng_file_exists(DIR));
    eng_arena_shutdown(&arena);
    (void)SDL_RemovePath(DIR "/grand.txt");
}

/* ── Surveillance ── */

UTEST(eng_files, watch_ignores_the_initial_version)
{
    prepare();
    write_file(DIR "/w1.txt", "v1", 2);
    EngFileWatch w;
    ASSERT_TRUE(eng_file_watch_init(&w, DIR "/w1.txt", 0.1));
    double t = 0.0;
    for (int i = 0; i < 10; i++) {
        t += 1.0;
        EXPECT_FALSE(eng_file_watch_poll(&w, t));
    }
    (void)SDL_RemovePath(DIR "/w1.txt");
}

UTEST(eng_files, watch_reports_a_modification_once_when_stable)
{
    prepare();
    write_file(DIR "/w2.txt", "v1", 2);
    EngFileWatch w;
    ASSERT_TRUE(eng_file_watch_init(&w, DIR "/w2.txt", 0.1));
    SDL_Delay(30);
    write_file(DIR "/w2.txt", "version 2", 9);

    double t = 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t)); /* vue, en attente de stabilité */
    t += 1.0;
    EXPECT_TRUE(eng_file_watch_poll(&w, t)); /* stable : signalée */
    for (int i = 0; i < 5; i++) {
        t += 1.0;
        EXPECT_FALSE(eng_file_watch_poll(&w, t)); /* une seule fois */
    }
    (void)SDL_RemovePath(DIR "/w2.txt");
}

UTEST(eng_files, watch_waits_while_the_file_keeps_changing)
{
    prepare();
    write_file(DIR "/w3.txt", "v1", 2);
    EngFileWatch w;
    ASSERT_TRUE(eng_file_watch_init(&w, DIR "/w3.txt", 0.1));
    double t = 0.0;
    SDL_Delay(30);
    write_file(DIR "/w3.txt", "partiel", 7);
    t += 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t));
    SDL_Delay(30);
    write_file(DIR "/w3.txt", "partiel et la suite", 19); /* écriture en cours */
    t += 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t)); /* a encore changé : pas signalée */
    t += 1.0;
    EXPECT_TRUE(eng_file_watch_poll(&w, t));
    (void)SDL_RemovePath(DIR "/w3.txt");
}

UTEST(eng_files, watch_reports_the_appearance_of_a_missing_file)
{
    prepare();
    (void)SDL_RemovePath(DIR "/w4.txt");
    EngFileWatch w;
    ASSERT_TRUE(eng_file_watch_init(&w, DIR "/w4.txt", 0.1));
    double t = 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t));
    write_file(DIR "/w4.txt", "nouveau", 7);
    t += 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t));
    t += 1.0;
    EXPECT_TRUE(eng_file_watch_poll(&w, t));
    (void)SDL_RemovePath(DIR "/w4.txt");
}

UTEST(eng_files, watch_survives_deletion_and_recreation)
{
    prepare();
    write_file(DIR "/w5.txt", "a", 1);
    EngFileWatch w;
    ASSERT_TRUE(eng_file_watch_init(&w, DIR "/w5.txt", 0.1));
    (void)SDL_RemovePath(DIR "/w5.txt");
    double t = 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t)); /* absent : pas d'erreur, pas de signal */
    t += 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t));
    SDL_Delay(30);
    write_file(DIR "/w5.txt", "recree", 6);
    t += 1.0;
    EXPECT_FALSE(eng_file_watch_poll(&w, t));
    t += 1.0;
    EXPECT_TRUE(eng_file_watch_poll(&w, t));
    (void)SDL_RemovePath(DIR "/w5.txt");
}

UTEST(eng_files, watch_checks_the_file_only_once_per_interval)
{
    prepare();
    write_file(DIR "/w6.txt", "v1", 2);
    EngFileWatch w;
    ASSERT_TRUE(eng_file_watch_init(&w, DIR "/w6.txt", 10.0));
    SDL_Delay(30);
    write_file(DIR "/w6.txt", "v2 plus long", 12);
    EXPECT_FALSE(eng_file_watch_poll(&w, 100.0)); /* contrôle : vue */
    EXPECT_FALSE(eng_file_watch_poll(&w, 101.0)); /* trop tôt : ignoré */
    EXPECT_FALSE(eng_file_watch_poll(&w, 109.0));
    EXPECT_TRUE(eng_file_watch_poll(&w, 110.5)); /* intervalle écoulé : confirmée */
    (void)SDL_RemovePath(DIR "/w6.txt");
}

/* Critère M0-19 : un fichier modifié est signalé en moins d'une seconde (horloge réelle,
 * intervalle de contrôle par défaut). */
UTEST(eng_files, a_modified_file_is_reported_in_under_one_second)
{
    prepare();
    write_file(DIR "/w7.txt", "v1", 2);
    EngFileWatch w;
    ASSERT_TRUE(eng_file_watch_init(&w, DIR "/w7.txt", ENG_FILE_WATCH_DEFAULT_INTERVAL));
    SDL_Delay(30);

    Uint64 start = SDL_GetTicks();
    write_file(DIR "/w7.txt", "contenu modifie", 15);
    bool reported = false;
    Uint64 elapsed = 0;
    while (elapsed < 3000) { /* on boucle comme la boucle de jeu : un contrôle par « image » */
        double now = (double)(SDL_GetTicks() - start) / 1000.0;
        if (eng_file_watch_poll(&w, now)) {
            reported = true;
            break;
        }
        SDL_Delay(8);
        elapsed = SDL_GetTicks() - start;
    }
    EXPECT_TRUE(reported);
    EXPECT_TRUE(elapsed < 1000);
    (void)SDL_RemovePath(DIR "/w7.txt");
}

UTEST(eng_files, watch_rejects_an_overlong_path)
{
    char path[600];
    memset(path, 'a', sizeof path - 1);
    path[sizeof path - 1] = '\0';
    EngFileWatch w;
    EXPECT_FALSE(eng_file_watch_init(&w, path, 0.1));
}
