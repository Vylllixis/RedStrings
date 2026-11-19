/* Fichier de machine à états d'animation (M4-19) : analyse stricte, messages d'erreur avec numéro
 * de ligne, limites, rechargement à chaud (une version invalide ne remplace jamais la bonne). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/anim_graph.h"
#include "test_util.h"
#include "utest.h"

#define DIR TEST_TMP_DIR "/animgraph"

static const char *g_good = "# machine de locomotion\n"
                            "param speed\n"
                            "param jump 0\n"
                            "clip idle idle.anim\n"
                            "clip walk walk.anim loop speed=1.5\n"
                            "clip run  run.anim   # commentaire en fin de ligne\n"
                            "clip jump jump.anim once\n"
                            "\n"
                            "state idle clip idle\n"
                            "state move blend speed idle:0 walk:1.5 run:4.5\n"
                            "state jump clip jump\n"
                            "transition idle -> move when speed > 0.1 fade 0.2\n"
                            "transition move -> idle when speed <= 0.1 fade 0.3\n"
                            "transition any -> jump when jump >= 0.5 fade 0.1\n"
                            "transition jump -> idle when ended\n"
                            "start idle\n";

static bool parse(const char *text, EngAnimGraphDef *d, char *err, size_t cap)
{
    return eng_animgraph_parse(text, d, err, cap);
}

UTEST(anim_graph, a_complete_file_is_read_into_the_definition)
{
    EngAnimGraphDef d;
    char err[128];
    ASSERT_TRUE(parse(g_good, &d, err, sizeof err));
    EXPECT_STREQ(err, "");
    EXPECT_EQ(d.param_count, 2U);
    EXPECT_EQ(d.clip_count, 4U);
    EXPECT_EQ(d.state_count, 3U);
    EXPECT_EQ(d.transition_count, 4U);
    EXPECT_EQ(d.start, 0U);
    EXPECT_STREQ(d.clips[1].path, "walk.anim");
    EXPECT_NEAR(d.clips[1].speed, 1.5F, 1e-7F);
    EXPECT_TRUE(d.clips[0].loop);
    EXPECT_FALSE(d.clips[3].loop);
    EXPECT_EQ(d.states[1].kind, ENG_ANIMSTATE_BLEND);
    EXPECT_EQ(d.states[1].sample_count, 3U);
    EXPECT_NEAR(d.states[1].sample_position[2], 4.5F, 1e-7F);
    EXPECT_EQ(d.states[1].sample_clip[1], 1U);
    EXPECT_EQ(d.states[2].clip, 3U);
    EXPECT_EQ(d.transitions[0].from, 0U);
    EXPECT_EQ(d.transitions[0].to, 1U);
    EXPECT_EQ(d.transitions[0].cond, ENG_ANIMCOND_GT);
    EXPECT_NEAR(d.transitions[0].threshold, 0.1F, 1e-7F);
    EXPECT_NEAR(d.transitions[0].fade, 0.2F, 1e-7F);
    EXPECT_EQ(d.transitions[1].cond, ENG_ANIMCOND_LE);
    EXPECT_EQ(d.transitions[2].from, ENG_ANIMGRAPH_ANY);
    EXPECT_EQ(d.transitions[2].cond, ENG_ANIMCOND_GE);
    EXPECT_EQ(d.transitions[3].cond, ENG_ANIMCOND_ENDED);
    EXPECT_NEAR(d.transitions[3].fade, 0.0F, 1e-7F);
    EXPECT_EQ(eng_animgraph_find_state(&d, "jump"), 2U);
    EXPECT_EQ(eng_animgraph_find_state(&d, "nope"), UINT32_MAX);
    EXPECT_EQ(eng_animgraph_find_param(&d, "jump"), 1U);
    EXPECT_EQ(eng_animgraph_find_clip(&d, "run"), 2U);
    EXPECT_EQ(eng_animgraph_find_clip(NULL, "run"), UINT32_MAX);
    /* CRLF et dernière ligne sans saut de ligne */
    ASSERT_TRUE(parse("clip a a.anim\r\nstate s clip a\r\nstart s", &d, err, sizeof err));
    EXPECT_EQ(d.state_count, 1U);
    /* paramètre : valeur initiale */
    ASSERT_TRUE(
        parse("param p 2.5\nclip a a.anim\nstate s clip a\nstart s\n", &d, err, sizeof err));
    EXPECT_NEAR(d.params[0].initial, 2.5F, 1e-7F);
}

/* true si `text` est refusé avec un message qui contient `needle` et une définition vide. */
static bool refused_with(const char *text, const char *needle)
{
    EngAnimGraphDef d;
    char err[256];
    memset(&d, 0x5A, sizeof d);
    const bool parsed = parse(text, &d, err, sizeof err);
    const bool found = strstr(err, needle) != NULL;
    if (!found) {
        printf("    erreur obtenue : « %s », attendu : « %s »\n", err, needle);
    }
    return !parsed && found && d.state_count == 0 && d.clip_count == 0 && d.start == 0;
}

#define EXPECT_REFUSED(text, needle) EXPECT_TRUE(refused_with((text), (needle)))

#define BASE "clip a a.anim\nclip b b.anim once\nparam p\nstate s clip a\nstate t clip a\n"

UTEST(anim_graph, errors_name_the_line_and_the_problem)
{
    EXPECT_REFUSED("coucou\n", "ligne 1 : instruction inconnue");
    EXPECT_REFUSED("param\n", "ligne 1 : param NOM");
    EXPECT_REFUSED("param x y\n", "ligne 1 : valeur initiale invalide");
    EXPECT_REFUSED("param p\nparam p\n", "ligne 2 : paramètre « p » déjà défini");
    EXPECT_REFUSED("param bad/name\n", "nom de paramètre invalide");
    EXPECT_REFUSED("clip a\n", "ligne 1 : clip NOM");
    EXPECT_REFUSED("clip a a.anim\nclip a b.anim\n", "ligne 2 : clip « a » déjà défini");
    EXPECT_REFUSED("clip a a.anim fast\n", "option de clip inconnue");
    EXPECT_REFUSED("clip a a.anim speed=x\n", "vitesse invalide");
    EXPECT_REFUSED("clip a a.anim speed=-1\n", "vitesse invalide");
    EXPECT_REFUSED("clip a a.anim speed=9\n", "vitesse invalide");
    EXPECT_REFUSED("state s clip nope\n", "clip inconnu « nope »");
    EXPECT_REFUSED("clip a a.anim\nstate s clip a\nstate s clip a\n",
                   "ligne 3 : état « s » déjà défini");
    EXPECT_REFUSED("clip a a.anim\nstate any clip a\n", "« any » est réservé");
    EXPECT_REFUSED("clip a a.anim\nstate s music a\n", "type d'état inconnu");
    EXPECT_REFUSED("clip a a.anim\nstate s clip a a\n", "un seul clip");
    EXPECT_REFUSED("clip a a.anim\nstate s blend q a:0\n", "paramètre inconnu « q »");
    EXPECT_REFUSED("param q\nclip a a.anim\nstate s blend q\n", "au moins un échantillon");
    EXPECT_REFUSED("param q\nclip a a.anim\nstate s blend q a\n", "attendu CLIP:POSITION");
    EXPECT_REFUSED("param q\nclip a a.anim\nstate s blend q z:0\n", "clip inconnu « z »");
    EXPECT_REFUSED("param q\nclip a a.anim\nstate s blend q a:x\n", "position invalide");
    EXPECT_REFUSED("param q\nclip a a.anim\nclip b b.anim\nstate s blend q a:2 b:1\n",
                   "strictement croissantes");
    EXPECT_REFUSED("param q\nclip a a.anim\nclip b b.anim\nstate s blend q a:1 b:1\n",
                   "strictement croissantes");
    EXPECT_REFUSED("param q\nclip a a.anim once\nstate s blend q a:0\n", "exige des cycles");
    EXPECT_REFUSED(BASE "transition s -> t\n", "transition DE -> VERS");
    EXPECT_REFUSED(BASE "transition s -> t when ended extra\n", "fin de transition inattendue");
    EXPECT_REFUSED(BASE "transition s => t when ended\n", "transition DE -> VERS");
    EXPECT_REFUSED(BASE "transition x -> t when ended\n", "état inconnu « x »");
    EXPECT_REFUSED(BASE "transition s -> x when ended\n", "état inconnu « x »");
    EXPECT_REFUSED(BASE "transition any -> any when ended\n", "état inconnu « any »");
    EXPECT_REFUSED(BASE "transition s -> s when ended\n", "vers lui-même");
    EXPECT_REFUSED(BASE "transition s -> t when q > 1\n", "paramètre inconnu « q »");
    EXPECT_REFUSED(BASE "transition s -> t when p ~ 1\n", "opérateur inconnu");
    EXPECT_REFUSED(BASE "transition s -> t when p > x\n", "valeur invalide");
    EXPECT_REFUSED(BASE "transition s -> t when p >\n", "condition : PARAM OP VALEUR");
    EXPECT_REFUSED(BASE "transition s -> t when p > 1 fade\n", "fin de transition inattendue");
    EXPECT_REFUSED(BASE "transition s -> t when p > 1 fade -1\n", "durée de fondu invalide");
    EXPECT_REFUSED(BASE "transition s -> t when p > 1 fade 11\n", "durée de fondu invalide");
    EXPECT_REFUSED(BASE "transition s -> t when p > 1 fade nan\n", "durée de fondu invalide");
    EXPECT_REFUSED(BASE "start x\n", "état inconnu « x »");
    EXPECT_REFUSED(BASE "start s\nstart t\n", "start déjà défini");
    EXPECT_REFUSED(BASE, "`start NOM` manquant");
    EXPECT_REFUSED("clip a a.anim\n", "aucun état défini");
    EXPECT_REFUSED("", "aucun état défini");
    EXPECT_REFUSED("\n\n# rien\n", "aucun état défini");
    /* le message d'une erreur tardive désigne la bonne ligne */
    EXPECT_REFUSED("clip a a.anim\nstate s clip a\n\n# vide\nstart s\nfoo\n", "ligne 6");
}

UTEST(anim_graph, limits_are_enforced_with_a_clear_message)
{
    char text[8192];
    size_t n = 0;
    for (uint32_t i = 0; i <= ENG_ANIMGRAPH_MAX_PARAMS; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, "param p%u\n", (unsigned)i);
    }
    EXPECT_REFUSED(text, "trop de paramètres");
    n = 0;
    for (uint32_t i = 0; i <= ENG_ANIMGRAPH_MAX_CLIPS; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, "clip c%u c.anim\n", (unsigned)i);
    }
    EXPECT_REFUSED(text, "trop de clips");
    n = (size_t)snprintf(text, sizeof text, "clip a a.anim\n");
    for (uint32_t i = 0; i <= ENG_ANIMGRAPH_MAX_STATES; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, "state s%u clip a\n", (unsigned)i);
    }
    EXPECT_REFUSED(text, "trop d'états");
    n = (size_t)snprintf(text, sizeof text,
                         "param p\nclip a a.anim\nstate s clip a\nstate t clip a\n");
    for (uint32_t i = 0; i <= ENG_ANIMGRAPH_MAX_TRANSITIONS; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, "transition s -> t when p > %u\n",
                              (unsigned)i);
    }
    EXPECT_REFUSED(text, "trop de transitions");
    n = (size_t)snprintf(text, sizeof text, "param q\n");
    for (uint32_t i = 0; i < 9; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, "clip c%u c.anim\n", (unsigned)i);
    }
    n += (size_t)snprintf(text + n, sizeof text - n, "state s blend q");
    for (uint32_t i = 0; i < 9; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, " c%u:%u", (unsigned)i, (unsigned)i);
    }
    EXPECT_REFUSED(text, "trop d'échantillons");
    /* noms et chemins trop longs */
    char longname[64];
    memset(longname, 'a', sizeof longname - 1U);
    longname[sizeof longname - 1U] = '\0';
    (void)snprintf(text, sizeof text, "param %s\n", longname);
    EXPECT_REFUSED(text, "nom de paramètre invalide");
    char longpath[200];
    memset(longpath, 'p', sizeof longpath - 1U);
    longpath[sizeof longpath - 1U] = '\0';
    (void)snprintf(text, sizeof text, "clip a %s\n", longpath);
    EXPECT_REFUSED(text, "chemin trop long");
    /* ligne démesurée */
    char huge[1200];
    memset(huge, 'x', sizeof huge - 1U);
    huge[sizeof huge - 1U] = '\0';
    EXPECT_REFUSED(huge, "ligne trop longue");
    /* trop d'éléments sur une ligne */
    char many[600];
    size_t m = 0;
    for (int i = 0; i < 40; i++) {
        m += (size_t)snprintf(many + m, sizeof many - m, "a ");
    }
    EXPECT_REFUSED(many, "ligne trop longue (plus de");
    /* limites atteintes exactement : acceptées */
    n = 0;
    for (uint32_t i = 0; i < ENG_ANIMGRAPH_MAX_PARAMS; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, "param p%u\n", (unsigned)i);
    }
    (void)snprintf(text + n, sizeof text - n, "clip a a.anim\nstate s clip a\nstart s\n");
    EngAnimGraphDef d;
    EXPECT_TRUE(parse(text, &d, NULL, 0));
    EXPECT_EQ(d.param_count, ENG_ANIMGRAPH_MAX_PARAMS);
}

UTEST(anim_graph, bad_arguments_and_small_error_buffers_are_safe)
{
    EngAnimGraphDef d;
    EXPECT_FALSE(eng_animgraph_parse(NULL, &d, NULL, 0));
    EXPECT_FALSE(eng_animgraph_parse(g_good, NULL, NULL, 0));
    char err[8];
    EXPECT_FALSE(eng_animgraph_parse("coucou\n", &d, err, sizeof err));
    EXPECT_LT(strlen(err), sizeof err); /* tronqué proprement */
    EXPECT_FALSE(eng_animgraph_parse("coucou\n", &d, NULL, 0));
    char one[1];
    EXPECT_FALSE(eng_animgraph_parse("coucou\n", &d, one, sizeof one));
    EXPECT_STREQ(one, "");
    EXPECT_FALSE(eng_animgraph_parse("", &d, err, 0));
}

static void write_text(const char *path, const char *text)
{
    (void)SDL_SaveFile(path, text, strlen(text));
}

/* Attend (par contrôles successifs, comme une boucle de jeu) qu'une modification soit signalée. */
static bool poll_until_loaded(EngAnimGraphFile *f, EngArena *scratch, double *now)
{
    for (int i = 0; i < 8; i++) {
        *now += 1.0;
        if (eng_animgraph_file_poll(f, scratch, *now)) {
            return true;
        }
    }
    return false;
}

UTEST(anim_graph, a_watched_file_reloads_valid_versions_and_keeps_the_good_one_on_errors)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
    const char *path = DIR "/loco.animgraph";
    write_text(path, g_good);
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&scratch, "scratch", (size_t)1 << 16));
    EngAnimGraphFile f;
    ASSERT_TRUE(eng_animgraph_file_open(&f, &scratch, path));
    EXPECT_TRUE(f.valid);
    EXPECT_EQ(f.version, 1U);
    EXPECT_STREQ(f.error, "");
    EXPECT_EQ(scratch.used, 0U); /* tampon de lecture restitué */
    EXPECT_EQ(f.def.state_count, 3U);
    double now = 0.0;
    EXPECT_FALSE(eng_animgraph_file_poll(&f, &scratch, now + 1.0)); /* rien n'a changé */

    /* nouvelle version valide : un état de moins */
    write_text(path, "param speed\nclip idle idle.anim\nstate idle clip idle\nstart idle\n# v2\n");
    ASSERT_TRUE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 2U);
    EXPECT_EQ(f.def.state_count, 1U);

    /* version invalide : refusée, l'ancienne reste, l'erreur est conservée */
    write_text(path, "param speed\nclip idle idle.anim\nstate idle clip idle\nstart nowhere\n");
    EXPECT_FALSE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 2U);
    EXPECT_EQ(f.def.state_count, 1U);
    EXPECT_NE(strstr(f.error, "état inconnu « nowhere »"), NULL);
    EXPECT_EQ(scratch.used, 0U);

    /* correction : rechargée, l'erreur disparaît */
    write_text(path, g_good);
    ASSERT_TRUE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 3U);
    EXPECT_EQ(f.def.state_count, 3U);
    EXPECT_STREQ(f.error, "");

    /* fichier binaire */
    const char binary[] = {'p', 'a', '\0', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    (void)SDL_SaveFile(path, binary, sizeof binary);
    EXPECT_FALSE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_NE(strstr(f.error, "binaire"), NULL);
    EXPECT_EQ(f.version, 3U);
    eng_arena_shutdown(&scratch);
}

UTEST(anim_graph, opening_a_missing_or_invalid_file_fails_cleanly)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&scratch, "scratch", (size_t)1 << 16));
    EngAnimGraphFile f;
    EXPECT_FALSE(eng_animgraph_file_open(&f, &scratch, DIR "/absent.animgraph"));
    EXPECT_FALSE(f.valid);
    EXPECT_NE(strstr(f.error, "illisible"), NULL);
    write_text(DIR "/bad.animgraph", "coucou\n");
    EXPECT_FALSE(eng_animgraph_file_open(&f, &scratch, DIR "/bad.animgraph"));
    EXPECT_NE(strstr(f.error, "ligne 1"), NULL);
    EXPECT_FALSE(eng_animgraph_file_open(NULL, &scratch, "x"));
    EXPECT_FALSE(eng_animgraph_file_open(&f, NULL, "x"));
    EXPECT_FALSE(eng_animgraph_file_open(&f, &scratch, NULL));
    char longpath[600];
    memset(longpath, 'a', sizeof longpath - 1U);
    longpath[sizeof longpath - 1U] = '\0';
    EXPECT_FALSE(eng_animgraph_file_open(&f, &scratch, longpath));
    EXPECT_FALSE(eng_animgraph_file_poll(NULL, &scratch, 1.0));
    EXPECT_FALSE(eng_animgraph_file_poll(&f, NULL, 1.0));
    eng_arena_shutdown(&scratch);
}

UTEST(anim_graph, boundary_values_are_accepted_and_one_step_further_is_refused)
{
    EngAnimGraphDef d;
    char text[2048];
    char name31[32];
    memset(name31, 'n', 31);
    name31[31] = '\0';
    (void)snprintf(text, sizeof text, "param %s\nclip a a.anim\nstate s clip a\nstart s\n", name31);
    EXPECT_TRUE(parse(text, &d, NULL, 0)); /* 31 caractères : accepté (32 est refusé plus haut) */
    char path127[128];
    memset(path127, 'p', 127);
    path127[127] = '\0';
    (void)snprintf(text, sizeof text, "clip a %s\nstate s clip a\nstart s\n", path127);
    EXPECT_TRUE(parse(text, &d, NULL, 0));
    char path128[129];
    memset(path128, 'p', 128);
    path128[128] = '\0';
    (void)snprintf(text, sizeof text, "clip a %s\nstate s clip a\nstart s\n", path128);
    EXPECT_FALSE(parse(text, &d, NULL, 0));
    EXPECT_TRUE(parse("clip a a.anim speed=0\nstate s clip a\nstart s\n", &d, NULL, 0));
    EXPECT_TRUE(parse("clip a a.anim speed=8\nstate s clip a\nstart s\n", &d, NULL, 0));
    EXPECT_FALSE(parse("clip a a.anim speed=8.01\nstate s clip a\nstart s\n", &d, NULL, 0));
    const char *two = "param p\nclip a a.anim\nstate s clip a\nstate t clip a\n";
    (void)snprintf(text, sizeof text, "%stransition s -> t when p > 1 fade 0\nstart s\n", two);
    EXPECT_TRUE(parse(text, &d, NULL, 0));
    (void)snprintf(text, sizeof text, "%stransition s -> t when p > 1 fade 10\nstart s\n", two);
    EXPECT_TRUE(parse(text, &d, NULL, 0));
    (void)snprintf(text, sizeof text, "%stransition s -> t when p > 1 fade 10.01\nstart s\n", two);
    EXPECT_FALSE(parse(text, &d, NULL, 0));
    /* huit échantillons : accepté ; position négative et zéro : acceptées */
    size_t n = (size_t)snprintf(text, sizeof text, "param q\n");
    for (int i = 0; i < 8; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, "clip c%d c.anim\n", i);
    }
    n += (size_t)snprintf(text + n, sizeof text - n, "state s blend q");
    for (int i = 0; i < 8; i++) {
        n += (size_t)snprintf(text + n, sizeof text - n, " c%d:%d", i, i - 3);
    }
    (void)snprintf(text + n, sizeof text - n, "\nstart s\n");
    EXPECT_TRUE(parse(text, &d, NULL, 0));
    EXPECT_EQ(d.states[0].sample_count, 8U);
    EXPECT_NEAR(d.states[0].sample_position[0], -3.0F, 1e-7F);
    /* ligne de 511 caractères : acceptée ; 512 : refusée */
    char line[600];
    memset(line, ' ', 511);
    line[511] = '\0';
    memcpy(line, "# c", 3);
    EXPECT_TRUE(parse(line, &d, NULL, 0) == false); /* seul un commentaire : aucun état */
    char err[128];
    (void)parse(line, &d, err, sizeof err);
    EXPECT_NE(strstr(err, "aucun état"), NULL); /* et surtout pas « ligne trop longue » */
    memset(line, ' ', 512);
    line[512] = '\0';
    memcpy(line, "# c", 3);
    (void)parse(line, &d, err, sizeof err);
    EXPECT_NE(strstr(err, "ligne trop longue"), NULL);
}

UTEST(anim_graph, root_motion_names_the_root_joint_once)
{
    EngAnimGraphDef d;
    char err[128];
    ASSERT_TRUE(
        parse("clip a a.anim\nstate s clip a\nroot_motion Hips\nstart s\n", &d, err, sizeof err));
    EXPECT_TRUE(d.has_root_motion);
    EXPECT_STREQ(d.root_joint, "Hips");
    ASSERT_TRUE(parse("clip a a.anim\nstate s clip a\nstart s\n", &d, err, sizeof err));
    EXPECT_FALSE(d.has_root_motion);
    EXPECT_STREQ(d.root_joint, "");
    EXPECT_REFUSED("clip a a.anim\nstate s clip a\nroot_motion A\nroot_motion B\nstart s\n",
                   "ligne 4 : root_motion déjà défini");
    EXPECT_REFUSED("root_motion\n", "root_motion ARTICULATION");
    EXPECT_REFUSED("root_motion a b\n", "root_motion ARTICULATION");
    EXPECT_REFUSED("root_motion bad/name\n", "nom de articulation invalide");
}
