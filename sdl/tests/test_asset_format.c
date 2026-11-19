#include <SDL3/SDL.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/asset_format.h"
#include "test_util.h"
#include "utest.h"

#define DIR TEST_TMP_DIR "/asset_format"

/* Fichier de référence : 3 sections de tailles 5, 16 et 0 (le remplissage et la section vide sont
 * ainsi couverts). Construit par eng_asset_build, puis corrompu octet par octet par les tests. */
#define MAX_FILE 512

static const uint8_t PAYLOAD_A[5] = {1, 2, 3, 4, 5};
static const uint8_t PAYLOAD_B[16] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
                                      0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF};

static size_t build_reference(uint8_t *out)
{
    const EngAssetSectionDesc sections[3] = {
        {ENG_ASSET_TAG_INFO, PAYLOAD_A, sizeof PAYLOAD_A, 1},
        {ENG_ASSET_TAG_VERT, PAYLOAD_B, sizeof PAYLOAD_B, 4},
        {ENG_ASSET_TAG_STRS, NULL, 0, 0},
    };
    size_t written = 0;
    if (!eng_asset_build(ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, sections, 3, out, MAX_FILE,
                         &written)) {
        return 0;
    }
    return written;
}

static void set_u32(uint8_t *file, size_t at, uint32_t v)
{
    file[at] = (uint8_t)(v & 0xFFU);
    file[at + 1] = (uint8_t)((v >> 8) & 0xFFU);
    file[at + 2] = (uint8_t)((v >> 16) & 0xFFU);
    file[at + 3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t get_u32(const uint8_t *file, size_t at)
{
    return (uint32_t)file[at] | ((uint32_t)file[at + 1] << 8) | ((uint32_t)file[at + 2] << 16) |
           ((uint32_t)file[at + 3] << 24);
}

static EngAssetStatus check(const uint8_t *file, size_t size)
{
    EngAssetView view;
    return eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, &view);
}

/* Entrées de la table : 32 + 16 * i ; champs tag/offset/size/count. */
#define ENTRY(i) (32U + 16U * (i))

UTEST(eng_asset_format, fourcc_is_little_endian_ascii)
{
    const uint8_t expected[4] = {'O', 'W', 'M', 'H'};
    uint32_t magic = ENG_ASSET_MAGIC_MESH;
    EXPECT_EQ(memcmp(&magic, expected, 4), 0);
    EXPECT_EQ(ENG_FOURCC('a', 'b', 'c', 'd'), (uint32_t)0x64636261U);
}

UTEST(eng_asset_format, magics_and_tags_are_distinct)
{
    const uint32_t magics[4] = {ENG_ASSET_MAGIC_MESH, ENG_ASSET_MAGIC_TEX, ENG_ASSET_MAGIC_SKEL,
                                ENG_ASSET_MAGIC_ANIM};
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            EXPECT_NE(magics[i], magics[j]);
        }
    }
    const uint32_t tags[14] = {ENG_ASSET_TAG_INFO, ENG_ASSET_TAG_STRS, ENG_ASSET_TAG_VERT,
                               ENG_ASSET_TAG_INDX, ENG_ASSET_TAG_PART, ENG_ASSET_TAG_MATL,
                               ENG_ASSET_TAG_TREF, ENG_ASSET_TAG_MIPS, ENG_ASSET_TAG_DATA,
                               ENG_ASSET_TAG_JNTS, ENG_ASSET_TAG_IBMS, ENG_ASSET_TAG_CHAN,
                               ENG_ASSET_TAG_TIME, ENG_ASSET_TAG_VALS};
    for (int i = 0; i < 14; i++) {
        EXPECT_NE(tags[i], (uint32_t)0);
        for (int j = i + 1; j < 14; j++) {
            EXPECT_NE(tags[i], tags[j]);
        }
    }
}

UTEST(eng_asset_format, build_layout_is_exactly_the_documented_one)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    /* en-tête 32 + table 3*16 = 80 ; A à 80 (5 octets) ; B à 96 (16) ; STRS à 112 (0) */
    ASSERT_EQ(size, (size_t)112);
    EXPECT_EQ(get_u32(file, 0), ENG_ASSET_MAGIC_MESH);
    EXPECT_EQ(get_u32(file, 4), (uint32_t)0x01020304U);
    EXPECT_EQ(file[4], (uint8_t)0x04); /* petit-boutiste : l'octet de poids faible d'abord */
    EXPECT_EQ(file[8], (uint8_t)ENG_ASSET_VERSION_MESH);
    EXPECT_EQ(file[9], (uint8_t)0);
    EXPECT_EQ(file[10], (uint8_t)32);
    EXPECT_EQ(file[11], (uint8_t)0);
    EXPECT_EQ(get_u32(file, 12), (uint32_t)0);
    EXPECT_EQ(get_u32(file, 16), (uint32_t)112);
    EXPECT_EQ(get_u32(file, 20), (uint32_t)3);
    EXPECT_EQ(get_u32(file, 24), (uint32_t)0);
    EXPECT_EQ(get_u32(file, 28), (uint32_t)0);

    EXPECT_EQ(get_u32(file, ENTRY(0) + 0), ENG_ASSET_TAG_INFO);
    EXPECT_EQ(get_u32(file, ENTRY(0) + 4), (uint32_t)80);
    EXPECT_EQ(get_u32(file, ENTRY(0) + 8), (uint32_t)5);
    EXPECT_EQ(get_u32(file, ENTRY(0) + 12), (uint32_t)1);
    EXPECT_EQ(get_u32(file, ENTRY(1) + 4), (uint32_t)96);
    EXPECT_EQ(get_u32(file, ENTRY(1) + 8), (uint32_t)16);
    EXPECT_EQ(get_u32(file, ENTRY(1) + 12), (uint32_t)4);
    EXPECT_EQ(get_u32(file, ENTRY(2) + 4), (uint32_t)112);
    EXPECT_EQ(get_u32(file, ENTRY(2) + 8), (uint32_t)0);

    EXPECT_EQ(memcmp(file + 80, PAYLOAD_A, 5), 0);
    for (size_t i = 85; i < 96; i++) {
        EXPECT_EQ(file[i], (uint8_t)0); /* remplissage à zéro */
    }
    EXPECT_EQ(memcmp(file + 96, PAYLOAD_B, 16), 0);
}

UTEST(eng_asset_format, build_is_deterministic_even_over_dirty_memory)
{
    uint8_t a[MAX_FILE];
    uint8_t b[MAX_FILE];
    memset(a, 0x00, sizeof a);
    memset(b, 0xFF, sizeof b);
    const size_t size_a = build_reference(a);
    const size_t size_b = build_reference(b);
    ASSERT_EQ(size_a, size_b);
    EXPECT_EQ(memcmp(a, b, size_a), 0);
}

UTEST(eng_asset_format, valid_file_opens_and_sections_are_found)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    EngAssetView view;
    ASSERT_EQ(
        eng_asset_open(file, size, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, "ref", &view),
        ENG_ASSET_OK);
    EXPECT_EQ(view.magic, ENG_ASSET_MAGIC_MESH);
    EXPECT_EQ(view.version, (uint16_t)ENG_ASSET_VERSION_MESH);
    EXPECT_EQ(view.section_count, (uint32_t)3);
    EXPECT_EQ(view.size, size);

    EngAssetSection s;
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_VERT, &s));
    EXPECT_EQ(s.tag, ENG_ASSET_TAG_VERT);
    EXPECT_EQ(s.size, (uint32_t)16);
    EXPECT_EQ(s.count, (uint32_t)4);
    EXPECT_EQ(memcmp(s.data, PAYLOAD_B, 16), 0);
    EXPECT_TRUE(s.data == file + 96);

    ASSERT_TRUE(eng_asset_section_at(&view, 0, &s));
    EXPECT_EQ(s.tag, ENG_ASSET_TAG_INFO);
    EXPECT_EQ(memcmp(s.data, PAYLOAD_A, 5), 0);
    ASSERT_TRUE(eng_asset_section_at(&view, 2, &s));
    EXPECT_EQ(s.tag, ENG_ASSET_TAG_STRS);
    EXPECT_EQ(s.size, (uint32_t)0);

    EXPECT_FALSE(eng_asset_find_section(&view, ENG_ASSET_TAG_INDX, &s));
    EXPECT_FALSE(eng_asset_section_at(&view, 3, &s));
    EXPECT_FALSE(eng_asset_section_at(NULL, 0, &s));
    EngAssetView empty = {0};
    EXPECT_FALSE(eng_asset_find_section(&empty, ENG_ASSET_TAG_INFO, &s));
}

UTEST(eng_asset_format, file_without_section_is_valid)
{
    uint8_t file[MAX_FILE];
    size_t size = 0;
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, NULL, 0, file,
                                sizeof file, &size));
    EXPECT_EQ(size, (size_t)32);
    EngAssetView view;
    EXPECT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, &view),
              ENG_ASSET_OK);
    EXPECT_EQ(view.section_count, (uint32_t)0);
}

/* ── Critère : numéro de version vérifié au chargement ── */

UTEST(eng_asset_format, version_must_match_exactly)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    EngAssetView view;
    /* Le moteur lit la version 2 alors que le fichier est en version 1 : à recuire. */
    EXPECT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, 2, &view), ENG_ASSET_VERSION_OLDER);
    EXPECT_TRUE(view.data == NULL); /* vue mise à zéro */
    /* Fichier d'une version plus récente que le moteur. */
    file[8] = (uint8_t)(ENG_ASSET_VERSION_MESH + 1U);
    EXPECT_EQ(check(file, size), ENG_ASSET_VERSION_NEWER);
    file[8] = 0; /* version 0 : plus ancienne que toute version valide */
    EXPECT_EQ(check(file, size), ENG_ASSET_VERSION_OLDER);
    file[8] = 0xFF;
    file[9] = 0xFF; /* 65535 */
    EXPECT_EQ(check(file, size), ENG_ASSET_VERSION_NEWER);
}

UTEST(eng_asset_format, version_is_checked_before_the_rest_of_the_header)
{
    /* Un format futur peut changer tout le reste : la version doit être jugée en premier pour
     * donner un message utile (« moteur trop ancien ») plutôt qu'un « en-tête invalide ». */
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    file[8] = 7;
    set_u32(file, 16, 12345); /* file_size faux aussi */
    set_u32(file, 24, 99);    /* réservé non nul aussi */
    EXPECT_EQ(check(file, size), ENG_ASSET_VERSION_NEWER);
}

UTEST(eng_asset_format, wrong_magic_and_wrong_type_are_rejected)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    EngAssetView view;
    EXPECT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, &view),
              ENG_ASSET_BAD_MAGIC);
    file[0] ^= 0x01;
    EXPECT_EQ(check(file, size), ENG_ASSET_BAD_MAGIC);
}

UTEST(eng_asset_format, endian_tag_is_checked)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    set_u32(file, 4, 0x04030201U); /* ce que lirait un fichier écrit en grand-boutiste */
    EXPECT_EQ(check(file, size), ENG_ASSET_BAD_ENDIAN);
}

UTEST(eng_asset_format, too_small_inputs_are_rejected)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    EXPECT_EQ(check(NULL, size), ENG_ASSET_TOO_SMALL);
    EXPECT_EQ(check(file, 0), ENG_ASSET_TOO_SMALL);
    EXPECT_EQ(check(file, 31), ENG_ASSET_TOO_SMALL);
    EXPECT_EQ(eng_asset_check(NULL, 0, ENG_ASSET_MAGIC_MESH, 1, NULL), ENG_ASSET_TOO_SMALL);
}

UTEST(eng_asset_format, header_fields_must_have_their_fixed_values)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    const size_t fields[4] = {10, 12, 24, 28}; /* header_size, flags, reserved0, reserved1 */
    for (int i = 0; i < 4; i++) {
        build_reference(file);
        file[fields[i]] ^= 0x01;
        EXPECT_EQ(check(file, size), ENG_ASSET_BAD_HEADER);
    }
    build_reference(file);
    file[13] = 0x80; /* un bit haut des flags */
    EXPECT_EQ(check(file, size), ENG_ASSET_BAD_HEADER);
}

UTEST(eng_asset_format, truncation_and_trailing_bytes_are_detected)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    /* Tronqué à chaque longueur possible : jamais accepté, jamais de lecture hors tampon (ASan). */
    for (size_t n = 0; n < size; n++) {
        EXPECT_NE(check(file, n), ENG_ASSET_OK);
    }
    memset(file + size, 0, 16);
    EXPECT_EQ(check(file, size + 1), ENG_ASSET_SIZE_MISMATCH);
    EXPECT_EQ(check(file, size + 16), ENG_ASSET_SIZE_MISMATCH);
    EXPECT_EQ(check(file, size), ENG_ASSET_OK);
    /* file_size de l'en-tête incohérent avec la taille réelle, sections intactes : refusé aussi. */
    set_u32(file, 16, (uint32_t)size + 16U);
    EXPECT_EQ(check(file, size), ENG_ASSET_SIZE_MISMATCH);
    set_u32(file, 16, (uint32_t)size - 1U);
    EXPECT_EQ(check(file, size), ENG_ASSET_SIZE_MISMATCH);
}

UTEST(eng_asset_format, section_count_limits)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    set_u32(file, 20, ENG_ASSET_MAX_SECTIONS + 1U);
    EXPECT_EQ(check(file, size), ENG_ASSET_BAD_TABLE);
    set_u32(file, 20, 0xFFFFFFFFU);
    EXPECT_EQ(check(file, size), ENG_ASSET_BAD_TABLE);
    /* Table qui sort du fichier : 7 entrées de 16 octets dépassent les 112 octets. */
    set_u32(file, 20, 6);
    EXPECT_EQ(check(file, size), ENG_ASSET_BAD_TABLE);
}

UTEST(eng_asset_format, section_entry_corruptions_are_rejected)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);

    build_reference(file);
    set_u32(file, ENTRY(1), 0);
    EXPECT_EQ(check(file, size), ENG_ASSET_BAD_TAG);

    build_reference(file);
    set_u32(file, ENTRY(1), ENG_ASSET_TAG_INFO);
    EXPECT_EQ(check(file, size), ENG_ASSET_DUPLICATE_TAG);

    build_reference(file);
    set_u32(file, ENTRY(1) + 4, 97); /* non aligné */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_ALIGN);

    build_reference(file);
    set_u32(file, ENTRY(0) + 4, 32); /* dans la table */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_OVERLAP);

    build_reference(file);
    set_u32(file, ENTRY(1) + 4, 80); /* même place que la section 0 */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_OVERLAP);

    build_reference(file);
    set_u32(file, ENTRY(0) + 8,
            17); /* la section 0 (à 80) s'étend jusqu'à 97 et mord sur la 1 (à 96) */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_OVERLAP);

    build_reference(file);
    set_u32(file, ENTRY(1) + 8, 17); /* 96 + 17 = 113 > 112 */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_RANGE);

    build_reference(file);
    set_u32(file, ENTRY(1) + 4, 0xFFFFFFF0U); /* aligné, loin après la fin */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_RANGE);

    build_reference(file);
    set_u32(file, ENTRY(1) + 8, 0xFFFFFFFFU); /* 96 + 4 Gio : déborderait en 32 bits */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_RANGE);

    build_reference(file);
    set_u32(file, ENTRY(2) + 4, 128); /* section vide au-delà de la fin : refusée */
    EXPECT_EQ(check(file, size), ENG_ASSET_SECTION_RANGE);

    build_reference(file);
    EXPECT_EQ(check(file, size),
              ENG_ASSET_OK); /* contrôle : le fichier de référence reste valide */
}

UTEST(eng_asset_format, count_field_is_free_and_never_validated_by_the_container)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    set_u32(file, ENTRY(1) + 12, 0xFFFFFFFFU);
    EXPECT_EQ(check(file, size), ENG_ASSET_OK);
}

UTEST(eng_asset_format, padding_is_not_verified_on_load)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    file[90] = 0x55; /* octet de remplissage entre A et B */
    EXPECT_EQ(check(file, size), ENG_ASSET_OK);
}

UTEST(eng_asset_format, byte_flip_fuzz_never_reads_out_of_bounds)
{
    /* Chaque octet modifié (3 valeurs) : le résultat peut être OK ou une erreur, mais si OK alors
     * toutes les sections doivent tenir dans le tampon. ASan/UBSan détectent le reste. */
    uint8_t reference[MAX_FILE];
    const size_t size = build_reference(reference);
    const uint8_t values[3] = {0x00, 0x7F, 0xFF};
    for (size_t at = 0; at < size; at++) {
        for (int v = 0; v < 3; v++) {
            uint8_t *copy = SDL_malloc(size); /* tampon exact : ASan voit tout dépassement */
            ASSERT_TRUE(copy != NULL);
            memcpy(copy, reference, size);
            copy[at] = values[v];
            EngAssetView view;
            if (eng_asset_check(copy, size, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, &view) ==
                ENG_ASSET_OK) {
                for (uint32_t i = 0; i < view.section_count; i++) {
                    EngAssetSection s;
                    if (!eng_asset_section_at(&view, i, &s)) {
                        EXPECT_TRUE(false); /* pas ASSERT : `copy` doit être libéré */
                        break;
                    }
                    EXPECT_TRUE(s.data >= copy);
                    EXPECT_TRUE((size_t)(s.data - copy) + s.size <= size);
                }
            }
            SDL_free(copy);
        }
    }
}

/* ── Journal et statuts ── */

UTEST(eng_asset_format, open_reports_errors_and_zeroes_the_view)
{
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    EngAssetView view;
    memset(&view, 0xAB, sizeof view);
    file[8] = 9;
    EXPECT_EQ(
        eng_asset_open(file, size, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, "a.mesh", &view),
        ENG_ASSET_VERSION_NEWER);
    EXPECT_TRUE(view.data == NULL);
    EXPECT_EQ(view.section_count, (uint32_t)0);
    /* Sans nom et sans sortie : accepté. */
    EXPECT_EQ(eng_asset_open(file, size, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, NULL, NULL),
              ENG_ASSET_VERSION_NEWER);
    EXPECT_EQ(eng_asset_open(file, 3, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, "b", NULL),
              ENG_ASSET_TOO_SMALL);
}

UTEST(eng_asset_format, status_names_are_distinct_and_stable)
{
    EXPECT_STREQ(eng_asset_status_name(ENG_ASSET_OK), "ok");
    EXPECT_STREQ(eng_asset_status_name(ENG_ASSET_VERSION_OLDER), "version_older");
    EXPECT_STREQ(eng_asset_status_name(ENG_ASSET_VERSION_NEWER), "version_newer");
    EngAssetStatus invalid;
    const int raw = 99;
    memcpy(&invalid, &raw, sizeof invalid); /* valeur hors énumération, sans conversion directe */
    EXPECT_STREQ(eng_asset_status_name(invalid), "unknown");
    for (int i = 0; i <= ENG_ASSET_SECTION_RANGE; i++) {
        for (int j = i + 1; j <= ENG_ASSET_SECTION_RANGE; j++) {
            EXPECT_STRNE(eng_asset_status_name((EngAssetStatus)i),
                         eng_asset_status_name((EngAssetStatus)j));
        }
        EXPECT_STRNE(eng_asset_status_name((EngAssetStatus)i), "unknown");
    }
}

/* ── Écriture : refus ── */

UTEST(eng_asset_format, build_rejects_invalid_inputs)
{
    uint8_t out[MAX_FILE];
    const uint8_t byte = 1;
    const EngAssetSectionDesc ok = {ENG_ASSET_TAG_INFO, &byte, 1, 1};
    EXPECT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, &ok, 1, out, sizeof out, NULL));

    EXPECT_FALSE(
        eng_asset_build(ENG_ASSET_MAGIC_MESH, 0, &ok, 1, out, sizeof out, NULL)); /* version 0 */
    EXPECT_FALSE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, &ok, 1, NULL, sizeof out, NULL));
    EXPECT_FALSE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, NULL, 1, out, sizeof out, NULL));

    const EngAssetSectionDesc zero_tag = {0, &byte, 1, 0};
    EXPECT_FALSE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, &zero_tag, 1, out, sizeof out, NULL));
    EXPECT_EQ(eng_asset_file_size(&zero_tag, 1), (size_t)0);

    const EngAssetSectionDesc null_data = {ENG_ASSET_TAG_INFO, NULL, 4, 0};
    EXPECT_FALSE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, &null_data, 1, out, sizeof out, NULL));

    const EngAssetSectionDesc dup[2] = {ok, ok};
    EXPECT_FALSE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, dup, 2, out, sizeof out, NULL));
    EXPECT_EQ(eng_asset_file_size(dup, 2), (size_t)0);

    const EngAssetSectionDesc huge = {ENG_ASSET_TAG_INFO, &byte, 0xFFFFFFF0U, 0};
    EXPECT_EQ(eng_asset_file_size(&huge, 1), (size_t)0); /* dépasse 32 bits avec l'en-tête */

    EngAssetSectionDesc many[ENG_ASSET_MAX_SECTIONS + 1U];
    for (uint32_t i = 0; i < ENG_ASSET_MAX_SECTIONS + 1U; i++) {
        many[i] = (EngAssetSectionDesc){0x100U + i, NULL, 0, 0};
    }
    EXPECT_EQ(eng_asset_file_size(many, ENG_ASSET_MAX_SECTIONS), (size_t)(32 + 16 * 64));
    EXPECT_EQ(eng_asset_file_size(many, ENG_ASSET_MAX_SECTIONS + 1U), (size_t)0);
}

UTEST(eng_asset_format, build_respects_capacity_and_leaves_output_untouched_on_refusal)
{
    uint8_t out[MAX_FILE];
    const uint8_t byte = 1;
    const EngAssetSectionDesc s = {ENG_ASSET_TAG_INFO, &byte, 1, 1};
    const size_t needed = eng_asset_file_size(&s, 1);
    ASSERT_EQ(
        needed,
        (size_t)49); /* 32 + 16 = 48 ; section à 48 ; 1 octet -> 49 (pas de remplissage final) */
    memset(out, 0xCD, sizeof out);
    size_t written = 12345;
    EXPECT_FALSE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, &s, 1, out, needed - 1, &written));
    EXPECT_EQ(written, (size_t)12345);
    for (size_t i = 0; i < sizeof out; i++) {
        EXPECT_EQ(out[i], (uint8_t)0xCD);
    }
    EXPECT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, &s, 1, out, needed, &written));
    EXPECT_EQ(written, needed);
    EXPECT_EQ(out[needed], (uint8_t)0xCD); /* rien écrit au-delà de la taille annoncée */
}

UTEST(eng_asset_format, build_then_check_round_trip_with_64_sections)
{
    static uint8_t data[ENG_ASSET_MAX_SECTIONS][7];
    EngAssetSectionDesc descs[ENG_ASSET_MAX_SECTIONS];
    for (uint32_t i = 0; i < ENG_ASSET_MAX_SECTIONS; i++) {
        memset(data[i], (int)i, sizeof data[i]);
        descs[i] = (EngAssetSectionDesc){0x4000U + i, data[i], (uint32_t)sizeof data[i], i};
    }
    const size_t size = eng_asset_file_size(descs, ENG_ASSET_MAX_SECTIONS);
    ASSERT_TRUE(size > 0);
    uint8_t *file = SDL_malloc(size);
    ASSERT_TRUE(file != NULL);
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_ANIM, ENG_ASSET_VERSION_ANIM, descs,
                                ENG_ASSET_MAX_SECTIONS, file, size, NULL));
    EngAssetView view;
    ASSERT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_ANIM, ENG_ASSET_VERSION_ANIM, &view),
              ENG_ASSET_OK);
    for (uint32_t i = 0; i < ENG_ASSET_MAX_SECTIONS; i++) {
        EngAssetSection s;
        ASSERT_TRUE(eng_asset_find_section(&view, 0x4000U + i, &s));
        EXPECT_EQ(s.count, i);
        EXPECT_EQ(s.size, (uint32_t)7);
        EXPECT_EQ(memcmp(s.data, data[i], 7), 0);
        EXPECT_EQ(((uintptr_t)(s.data - file)) % ENG_ASSET_ALIGN, (uintptr_t)0);
    }
    SDL_free(file);
}

/* ── Chargement depuis un fichier ── */

static void prepare(void)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
}

UTEST(eng_asset_format, load_reads_a_file_and_checks_the_version)
{
    prepare();
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/ok.mesh", DIR);
    ASSERT_TRUE(SDL_SaveFile(path, file, size));

    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "asset_test", (size_t)64 * 1024));
    EngAssetView view;
    ASSERT_TRUE(eng_asset_load(&arena, path, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, &view));
    EXPECT_EQ(view.section_count, (uint32_t)3);
    EXPECT_EQ(((uintptr_t)view.data) % ENG_ASSET_ALIGN, (uintptr_t)0); /* tampon aligné sur 16 */
    EngAssetSection s;
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_VERT, &s));
    EXPECT_EQ(((uintptr_t)s.data) % ENG_ASSET_ALIGN, (uintptr_t)0); /* donc les sections aussi */

    /* Mauvaise version attendue : refus, vue vide. */
    EXPECT_FALSE(eng_asset_load(&arena, path, ENG_ASSET_MAGIC_MESH, 2, &view));
    EXPECT_TRUE(view.data == NULL);
    /* Mauvais type. */
    EXPECT_FALSE(eng_asset_load(&arena, path, ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, &view));
    /* Fichier absent. */
    char missing[256];
    SDL_snprintf(missing, sizeof missing, "%s/absent.mesh", DIR);
    EXPECT_FALSE(
        eng_asset_load(&arena, missing, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, &view));
    EXPECT_TRUE(view.data == NULL);
    eng_arena_shutdown(&arena);
}

UTEST(eng_asset_format, load_rejects_a_truncated_file_on_disk)
{
    prepare();
    uint8_t file[MAX_FILE];
    const size_t size = build_reference(file);
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/cut.mesh", DIR);
    ASSERT_TRUE(SDL_SaveFile(path, file, size - 1));
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "asset_test", (size_t)64 * 1024));
    EngAssetView view;
    EXPECT_FALSE(eng_asset_load(&arena, path, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, &view));
    eng_arena_shutdown(&arena);
}
