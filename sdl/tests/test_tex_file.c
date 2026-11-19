#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/asset_format.h"
#include "engine/image.h"
#include "engine/tex_file.h"
#include "utest.h"

/* Fichiers .tex (M2-02) : écriture, lecture, et refus de chaque incohérence. Les fichiers fautifs
 * sont fabriqués en modifiant UN champ d'un fichier valide (poke_*), ce qui prouve que chaque
 * vérification du lecteur est déclenchée par son propre champ et non par un effet de bord. */

typedef struct Fixture {
    EngArena arena;
    EngArena scratch;
} Fixture;

static bool fixture_open(Fixture *f)
{
    if (!eng_arena_init(&f->arena, "tex-test", (size_t)4 << 20)) {
        return false;
    }
    if (!eng_arena_init(&f->scratch, "tex-scratch", (size_t)4 << 20)) {
        eng_arena_shutdown(&f->arena);
        return false;
    }
    return true;
}

static void fixture_close(Fixture *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_arena_shutdown(&f->arena);
}

/* Image de test : dégradé déterministe, avec de l'alpha. */
static bool make_image(EngImage *image, uint32_t w, uint32_t h)
{
    if (!eng_image_create(w, h, NULL, image)) {
        return false;
    }
    for (uint32_t i = 0; i < w * h; i++) {
        image->pixels[4U * i + 0U] = (uint8_t)(i * 7U);
        image->pixels[4U * i + 1U] = (uint8_t)(255U - (i * 3U) % 256U);
        image->pixels[4U * i + 2U] = (uint8_t)((i % w) * 31U);
        image->pixels[4U * i + 3U] = (uint8_t)(128U + (i % 100U));
    }
    return true;
}

/* Pointeur sur le contenu de la section `tag` d'un fichier (modifiable), ou NULL. */
static uint8_t *section_bytes(uint8_t *file, size_t size, uint32_t tag, uint32_t *section_size)
{
    EngAssetView view;
    EngAssetSection s;
    if (eng_asset_check(file, size, ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, &view) !=
            ENG_ASSET_OK ||
        !eng_asset_find_section(&view, tag, &s)) {
        return NULL;
    }
    if (section_size != NULL) {
        *section_size = s.size;
    }
    return file + (s.data - file);
}

static void poke_u32(uint8_t *section, size_t at, uint32_t value)
{
    memcpy(section + at, &value, sizeof value);
}

/* Copie modifiable du fichier encodé. */
static uint8_t *copy_of(const void *file, size_t size)
{
    uint8_t *copy = SDL_malloc(size);
    if (copy != NULL) {
        memcpy(copy, file, size);
    }
    return copy;
}

static bool opens(const uint8_t *file, size_t size)
{
    EngTexView view;
    return eng_tex_open(file, size, "test", &view);
}

/* ── Formats ── */

/* Une valeur hors énumération, sans conversion explicite (l'analyseur statique la refuse). */
static EngTexFormat fmt_from(uint32_t v)
{
    EngTexFormat f;
    memcpy(&f, &v, sizeof f);
    return f;
}

UTEST(eng_tex_file, level_sizes_follow_the_block_table)
{
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_RGBA8_UNORM, 5, 3), (size_t)60);
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_RGBA8_SRGB, 1, 1), (size_t)4);
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_BC7_UNORM, 4, 4), (size_t)16);
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_BC7_SRGB, 5, 5), (size_t)64); /* 2 x 2 blocs */
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_BC7_UNORM, 1, 1), (size_t)16);
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_BC5_UNORM, 8, 4), (size_t)32);
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_BC4_UNORM, 8, 8), (size_t)32);
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_RGBA16F, 3, 2), (size_t)48);
    EXPECT_EQ(eng_tex_level_bytes(ENG_TEX_R8_UNORM, 7, 3), (size_t)21);
    EXPECT_EQ(eng_tex_level_bytes(fmt_from(0), 4, 4), (size_t)0);
    EXPECT_EQ(eng_tex_level_bytes(fmt_from(9), 4, 4), (size_t)0);
    uint32_t bytes = 99;
    uint32_t block = 99;
    EXPECT_FALSE(eng_tex_format_block(fmt_from(77), &bytes, &block));
    EXPECT_EQ(bytes, (uint32_t)99); /* sorties intactes en cas d'échec */
    EXPECT_TRUE(eng_tex_format_block(ENG_TEX_BC7_UNORM, &bytes, &block));
    EXPECT_EQ(bytes, (uint32_t)16);
    EXPECT_EQ(block, (uint32_t)4);
}

/* ── Aller-retour ── */

UTEST(eng_tex_file, encode_then_open_gives_the_image_and_its_mips)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const uint32_t sizes[4][2] = {{16, 16}, {5, 3}, {1, 1}, {64, 2}};
    for (int srgb = 0; srgb < 2; srgb++) {
        for (int k = 0; k < 4; k++) {
            const uint32_t w = sizes[k][0];
            const uint32_t h = sizes[k][1];
            EngImage image;
            ASSERT_TRUE(make_image(&image, w, h));
            const void *file = NULL;
            size_t size = 0;
            ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, srgb != 0, &file, &size));
            EXPECT_EQ(((uintptr_t)file) % 16U, (uintptr_t)0);

            EngTexView view;
            ASSERT_TRUE(eng_tex_open(file, size, "rt", &view));
            EXPECT_EQ(view.info.format, srgb != 0 ? ENG_TEX_RGBA8_SRGB : ENG_TEX_RGBA8_UNORM);
            EXPECT_EQ(view.info.width, w);
            EXPECT_EQ(view.info.height, h);
            EXPECT_EQ(view.info.layer_count, (uint32_t)1);
            EXPECT_EQ(view.info.flags, (uint32_t)0);
            const uint32_t levels = eng_image_mip_count(w, h);
            ASSERT_EQ(view.info.mip_count, levels);

            /* niveau 0 : l'image ; niveaux suivants : ceux de eng_image_generate_mips */
            const uint8_t *level0 = NULL;
            uint32_t bytes = 0;
            ASSERT_TRUE(eng_tex_subresource(&view, 0, 0, &level0, &bytes));
            EXPECT_EQ(bytes, w * h * 4U);
            EXPECT_EQ(memcmp(level0, image.pixels, bytes), 0);
            const size_t chain = eng_image_mip_chain_size(w, h, levels);
            uint8_t *expected = SDL_malloc(chain);
            ASSERT_TRUE(expected != NULL);
            if (levels > 1U) {
                ASSERT_TRUE(
                    eng_image_generate_mips(image.pixels, w, h, levels, srgb != 0, expected));
            }
            size_t at = 0;
            for (uint32_t level = 1; level < levels; level++) {
                uint32_t lw = 0;
                uint32_t lh = 0;
                eng_image_mip_size(w, h, level, &lw, &lh);
                const uint8_t *got = NULL;
                ASSERT_TRUE(eng_tex_subresource(&view, 0, level, &got, &bytes));
                EXPECT_EQ(bytes, lw * lh * 4U);
                EXPECT_EQ(((uintptr_t)(got - view.data)) % 16U, (uintptr_t)0);
                EXPECT_EQ(memcmp(got, expected + at, bytes), 0);
                at += bytes;
            }
            SDL_free(expected);

            EngImage back;
            bool back_srgb = srgb == 0;
            ASSERT_TRUE(eng_tex_to_image(&view, &back, &back_srgb));
            EXPECT_EQ(back_srgb, srgb != 0);
            EXPECT_EQ(back.width, w);
            EXPECT_EQ(back.height, h);
            EXPECT_EQ(memcmp(back.pixels, image.pixels, (size_t)w * h * 4U), 0);
            eng_image_free(&back);
            eng_image_free(&image);
        }
    }
    fixture_close(&f);
}

UTEST(eng_tex_file, srgb_flag_changes_the_mips_but_not_the_base)
{
    /* damier noir/blanc : le gris moyen est 188 en sRGB (moyenne en lumière linéaire), 128 en
     * linéaire (voir engine/image.h) : le fichier doit porter le bon */
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngImage image;
    ASSERT_TRUE(eng_image_create(2, 2, NULL, &image));
    for (uint32_t i = 0; i < 4; i++) {
        const uint8_t v = ((i % 2U) ^ (i / 2U)) != 0U ? 255 : 0;
        uint8_t *px = image.pixels + (size_t)i * 4U;
        memset(px, v, 3);
        px[3] = 255;
    }
    const void *srgb_file = NULL;
    const void *linear_file = NULL;
    size_t srgb_size = 0;
    size_t linear_size = 0;
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, true, &srgb_file, &srgb_size));
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, false, &linear_file, &linear_size));
    EngTexView a;
    EngTexView b;
    ASSERT_TRUE(eng_tex_open(srgb_file, srgb_size, "s", &a));
    ASSERT_TRUE(eng_tex_open(linear_file, linear_size, "l", &b));
    const uint8_t *pa = NULL;
    const uint8_t *pb = NULL;
    uint32_t bytes = 0;
    ASSERT_TRUE(eng_tex_subresource(&a, 0, 1, &pa, &bytes));
    ASSERT_TRUE(eng_tex_subresource(&b, 0, 1, &pb, &bytes));
    EXPECT_NEAR((double)pa[0], 188.0, 1.0);
    EXPECT_NEAR((double)pb[0], 128.0, 1.0);
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, encoding_is_deterministic_and_restores_scratch)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngImage image;
    ASSERT_TRUE(make_image(&image, 9, 9));
    const void *a = NULL;
    const void *b = NULL;
    size_t sa = 0;
    size_t sb = 0;
    const size_t before = eng_arena_remaining(&f.scratch);
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, true, &a, &sa));
    EXPECT_EQ(eng_arena_remaining(&f.scratch), before);
    /* l'arène de travail est réutilisée sur des octets « sales » : même résultat */
    memset(eng_arena_alloc(&f.scratch, 4096, 16), 0xAB, 4096);
    eng_arena_reset(&f.scratch);
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, true, &b, &sb));
    ASSERT_EQ(sa, sb);
    EXPECT_EQ(memcmp(a, b, sa), 0);
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, encode_refuses_invalid_images_and_restores_the_arena)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const void *file = (const void *)1;
    size_t size = 5;
    EngImage none = {.width = 4, .height = 4};
    EXPECT_FALSE(eng_tex_encode(&f.arena, &f.scratch, &none, true, &file, &size));
    EXPECT_TRUE(file == NULL);
    EXPECT_EQ(size, (size_t)0);
    uint8_t dummy[4] = {0};
    EngImage zero = {.width = 0, .height = 4, .pixels = dummy};
    EXPECT_FALSE(eng_tex_encode(&f.arena, &f.scratch, &zero, true, &file, &size));

    /* arène du fichier trop petite : échec propre, arène restituée */
    EngImage image;
    ASSERT_TRUE(make_image(&image, 64, 64));
    uint8_t storage[256];
    EngArena tiny;
    eng_arena_init_buffer(&tiny, "tiny", storage, sizeof storage);
    const size_t before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_tex_encode(&tiny, &f.scratch, &image, true, &file, &size));
    EXPECT_EQ(eng_arena_remaining(&tiny), before);
    EXPECT_TRUE(file == NULL);
    /* arène de travail trop petite */
    uint8_t small[256];
    EngArena tiny_scratch;
    eng_arena_init_buffer(&tiny_scratch, "tiny2", small, sizeof small);
    EXPECT_FALSE(eng_tex_encode(&f.arena, &tiny_scratch, &image, true, &file, &size));
    eng_image_free(&image);
    fixture_close(&f);
}

/* ── Fichiers fautifs : un champ à la fois ── */

typedef struct Bad {
    const char *what;
    uint32_t tag;
    uint32_t at;
    uint32_t value;
} Bad;

UTEST(eng_tex_file, each_corrupted_field_is_rejected)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngImage image;
    ASSERT_TRUE(make_image(&image, 8, 8)); /* 4 niveaux : 8, 4, 2, 1 */
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, true, &file, &size));
    ASSERT_TRUE(opens(file, size));

    /* INFO : format@0 width@4 height@8 mip_count@12 layer_count@16 flags@20 reserved@24,28
     * MIPS : (offset, size) par niveau, à 8 octets chacun ; DATA : octets */
    const Bad bad[] = {
        {"format 0", ENG_ASSET_TAG_INFO, 0, 0},
        {"format 9", ENG_ASSET_TAG_INFO, 0, 9},
        {"largeur 0", ENG_ASSET_TAG_INFO, 4, 0},
        {"hauteur 0", ENG_ASSET_TAG_INFO, 8, 0},
        {"largeur trop grande", ENG_ASSET_TAG_INFO, 4, ENG_IMAGE_MAX_SIZE + 1U},
        {"hauteur trop grande", ENG_ASSET_TAG_INFO, 8, ENG_IMAGE_MAX_SIZE + 1U},
        {"0 niveau", ENG_ASSET_TAG_INFO, 12, 0},
        {"5 niveaux pour 8 x 8", ENG_ASSET_TAG_INFO, 12, 5},
        {"3 niveaux : table trop longue", ENG_ASSET_TAG_INFO, 12, 3},
        {"0 couche", ENG_ASSET_TAG_INFO, 16, 0},
        {"257 couches", ENG_ASSET_TAG_INFO, 16, 257},
        {"2 couches : table trop courte", ENG_ASSET_TAG_INFO, 16, 2},
        {"drapeau inconnu", ENG_ASSET_TAG_INFO, 20, 2},
        {"cube a une seule couche", ENG_ASSET_TAG_INFO, 20, ENG_TEX_FLAG_CUBE},
        {"reserved 0", ENG_ASSET_TAG_INFO, 24, 1},
        {"reserved 1", ENG_ASSET_TAG_INFO, 28, 1},
        {"niveau 0 : taille fausse", ENG_ASSET_TAG_MIPS, 4, 8 * 8 * 4 - 4},
        {"niveau 1 : taille fausse", ENG_ASSET_TAG_MIPS, 12, 4 * 4 * 4 + 4},
        {"niveau 0 : offset non aligné", ENG_ASSET_TAG_MIPS, 0, 4},
        {"niveau 1 : offset avant la fin du niveau 0", ENG_ASSET_TAG_MIPS, 8, 16},
        {"niveau 2 : offset hors de DATA", ENG_ASSET_TAG_MIPS, 16, 0x10000},
        {"niveau 3 : offset énorme", ENG_ASSET_TAG_MIPS, 24, 0xFFFFFFF0U},
        {"niveau 3 : taille énorme", ENG_ASSET_TAG_MIPS, 28, 0xFFFFFFFFU},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        uint8_t *copy = copy_of(file, size);
        ASSERT_TRUE(copy != NULL);
        uint8_t *section = section_bytes(copy, size, bad[i].tag, NULL);
        if (section == NULL) {
            SDL_free(copy);
        }
        ASSERT_TRUE(section != NULL);
        poke_u32(section, bad[i].at, bad[i].value);
        if (opens(copy, size)) {
            printf("accepté à tort : %s\n", bad[i].what);
            EXPECT_TRUE(false);
        }
        SDL_free(copy);
    }
    /* contrôle : une valeur inchangée reste acceptée (le banc ne rejette pas tout) */
    uint8_t *copy = copy_of(file, size);
    ASSERT_TRUE(copy != NULL);
    poke_u32(section_bytes(copy, size, ENG_ASSET_TAG_INFO, NULL), 4, 8);
    EXPECT_TRUE(opens(copy, size));
    SDL_free(copy);
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, missing_sections_and_wrong_container_are_rejected)
{
    const uint8_t byte = 0;
    uint8_t out[512];
    size_t size = 0;
    const EngAssetSectionDesc only_info[1] = {{ENG_ASSET_TAG_INFO, &byte, 1, 1}};
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, only_info, 1, out,
                                sizeof out, &size));
    EXPECT_FALSE(opens(out, size));

    uint8_t info[32] = {0};
    const uint32_t fields[6] = {ENG_TEX_RGBA8_UNORM, 1, 1, 1, 1, 0};
    memcpy(info, fields, sizeof fields);
    const uint32_t mips[2] = {0, 4};
    const uint8_t texel[4] = {1, 2, 3, 4};
    const EngAssetSectionDesc good[3] = {{ENG_ASSET_TAG_INFO, info, 32, 1},
                                         {ENG_ASSET_TAG_MIPS, mips, 8, 1},
                                         {ENG_ASSET_TAG_DATA, texel, 4, 4}};
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, good, 3, out,
                                sizeof out, &size));
    EXPECT_TRUE(opens(out, size)); /* le plus petit fichier valide : 1 x 1 */

    /* un fichier .mesh n'est pas un .tex ; une autre version est refusée */
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, good, 3, out,
                                sizeof out, &size));
    EXPECT_FALSE(opens(out, size));
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX + 1U, good, 3, out,
                                sizeof out, &size));
    EXPECT_FALSE(opens(out, size));

    /* INFO de mauvaise taille ; DATA dont le compte n'est pas la taille */
    const EngAssetSectionDesc short_info[3] = {{ENG_ASSET_TAG_INFO, info, 28, 1},
                                               {ENG_ASSET_TAG_MIPS, mips, 8, 1},
                                               {ENG_ASSET_TAG_DATA, texel, 4, 4}};
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, short_info, 3, out,
                                sizeof out, &size));
    EXPECT_FALSE(opens(out, size));
    const EngAssetSectionDesc bad_count[3] = {{ENG_ASSET_TAG_INFO, info, 32, 1},
                                              {ENG_ASSET_TAG_MIPS, mips, 8, 1},
                                              {ENG_ASSET_TAG_DATA, texel, 4, 5}};
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, bad_count, 3, out,
                                sizeof out, &size));
    EXPECT_FALSE(opens(out, size));
}

UTEST(eng_tex_file, bc7_converts_with_its_whole_chain_and_other_compressed_formats_do_not)
{
    /* BC7 5 x 5 : 2 x 2 blocs = 64 octets ; mips 2 x 2 puis 1 x 1 : un bloc (16 octets) chacun */
    uint8_t info[32] = {0};
    const uint32_t fields[6] = {ENG_TEX_BC7_SRGB, 5, 5, 3, 1, 0};
    memcpy(info, fields, sizeof fields);
    uint32_t mips[6] = {0, 64, 64, 16, 80, 16};
    uint8_t data[96] = {0};
    const EngAssetSectionDesc sections[3] = {{ENG_ASSET_TAG_INFO, info, 32, 1},
                                             {ENG_ASSET_TAG_MIPS, mips, sizeof mips, 3},
                                             {ENG_ASSET_TAG_DATA, data, sizeof data, sizeof data}};
    uint8_t out[512];
    size_t size = 0;
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, sections, 3, out,
                                sizeof out, &size));
    EngTexView view;
    ASSERT_TRUE(eng_tex_open(out, size, "bc7", &view));
    EXPECT_EQ(view.info.format, ENG_TEX_BC7_SRGB);
    const uint8_t *p = NULL;
    uint32_t bytes = 0;
    ASSERT_TRUE(eng_tex_subresource(&view, 0, 2, &p, &bytes));
    EXPECT_EQ(bytes, (uint32_t)16);
    EngImage image;
    bool srgb = true;
    /* BC7 : toute la chaîne, restée compressée */
    ASSERT_TRUE(eng_tex_to_image(&view, &image, &srgb));
    EXPECT_TRUE(srgb);
    EXPECT_EQ(image.encoding, ENG_IMAGE_BC7_SRGB);
    EXPECT_EQ(image.width, (uint32_t)5);
    EXPECT_EQ(image.mip_count, (uint32_t)3);
    EXPECT_EQ(memcmp(image.pixels, data, 96), 0);
    eng_image_free(&image);

    /* BC5 : validé par l'ouverture, refusé à la conversion */
    uint8_t bc5_info[32] = {0};
    const uint32_t bf[5] = {ENG_TEX_BC5_UNORM, 4, 4, 1, 1};
    memcpy(bc5_info, bf, sizeof bf);
    const uint32_t bc5_mips[2] = {0, 16};
    const EngAssetSectionDesc bc5[3] = {{ENG_ASSET_TAG_INFO, bc5_info, 32, 1},
                                        {ENG_ASSET_TAG_MIPS, bc5_mips, sizeof bc5_mips, 1},
                                        {ENG_ASSET_TAG_DATA, data, 16, 16}};
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, bc5, 3, out, sizeof out,
                                &size));
    ASSERT_TRUE(eng_tex_open(out, size, "bc5", &view));
    EXPECT_FALSE(eng_tex_to_image(&view, &image, &srgb));
    EXPECT_TRUE(image.pixels == NULL);

    /* une cube map valide : 6 couches carrées, 1 niveau de 1 x 1 RGBA8 */
    uint8_t cube_info[32] = {0};
    const uint32_t cf[6] = {ENG_TEX_RGBA8_UNORM, 1, 1, 1, 6, ENG_TEX_FLAG_CUBE};
    memcpy(cube_info, cf, sizeof cf);
    uint32_t cube_mips[12];
    uint8_t cube_data[6 * 16] = {0};
    for (uint32_t layer = 0; layer < 6; layer++) {
        const size_t k = (size_t)layer * 2U;
        cube_mips[k] = layer * 16U;
        cube_mips[k + 1U] = 4;
    }
    const EngAssetSectionDesc cube[3] = {{ENG_ASSET_TAG_INFO, cube_info, 32, 1},
                                         {ENG_ASSET_TAG_MIPS, cube_mips, sizeof cube_mips, 6},
                                         {ENG_ASSET_TAG_DATA, cube_data, 96 - 12, 96 - 12}};
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, cube, 3, out,
                                sizeof out, &size));
    ASSERT_TRUE(eng_tex_open(out, size, "cube", &view));

    /* une cube map BC7 (6 couches de 4 x 4, un bloc chacune) : valide, mais pas convertible en
     * image */
    uint8_t bc7_cube_info[32] = {0};
    const uint32_t bcf[6] = {ENG_TEX_BC7_UNORM, 4, 4, 1, 6, ENG_TEX_FLAG_CUBE};
    memcpy(bc7_cube_info, bcf, sizeof bcf);
    uint32_t bc7_cube_mips[12];
    for (uint32_t layer = 0; layer < 6; layer++) {
        bc7_cube_mips[(size_t)layer * 2U] = layer * 16U;
        bc7_cube_mips[(size_t)layer * 2U + 1U] = 16;
    }
    uint8_t bc7_cube_data[96] = {0};
    const EngAssetSectionDesc bc7_cube[3] = {
        {ENG_ASSET_TAG_INFO, bc7_cube_info, 32, 1},
        {ENG_ASSET_TAG_MIPS, bc7_cube_mips, sizeof bc7_cube_mips, 6},
        {ENG_ASSET_TAG_DATA, bc7_cube_data, sizeof bc7_cube_data, sizeof bc7_cube_data}};
    uint8_t bc7_out[512];
    size_t bc7_size = 0;
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, bc7_cube, 3, bc7_out,
                                sizeof bc7_out, &bc7_size));
    EngTexView bc7_view;
    ASSERT_TRUE(eng_tex_open(bc7_out, bc7_size, "bc7-cube", &bc7_view));
    EXPECT_FALSE(eng_tex_to_image(&bc7_view, &image, &srgb));
    EXPECT_TRUE(image.pixels == NULL);
    EXPECT_TRUE(eng_tex_subresource(&view, 5, 0, &p, &bytes));
    EXPECT_FALSE(eng_tex_subresource(&view, 6, 0, &p, &bytes));
    EXPECT_FALSE(eng_tex_subresource(&view, 0, 1, &p, &bytes));
    EngTexView empty = {0};
    EXPECT_FALSE(eng_tex_subresource(&empty, 0, 0, &p, &bytes));
}

/* ── BC7 (M2-03) ── */

/* Un bloc BC7 valide commence par son mode : le nombre de zéros de poids faible du premier octet,
 * de 0 à 7 (un octet nul serait le mode 8, réservé). */
static bool block_has_valid_mode(const uint8_t *block)
{
    return block[0] != 0U;
}

UTEST(eng_tex_file, bc7_chain_sizes_and_image_creation)
{
    EXPECT_EQ(eng_image_encoded_chain_size(1, 1, 1), (size_t)16);
    EXPECT_EQ(eng_image_encoded_chain_size(4, 4, 3),
              (size_t)48); /* 4x4, 2x2, 1x1 : un bloc chacun */
    EXPECT_EQ(eng_image_encoded_chain_size(5, 3, 3), (size_t)64); /* 2x1 blocs, puis 1, puis 1 */
    EXPECT_EQ(eng_image_encoded_chain_size(8, 8, 1), (size_t)64);
    EXPECT_EQ(eng_image_encoded_chain_size(0, 8, 1), (size_t)0);
    EXPECT_EQ(eng_image_encoded_chain_size(8, 8, 0), (size_t)0);
    EXPECT_EQ(eng_image_encoded_chain_size(8, 8, 5),
              (size_t)0); /* plus que la chaîne (4 niveaux) */

    uint8_t chain[64];
    memset(chain, 0x5A, sizeof chain);
    EngImage image;
    EXPECT_TRUE(eng_image_create_encoded(8, 8, ENG_IMAGE_BC7, 1, chain, 64, &image));
    EXPECT_EQ(image.encoding, ENG_IMAGE_BC7);
    EXPECT_EQ(image.mip_count, (uint32_t)1);
    EXPECT_EQ(memcmp(image.pixels, chain, 64), 0);
    EXPECT_TRUE(image.pixels != chain); /* une copie */
    eng_image_free(&image);
    EXPECT_TRUE(image.pixels == NULL);

    EXPECT_FALSE(eng_image_create_encoded(8, 8, ENG_IMAGE_BC7, 1, chain, 63, &image));
    EXPECT_TRUE(image.pixels == NULL);
    EXPECT_FALSE(eng_image_create_encoded(8, 8, ENG_IMAGE_BC7, 1, chain, 16, &image));
    EXPECT_FALSE(eng_image_create_encoded(8, 8, ENG_IMAGE_RGBA8, 1, chain, 64, &image));
    EXPECT_FALSE(eng_image_create_encoded(8, 8, ENG_IMAGE_BC7, 1, NULL, 64, &image));
    EXPECT_FALSE(eng_image_create_encoded(8, 8, ENG_IMAGE_BC7, 0, chain, 64, &image));
    EXPECT_FALSE(
        eng_image_create_encoded(ENG_IMAGE_MAX_SIZE + 1U, 8, ENG_IMAGE_BC7, 1, chain, 64, &image));

    /* Limite inclusive : le côté maximal est accepté, un de plus est refusé (largeur et hauteur).
     */
    static uint8_t strip[(ENG_IMAGE_MAX_SIZE / 4U) * 16U];
    ASSERT_EQ(eng_image_encoded_chain_size(ENG_IMAGE_MAX_SIZE, 1, 1), sizeof strip);
    EXPECT_TRUE(eng_image_create_encoded(ENG_IMAGE_MAX_SIZE, 1, ENG_IMAGE_BC7, 1, strip,
                                         sizeof strip, &image));
    eng_image_free(&image);
    EXPECT_TRUE(eng_image_create_encoded(1, ENG_IMAGE_MAX_SIZE, ENG_IMAGE_BC7, 1, strip,
                                         sizeof strip, &image));
    eng_image_free(&image);
    EXPECT_FALSE(eng_image_create_encoded(1, ENG_IMAGE_MAX_SIZE + 1U, ENG_IMAGE_BC7, 1, strip,
                                          sizeof strip, &image));
}

UTEST(eng_tex_file, bc7_encode_gives_a_valid_file_of_the_expected_layout)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    static const uint32_t sizes[4][2] = {{8, 8}, {5, 3}, {1, 1}, {17, 9}};
    for (int k = 0; k < 4; k++) {
        const uint32_t w = sizes[k][0];
        const uint32_t h = sizes[k][1];
        EngImage image;
        ASSERT_TRUE(make_image(&image, w, h));
        for (int srgb = 0; srgb < 2; srgb++) {
            const void *file = NULL;
            size_t size = 0;
            ASSERT_TRUE(eng_tex_encode_bc7(&f.arena, &f.scratch, &image, srgb != 0, &file, &size));
            EngTexView view;
            ASSERT_TRUE(eng_tex_open(file, size, "bc7", &view));
            EXPECT_EQ(view.info.format, srgb != 0 ? ENG_TEX_BC7_SRGB : ENG_TEX_BC7_UNORM);
            EXPECT_EQ(view.info.width, w);
            EXPECT_EQ(view.info.height, h);
            EXPECT_EQ(view.info.mip_count, eng_image_mip_count(w, h));
            EXPECT_EQ(view.info.layer_count, (uint32_t)1);
            size_t total = 0;
            for (uint32_t level = 0; level < view.info.mip_count; level++) {
                const uint8_t *p = NULL;
                uint32_t bytes = 0;
                ASSERT_TRUE(eng_tex_subresource(&view, 0, level, &p, &bytes));
                uint32_t lw = 0;
                uint32_t lh = 0;
                eng_image_mip_size(w, h, level, &lw, &lh);
                EXPECT_EQ(bytes, (uint32_t)(((lw + 3U) / 4U) * ((lh + 3U) / 4U) * 16U));
                for (uint32_t b = 0; b + 16U <= bytes; b += 16U) {
                    EXPECT_TRUE(block_has_valid_mode(p + b));
                }
                total += bytes;
            }
            EXPECT_EQ(total, eng_image_encoded_chain_size(w, h, view.info.mip_count));
            /* et la conversion rend exactement cette chaîne */
            EngImage out;
            bool is_srgb = srgb == 0;
            ASSERT_TRUE(eng_tex_to_image(&view, &out, &is_srgb));
            EXPECT_EQ(is_srgb, srgb != 0);
            EXPECT_EQ(out.encoding, srgb != 0 ? ENG_IMAGE_BC7_SRGB : ENG_IMAGE_BC7);
            EXPECT_EQ(out.mip_count, view.info.mip_count);
            eng_image_free(&out);
        }
        eng_image_free(&image);
    }
    fixture_close(&f);
}

UTEST(eng_tex_file, bc7_encoding_is_deterministic_and_four_times_smaller_than_rgba8)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngImage image;
    ASSERT_TRUE(make_image(&image, 64, 64));
    const void *a = NULL;
    const void *b = NULL;
    const void *raw = NULL;
    size_t size_a = 0;
    size_t size_b = 0;
    size_t size_raw = 0;
    ASSERT_TRUE(eng_tex_encode_bc7(&f.arena, &f.scratch, &image, true, &a, &size_a));
    ASSERT_TRUE(eng_tex_encode_bc7(&f.arena, &f.scratch, &image, true, &b, &size_b));
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, true, &raw, &size_raw));
    ASSERT_EQ(size_a, size_b);
    EXPECT_EQ(memcmp(a, b, size_a), 0);
    /* mêmes niveaux, 1 octet par texel contre 4 */
    EngTexView va;
    EngTexView vr;
    ASSERT_TRUE(eng_tex_open(a, size_a, "a", &va));
    ASSERT_TRUE(eng_tex_open(raw, size_raw, "raw", &vr));
    /* 64 x 64, 7 niveaux : BC7 = 4096 + 1024 + 256 + 64 + 16 + 16 + 16 = 5488 octets (les trois
     * derniers niveaux coûtent un bloc entier) ; RGBA8 = 21844. Un peu plus du quart, pas moins. */
    EXPECT_EQ(va.data_size, (uint32_t)5488);
    EXPECT_EQ(vr.data_size, (uint32_t)21844);
    /* le sRGB change la mesure d'erreur donc (en général) les octets ; le drapeau change toujours
     */
    const void *lin = NULL;
    size_t size_lin = 0;
    ASSERT_TRUE(eng_tex_encode_bc7(&f.arena, &f.scratch, &image, false, &lin, &size_lin));
    EngTexView vl;
    ASSERT_TRUE(eng_tex_open(lin, size_lin, "lin", &vl));
    EXPECT_EQ(vl.info.format, ENG_TEX_BC7_UNORM);
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, bc7_encode_refuses_bad_input_and_restores_the_arenas)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const size_t arena_before = eng_arena_remaining(&f.arena);
    const size_t scratch_before = eng_arena_remaining(&f.scratch);
    const void *file = (const void *)1;
    size_t size = 7;
    EngImage empty = {0};
    EXPECT_FALSE(eng_tex_encode_bc7(&f.arena, &f.scratch, &empty, true, &file, &size));
    EXPECT_TRUE(file == NULL);
    EXPECT_EQ(size, (size_t)0);
    /* une image déjà compressée ne se recompresse pas */
    uint8_t chain[16] = {0};
    EngImage encoded;
    ASSERT_TRUE(eng_image_create_encoded(4, 4, ENG_IMAGE_BC7, 1, chain, 16, &encoded));
    EXPECT_FALSE(eng_tex_encode_bc7(&f.arena, &f.scratch, &encoded, true, &file, &size));
    EXPECT_FALSE(eng_tex_encode(&f.arena, &f.scratch, &encoded, true, &file, &size));
    eng_image_free(&encoded);
    EXPECT_EQ(eng_arena_remaining(&f.arena), arena_before);
    EXPECT_EQ(eng_arena_remaining(&f.scratch), scratch_before);

    /* mémoire insuffisante : refus propre, arènes intactes */
    EngImage image;
    ASSERT_TRUE(make_image(&image, 64, 64));
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 4096));
    const size_t tiny_before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_tex_encode_bc7(&tiny, &f.scratch, &image, true, &file, &size));
    EXPECT_EQ(eng_arena_remaining(&tiny), tiny_before);
    EXPECT_EQ(eng_arena_remaining(&f.scratch), scratch_before);
    eng_arena_shutdown(&tiny);
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, bc7_byte_flip_fuzz_never_reads_out_of_bounds)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngImage image;
    ASSERT_TRUE(make_image(&image, 9, 5));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_tex_encode_bc7(&f.arena, &f.scratch, &image, true, &file, &size));
    const uint8_t values[3] = {0x00, 0x7F, 0xFF};
    for (size_t at = 0; at < size; at++) {
        for (int v = 0; v < 3; v++) {
            uint8_t *copy = copy_of(file, size);
            ASSERT_TRUE(copy != NULL);
            copy[at] = values[v];
            EngTexView view;
            if (eng_tex_open(copy, size, "fuzz", &view)) {
                EngImage out;
                bool srgb = false;
                if (eng_tex_to_image(&view, &out, &srgb)) {
                    EXPECT_EQ(eng_image_encoded_chain_size(out.width, out.height, out.mip_count) >
                                  0U,
                              true);
                    eng_image_free(&out);
                }
            }
            SDL_free(copy);
        }
    }
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, layer_limit_is_inclusive)
{
    static uint8_t out[32768];
    static uint8_t data[(ENG_TEX_MAX_LAYERS + 1U) * 16U];
    static uint32_t mips[(ENG_TEX_MAX_LAYERS + 1U) * 2U];
    for (uint32_t layers = ENG_TEX_MAX_LAYERS; layers <= ENG_TEX_MAX_LAYERS + 1U; layers++) {
        uint32_t info[8] = {ENG_TEX_RGBA8_UNORM, 1, 1, 1, layers, 0, 0, 0};
        for (uint32_t i = 0; i < layers; i++) {
            mips[(size_t)i * 2U] = i * 16U;
            mips[(size_t)i * 2U + 1U] = 4;
        }
        const EngAssetSectionDesc s[3] = {
            {ENG_ASSET_TAG_INFO, info, sizeof info, 1},
            {ENG_ASSET_TAG_MIPS, mips, layers * 8U, layers},
            {ENG_ASSET_TAG_DATA, data, layers * 16U - 12U, layers * 16U - 12U}};
        size_t size = 0;
        ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, s, 3, out,
                                    sizeof out, &size));
        EngTexView view;
        EXPECT_EQ(eng_tex_open(out, size, "layers", &view), layers == ENG_TEX_MAX_LAYERS);
    }
}

UTEST(eng_tex_file, open_zeroes_the_view_on_failure)
{
    EngTexView view;
    memset(&view, 0xAB, sizeof view);
    const uint8_t junk[40] = {1, 2, 3};
    EXPECT_FALSE(eng_tex_open(junk, sizeof junk, NULL, &view));
    EXPECT_TRUE(view.data == NULL);
    EXPECT_EQ(view.info.width, (uint32_t)0);
    EXPECT_FALSE(eng_tex_open(NULL, 0, "x", &view));
}

UTEST(eng_tex_file, byte_flip_fuzz_never_reads_out_of_bounds)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngImage image;
    ASSERT_TRUE(make_image(&image, 5, 3));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, false, &file, &size));
    const uint8_t values[3] = {0x00, 0x7F, 0xFF};
    for (size_t at = 0; at < size; at++) {
        for (int v = 0; v < 3; v++) {
            uint8_t *copy = copy_of(file, size); /* tampon exact : ASan voit tout dépassement */
            ASSERT_TRUE(copy != NULL);
            copy[at] = values[v];
            EngTexView view;
            if (eng_tex_open(copy, size, "fuzz", &view)) {
                for (uint32_t layer = 0; layer < view.info.layer_count; layer++) {
                    for (uint32_t level = 0; level < view.info.mip_count; level++) {
                        const uint8_t *p = NULL;
                        uint32_t bytes = 0;
                        if (!eng_tex_subresource(&view, layer, level, &p, &bytes)) {
                            EXPECT_TRUE(false);
                            break;
                        }
                        EXPECT_TRUE(p >= copy && (size_t)(p - copy) + bytes <= size);
                    }
                }
                EngImage out;
                bool srgb;
                if (eng_tex_to_image(&view, &out, &srgb)) {
                    eng_image_free(&out);
                }
            }
            SDL_free(copy);
        }
    }
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, truncations_are_always_rejected)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngImage image;
    ASSERT_TRUE(make_image(&image, 4, 4));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_tex_encode(&f.arena, &f.scratch, &image, true, &file, &size));
    for (size_t n = 0; n < size; n++) {
        uint8_t *copy = copy_of(file, n > 0 ? n : 1);
        ASSERT_TRUE(copy != NULL);
        EXPECT_FALSE(opens(copy, n));
        SDL_free(copy);
    }
    EXPECT_TRUE(opens(file, size));
    eng_image_free(&image);
    fixture_close(&f);
}

UTEST(eng_tex_file, image_create_validates_and_owns_its_pixels)
{
    EngImage image = {.width = 9};
    EXPECT_FALSE(eng_image_create(0, 4, NULL, &image));
    EXPECT_TRUE(image.pixels == NULL);
    EXPECT_EQ(image.width, (uint32_t)0);
    EXPECT_FALSE(eng_image_create(4, ENG_IMAGE_MAX_SIZE + 1U, NULL, &image));
    const uint8_t rgba[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_TRUE(eng_image_create(2, 1, rgba, &image));
    EXPECT_EQ(memcmp(image.pixels, rgba, 8), 0);
    EXPECT_TRUE(image.pixels != rgba); /* copie */
    eng_image_free(&image);
    EXPECT_TRUE(image.pixels == NULL);
    eng_image_free(&image); /* idempotent */
    ASSERT_TRUE(eng_image_create(3, 3, NULL, &image));
    for (int i = 0; i < 36; i++) {
        EXPECT_EQ(image.pixels[i], (uint8_t)0); /* NULL : mis à zéro */
    }
    eng_image_free(&image);
}
