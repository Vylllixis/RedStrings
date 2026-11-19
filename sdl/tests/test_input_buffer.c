/* Buffer d'entrées de combat (M5-09). */
#include "engine/input_buffer.h"
#include "utest.h"

UTEST(input_buffer, an_attack_pressed_during_recovery_starts_as_soon_as_the_window_opens)
{
    EngInputBuffer b;
    eng_ibuf_init(&b, 0); /* défaut : 10 ticks */
    EXPECT_EQ(b.window, ENG_IBUF_DEFAULT_WINDOW);
    (void)eng_ibuf_push(&b, ENG_ACT_LIGHT, 100); /* pressée en recovery */
    /* fenêtre d'annulation fermée : l'appelant n'autorise rien ; l'entrée reste */
    EXPECT_EQ(eng_ibuf_take(&b, 103, 0U), ENG_ACT_NONE);
    EXPECT_EQ(b.count, 1U);
    /* la fenêtre s'ouvre au tick 108 : l'attaque part immédiatement */
    EXPECT_EQ(eng_ibuf_take(&b, 108, ENG_ACT_MASK(ENG_ACT_LIGHT)), ENG_ACT_LIGHT);
    EXPECT_EQ(b.count, 0U);
    EXPECT_EQ(eng_ibuf_take(&b, 108, ENG_ACT_ALL), ENG_ACT_NONE); /* consommée */
}

UTEST(input_buffer, an_entry_expires_after_exactly_the_window)
{
    EngInputBuffer b;
    eng_ibuf_init(&b, 10);
    (void)eng_ibuf_push(&b, ENG_ACT_HEAVY, 50);
    EXPECT_EQ(eng_ibuf_peek(&b, 60, ENG_ACT_ALL), ENG_ACT_HEAVY); /* âge 10 : encore valide */
    EXPECT_EQ(eng_ibuf_peek(&b, 61, ENG_ACT_ALL), ENG_ACT_NONE);  /* âge 11 : périmée */
    EXPECT_EQ(eng_ibuf_take(&b, 61, ENG_ACT_ALL), ENG_ACT_NONE);
    EXPECT_EQ(b.count, 0U); /* purgée */
    (void)eng_ibuf_push(&b, ENG_ACT_HEAVY, 70);
    EXPECT_EQ(eng_ibuf_peek(&b, 69, ENG_ACT_ALL), ENG_ACT_NONE); /* pression « dans le futur » */
}

UTEST(input_buffer, priorities_are_dodge_block_heavy_light_and_ties_keep_the_oldest)
{
    EngInputBuffer b;
    eng_ibuf_init(&b, 10);
    (void)eng_ibuf_push(&b, ENG_ACT_LIGHT, 10);
    (void)eng_ibuf_push(&b, ENG_ACT_HEAVY, 10);
    (void)eng_ibuf_push(&b, ENG_ACT_BLOCK, 10);
    (void)eng_ibuf_push(&b, ENG_ACT_DODGE, 10);
    EXPECT_EQ(eng_ibuf_take(&b, 11, ENG_ACT_ALL), ENG_ACT_DODGE);
    EXPECT_EQ(eng_ibuf_take(&b, 11, ENG_ACT_ALL), ENG_ACT_BLOCK);
    EXPECT_EQ(eng_ibuf_take(&b, 11, ENG_ACT_ALL), ENG_ACT_HEAVY);
    EXPECT_EQ(eng_ibuf_take(&b, 11, ENG_ACT_ALL), ENG_ACT_LIGHT);
    EXPECT_EQ(eng_ibuf_take(&b, 11, ENG_ACT_ALL), ENG_ACT_NONE);
    /* même action deux fois : une par prise, la plus ancienne d'abord */
    (void)eng_ibuf_push(&b, ENG_ACT_LIGHT, 20);
    (void)eng_ibuf_push(&b, ENG_ACT_LIGHT, 22);
    EXPECT_EQ(eng_ibuf_take(&b, 23, ENG_ACT_ALL), ENG_ACT_LIGHT);
    EXPECT_EQ(b.entries[0].tick, 22U);
}

UTEST(input_buffer, the_allowed_mask_filters_and_keeps_the_others_waiting)
{
    EngInputBuffer b;
    eng_ibuf_init(&b, 10);
    (void)eng_ibuf_push(&b, ENG_ACT_DODGE, 10);
    (void)eng_ibuf_push(&b, ENG_ACT_LIGHT, 11);
    EXPECT_EQ(eng_ibuf_take(&b, 12, ENG_ACT_MASK(ENG_ACT_LIGHT)), ENG_ACT_LIGHT);
    EXPECT_EQ(b.count, 1U); /* l'esquive attend toujours */
    EXPECT_EQ(eng_ibuf_take(&b, 13, ENG_ACT_MASK(ENG_ACT_DODGE)), ENG_ACT_DODGE);
}

UTEST(input_buffer, a_full_buffer_drops_the_oldest_and_bad_actions_are_refused)
{
    EngInputBuffer b;
    eng_ibuf_init(&b, 100);
    for (uint32_t i = 0; i < ENG_IBUF_SIZE + 3U; i++) {
        EXPECT_TRUE(eng_ibuf_push(&b, ENG_ACT_LIGHT, i));
    }
    EXPECT_EQ(b.count, ENG_IBUF_SIZE);
    EXPECT_EQ(b.entries[0].tick, 3U); /* 0, 1, 2 écrasées */
    EXPECT_FALSE(eng_ibuf_push(&b, ENG_ACT_NONE, 1));
    EXPECT_FALSE(eng_ibuf_push(&b, (EngCombatAction)ENG_ACT_COUNT, 1));
    EXPECT_FALSE(eng_ibuf_push(NULL, ENG_ACT_LIGHT, 1));
    EXPECT_EQ(eng_ibuf_priority(ENG_ACT_NONE), 0U);
    EXPECT_EQ(eng_ibuf_priority(ENG_ACT_LIGHT), 1U);
    EXPECT_EQ(eng_ibuf_priority(ENG_ACT_HEAVY), 2U);
    EXPECT_EQ(eng_ibuf_priority(ENG_ACT_BLOCK), 3U);
    EXPECT_EQ(eng_ibuf_priority(ENG_ACT_DODGE), 4U);
    eng_ibuf_clear(&b);
    EXPECT_EQ(b.count, 0U);
    EXPECT_EQ(eng_ibuf_take(NULL, 0, ENG_ACT_ALL), ENG_ACT_NONE);
    EXPECT_EQ(eng_ibuf_peek(NULL, 0, ENG_ACT_ALL), ENG_ACT_NONE);
    eng_ibuf_init(NULL, 5);
    eng_ibuf_clear(NULL);
}
