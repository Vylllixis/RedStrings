/* Machine à états de combat (M5-02). La table attendue est recopiée ici à la main depuis
 * docs/combat-design.md §5 : si le code diverge du document, un test casse. */
#include <string.h>

#include "engine/assert.h"
#include "engine/combat_state.h"
#include "utest.h"

static const struct {
    EngCombatState from;
    EngCombatState to;
} LEGAL_PAIRS[] = {
    {ENG_CS_IDLE, ENG_CS_MOVE},       {ENG_CS_IDLE, ENG_CS_ATTACK},
    {ENG_CS_IDLE, ENG_CS_DODGE},      {ENG_CS_IDLE, ENG_CS_PARRY},
    {ENG_CS_IDLE, ENG_CS_HITSTUN},    {ENG_CS_IDLE, ENG_CS_STAGGER},
    {ENG_CS_IDLE, ENG_CS_DEAD},       {ENG_CS_MOVE, ENG_CS_IDLE},
    {ENG_CS_MOVE, ENG_CS_ATTACK},     {ENG_CS_MOVE, ENG_CS_DODGE},
    {ENG_CS_MOVE, ENG_CS_PARRY},      {ENG_CS_MOVE, ENG_CS_HITSTUN},
    {ENG_CS_MOVE, ENG_CS_STAGGER},    {ENG_CS_MOVE, ENG_CS_DEAD},
    {ENG_CS_ATTACK, ENG_CS_IDLE},     {ENG_CS_ATTACK, ENG_CS_ATTACK},
    {ENG_CS_ATTACK, ENG_CS_DODGE},    {ENG_CS_ATTACK, ENG_CS_HITSTUN},
    {ENG_CS_ATTACK, ENG_CS_STAGGER},  {ENG_CS_ATTACK, ENG_CS_DEAD},
    {ENG_CS_DODGE, ENG_CS_IDLE},      {ENG_CS_DODGE, ENG_CS_HITSTUN},
    {ENG_CS_DODGE, ENG_CS_STAGGER},   {ENG_CS_DODGE, ENG_CS_DEAD},
    {ENG_CS_PARRY, ENG_CS_BLOCK},     {ENG_CS_PARRY, ENG_CS_IDLE},
    {ENG_CS_PARRY, ENG_CS_HITSTUN},   {ENG_CS_PARRY, ENG_CS_STAGGER},
    {ENG_CS_PARRY, ENG_CS_DEAD},      {ENG_CS_BLOCK, ENG_CS_IDLE},
    {ENG_CS_BLOCK, ENG_CS_ATTACK},    {ENG_CS_BLOCK, ENG_CS_DODGE},
    {ENG_CS_BLOCK, ENG_CS_HITSTUN},   {ENG_CS_BLOCK, ENG_CS_STAGGER},
    {ENG_CS_BLOCK, ENG_CS_DEAD},      {ENG_CS_HITSTUN, ENG_CS_IDLE},
    {ENG_CS_HITSTUN, ENG_CS_STAGGER}, {ENG_CS_HITSTUN, ENG_CS_DEAD},
    {ENG_CS_STAGGER, ENG_CS_IDLE},    {ENG_CS_STAGGER, ENG_CS_DEAD},
};
#define N_LEGAL (sizeof LEGAL_PAIRS / sizeof LEGAL_PAIRS[0])

/* Valeur d'énumération hors bornes, sans conversion que l'analyseur statique juge suspecte. */
static EngCombatState bogus(int v)
{
    EngCombatState s;
    memcpy(&s, &v, sizeof s);
    return s;
}

static bool in_table(EngCombatState a, EngCombatState b)
{
    for (size_t i = 0; i < N_LEGAL; i++) {
        if (LEGAL_PAIRS[i].from == a && LEGAL_PAIRS[i].to == b) {
            return true;
        }
    }
    return false;
}

UTEST(combat_state, the_table_matches_the_design_document_for_every_pair)
{
    for (int a = 0; a < ENG_CS_COUNT; a++) {
        for (int b = 0; b < ENG_CS_COUNT; b++) {
            EXPECT_EQ(eng_combat_legal((EngCombatState)a, (EngCombatState)b),
                      in_table((EngCombatState)a, (EngCombatState)b));
        }
    }
}

UTEST(combat_state, dead_is_final_and_every_other_state_can_die)
{
    for (int b = 0; b < ENG_CS_COUNT; b++) {
        EXPECT_FALSE(eng_combat_legal(ENG_CS_DEAD, (EngCombatState)b));
    }
    EXPECT_EQ(eng_combat_legal_mask(ENG_CS_DEAD), 0U);
    for (int a = 0; a < ENG_CS_DEAD; a++) {
        EXPECT_TRUE(eng_combat_legal((EngCombatState)a, ENG_CS_DEAD));
    }
}

UTEST(combat_state, out_of_range_states_are_refused_everywhere)
{
    EXPECT_FALSE(eng_combat_legal(bogus(99), ENG_CS_IDLE));
    EXPECT_FALSE(eng_combat_legal(ENG_CS_IDLE, bogus(99)));
    EXPECT_FALSE(eng_combat_legal(bogus(-1), ENG_CS_IDLE));
    EXPECT_EQ(eng_combat_legal_mask(bogus(99)), 0U);
    EXPECT_STREQ(eng_combat_state_name(bogus(99)), "?");
    EXPECT_STREQ(eng_combat_state_name(ENG_CS_STAGGER), "stagger");
    EXPECT_STREQ(eng_combat_state_name(ENG_CS_PARRY), "parry");
}

UTEST(combat_state, a_legal_request_changes_state_and_resets_ticks)
{
    EngCombatFsm f;
    eng_fsm_init(&f);
    eng_fsm_tick(&f);
    eng_fsm_tick(&f);
    EXPECT_EQ(f.ticks, 2U);
    EXPECT_TRUE(eng_fsm_request(&f, ENG_CS_ATTACK));
    EXPECT_EQ(f.state, ENG_CS_ATTACK);
    EXPECT_EQ(f.previous, ENG_CS_IDLE);
    EXPECT_EQ(f.ticks, 0U);
    EXPECT_EQ(f.transitions, 1U);
    EXPECT_EQ(f.refused, 0U);
}

UTEST(combat_state, an_illegal_request_is_refused_counted_and_leaves_the_state_untouched)
{
    EngCombatFsm f;
    eng_fsm_init(&f);
    eng_assert_set_break(false);
    const int before = eng_assert_failure_count();
    EXPECT_TRUE(eng_fsm_request(&f, ENG_CS_STAGGER));
    EXPECT_FALSE(eng_fsm_request(&f, ENG_CS_BLOCK)); /* STAGGER -> BLOCK interdit */
    EXPECT_EQ(f.state, ENG_CS_STAGGER);
    EXPECT_EQ(f.refused, 1U);
    EXPECT_EQ(f.transitions, 1U);
#if ENG_ASSERTS_ENABLED
    EXPECT_EQ(eng_assert_failure_count(), before + 1); /* « Aucune transition illégale » */
#else
    EXPECT_EQ(eng_assert_failure_count(), before);
#endif
    eng_assert_set_break(true);
}

UTEST(combat_state, requesting_the_current_state_is_not_a_transition_except_attack_to_attack)
{
    EngCombatFsm f;
    eng_fsm_init(&f);
    eng_fsm_tick(&f);
    EXPECT_TRUE(eng_fsm_request(&f, ENG_CS_IDLE));
    EXPECT_EQ(f.ticks, 1U); /* pas remis à zéro */
    EXPECT_EQ(f.transitions, 0U);
    EXPECT_TRUE(eng_fsm_request(&f, ENG_CS_ATTACK));
    eng_fsm_tick(&f);
    EXPECT_TRUE(eng_fsm_request(&f, ENG_CS_ATTACK)); /* combo */
    EXPECT_EQ(f.ticks, 0U);
    EXPECT_EQ(f.transitions, 2U);
    EXPECT_EQ(f.previous, ENG_CS_ATTACK);
}

UTEST(combat_state, nothing_leaves_dead_and_invalid_requests_do_not_count_as_state)
{
    EngCombatFsm f;
    eng_fsm_init(&f);
    EXPECT_TRUE(eng_fsm_request(&f, ENG_CS_DEAD));
    eng_assert_set_break(false);
    EXPECT_FALSE(eng_fsm_request(&f, ENG_CS_IDLE));
    EXPECT_FALSE(eng_fsm_request(&f, bogus(42)));
    EXPECT_EQ(f.state, ENG_CS_DEAD);
    EXPECT_EQ(f.refused, 2U);
    EXPECT_TRUE(eng_fsm_request(&f, ENG_CS_DEAD)); /* rester mort : sans effet */
    EXPECT_EQ(f.refused, 2U);
    eng_assert_set_break(true);
}

UTEST(combat_state, the_mask_lists_exactly_the_reachable_states)
{
    const uint32_t m = eng_combat_legal_mask(ENG_CS_STAGGER);
    EXPECT_EQ(m, ((1U << ENG_CS_IDLE) | (1U << ENG_CS_DEAD)));
}

UTEST(combat_state, ticks_saturate_instead_of_wrapping)
{
    EngCombatFsm f;
    eng_fsm_init(&f);
    f.ticks = UINT32_MAX;
    eng_fsm_tick(&f);
    EXPECT_EQ(f.ticks, UINT32_MAX);
}

UTEST(combat_state, the_first_value_past_the_last_state_is_out_of_range)
{
    EXPECT_FALSE(eng_combat_legal(bogus(ENG_CS_COUNT), ENG_CS_IDLE));
    EXPECT_FALSE(eng_combat_legal(ENG_CS_IDLE, bogus(ENG_CS_COUNT)));
    EXPECT_EQ(eng_combat_legal_mask(bogus(ENG_CS_COUNT)), 0U);
    EXPECT_STREQ(eng_combat_state_name(bogus(ENG_CS_COUNT)), "?");
    EXPECT_STREQ(eng_combat_state_name(ENG_CS_DEAD), "dead");
    EngCombatFsm f;
    eng_fsm_init(&f);
    eng_assert_set_break(false);
    EXPECT_FALSE(eng_fsm_request(&f, bogus(ENG_CS_COUNT))); /* refus, état inchangé */
    EXPECT_EQ(f.state, ENG_CS_IDLE);
    EXPECT_EQ(f.refused, 1U);
    eng_assert_set_break(true);
}
