/* Texte en champ de distance : lettres nettes au seuil, distance de part et d'autre du bord. */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/files.h"
#include "engine/text_sdf.h"
#include "utest.h"

#ifndef OW_FONT_PATH
#error "OW_FONT_PATH doit désigner une police TrueType"
#endif

static EngArena g_arena;
static EngFileData g_font;

static bool font(void)
{
    if (g_arena.base == NULL) {
        if (!eng_arena_init(&g_arena, "texte", (size_t)32 << 20) ||
            !eng_file_read_all(&g_arena, OW_FONT_PATH, &g_font)) {
            return false;
        }
    }
    return g_font.size > 1000U;
}

/* part des pixels au-dessus du seuil dans le rectangle [x0, x1[ x toute la hauteur */
static float inked(const EngTextSdf *s, uint32_t x0, uint32_t x1)
{
    uint32_t hits = 0;
    for (uint32_t y = 0; y < s->height; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            hits += s->pixels[((size_t)y * s->width) + x] >= ENG_TEXT_SDF_EDGE ? 1U : 0U;
        }
    }
    return (float)hits / (float)((x1 - x0) * s->height);
}

UTEST(text_sdf, a_line_of_text_becomes_a_distance_field_with_a_margin_all_around)
{
    ASSERT_TRUE(font());
    EngTextSdf s;
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "RED", 96.0F, 0.0F, 12.0F, &s));
    EXPECT_GT(s.width, 150U); /* trois capitales de 96 px */
    EXPECT_LT(s.width, 300U);
    EXPECT_GT(s.height, 96U); /* la hauteur de ligne et deux marges de 13 px */
    EXPECT_LT(s.height, 180U);
    EXPECT_EQ(s.spread, 12.0F);
    EXPECT_GT(s.baseline, 60.0F);
    EXPECT_LT(s.baseline, (float)s.height);
    /* la marge est entièrement « dehors » : jamais le bord d'une lettre contre le bord de l'image
     */
    for (uint32_t x = 0; x < s.width; x++) {
        ASSERT_LT(s.pixels[x], ENG_TEXT_SDF_EDGE);
        ASSERT_LT(s.pixels[((size_t)(s.height - 1U) * s.width) + x], ENG_TEXT_SDF_EDGE);
    }
    for (uint32_t y = 0; y < s.height; y++) {
        ASSERT_LT(s.pixels[(size_t)y * s.width], ENG_TEXT_SDF_EDGE);
        ASSERT_LT(s.pixels[((size_t)y * s.width) + s.width - 1U], ENG_TEXT_SDF_EDGE);
    }
    /* de l'encre, mais pas un pavé : entre 15 et 60 % des pixels sont dans une lettre */
    const float ink = inked(&s, 0U, s.width);
    EXPECT_GT(ink, 0.15F);
    EXPECT_LT(ink, 0.6F);
    /* un vrai champ de distance : il y a des valeurs loin dedans, loin dehors, et tout le dégradé
     * entre les deux (pas une image noir et blanc) */
    uint32_t histogram[8] = {0};
    uint8_t highest = 0;
    for (size_t i = 0; i < (size_t)s.width * s.height; i++) {
        histogram[s.pixels[i] / 32U]++;
        highest = s.pixels[i] > highest ? s.pixels[i] : highest;
    }
    for (int b = 0; b < 6; b++) { /* 0 à 191 : toutes les tranches sont peuplées */
        EXPECT_GT(histogram[b], 50U);
    }
    EXPECT_GT(highest, 160); /* le cœur des traits (demi-épaisseur d'environ 6 px sur 12) */
    /* la valeur croît de 128 / spread par pixel en traversant un bord : pente de 8 à 14 par
     * pixel le long de la ligne qui coupe le fût du R (première colonne d'encre) */
    const uint32_t row = (uint32_t)(s.baseline - 30.0F);
    uint32_t x_edge = 0;
    while (x_edge + 1U < s.width && s.pixels[((size_t)row * s.width) + x_edge] < 128U) {
        x_edge++;
    }
    ASSERT_GT(x_edge, 3U);
    ASSERT_LT(x_edge + 2U, s.width);
    const int before = s.pixels[((size_t)row * s.width) + x_edge - 3U];
    const int after = s.pixels[((size_t)row * s.width) + x_edge];
    EXPECT_GT(after - before, 20); /* 3 pixels x ~10,7 par pixel = 32, à l'arrondi près */
    EXPECT_LT(after - before, 46);
}

UTEST(text_sdf, tracking_spaces_the_letters_and_a_space_leaves_a_gap)
{
    ASSERT_TRUE(font());
    EngTextSdf tight;
    EngTextSdf wide;
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "RED", 64.0F, 0.0F, 8.0F, &tight));
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "RED", 64.0F, 20.0F, 8.0F, &wide));
    EXPECT_EQ(wide.height, tight.height);
    EXPECT_NEAR((float)(wide.width - tight.width), 40.0F, 1.5F); /* deux intervalles de 20 px */
    /* deux lettres serrées, champ large (20 px) : la boîte de la seconde recouvre le bord de la
     * première sans l'effacer (la plus proche gagne) : « HH » a deux fois l'encre de « H » */
    EngTextSdf one;
    EngTextSdf two;
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "H", 64.0F, 0.0F, 20.0F, &one));
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "HH", 64.0F, 0.0F, 20.0F, &two));
    const float ink_one = inked(&one, 0U, one.width) * (float)(one.width * one.height);
    const float ink_two = inked(&two, 0U, two.width) * (float)(two.width * two.height);
    EXPECT_NEAR(ink_two / ink_one, 2.0F, 0.06F);
    /* « A B » : une colonne entièrement vide au milieu (l'espace), de l'encre de chaque côté */
    EngTextSdf gap;
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "A B", 64.0F, 0.0F, 8.0F, &gap));
    const uint32_t mid = gap.width / 2U;
    EXPECT_EQ(inked(&gap, mid - 2U, mid + 2U), 0.0F);
    EXPECT_GT(inked(&gap, 9U, mid - 6U), 0.1F);
    EXPECT_GT(inked(&gap, mid + 6U, gap.width - 9U), 0.1F);
    /* plus grand : image plus grande, dans le même rapport */
    EngTextSdf big;
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "RED", 128.0F, 0.0F, 8.0F, &big));
    EXPECT_NEAR((float)(big.width - 18U) / (float)(tight.width - 18U), 2.0F, 0.06F);
    /* les accents (UTF-8) sont des lettres comme les autres : « É » est plus haut que « E » */
    EngTextSdf plain;
    EngTextSdf accent;
    ASSERT_TRUE(
        eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "E", 64.0F, 0.0F, 8.0F, &plain));
    ASSERT_TRUE(eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "\xC3\x89", 64.0F, 0.0F,
                                   8.0F, &accent));
    EXPECT_EQ(accent.width, plain.width);
    EXPECT_GT(inked(&accent, 0U, accent.width), inked(&plain, 0U, plain.width));
}

UTEST(text_sdf, sampling_interpolates_between_pixels_and_clamps_at_the_edges)
{
    ASSERT_TRUE(font());
    EngTextSdf s;
    ASSERT_TRUE(eng_text_sdf_build(&g_arena, g_font.data, g_font.size, "O", 64.0F, 0.0F, 8.0F, &s));
    /* au centre d'un pixel : sa valeur ; entre deux : la moyenne */
    const uint32_t x = s.width / 2U;
    const uint32_t y = (uint32_t)(s.baseline - 20.0F);
    const float a = (float)s.pixels[((size_t)y * s.width) + x] / 255.0F;
    const float b = (float)s.pixels[((size_t)y * s.width) + x + 1U] / 255.0F;
    EXPECT_NEAR(eng_text_sdf_sample(&s, (float)x + 0.5F, (float)y + 0.5F), a, 1e-6F);
    EXPECT_NEAR(eng_text_sdf_sample(&s, (float)x + 1.0F, (float)y + 0.5F), 0.5F * (a + b), 1e-6F);
    /* le centre du O est vide, son trait est plein */
    EXPECT_LT(eng_text_sdf_sample(&s, (float)s.width * 0.5F, s.baseline - 22.0F), 0.5F);
    /* hors de l'image : la valeur du bord (dehors), sans lecture hors du tableau */
    EXPECT_LT(eng_text_sdf_sample(&s, -50.0F, -50.0F), 0.5F);
    EXPECT_LT(eng_text_sdf_sample(&s, 1e6F, 1e6F), 0.5F);
    EXPECT_EQ(eng_text_sdf_sample(NULL, 1.0F, 1.0F), 0.0F);
    EXPECT_EQ(eng_text_sdf_sample(&s, NAN, 1.0F), 0.0F);
}

UTEST(text_sdf, bad_arguments_are_refused_and_the_arena_is_given_back)
{
    ASSERT_TRUE(font());
    EngTextSdf s;
    const EngArenaMark mark = eng_arena_mark(&g_arena);
    const uint8_t *f = g_font.data;
    const size_t n = g_font.size;
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "", 64.0F, 0.0F, 8.0F, &s)); /* vide */
    EXPECT_EQ(s.pixels, NULL);
    EXPECT_FALSE(
        eng_text_sdf_build(&g_arena, f, n, "   ", 64.0F, 0.0F, 8.0F, &s));       /* rien à voir */
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "A", 4.0F, 0.0F, 8.0F, &s)); /* trop petit */
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "A", 600.0F, 0.0F, 8.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "A", 64.0F, -1.0F, 8.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "A", 64.0F, 0.0F, 1.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "A", 64.0F, 0.0F, 80.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "A", NAN, 0.0F, 8.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, NULL, 64.0F, 0.0F, 8.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, NULL, n, "A", 64.0F, 0.0F, 8.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(NULL, f, n, "A", 64.0F, 0.0F, 8.0F, &s));
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, "A", 64.0F, 0.0F, 8.0F, NULL));
    static const uint8_t junk[64] = {1, 2, 3, 4, 5, 6, 7, 8};
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, junk, sizeof junk, "A", 64.0F, 0.0F, 8.0F, &s));
    /* une ligne trop longue pour une image de 4096 px de côté */
    static char longue[120];
    memset(longue, 'W', sizeof longue - 1U);
    EXPECT_FALSE(eng_text_sdf_build(&g_arena, f, n, longue, 256.0F, 0.0F, 8.0F, &s));
    EXPECT_EQ(eng_arena_mark(&g_arena).used, mark.used);
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "petite", 512U));
    EXPECT_FALSE(eng_text_sdf_build(&tiny, f, n, "RED", 64.0F, 0.0F, 8.0F, &s));
    EXPECT_EQ(s.width, 0U);
    eng_arena_shutdown(&tiny);
}
