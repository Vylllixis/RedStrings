/* Données d'attaque (M5-03) : analyse, phases, fenêtre d'annulation, rechargement à chaud. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/attack_data.h"
#include "test_util.h"
#include "utest.h"

#define DIR TEST_TMP_DIR "/attacks"

static const char *const GOOD =
    "# deux coups d'un combo et une lourde\n"
    "attack light1\n"
    "  startup 8\n  active 4\n  recovery 14\n  damage 10\n  posture 8\n  stamina 10\n"
    "  hitstop 3\n  cancel_from 6\n  next light2\n"
    "  hitbox hand_r 0 0 0  0 0 0.6  0.12   # arme\n"
    "end\n"
    "attack light2\n"
    "  startup 7\n  active 4\n  recovery 14\n  damage 12.5\n"
    "  hitbox hand_r 0 0 0 0 0 0.6 0.12\n"
    "  hitbox hand_l 0 0 0 0 0.1 0.3 0.2\n"
    "end\n";

static bool bad(const char *text, const char *expect)
{
    EngAttackSet s;
    char err[160];
    const bool ok = eng_attackset_parse(text, &s, err, sizeof err);
    if (ok) {
        return false;
    }
    if (s.count != 0) { /* tout ou rien : rien n'est à moitié lu */
        return false;
    }
    if (strstr(err, expect) == NULL) {
        (void)fprintf(stderr, "erreur attendue « %s », reçue « %s »\n", expect, err);
        return false;
    }
    return true;
}

UTEST(attack_data, a_valid_file_is_read_field_by_field)
{
    EngAttackSet s;
    char err[160];
    ASSERT_TRUE(eng_attackset_parse(GOOD, &s, err, sizeof err));
    ASSERT_EQ(s.count, 2U);
    const EngAttack *a = &s.attacks[0];
    EXPECT_STREQ(a->name, "light1");
    EXPECT_EQ(a->startup, 8U);
    EXPECT_EQ(a->active, 4U);
    EXPECT_EQ(a->recovery, 14U);
    EXPECT_EQ(a->damage, 10.0F);
    EXPECT_EQ(a->posture, 8.0F);
    EXPECT_EQ(a->stamina, 10.0F);
    EXPECT_EQ(a->hitstop, 3U);
    EXPECT_EQ(a->cancel_from, 6U);
    EXPECT_EQ(a->next, 1U); /* référence en avant résolue */
    ASSERT_EQ(a->hitbox_count, 1U);
    EXPECT_STREQ(a->hitboxes[0].joint, "hand_r");
    EXPECT_EQ(a->hitboxes[0].b[2], 0.6F);
    EXPECT_EQ(a->hitboxes[0].radius, 0.12F);
    const EngAttack *b = &s.attacks[1];
    EXPECT_EQ(b->damage, 12.5F);
    EXPECT_EQ(b->posture, 0.0F); /* défauts */
    EXPECT_EQ(b->stamina, 0.0F);
    EXPECT_EQ(b->hitstop, 0U);
    EXPECT_EQ(b->cancel_from, 14U); /* défaut = recovery : pas d'annulation anticipée */
    EXPECT_EQ(b->next, ENG_ATTACK_NONE);
    EXPECT_EQ(b->hitbox_count, 2U);
    EXPECT_STREQ(b->hitboxes[1].joint, "hand_l");
    EXPECT_EQ(eng_attackset_find(&s, "light2"), 1U);
    EXPECT_EQ(eng_attackset_find(&s, "nope"), ENG_ATTACK_NONE);
    EXPECT_EQ(eng_attackset_find(NULL, "light2"), ENG_ATTACK_NONE);
}

UTEST(attack_data, phases_follow_startup_active_recovery_exactly)
{
    EngAttackSet s;
    ASSERT_TRUE(eng_attackset_parse(GOOD, &s, NULL, 0));
    const EngAttack *a = &s.attacks[0]; /* 8 / 4 / 14 */
    EXPECT_EQ(eng_attack_total(a), 26U);
    EXPECT_EQ(eng_attack_phase(a, 0), ENG_ATTACK_STARTUP);
    EXPECT_EQ(eng_attack_phase(a, 7), ENG_ATTACK_STARTUP);
    EXPECT_EQ(eng_attack_phase(a, 8), ENG_ATTACK_ACTIVE);
    EXPECT_EQ(eng_attack_phase(a, 11), ENG_ATTACK_ACTIVE);
    EXPECT_EQ(eng_attack_phase(a, 12), ENG_ATTACK_RECOVERY);
    EXPECT_EQ(eng_attack_phase(a, 25), ENG_ATTACK_RECOVERY);
    EXPECT_EQ(eng_attack_phase(a, 26), ENG_ATTACK_DONE);
    EXPECT_EQ(eng_attack_phase(a, 99999), ENG_ATTACK_DONE);
    int active = 0;
    for (uint32_t t = 0; t < 40; t++) {
        active += eng_attack_is_active(a, t) ? 1 : 0;
    }
    EXPECT_EQ(active, 4);
}

UTEST(attack_data, the_cancel_window_opens_at_cancel_from_into_the_recovery_and_closes_at_the_end)
{
    EngAttackSet s;
    ASSERT_TRUE(eng_attackset_parse(GOOD, &s, NULL, 0));
    const EngAttack *a = &s.attacks[0]; /* fenêtre : 12 + 6 = 18 inclus, 26 exclu */
    EXPECT_FALSE(eng_attack_can_cancel(a, 17));
    EXPECT_TRUE(eng_attack_can_cancel(a, 18));
    EXPECT_TRUE(eng_attack_can_cancel(a, 25));
    EXPECT_FALSE(eng_attack_can_cancel(a, 26));
    EXPECT_FALSE(eng_attack_can_cancel(a, 5)); /* jamais pendant le startup / l'actif */
    const EngAttack *b = &s.attacks[1];        /* cancel_from = recovery : fenêtre vide */
    for (uint32_t t = 0; t < 40; t++) {
        EXPECT_FALSE(eng_attack_can_cancel(b, t));
    }
}

UTEST(attack_data, every_malformed_file_is_refused_with_its_line_number)
{
    EXPECT_TRUE(bad("", "ligne 1 : aucune attaque définie"));
    EXPECT_TRUE(bad("startup 3\n", "ligne 1 : « startup » hors d'une attaque"));
    EXPECT_TRUE(bad("attack\n", "`attack NOM` attendu"));
    EXPECT_TRUE(bad("attack a b\n", "`attack NOM` attendu"));
    EXPECT_TRUE(bad("attack a$\n", "`attack NOM` attendu"));
    EXPECT_TRUE(bad("attack a\nattack b\n", "ligne 2 : `attack` dans une attaque non terminée"));
    EXPECT_TRUE(bad("end\n", "ligne 1 : `end` sans `attack`"));
    EXPECT_TRUE(bad("attack a\n", "`end` manquant"));
    EXPECT_TRUE(bad("attack a\nbogus 1\nend\n", "ligne 2 : mot inconnu « bogus »"));
    EXPECT_TRUE(bad("attack a\nstartup 0\nend\n", "`startup` : entier de 1 à 600"));
    EXPECT_TRUE(bad("attack a\nactive 0\nend\n", "`active` : entier de 1 à 600"));
    EXPECT_TRUE(bad("attack a\nrecovery -1\nend\n", "`recovery`"));
    EXPECT_TRUE(bad("attack a\nstartup 601\nend\n", "`startup`"));
    EXPECT_TRUE(bad("attack a\nstartup 2.5\nend\n", "`startup`"));
    EXPECT_TRUE(bad("attack a\nstartup 1 2\nend\n", "`startup` attend une seule valeur"));
    EXPECT_TRUE(bad("attack a\nstartup\nend\n", "`startup` attend une seule valeur"));
    EXPECT_TRUE(bad("attack a\nstartup 1\nstartup 2\nend\n", "`startup` déclaré deux fois"));
    EXPECT_TRUE(bad("attack a\ndamage -1\nend\n", "`damage` : nombre >= 0"));
    EXPECT_TRUE(bad("attack a\ndamage nan\nend\n", "`damage`"));
    EXPECT_TRUE(bad("attack a\ndamage inf\nend\n", "`damage`"));
    EXPECT_TRUE(bad("attack a\ndamage x\nend\n", "`damage`"));
    EXPECT_TRUE(bad("attack a\nposture -3\nend\n", "`posture`"));
    EXPECT_TRUE(bad("attack a\nstamina -3\nend\n", "`stamina`"));
    EXPECT_TRUE(bad("attack a\nhitstop 31\nend\n", "`hitstop` : entier de 0 à 30"));
    EXPECT_TRUE(bad("attack a\ncancel_from x\nend\n", "`cancel_from`"));
    EXPECT_TRUE(bad("attack a\nnext b$\nend\n", "`next` : nom invalide"));
    EXPECT_TRUE(
        bad("attack a\nstartup 1\nactive 1\nrecovery 1\ndamage 1\nend\n", "au moins une `hitbox`"));
    EXPECT_TRUE(bad("attack a\nactive 1\nrecovery 1\ndamage 1\nhitbox j 0 0 0 0 0 1 1\nend\n",
                    "obligatoires"));
    EXPECT_TRUE(bad("attack a\nhitbox j 0 0 0 0 0 1\nend\n", "`hitbox OS ax ay az"));
    EXPECT_TRUE(bad("attack a\nhitbox j 0 0 0 0 0 x 1\nend\n", "nombre invalide"));
    EXPECT_TRUE(bad("attack a\nhitbox j 0 0 0 0 0 1 0\nend\n", "rayon dans ]0 ; 10]"));
    EXPECT_TRUE(bad("attack a\nhitbox j 0 0 0 0 0 1 11\nend\n", "rayon dans ]0 ; 10]"));
    EXPECT_TRUE(bad("attack a\nhitbox j$ 0 0 0 0 0 1 1\nend\n", "nom d'os invalide"));
}

UTEST(attack_data, structural_limits_and_cross_checks_are_enforced)
{
    const char *req = "startup 1\nactive 1\nrecovery 5\ndamage 1\nhitbox j 0 0 0 0 0 1 1\n";
    char t[1024];
    (void)snprintf(t, sizeof t, "attack a\n%scancel_from 6\nend\n", req);
    EXPECT_TRUE(bad(t, "`cancel_from` (6) dépasse `recovery` (5)"));
    (void)snprintf(t, sizeof t, "attack a\n%scancel_from 5\nend\n", req);
    EngAttackSet s;
    EXPECT_TRUE(eng_attackset_parse(t, &s, NULL, 0)); /* = recovery : accepté */
    (void)snprintf(t, sizeof t, "attack a\n%snext zz\nend\n", req);
    EXPECT_TRUE(bad(t, "ligne 7 : `next` : attaque inconnue « zz »"));
    (void)snprintf(t, sizeof t, "attack a\n%send\nattack a\n%send\n", req, req);
    EXPECT_TRUE(bad(t, "attaque « a » déclarée deux fois"));
    (void)snprintf(t, sizeof t, "attack a\n%s%s%send\n", req,
                   "hitbox j 0 0 0 0 0 1 1\nhitbox j 0 0 0 0 0 1 1\nhitbox j 0 0 0 0 0 1 1\n",
                   "hitbox j 0 0 0 0 0 1 1\n");
    EXPECT_TRUE(bad(t, "plus de 4 hitboxes")); /* 1 + 4 = 5 hitboxes */
    /* un combo peut boucler sur lui-même */
    (void)snprintf(t, sizeof t, "attack a\n%snext a\nend\n", req);
    ASSERT_TRUE(eng_attackset_parse(t, &s, NULL, 0));
    EXPECT_EQ(s.attacks[0].next, 0U);
}

UTEST(attack_data, at_most_32_attacks_and_overlong_lines_are_refused)
{
    static char big[32768];
    size_t n = 0;
    for (int i = 0; i < 33; i++) {
        n += (size_t)snprintf(big + n, sizeof big - n,
                              "attack a%d\nstartup 1\nactive 1\nrecovery 1\ndamage 1\n"
                              "hitbox j 0 0 0 0 0 1 1\nend\n",
                              i);
    }
    EXPECT_TRUE(bad(big, "plus de 32 attaques"));
    big[0] = '\0';
    n = 0;
    for (int i = 0; i < 32; i++) {
        n += (size_t)snprintf(big + n, sizeof big - n,
                              "attack a%d\nstartup 1\nactive 1\nrecovery 1\ndamage 1\n"
                              "hitbox j 0 0 0 0 0 1 1\nend\n",
                              i);
    }
    EngAttackSet s;
    EXPECT_TRUE(eng_attackset_parse(big, &s, NULL, 0));
    EXPECT_EQ(s.count, 32U);
    char line[700];
    memset(line, 'x', sizeof line - 1);
    line[sizeof line - 1] = '\0';
    EXPECT_TRUE(bad(line, "ligne trop longue"));
}

UTEST(attack_data, error_buffers_are_handled_safely)
{
    EngAttackSet s;
    char err[8];
    EXPECT_FALSE(eng_attackset_parse("coucou\n", &s, err, sizeof err));
    EXPECT_LT(strlen(err), sizeof err);
    EXPECT_FALSE(eng_attackset_parse("coucou\n", &s, NULL, 0));
    char one[1];
    EXPECT_FALSE(eng_attackset_parse("coucou\n", &s, one, sizeof one));
    EXPECT_FALSE(eng_attackset_parse(NULL, &s, NULL, 0));
    EXPECT_FALSE(eng_attackset_parse("", NULL, NULL, 0));
}

static void write_text(const char *path, const char *text)
{
    (void)SDL_SaveFile(path, text, strlen(text));
}

static bool poll_until_loaded(EngAttackFile *f, EngArena *scratch, double *now)
{
    for (int i = 0; i < 8; i++) {
        *now += 1.0;
        if (eng_attackfile_poll(f, scratch, *now)) {
            return true;
        }
    }
    return false;
}

UTEST(attack_data, changing_an_active_frame_reloads_without_recompiling)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
    const char *path = DIR "/attacks.txt";
    write_text(path, GOOD);
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&scratch, "scratch", (size_t)1 << 16));
    EngAttackFile f;
    ASSERT_TRUE(eng_attackfile_open(&f, &scratch, path));
    EXPECT_EQ(f.version, 1U);
    EXPECT_EQ(f.set.attacks[0].active, 4U);
    EXPECT_EQ(scratch.used, 0U);
    double now = 0.0;
    EXPECT_FALSE(eng_attackfile_poll(&f, &scratch, now + 1.0));

    /* « Modifier une frame active sans recompiler » : active 4 -> 9 */
    const char *v2 = "attack light1\nstartup 8\nactive 9\nrecovery 14\ndamage 10\n"
                     "hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n";
    write_text(path, v2);
    ASSERT_TRUE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 2U);
    EXPECT_EQ(f.set.attacks[0].active, 9U);
    EXPECT_EQ(f.set.count, 1U);

    /* version invalide : refusée, l'ancienne reste */
    write_text(path, "attack light1\nstartup 0\nend\n");
    EXPECT_FALSE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 2U);
    EXPECT_EQ(f.set.attacks[0].active, 9U);
    EXPECT_NE(strstr(f.error, "`startup`"), NULL);
    EXPECT_EQ(scratch.used, 0U);

    write_text(path, GOOD);
    ASSERT_TRUE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_EQ(f.version, 3U);
    EXPECT_EQ(f.set.count, 2U);
    EXPECT_STREQ(f.error, "");

    const char binary[] = {'a', '\0', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    (void)SDL_SaveFile(path, binary, sizeof binary);
    EXPECT_FALSE(poll_until_loaded(&f, &scratch, &now));
    EXPECT_NE(strstr(f.error, "binaire"), NULL);
    EXPECT_EQ(f.version, 3U);
    eng_arena_shutdown(&scratch);
}

UTEST(attack_data, opening_a_missing_or_invalid_file_fails_cleanly)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&scratch, "scratch", (size_t)1 << 16));
    EngAttackFile f;
    EXPECT_FALSE(eng_attackfile_open(&f, &scratch, DIR "/absent.txt"));
    EXPECT_FALSE(f.valid);
    write_text(DIR "/bad.txt", "attack a\nend\n");
    EXPECT_FALSE(eng_attackfile_open(&f, &scratch, DIR "/bad.txt"));
    EXPECT_FALSE(f.valid);
    EXPECT_NE(f.error[0], '\0');
    EXPECT_FALSE(eng_attackfile_open(NULL, &scratch, DIR "/bad.txt"));
    EXPECT_FALSE(eng_attackfile_open(&f, NULL, DIR "/bad.txt"));
    EXPECT_FALSE(eng_attackfile_open(&f, &scratch, NULL));
    eng_arena_shutdown(&scratch);
}

static bool parses(const char *text)
{
    EngAttackSet s;
    return eng_attackset_parse(text, &s, NULL, 0);
}

UTEST(attack_data, numeric_and_name_boundaries_are_exact)
{
    char t[512];
    const char *tail = "damage 1\nhitbox j 0 0 0 0 0 1 1\nend\n";
    (void)snprintf(t, sizeof t, "attack a\nstartup 600\nactive 600\nrecovery 600\n%s", tail);
    EXPECT_TRUE(parses(t));
    (void)snprintf(t, sizeof t, "attack a\nstartup 1\nactive 601\nrecovery 1\n%s", tail);
    EXPECT_FALSE(parses(t));
    (void)snprintf(t, sizeof t, "attack a\nstartup 1\nactive 1\nrecovery 601\n%s", tail);
    EXPECT_FALSE(parses(t));
    (void)snprintf(t, sizeof t, "attack a\nstartup 1\nactive 1\nrecovery 0\n%s", tail);
    EXPECT_TRUE(parses(t));
    (void)snprintf(t, sizeof t, "attack a\nstartup 1\nactive 1\nrecovery 1\nhitstop 30\n%s", tail);
    EXPECT_TRUE(parses(t));
    (void)snprintf(t, sizeof t,
                   "attack a\nstartup 1\nactive 1\nrecovery 1\ndamage 100000\nhitbox j 0 0 0 0 0 1 "
                   "10\nend\n");
    EXPECT_TRUE(parses(t));
    (void)snprintf(t, sizeof t,
                   "attack a\nstartup 1\nactive 1\nrecovery 1\ndamage 100001\nhitbox j 0 0 0 0 0 1 "
                   "1\nend\n");
    EXPECT_FALSE(parses(t));
    (void)snprintf(t, sizeof t,
                   "attack a\nstartup 1\nactive 1\nrecovery 1\ndamage 1\nposture 0\nstamina 0\n"
                   "hitbox j 0 0 0 0 0 1 0.001\nend\n");
    EXPECT_TRUE(parses(t));
    /* noms : 31 caractères acceptés, 32 refusés */
    char name31[32];
    memset(name31, 'n', 31);
    name31[31] = '\0';
    (void)snprintf(t, sizeof t, "attack %s\nstartup 1\nactive 1\nrecovery 1\n%s", name31, tail);
    EXPECT_TRUE(parses(t));
    (void)snprintf(t, sizeof t, "attack %sn\nstartup 1\nactive 1\nrecovery 1\n%s", name31, tail);
    EXPECT_FALSE(parses(t));
    /* un `cancel_from` supérieur à 600 est refusé avant même la comparaison à recovery */
    (void)snprintf(t, sizeof t, "attack a\nstartup 1\nactive 1\nrecovery 600\ncancel_from 601\n%s",
                   tail);
    EXPECT_FALSE(parses(t));
    /* 16 mots sur une ligne passent le découpage mais ne forment pas une instruction valide :
     * on vérifie le message, pas seulement le refus */
    EXPECT_TRUE(bad("attack a\nhitbox j 0 0 0 0 0 1 1 1 1 1 1 1 1 1 1 1 1\nend\n", "trop de mots"));
    EXPECT_TRUE(bad("attack a\nhitbox j 0 0 0 0 0 1 1 1\nend\n", "`hitbox OS"));
}

UTEST(attack_data, a_phase_boundary_is_checked_at_the_exact_tick_for_other_data)
{
    EngAttackSet s;
    ASSERT_TRUE(eng_attackset_parse("attack a\nstartup 3\nactive 2\nrecovery 5\ncancel_from 2\n"
                                    "damage 1\nhitbox j 0 0 0 0 0 1 1\nend\n",
                                    &s, NULL, 0));
    const EngAttack *a = &s.attacks[0];
    const EngAttackPhase want[] = {ENG_ATTACK_STARTUP,  ENG_ATTACK_STARTUP,  ENG_ATTACK_STARTUP,
                                   ENG_ATTACK_ACTIVE,   ENG_ATTACK_ACTIVE,   ENG_ATTACK_RECOVERY,
                                   ENG_ATTACK_RECOVERY, ENG_ATTACK_RECOVERY, ENG_ATTACK_RECOVERY,
                                   ENG_ATTACK_RECOVERY, ENG_ATTACK_DONE};
    for (uint32_t t = 0; t < 11; t++) {
        EXPECT_EQ(eng_attack_phase(a, t), want[t]);
    }
    for (uint32_t t = 0; t < 12; t++) {
        EXPECT_EQ(eng_attack_can_cancel(a, t), t >= 7 && t < 10);
    }
}
