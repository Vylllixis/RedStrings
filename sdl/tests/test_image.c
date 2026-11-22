#include <SDL3/SDL.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/files.h"
#include "engine/image.h"
#include "test_util.h"
#include "utest.h"

/* Décodage d'images et mipmaps (M1-08), sans GPU. Les valeurs attendues sont calculées à la main.
 */

/* PNG 2 x 2 RGB : (255,0,0) (0,255,0) / (0,0,255) (255,255,255), produit hors du projet par un
 * encodeur indépendant (zlib de Python). */
static const uint8_t PNG_2X2[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xFD,
    0xD4, 0x9A, 0x73, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8,
    0xCF, 0xC0, 0xC0, 0x00, 0xC2, 0x0C, 0xFF, 0x81, 0x00, 0x00, 0x1F, 0xEE, 0x05, 0xFB, 0xF1,
    0xAB, 0xBA, 0x77, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

UTEST(eng_image, decodes_a_png_to_rgba_with_the_first_row_on_top)
{
    EngImage image;
    ASSERT_TRUE(eng_image_decode(PNG_2X2, sizeof PNG_2X2, &image));
    EXPECT_EQ(image.width, (uint32_t)2);
    EXPECT_EQ(image.height, (uint32_t)2);
    const uint8_t expected[16] = {255, 0,   0,   255, /* haut-gauche : rouge */
                                  0,   255, 0,   255, /* haut-droite : vert */
                                  0,   0,   255, 255, /* bas-gauche : bleu */
                                  255, 255, 255, 255 /* bas-droite : blanc */};
    EXPECT_EQ(memcmp(image.pixels, expected, sizeof expected), 0); /* RGB -> alpha 255 */
    eng_image_free(&image);
    EXPECT_TRUE(image.pixels == NULL);
    eng_image_free(&image); /* sans effet la deuxième fois */
}

UTEST(eng_image, corrupt_or_empty_data_is_refused_and_leaves_a_zeroed_image)
{
    EngImage image = {.width = 7, .height = 7, .pixels = (uint8_t *)1};
    EXPECT_FALSE(eng_image_decode(NULL, 10, &image));
    EXPECT_EQ(image.width, (uint32_t)0);
    EXPECT_TRUE(image.pixels == NULL);
    uint8_t garbage[40];
    memset(garbage, 0x5A, sizeof garbage);
    EXPECT_FALSE(eng_image_decode(garbage, sizeof garbage, &image));
    EXPECT_FALSE(eng_image_decode(PNG_2X2, 0, &image));
    /* PNG tronqué en plein milieu des données */
    EXPECT_FALSE(eng_image_decode(PNG_2X2, 45, &image));
    EXPECT_TRUE(image.pixels == NULL);
}

UTEST(eng_image, loads_a_file_and_restores_the_scratch_arena)
{
    /* chemin relatif à l'exécutable, comme le fait le moteur */
    char path[512];
    ASSERT_TRUE(eng_path_resolve(path, sizeof path, "image_test.png"));
    ASSERT_TRUE(SDL_SaveFile(path, PNG_2X2, sizeof PNG_2X2));
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&scratch, "image-test", (size_t)1 << 20));
    const EngArenaMark before = eng_arena_mark(&scratch);
    EngImage image;
    ASSERT_TRUE(eng_image_load(&scratch, path, &image));
    EXPECT_EQ(image.width, (uint32_t)2);
    EXPECT_EQ(image.pixels[0], (uint8_t)255);
    EXPECT_EQ(eng_arena_mark(&scratch).used, before.used); /* arène restituée */
    eng_image_free(&image);
    EXPECT_FALSE(eng_image_load(&scratch, "absent_image_test.png", &image));
    EXPECT_TRUE(image.pixels == NULL);
    eng_arena_shutdown(&scratch);
}

UTEST(eng_image, mip_counts_and_sizes_follow_the_halving_rule)
{
    EXPECT_EQ(eng_image_mip_count(1, 1), (uint32_t)1);
    EXPECT_EQ(eng_image_mip_count(2, 2), (uint32_t)2);
    EXPECT_EQ(eng_image_mip_count(64, 64), (uint32_t)7);
    EXPECT_EQ(eng_image_mip_count(512, 512), (uint32_t)10);
    EXPECT_EQ(eng_image_mip_count(5, 3), (uint32_t)3);   /* 5x3, 2x1, 1x1 */
    EXPECT_EQ(eng_image_mip_count(256, 1), (uint32_t)9); /* le plus grand côté décide */
    EXPECT_EQ(eng_image_mip_count(4096, 4096), (uint32_t)13);
    EXPECT_EQ(eng_image_mip_count(16384, 1), (uint32_t)15);
    EXPECT_EQ(eng_image_mip_count(0, 8), (uint32_t)0);
    uint32_t w;
    uint32_t h;
    eng_image_mip_size(5, 3, 1, &w, &h);
    EXPECT_EQ(w, (uint32_t)2);
    EXPECT_EQ(h, (uint32_t)1);
    eng_image_mip_size(5, 3, 2, &w, &h);
    EXPECT_EQ(w, (uint32_t)1);
    EXPECT_EQ(h, (uint32_t)1);
    eng_image_mip_size(256, 1, 3, &w, &h);
    EXPECT_EQ(w, (uint32_t)32);
    EXPECT_EQ(h, (uint32_t)1); /* ne descend jamais sous 1 */
    eng_image_mip_size(1, 256, 3, &w, &h);
    EXPECT_EQ(w, (uint32_t)1); /* idem pour la largeur */
    EXPECT_EQ(h, (uint32_t)32);
    /* chaîne 4x4 : 16 + 4 + 1 pixels, 4 octets chacun */
    EXPECT_EQ(eng_image_mip_chain_size(4, 4, 3), (size_t)(21 * 4));
    EXPECT_EQ(eng_image_mip_chain_size(4, 4, 1), (size_t)(16 * 4));
}

UTEST(eng_image, a_box_filter_averages_each_two_by_two_block)
{
    /* 4x2 : deux blocs 2x2. Bloc gauche R = 0, 40, 80, 120 (moyenne 60) ; bloc droit R = 200 sur
     * les 4 pixels. Les autres canaux et l'alpha ont leurs propres moyennes. */
    uint8_t base[4 * 2 * 4];
    const uint8_t r[8] = {0, 40, 200, 200, 80, 120, 200, 200};
    for (int i = 0; i < 8; i++) {
        base[(i * 4) + 0] = r[i];
        base[(i * 4) + 1] = (uint8_t)(i < 4 ? 100 : 20);
        base[(i * 4) + 2] = 7;
        base[(i * 4) + 3] = (uint8_t)(i % 2 == 0 ? 255 : 0);
    }
    uint8_t mips[(2 * 1 + 1 * 1) * 4];
    ASSERT_TRUE(eng_image_generate_mips(base, 4, 2, 3, false, mips));
    /* niveau 1 : 2 x 1 */
    EXPECT_EQ(mips[0], (uint8_t)60);
    EXPECT_EQ(mips[1], (uint8_t)60); /* (100 + 100 + 20 + 20) / 4 */
    EXPECT_EQ(mips[2], (uint8_t)7);
    EXPECT_EQ(mips[3], (uint8_t)128); /* alpha 255, 0, 255, 0 : 127,5 arrondi */
    EXPECT_EQ(mips[4], (uint8_t)200);
    /* niveau 2 : 1 x 1, moyenne de (60, 200) = 130 */
    EXPECT_EQ(mips[8], (uint8_t)130);
}

UTEST(eng_image, srgb_mips_average_in_linear_light)
{
    /* Damier noir / blanc 2x2. En sRGB, le gris moyen est 188 (la lumière linéaire 0,5 encodée),
     * pas 128 (moyenne naïve des valeurs encodées, trop sombre). L'alpha reste linéaire. */
    const uint8_t base[16] = {0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255};
    uint8_t srgb[4];
    uint8_t linear[4];
    ASSERT_TRUE(eng_image_generate_mips(base, 2, 2, 2, true, srgb));
    ASSERT_TRUE(eng_image_generate_mips(base, 2, 2, 2, false, linear));
    EXPECT_TRUE(srgb[0] == 188 || srgb[0] == 187); /* 187,52 selon l'arrondi flottant */
    EXPECT_EQ(srgb[0], srgb[1]);
    EXPECT_EQ(srgb[1], srgb[2]);
    EXPECT_EQ(srgb[3], (uint8_t)255);
    EXPECT_EQ(linear[0], (uint8_t)128);
    /* noir et blanc purs restent purs (le ré-encodage ne dérive pas) */
    const uint8_t white[16] = {255, 255, 255, 255, 255, 255, 255, 255,
                               255, 255, 255, 255, 255, 255, 255, 255};
    ASSERT_TRUE(eng_image_generate_mips(white, 2, 2, 2, true, srgb));
    EXPECT_EQ(srgb[0], (uint8_t)255);
    const uint8_t black[16] = {0};
    ASSERT_TRUE(eng_image_generate_mips(black, 2, 2, 2, true, srgb));
    EXPECT_EQ(srgb[0], (uint8_t)0);
}

UTEST(eng_image, odd_sizes_cover_every_source_pixel)
{
    /* 3x1 -> 1x1 : les TROIS pixels comptent (0, 90, 180 -> 90) ; un filtre 2x2 naïf ignorerait
     * le troisième. 5x1 -> 2x1 -> 1x1 : tous les pixels comptent aussi. */
    const uint8_t three[12] = {0, 0, 0, 255, 90, 0, 0, 255, 180, 0, 0, 255};
    uint8_t out[4];
    ASSERT_TRUE(eng_image_generate_mips(three, 3, 1, 2, false, out));
    EXPECT_EQ(out[0], (uint8_t)90);
    const uint8_t five[20] = {0, 0,   0,   255, 0, 0,   0,   255, 100, 0,
                              0, 255, 100, 0,   0, 255, 100, 0,   0,   255};
    uint8_t chain[(2 + 1) * 4];
    ASSERT_TRUE(eng_image_generate_mips(five, 5, 1, 3, false, chain));
    EXPECT_EQ(chain[0], (uint8_t)0);   /* pixels 0 et 1 */
    EXPECT_EQ(chain[4], (uint8_t)100); /* pixels 2, 3 et 4 */
    EXPECT_EQ(chain[8], (uint8_t)50);  /* (0 + 100) / 2 */
}

UTEST(eng_image, invalid_mip_requests_are_refused)
{
    uint8_t base[4 * 4 * 4] = {0};
    uint8_t out[64];
    EXPECT_FALSE(eng_image_generate_mips(base, 4, 4, 0, false, out)); /* 0 niveau */
    EXPECT_FALSE(eng_image_generate_mips(base, 4, 4, 4, false, out)); /* chaîne : 3 niveaux */
    EXPECT_FALSE(eng_image_generate_mips(NULL, 4, 4, 2, false, out));
    EXPECT_FALSE(eng_image_generate_mips(base, 4, 4, 2, false, NULL));
    EXPECT_FALSE(eng_image_generate_mips(base, 0, 4, 1, false, out));
    EXPECT_TRUE(eng_image_generate_mips(base, 4, 4, 1, false, NULL)); /* 1 niveau : rien à écrire */
}

/* PNG gris 16385 x 1 (un pixel de plus que la limite) et 16384 x 1 (la limite), produits hors du
 * projet : de minuscules fichiers, mais de grandes images. */
static const uint8_t PNG_TOO_WIDE[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0xEC, 0x36, 0x82,
    0xBA, 0x00, 0x00, 0x00, 0x27, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0xED, 0xC1, 0x31, 0x01, 0x00,
    0x00, 0x00, 0xC2, 0xA0, 0xF5, 0x4F, 0x6D, 0x0C, 0x1F, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x01, 0x40, 0x02, 0x00, 0x01,
    0x59, 0xAD, 0x81, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
static const uint8_t PNG_TOO_TALL[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00,
                                       0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
                                       0x00, 0x01, 0x00, 0x00, 0x40, 0x01, 0x08, 0x00, 0x00,
                                       0x00, 0x00, 0x7E, 0x75, 0xEE, 0x2C, 0x00, 0x00, 0x00,
                                       0x35, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0xED, 0xC1,
                                       0x01, 0x01, 0x00, 0x00, 0x00, 0x80, 0x90, 0xFE, 0xAF,
                                       0xEE, 0x08, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8, 0x01,
                                       0x80, 0x02, 0x00, 0x01, 0xBC, 0x08, 0x24, 0xE5, 0x00,
                                       0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42,
                                       0x60, 0x82}; /* 1 x 16385 */
static const uint8_t PNG_AT_LIMIT[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x03, 0xF4, 0xE9,
    0x84, 0x00, 0x00, 0x00, 0x27, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0xED, 0xC1, 0x31, 0x01, 0x00,
    0x00, 0x00, 0xC2, 0xA0, 0xF5, 0x4F, 0x6D, 0x0C, 0x1F, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBB, 0x01, 0x40, 0x01, 0x00, 0x01,
    0xC0, 0x7A, 0x7D, 0xE7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

UTEST(eng_image, the_size_limit_is_enforced_on_the_decoded_dimensions)
{
    EngImage image;
    EXPECT_FALSE(eng_image_decode(PNG_TOO_WIDE, sizeof PNG_TOO_WIDE, &image));
    EXPECT_FALSE(eng_image_decode(PNG_TOO_TALL, sizeof PNG_TOO_TALL, &image));
    EXPECT_TRUE(image.pixels == NULL);
    ASSERT_TRUE(eng_image_decode(PNG_AT_LIMIT, sizeof PNG_AT_LIMIT, &image));
    EXPECT_EQ(image.width, (uint32_t)ENG_IMAGE_MAX_SIZE);
    EXPECT_EQ(image.height, (uint32_t)1);
    EXPECT_EQ(image.pixels[0], (uint8_t)0); /* gris 0 -> (0, 0, 0, 255) */
    EXPECT_EQ(image.pixels[3], (uint8_t)255);
    eng_image_free(&image);
}
