/* Événements d'animation (M5-04) : analyse, déclenchement exactement une fois par passage,
 * rebouclage, fin de clip, rechargement à chaud. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/anim_events.h"
#include "test_util.h"
#include "utest.h"

#define DIR TEST_TMP_DIR "/animevents"

static const char *const GOOD =
    "# attaque légère\n"
    "event light1 0.300 cancel_open\n"
    "event light1 0.133 hitbox_on\n" /* désordre voulu */
    "event light1 0.200 sound swoosh_light\n"
    "event light1 0.200 hitbox_off   # même instant : ordre du fichier\n"
    "event heavy 0.350 effect trail_heavy\n";

static bool bad(const char *text, const char *expect)
{
    EngAnimEventSet s;
    char err[160];
    if (eng_animevents_parse(text, &s, err, sizeof err) || s.count != 0) {
        return false;
    }
    if (strstr(err, expect) == NULL) {
        (void)fprintf(stderr, "erreur attendue « %s », reçue « %s »\n", expect, err);
        return false;
    }
    return true;
}

UTEST(anim_events, a_file_is_read_sorted_by_clip_then_time_keeping_file_order_on_ties)
{
    EngAnimEventSet s;
    char err[160];
    ASSERT_TRUE(eng_animevents_parse(GOOD, &s, err, sizeof err));
    ASSERT_EQ(s.count, 5U);
    EXPECT_STREQ(s.events[0].clip, "heavy");
    EXPECT_EQ(s.events[0].type, ENG_ANIMEVENT_EFFECT);
    EXPECT_STREQ(s.events[0].arg, "trail_heavy");
    EXPECT_STREQ(s.events[1].clip, "light1");
    EXPECT_EQ(s.events[1].type, ENG_ANIMEVENT_HITBOX_ON);
    EXPECT_EQ(s.events[1].time, 0.133F);
    EXPECT_STREQ(s.events[1].arg, "");
    EXPECT_EQ(s.events[2].type, ENG_ANIMEVENT_SOUND); /* à 0,2 : « sound » avant « hitbox_off » */
    EXPECT_STREQ(s.events[2].arg, "swoosh_light");
    EXPECT_EQ(s.events[3].type, ENG_ANIMEVENT_HITBOX_OFF);
    EXPECT_EQ(s.events[4].type, ENG_ANIMEVENT_CANCEL_OPEN);
    EXPECT_STREQ(eng_animevent_type_name(ENG_ANIMEVENT_CANCEL_CLOSE), "cancel_close");
    EXPECT_STREQ(eng_animevent_type_name(ENG_ANIMEVENT_TYPE_COUNT), "?");
}

UTEST(anim_events, every_malformed_line_is_refused_with_its_number)
{
    EXPECT_TRUE(bad("coucou\n", "ligne 1 : mot inconnu « coucou »"));
    EXPECT_TRUE(bad("event a 0.1\n", "`event CLIP TEMPS TYPE [ARG]` attendu"));
    EXPECT_TRUE(bad("event a 0.1 hitbox_on x y\n", "`event CLIP TEMPS TYPE [ARG]` attendu"));
    EXPECT_TRUE(bad("event a$ 0.1 hitbox_on\n", "nom de clip invalide"));
    EXPECT_TRUE(bad("event a x hitbox_on\n", "temps dans [0 ; 3600]"));
    EXPECT_TRUE(bad("event a -0.1 hitbox_on\n", "temps dans [0 ; 3600]"));
    EXPECT_TRUE(bad("event a nan hitbox_on\n", "temps dans [0 ; 3600]"));
    EXPECT_TRUE(bad("event a inf hitbox_on\n", "temps dans [0 ; 3600]"));
    EXPECT_TRUE(bad("event a 3601 hitbox_on\n", "temps dans [0 ; 3600]"));
    EXPECT_TRUE(bad("event a 0.1 boom\n", "type inconnu « boom »"));
    EXPECT_TRUE(bad("event a 0.1 sound\n", "`sound` demande un argument"));
    EXPECT_TRUE(bad("event a 0.1 effect\n", "`effect` demande un argument"));
    EXPECT_TRUE(bad("event a 0.1 hitbox_on x\n", "`hitbox_on` n'accepte pas d'argument"));
    EXPECT_TRUE(bad("event a 0.1 sound x$\n", "argument invalide"));
    EXPECT_TRUE(bad("event a 0.1 sound ok\nevent a 0.1 bogus\n", "ligne 2 : type inconnu"));
    EXPECT_TRUE(bad("event a 1 hitbox_on 2 3 4 5 6 7 8\n", "trop de mots"));
    char line[400];
    memset(line, 'x', sizeof line - 1);
    line[sizeof line - 1] = '\0';
    EXPECT_TRUE(bad(line, "ligne trop longue"));
}

UTEST(anim_events, limits_and_boundary_values)
{
    EngAnimEventSet s;
    EXPECT_TRUE(eng_animevents_parse("", &s, NULL, 0)); /* un fichier vide est valide */
    EXPECT_EQ(s.count, 0U);
    EXPECT_TRUE(
        eng_animevents_parse("event a 0 hitbox_on\nevent a 3600 hitbox_off\n", &s, NULL, 0));
    EXPECT_EQ(s.count, 2U);
    static char big[8192];
    size_t n = 0;
    for (int i = 0; i < 129; i++) {
        n += (size_t)snprintf(big + n, sizeof big - n, "event a %d.5 hitbox_on\n", i);
    }
    EXPECT_TRUE(bad(big, "plus de 128 événements"));
    n = 0;
    for (int i = 0; i < 128; i++) {
        n += (size_t)snprintf(big + n, sizeof big - n, "event a %d.5 hitbox_on\n", i);
    }
    EXPECT_TRUE(eng_animevents_parse(big, &s, NULL, 0));
    EXPECT_EQ(s.count, 128U);
    char name32[40];
    memset(name32, 'n', 32);
    name32[32] = '\0';
    char t[128];
    (void)snprintf(t, sizeof t, "event %s 0 hitbox_on\n", name32);
    EXPECT_FALSE(eng_animevents_parse(t, &s, NULL, 0));
    (void)snprintf(t, sizeof t, "event %s 0 hitbox_on\n", name32 + 1);
    EXPECT_TRUE(eng_animevents_parse(t, &s, NULL, 0));
    char one[1];
    EXPECT_FALSE(eng_animevents_parse("coucou\n", &s, one, sizeof one));
    EXPECT_FALSE(eng_animevents_parse("coucou\n", &s, NULL, 0));
    EXPECT_FALSE(eng_animevents_parse(NULL, &s, NULL, 0));
    EXPECT_FALSE(eng_animevents_parse("", NULL, NULL, 0));
}

static EngAnimEventSet make_set(const char *text)
{
    EngAnimEventSet s;
    (void)eng_animevents_parse(text, &s, NULL, 0);
    return s;
}

/* Joue un clip pas à pas et compte, par événement, combien de fois il est déclenché. */
static void play(const EngAnimEventSet *set, const char *clip, float duration, float step,
                 int steps, bool loop, int *hits)
{
    float t = 0.0F;
    for (int i = 0; i < steps; i++) {
        uint32_t idx[16];
        const float next = t + step;
        const uint32_t n = eng_animevents_collect(set, clip, duration, t, next, loop, idx, 16);
        for (uint32_t k = 0; k < n && k < 16; k++) {
            hits[idx[k]]++;
        }
        t = next;
    }
}

UTEST(anim_events, a_once_clip_fires_each_event_exactly_once_for_any_step_size)
{
    const EngAnimEventSet s = make_set("event a 0 hitbox_on\nevent a 0.2 sound x\n"
                                       "event a 0.5 cancel_open\nevent a 1 hitbox_off\n");
    const float steps[] = {1.0F / 60.0F, 1.0F / 30.0F, 0.07F, 0.25F, 1.0F, 3.0F};
    for (size_t k = 0; k < sizeof steps / sizeof steps[0]; k++) {
        int hits[8] = {0};
        play(&s, "a", 1.0F, steps[k], 200, false, hits); /* bien au-delà de la fin */
        for (uint32_t i = 0; i < s.count; i++) {
            EXPECT_EQ(hits[i], 1); /* y compris l'événement posé exactement à la durée */
        }
    }
}

UTEST(anim_events, a_looping_clip_fires_each_event_once_per_cycle_across_the_wrap)
{
    const EngAnimEventSet s = make_set("event a 0 hitbox_on\nevent a 0.25 sound x\n"
                                       "event a 0.9 hitbox_off\n");
    const float steps[] = {1.0F / 60.0F, 0.07F, 0.3F, 0.5F, 1.0F, 2.5F};
    for (size_t k = 0; k < sizeof steps / sizeof steps[0]; k++) {
        /* durée totale jouée = 4 s exactement : 4 cycles d'une seconde */
        const int count = (int)(4.0F / steps[k] + 0.5F);
        int hits[4] = {0};
        float t = 0.0F;
        for (int i = 0; i < count; i++) { /* temps déplié, comme le fait la machine d'animation */
            uint32_t idx[16];
            const float next = t + steps[k];
            const uint32_t n = eng_animevents_collect(&s, "a", 1.0F, t, next, true, idx, 16);
            for (uint32_t q = 0; q < n && q < 16; q++) {
                hits[idx[q]]++;
            }
            t = next;
        }
        /* exactement une fois par cycle traversé : événement à e, temps final t -> ceil(t - e) */
        for (uint32_t i = 0; i < s.count; i++) {
            const double e = (double)s.events[i].time;
            const int want = ((double)t > e) ? (int)ceil((double)t - e) : 0;
            EXPECT_EQ(hits[i], want);
        }
    }
}

UTEST(anim_events, a_single_step_spanning_several_cycles_reports_every_passage_in_order)
{
    const EngAnimEventSet s = make_set("event a 0.25 sound x\nevent a 0.75 hitbox_off\n");
    uint32_t idx[16];
    const uint32_t n = eng_animevents_collect(&s, "a", 1.0F, 0.0F, 2.5F, true, idx, 16);
    ASSERT_EQ(n, 5U); /* 0,25 0,75 | 1,25 1,75 | 2,25 */
    const uint32_t want[] = {0, 1, 0, 1, 0};
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(idx[i], want[i]);
    }
    /* le total réel est rendu même si `out` est trop petit */
    uint32_t small[2];
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 2.5F, true, small, 2), 5U);
    EXPECT_EQ(small[0], 0U);
    EXPECT_EQ(small[1], 1U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 2.5F, true, NULL, 0), 5U);
}

UTEST(anim_events, the_interval_is_closed_on_the_left_and_open_on_the_right)
{
    const EngAnimEventSet s = make_set("event a 0.5 sound x\n");
    uint32_t idx[4];
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.5F, 0.6F, false, idx, 4), 1U); /* t0 inclus */
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.4F, 0.5F, false, idx, 4), 0U); /* t1 exclu */
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.4F, 0.50001F, false, idx, 4), 1U);
}

UTEST(anim_events, other_clips_empty_or_reversed_steps_and_bad_arguments_fire_nothing)
{
    const EngAnimEventSet s = make_set("event a 0.5 sound x\n");
    uint32_t idx[4];
    EXPECT_EQ(eng_animevents_collect(&s, "b", 1.0F, 0.0F, 1.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.5F, 0.5F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.9F, 0.1F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(NULL, "a", 1.0F, 0.0F, 1.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, NULL, 1.0F, 0.0F, 1.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 0.0F, 0.0F, 1.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", -1.0F, 0.0F, 1.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, NAN, 1.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, INFINITY, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", INFINITY, 0.0F, 1.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, -5.0F, 0.6F, false, idx, 4),
              1U); /* t0 < 0 → 0 */
}

UTEST(anim_events, an_absurd_step_over_more_than_1024_cycles_is_refused_not_looped)
{
    const EngAnimEventSet s = make_set("event a 0.5 sound x\n");
    uint32_t idx[4];
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 1025.0F, true, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 1000.0F, true, idx, 4), 1000U);
}

UTEST(anim_events, an_event_past_the_clip_duration_never_fires)
{
    const EngAnimEventSet s = make_set("event a 1.5 sound x\n");
    uint32_t idx[4];
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 9.0F, false, idx, 4), 0U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 9.0F, true, idx, 4), 0U);
}

UTEST(anim_events, a_looping_event_at_the_duration_fires_once_per_cycle_end)
{
    const EngAnimEventSet s = make_set("event a 1 sound x\n");
    uint32_t idx[8];
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 1.0F, true, idx, 8), 0U); /* t1 exclu */
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 1.01F, true, idx, 8), 1U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 3.5F, true, idx, 8), 3U);
}

static void write_text(const char *path, const char *text)
{
    (void)SDL_SaveFile(path, text, strlen(text));
}

static bool poll_until_loaded(EngAnimEventFile *f, EngArena *scratch, double *now)
{
    for (int i = 0; i < 8; i++) {
        *now += 1.0;
        if (eng_animeventfile_poll(f, scratch, *now)) {
            return true;
        }
    }
    return false;
}

UTEST(anim_events, a_watched_file_reloads_and_keeps_the_good_version_on_errors)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
    const char *path = DIR "/a.events";
    write_text(path, GOOD);
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&scratch, "scratch", (size_t)1 << 16));
    EngAnimEventFile f;
    ASSERT_TRUE(eng_animeventfile_open(&f, &scratch, path));
    EXPECT_EQ(f.version, 1U);
    EXPECT_EQ(f.set.count, 5U);
    EXPECT_EQ(scratch.used, 0U);
    double now = 0.0;
    EXPECT_FALSE(eng_animeventfile_poll(&f, &scratch, now + 1.0));
    write_text(path, "event light1 0.5 hitbox_on\n");
    ASSERT_TRUE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 2U);
    EXPECT_EQ(f.set.count, 1U);
    write_text(path, "event light1 oops hitbox_on\n");
    EXPECT_FALSE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 2U);
    EXPECT_EQ(f.set.count, 1U);
    EXPECT_NE(strstr(f.error, "temps"), NULL);
    write_text(path, GOOD);
    ASSERT_TRUE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 3U);
    EXPECT_STREQ(f.error, "");
    const char binary[] = {'e', '\0', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    (void)SDL_SaveFile(path, binary, sizeof binary);
    EXPECT_FALSE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_NE(strstr(f.error, "binaire"), NULL);
    EXPECT_FALSE(eng_animeventfile_open(&f, &scratch, DIR "/absent.events"));
    EXPECT_FALSE(eng_animeventfile_open(NULL, &scratch, path));
    EXPECT_FALSE(eng_animeventfile_open(&f, NULL, path));
    EXPECT_FALSE(eng_animeventfile_open(&f, &scratch, NULL));
    eng_arena_shutdown(&scratch);
}

UTEST(anim_events, cycles_are_counted_with_a_duration_other_than_one_second)
{
    const EngAnimEventSet s = make_set("event a 0.1 sound x\nevent a 0.4 hitbox_off\n");
    uint32_t idx[64];
    /* durée 0,5 s : en 2 s, 4 cycles complets -> 4 passages de chaque événement */
    EXPECT_EQ(eng_animevents_collect(&s, "a", 0.5F, 0.0F, 2.0F, true, idx, 64), 8U);
    /* ordre chronologique : 0,1 0,4 | 0,6 0,9 ... */
    EXPECT_EQ(idx[0], 0U);
    EXPECT_EQ(idx[1], 1U);
    EXPECT_EQ(idx[2], 0U);
    /* durée 2 s, 3 s jouées : 0,1 0,4 (cycle 0) et 2,1 2,4 (cycle 1) ; sans boucle, 2 s au plus */
    EXPECT_EQ(eng_animevents_collect(&s, "a", 2.0F, 0.0F, 3.0F, true, idx, 64), 4U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 2.0F, 0.0F, 3.0F, false, idx, 64), 2U);
    /* le départ au milieu d'un cycle ne compte pas les événements déjà passés */
    EXPECT_EQ(eng_animevents_collect(&s, "a", 0.5F, 1.05F, 1.45F, true, idx, 64),
              2U); /* 1,1 et 1,4 */
    /* limite exacte de la garde : 1024 cycles acceptés, 1025 refusés */
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 1024.5F, true, idx, 4), 2050U);
    EXPECT_EQ(eng_animevents_collect(&s, "a", 1.0F, 0.0F, 1025.5F, true, idx, 4), 0U);
}
