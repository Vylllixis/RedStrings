/* Clips d'animation (M4-13) : échantillonnage, format .anim, import glTF. Les GLB sont construits
 * en mémoire (aucun fichier Mixamo réel). Valeurs attendues calculées à la main. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/anim_clip.h"
#include "engine/asset_format.h"
#include "engine/skeleton.h"
#include "utest.h"

typedef struct Fx {
    EngArena arena;
    EngArena scratch;
} Fx;

static bool fx_init(Fx *f)
{
    return eng_arena_init(&f->arena, "anim-test", (size_t)1 << 20) &&
           eng_arena_init(&f->scratch, "anim-scratch", (size_t)1 << 20);
}

static void fx_done(Fx *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_arena_shutdown(&f->arena);
}

/* ── Clip construit à la main ──
 * canal 0 : articulation 0, translation, instants 0 0,5 1 ; canal 1 : articulation 1, rotation,
 * instants 0 1 (identité -> 90° autour de Y) ; canal 2 : articulation 1, échelle, instant 0,25. */
static EngAnimChannel g_channels[3] = {
    {0, ENG_ANIM_TRANSLATION, 3, 0}, {1, ENG_ANIM_ROTATION, 2, 3}, {1, ENG_ANIM_SCALE, 1, 5}};
static float g_times[6] = {0.0F, 0.5F, 1.0F, 0.0F, 1.0F, 0.25F};
static float g_values[9 + 8 + 3] = {0, 1, 0, 2, 1, 0,           4, 1,           2, /* translation */
                                    0, 0, 0, 1, 0, 0.70710678F, 0, 0.70710678F,    /* rotation */
                                    1, 2, 3};                                      /* échelle */
static uint32_t g_value_first[3] = {0, 9, 17};

static EngAnimClip hand_clip(void)
{
    return (EngAnimClip){.duration = 1.0F,
                         .channel_count = 3,
                         .channels = g_channels,
                         .key_total = 6,
                         .times = g_times,
                         .values = g_values,
                         .value_first = g_value_first,
                         .skeleton_path = "models/perso.skel"};
}

UTEST(anim, sampling_interpolates_linearly_and_holds_outside_the_keys)
{
    const EngAnimClip clip = hand_clip();
    float v[4];
    ASSERT_EQ(eng_anim_sample(&clip, 0, 0.25F, v), 3U); /* entre (0,1,0) et (2,1,0), à mi-chemin */
    EXPECT_NEAR(v[0], 1.0F, 1e-6F);
    EXPECT_NEAR(v[1], 1.0F, 1e-6F);
    EXPECT_NEAR(v[2], 0.0F, 1e-6F);
    ASSERT_EQ(eng_anim_sample(&clip, 0, 0.75F, v), 3U); /* entre (2,1,0) et (4,1,2) */
    EXPECT_NEAR(v[0], 3.0F, 1e-6F);
    EXPECT_NEAR(v[2], 1.0F, 1e-6F);
    ASSERT_EQ(eng_anim_sample(&clip, 0, 0.5F, v), 3U); /* exactement sur une clé */
    EXPECT_NEAR(v[0], 2.0F, 1e-6F);
    eng_anim_sample(&clip, 0, -3.0F, v); /* avant : première valeur */
    EXPECT_NEAR(v[0], 0.0F, 1e-6F);
    eng_anim_sample(&clip, 0, 0.0F, v);
    EXPECT_NEAR(v[0], 0.0F, 1e-6F);
    eng_anim_sample(&clip, 0, 1.0F, v); /* sur la dernière : sa valeur */
    EXPECT_NEAR(v[0], 4.0F, 1e-6F);
    eng_anim_sample(&clip, 0, 99.0F, v); /* après : dernière valeur */
    EXPECT_NEAR(v[2], 2.0F, 1e-6F);
    ASSERT_EQ(eng_anim_sample(&clip, 2, 7.0F, v), 3U); /* une seule clé : constante */
    EXPECT_NEAR(v[0], 1.0F, 1e-6F);
    EXPECT_NEAR(v[2], 3.0F, 1e-6F);
}

UTEST(anim, rotation_uses_the_shortest_spherical_path)
{
    EngAnimClip clip = hand_clip();
    float v[4];
    ASSERT_EQ(eng_anim_sample(&clip, 1, 0.5F, v), 4U); /* 45° autour de Y */
    EXPECT_NEAR(v[0], 0.0F, 1e-6F);
    EXPECT_NEAR(v[1], 0.38268343F, 1e-6F); /* sin 22,5° */
    EXPECT_NEAR(v[2], 0.0F, 1e-6F);
    EXPECT_NEAR(v[3], 0.92387953F, 1e-6F); /* cos 22,5° */
    eng_anim_sample(&clip, 1, 0.0F, v);
    EXPECT_NEAR(v[3], 1.0F, 1e-6F);

    /* seconde clé négée (même rotation) : le résultat reste le chemin court, pas un tour de 360° */
    static float neg_values[20];
    memcpy(neg_values, g_values, sizeof neg_values);
    for (int i = 13; i < 17; i++) {
        neg_values[i] = -neg_values[i];
    }
    clip.values = neg_values;
    eng_anim_sample(&clip, 1, 0.5F, v);
    EXPECT_NEAR(fabsf(v[1]), 0.38268343F, 1e-6F);
    EXPECT_NEAR(fabsf(v[3]), 0.92387953F, 1e-6F);
    EXPECT_NEAR((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]) + (v[3] * v[3]), 1.0F, 1e-6F);

    /* deux clés presque identiques : repli sur la moyenne normalisée, sans NaN */
    static float close_values[20];
    memcpy(close_values, g_values, sizeof close_values);
    close_values[13] = 0.0F;
    close_values[14] = 0.0001F;
    close_values[15] = 0.0F;
    close_values[16] = 0.99999999F;
    clip.values = close_values;
    eng_anim_sample(&clip, 1, 0.5F, v);
    EXPECT_TRUE(isfinite(v[1]) && isfinite(v[3]));
    EXPECT_NEAR(v[1], 0.00005F, 1e-6F);
}

UTEST(anim, find_channel_and_invalid_sampling_arguments)
{
    const EngAnimClip clip = hand_clip();
    EXPECT_EQ(eng_anim_find_channel(&clip, 0, ENG_ANIM_TRANSLATION), 0U);
    EXPECT_EQ(eng_anim_find_channel(&clip, 1, ENG_ANIM_ROTATION), 1U);
    EXPECT_EQ(eng_anim_find_channel(&clip, 1, ENG_ANIM_SCALE), 2U);
    EXPECT_EQ(eng_anim_find_channel(&clip, 0, ENG_ANIM_ROTATION), UINT32_MAX);
    EXPECT_EQ(eng_anim_find_channel(&clip, 2, ENG_ANIM_TRANSLATION), UINT32_MAX);
    EXPECT_EQ(eng_anim_find_channel(&clip, 1, 9), UINT32_MAX);
    EXPECT_EQ(eng_anim_find_channel(NULL, 0, 0), UINT32_MAX);
    float v[4] = {-1, -1, -1, -1};
    EXPECT_EQ(eng_anim_sample(NULL, 0, 0.0F, v), 0U);
    EXPECT_EQ(eng_anim_sample(&clip, 3, 0.0F, v), 0U);
    EXPECT_EQ(eng_anim_sample(&clip, 0, NAN, v), 0U);
    EXPECT_EQ(eng_anim_sample(&clip, 0, 0.0F, NULL), 0U);
    EXPECT_EQ(v[0], -1.0F); /* rien d'écrit en cas de refus */
    EXPECT_EQ(eng_anim_path_components(ENG_ANIM_TRANSLATION), 3U);
    EXPECT_EQ(eng_anim_path_components(ENG_ANIM_ROTATION), 4U);
    EXPECT_EQ(eng_anim_path_components(ENG_ANIM_SCALE), 3U);
    EXPECT_EQ(eng_anim_path_components(3), 0U);
}

/* ── Format .anim ── */

UTEST(anim, encode_decode_roundtrip_is_exact_and_deterministic)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const EngAnimClip clip = hand_clip();
    const void *a = NULL;
    const void *b = NULL;
    size_t sa = 0;
    size_t sb = 0;
    ASSERT_TRUE(eng_anim_encode(&f.arena, &clip, &a, &sa));
    ASSERT_TRUE(eng_anim_encode(&f.arena, &clip, &b, &sb));
    ASSERT_EQ(sa, sb);
    EXPECT_EQ(memcmp(a, b, sa), 0);
    EngAnimClip back;
    ASSERT_TRUE(eng_anim_decode(&f.arena, a, sa, "test", &back));
    EXPECT_EQ(back.duration, clip.duration);
    ASSERT_EQ(back.channel_count, 3U);
    ASSERT_EQ(back.key_total, 6U);
    EXPECT_STREQ(back.skeleton_path, "models/perso.skel");
    EXPECT_EQ(memcmp(back.channels, clip.channels, sizeof g_channels), 0);
    EXPECT_EQ(memcmp((const uint8_t *)back.times, (const uint8_t *)clip.times, sizeof g_times), 0);
    EXPECT_EQ(memcmp((const uint8_t *)back.values, (const uint8_t *)clip.values, sizeof g_values),
              0);
    for (uint32_t c = 0; c < 3; c++) {
        EXPECT_EQ(back.value_first[c], clip.value_first[c]);
    }
    float v[4];
    ASSERT_EQ(eng_anim_sample(&back, 1, 0.5F, v), 4U);
    EXPECT_NEAR(v[1], 0.38268343F, 1e-6F);
    fx_done(&f);
}

static bool encodes(Fx *f, const EngAnimClip *clip)
{
    const void *data;
    size_t size;
    return eng_anim_encode(&f->arena, clip, &data, &size);
}

UTEST(anim, encode_refuses_invalid_clips)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngAnimClip c = hand_clip();
    EXPECT_TRUE(encodes(&f, &c));

    c = hand_clip();
    c.duration = 0.9F; /* < dernière clé */
    EXPECT_FALSE(encodes(&f, &c));
    c = hand_clip();
    c.duration = NAN;
    EXPECT_FALSE(encodes(&f, &c));
    c.duration = -1.0F;
    EXPECT_FALSE(encodes(&f, &c));

    static EngAnimChannel ch[3];
    static float times[6];
    static float values[20];
    static uint32_t first[3];
    memcpy(ch, g_channels, sizeof ch);
    memcpy(times, g_times, sizeof times);
    memcpy(values, g_values, sizeof values);
    memcpy(first, g_value_first, sizeof first);
    c = hand_clip();
    c.channels = ch;
    c.times = times;
    c.values = values;
    c.value_first = first;

    times[1] = 0.0F; /* non strictement croissants */
    EXPECT_FALSE(encodes(&f, &c));
    times[1] = 0.5F;
    times[0] = -0.5F; /* négatif */
    EXPECT_FALSE(encodes(&f, &c));
    times[0] = 0.0F;
    times[2] = 1.5F; /* > durée */
    EXPECT_FALSE(encodes(&f, &c));
    times[2] = 1.0F;
    times[4] = NAN;
    EXPECT_FALSE(encodes(&f, &c));
    times[4] = 1.0F;
    values[3] = INFINITY;
    EXPECT_FALSE(encodes(&f, &c));
    values[3] = 2.0F;
    values[12] = 0.5F; /* quaternion non unitaire */
    EXPECT_FALSE(encodes(&f, &c));
    values[12] = 1.0F;

    ch[1].path = 7;
    EXPECT_FALSE(encodes(&f, &c));
    ch[1].path = ENG_ANIM_ROTATION;
    ch[1].joint = 256;
    EXPECT_FALSE(encodes(&f, &c));
    ch[1].joint = 1;
    ch[2].path = ENG_ANIM_ROTATION; /* doublon (1, rotation) */
    EXPECT_FALSE(encodes(&f, &c));
    ch[2].path = ENG_ANIM_SCALE;
    ch[0].joint = 2; /* ordre décroissant */
    EXPECT_FALSE(encodes(&f, &c));
    ch[0].joint = 0;
    ch[1].key_count = 0;
    EXPECT_FALSE(encodes(&f, &c));
    ch[1].key_count = 2;
    ch[1].first_key = 4; /* première clé incohérente */
    EXPECT_FALSE(encodes(&f, &c));
    ch[1].first_key = 3;
    first[2] = 16; /* premier flottant incohérent */
    EXPECT_FALSE(encodes(&f, &c));
    first[2] = 17;
    c.key_total = 5;
    EXPECT_FALSE(encodes(&f, &c));
    c.key_total = 6;
    EXPECT_TRUE(encodes(&f, &c)); /* tout rétabli */

    c.skeleton_path = "";
    EXPECT_FALSE(encodes(&f, &c));
    c.skeleton_path = NULL;
    EXPECT_FALSE(encodes(&f, &c));
    char longp[300];
    memset(longp, 'a', sizeof longp - 1U);
    longp[255] = '\0';
    c.skeleton_path = longp; /* 255 octets : accepté */
    EXPECT_TRUE(encodes(&f, &c));
    longp[255] = 'a';
    longp[256] = '\0';
    EXPECT_FALSE(encodes(&f, &c));
    c = hand_clip();
    c.channel_count = 0;
    EXPECT_FALSE(encodes(&f, &c));
    EXPECT_FALSE(encodes(&f, NULL));
    const void *data;
    size_t size;
    c = hand_clip();
    EXPECT_FALSE(eng_anim_encode(NULL, &c, &data, &size));
    EXPECT_FALSE(eng_anim_encode(&f.arena, &c, NULL, &size));
    EXPECT_FALSE(eng_anim_encode(&f.arena, &c, &data, NULL));
    fx_done(&f);
}

UTEST(anim, decode_rejects_corrupted_files)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const EngAnimClip clip = hand_clip();
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_anim_encode(&f.arena, &clip, &data, &size));
    uint8_t *copy = eng_arena_alloc(&f.arena, size, 16);
    ASSERT_TRUE(copy != NULL);
    EngAnimClip out;
    memcpy(copy, data, size);
    EXPECT_TRUE(eng_anim_decode(&f.arena, copy, size, "ok", &out));
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size - 1U, "tronque", &out));
    EXPECT_EQ(out.channel_count, 0U);
    EXPECT_FALSE(eng_anim_decode(&f.arena, NULL, 0, "vide", &out));
    memcpy(copy, data, size);
    copy[0] ^= 0xFFU;
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "magie", &out));

    EngAssetView view;
    EngAssetSection info;
    EngAssetSection chan;
    EngAssetSection time;
    EngAssetSection vals;
    EngAssetSection strs;
    memcpy(copy, data, size);
    ASSERT_EQ(eng_asset_open(copy, size, ENG_ASSET_MAGIC_ANIM, ENG_ASSET_VERSION_ANIM, "x", &view),
              ENG_ASSET_OK);
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_INFO, &info));
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_CHAN, &chan));
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_TIME, &time));
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_VALS, &vals));
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_STRS, &strs));
    uint8_t *i_bytes = copy + (info.data - (const uint8_t *)copy);
    uint8_t *c_bytes = copy + (chan.data - (const uint8_t *)copy);
    uint8_t *t_bytes = copy + (time.data - (const uint8_t *)copy);
    uint8_t *v_bytes = copy + (vals.data - (const uint8_t *)copy);
    uint8_t *s_bytes = copy + (strs.data - (const uint8_t *)copy);

    /* nombre de canaux de INFO : 2 (incohérent), 0, 769 */
    const uint32_t counts[3] = {2, 0, 769};
    for (int i = 0; i < 3; i++) {
        memcpy(i_bytes + 4, &counts[i], 4);
        EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "n", &out));
    }
    memcpy(copy, data, size);
    i_bytes[12] = 1; /* champ réservé */
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "reserve", &out));
    memcpy(copy, data, size);
    const uint32_t far = 1000; /* chemin hors de STRS */
    memcpy(i_bytes + 8, &far, 4);
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "chemin", &out));
    const uint32_t at_end = strs.size; /* un octet après la fin */
    memcpy(i_bytes + 8, &at_end, 4);
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "chemin2", &out));
    const uint32_t last = strs.size - 1U; /* sur le zéro final : chemin vide, refusé */
    memcpy(i_bytes + 8, &last, 4);
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "chemin3", &out));
    memcpy(copy, data, size);
    s_bytes[strs.size - 1U] = 'x'; /* STRS non terminée */
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "strs", &out));
    memcpy(copy, data, size);
    const uint32_t bad_path = 5; /* propriété inconnue du premier canal */
    memcpy(c_bytes + 4, &bad_path, 4);
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "propriete", &out));
    memcpy(copy, data, size);
    const float back_in_time = -1.0F; /* instant négatif */
    memcpy(t_bytes, &back_in_time, 4);
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "temps", &out));
    memcpy(copy, data, size);
    const float nan = NAN;
    memcpy(v_bytes + 4, &nan, 4);
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "nan", &out));
    memcpy(copy, data, size);
    const uint32_t huge = 0x7FFFFFFFU; /* nombre de clés absurde : pas de débordement */
    memcpy(c_bytes + 8, &huge, 4);
    EXPECT_FALSE(eng_anim_decode(&f.arena, copy, size, "enorme", &out));
    memcpy(copy, data, size);
    EXPECT_TRUE(eng_anim_decode(&f.arena, copy, size, "retour", &out));
    fx_done(&f);
}

UTEST(anim, memory_exhaustion_fails_cleanly_and_restores_the_arena)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const EngAnimClip clip = hand_clip();
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_anim_encode(&f.arena, &clip, &data, &size));
    static _Alignas(16) uint8_t storage[8192];
    bool decode_failed = false;
    bool encode_failed = false;
    for (size_t cap = 0; cap <= 2048U; cap += 4U) {
        EngArena tiny;
        EngAnimClip out;
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        if (eng_anim_decode(&tiny, data, size, "tiny", &out)) {
            EXPECT_EQ(out.channel_count, 3U);
        } else {
            decode_failed = true;
            EXPECT_EQ(tiny.used, (size_t)0);
        }
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        const void *again = NULL;
        size_t again_size = 0;
        if (eng_anim_encode(&tiny, &clip, &again, &again_size)) {
            EXPECT_EQ(again_size, size);
            EXPECT_EQ(memcmp(again, data, size), 0);
        } else {
            encode_failed = true;
            EXPECT_EQ(tiny.used, (size_t)0);
        }
    }
    EXPECT_TRUE(decode_failed);
    EXPECT_TRUE(encode_failed);
    fx_done(&f);
}

UTEST(anim, load_reads_a_file_and_refuses_a_missing_one)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const EngAnimClip clip = hand_clip();
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_anim_encode(&f.arena, &clip, &data, &size));
    const char *path = "test_anim_tmp.anim";
    FILE *fp = fopen(path, "wb");
    ASSERT_TRUE(fp != NULL);
    ASSERT_EQ(fwrite(data, 1, size, fp), size);
    fclose(fp);
    EngAnimClip back;
    EXPECT_TRUE(eng_anim_load(&f.arena, &f.scratch, path, &back));
    EXPECT_EQ(back.channel_count, 3U);
    EXPECT_EQ(f.scratch.used, (size_t)0);
    remove(path);
    EXPECT_FALSE(eng_anim_load(&f.arena, &f.scratch, "n_existe_pas.anim", &back));
    EXPECT_EQ(back.channel_count, 0U);
    EXPECT_FALSE(eng_anim_load(&f.arena, &f.scratch, NULL, &back));
    fx_done(&f);
}

/* ── GLB d'animation en mémoire ── */

#define GLB_MAX 65536U

typedef struct Acc {
    const float *data;
    uint32_t count;
    const char *type; /* SCALAR, VEC3, VEC4, MAT4 */
} Acc;

static uint32_t type_floats(const char *type)
{
    return strcmp(type, "SCALAR") == 0 ? 1U
           : strcmp(type, "VEC3") == 0 ? 3U
           : strcmp(type, "VEC4") == 0 ? 4U
                                       : 16U;
}

/* skin_json : objet complet du skin 0 ; anims_json : contenu du tableau « animations ». */
static size_t build_glb(uint8_t *out, size_t cap, const char *nodes, const char *skin_json,
                        const char *anims_json, const Acc *accs, uint32_t acc_count)
{
    static char json[131072];
    uint32_t bin_len = 0;
    for (uint32_t i = 0; i < acc_count; i++) {
        bin_len += accs[i].count * type_floats(accs[i].type) * 4U;
    }
    int n = snprintf(json, sizeof json,
                     "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
                     "\"nodes\":[%s],\"skins\":[%s],\"animations\":[%s],"
                     "\"buffers\":[{\"byteLength\":%u}],\"bufferViews\":[",
                     nodes, skin_json, anims_json, bin_len);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < acc_count; i++) {
        const uint32_t len = accs[i].count * type_floats(accs[i].type) * 4U;
        n += snprintf(json + n, sizeof json - (size_t)n,
                      "%s{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}", i ? "," : "", offset,
                      len);
        offset += len;
    }
    n += snprintf(json + n, sizeof json - (size_t)n, "],\"accessors\":[");
    for (uint32_t i = 0; i < acc_count; i++) {
        n += snprintf(json + n, sizeof json - (size_t)n,
                      "%s{\"bufferView\":%u,\"componentType\":5126,\"count\":%u,\"type\":\"%s\"}",
                      i ? "," : "", i, accs[i].count, accs[i].type);
    }
    n += snprintf(json + n, sizeof json - (size_t)n, "]}");
    size_t jlen = (size_t)n;
    while ((jlen % 4U) != 0) {
        json[jlen++] = ' ';
    }
    const size_t total = 12U + 8U + jlen + 8U + bin_len;
    if (total > cap) {
        return 0;
    }
    uint8_t *p = out;
    const uint32_t magic = 0x46546C67U;
    const uint32_t version = 2;
    const uint32_t len32 = (uint32_t)total;
    memcpy(p, &magic, 4);
    memcpy(p + 4, &version, 4);
    memcpy(p + 8, &len32, 4);
    p += 12;
    const uint32_t jl = (uint32_t)jlen;
    const uint32_t jt = 0x4E4F534AU;
    memcpy(p, &jl, 4);
    memcpy(p + 4, &jt, 4);
    memcpy(p + 8, json, jlen);
    p += 8 + jlen;
    const uint32_t bt = 0x004E4942U;
    memcpy(p, &bin_len, 4);
    memcpy(p + 4, &bt, 4);
    p += 8;
    for (uint32_t i = 0; i < acc_count; i++) {
        const uint32_t len = accs[i].count * type_floats(accs[i].type) * 4U;
        memcpy(p, accs[i].data, len);
        p += len;
    }
    return total;
}

/* Chaîne sans Armature : Hips (racine de la scène) -> Spine -> Head ; nœud 3 : accessoire hors skin
 */
#define CHAIN                                                                                      \
    "{\"name\":\"Hips\",\"translation\":[0,1,0],\"children\":[1]},"                                \
    "{\"name\":\"Spine\",\"translation\":[0,0.5,0],\"children\":[2]},"                             \
    "{\"name\":\"Head\",\"translation\":[0,0.25,0]},{\"name\":\"Prop\"}"
#define CHAIN_SKIN "{\"joints\":[0,1,2]}"

static const float T3[3] = {0.0F, 0.5F, 1.0F};
static const float T2[2] = {0.0F, 1.0F};
static const float HIPS_T[9] = {0, 1, 0, 1, 1, 0, 2, 1, 0};
static const float SPINE_R[8] = {0, 0, 0, 1, 0, 0.70710678F, 0, 0.70710678F};
static const float PROP_T[6] = {0, 0, 0, 5, 5, 5};

#define CHAIN_ANIM                                                                                 \
    "{\"name\":\"marche\",\"samplers\":[{\"input\":0,\"output\":1},{\"input\":2,\"output\":3},"    \
    "{\"input\":2,\"output\":4}],\"channels\":["                                                   \
    "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}},"                            \
    "{\"sampler\":1,\"target\":{\"node\":1,\"path\":\"rotation\"}},"                               \
    "{\"sampler\":2,\"target\":{\"node\":3,\"path\":\"translation\"}}]}"

static size_t chain_glb(uint8_t *glb)
{
    const Acc accs[5] = {{T3, 3, "SCALAR"},
                         {HIPS_T, 3, "VEC3"},
                         {T2, 2, "SCALAR"},
                         {SPINE_R, 2, "VEC4"},
                         {PROP_T, 2, "VEC3"}};
    return build_glb(glb, GLB_MAX, CHAIN, CHAIN_SKIN, CHAIN_ANIM, accs, 5);
}

UTEST(anim, import_keeps_joint_channels_and_ignores_the_others)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const size_t size = chain_glb(glb);
    ASSERT_TRUE(size > 0);
    EXPECT_EQ(eng_anim_gltf_clip_count(glb, size), 1U);
    EngAnimClip clip;
    EngAnimImportStats st;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "perso.skel", &clip, &st));
    EXPECT_EQ(st.gltf_channels, 3U);
    EXPECT_EQ(st.kept_channels, 2U);
    EXPECT_EQ(st.ignored_channels, 1U); /* l'accessoire */
    EXPECT_EQ(st.rebaked_joints, 0U);
    ASSERT_EQ(clip.channel_count, 2U);
    EXPECT_EQ(clip.duration, 1.0F);
    EXPECT_EQ(clip.key_total, 5U);
    EXPECT_STREQ(clip.skeleton_path, "perso.skel");
    const uint32_t t = eng_anim_find_channel(&clip, 0, ENG_ANIM_TRANSLATION);
    const uint32_t r = eng_anim_find_channel(&clip, 1, ENG_ANIM_ROTATION);
    ASSERT_NE(t, UINT32_MAX);
    ASSERT_NE(r, UINT32_MAX);
    EXPECT_EQ(clip.channels[t].key_count, 3U);
    float v[4];
    eng_anim_sample(&clip, t, 0.25F, v); /* mi-chemin de (0,1,0) -> (1,1,0) */
    EXPECT_NEAR(v[0], 0.5F, 1e-6F);
    eng_anim_sample(&clip, r, 0.5F, v);
    EXPECT_NEAR(v[1], 0.38268343F, 1e-6F);
    EXPECT_NEAR(v[3], 0.92387953F, 1e-6F);
    EXPECT_EQ(f.scratch.used, (size_t)0);
    fx_done(&f);
}

UTEST(anim, import_follows_the_skeleton_joint_order_when_the_skin_lists_children_first)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const Acc accs[5] = {{T3, 3, "SCALAR"},
                         {HIPS_T, 3, "VEC3"},
                         {T2, 2, "SCALAR"},
                         {SPINE_R, 2, "VEC4"},
                         {PROP_T, 2, "VEC3"}};
    /* skin : Head, Spine, Hips -> squelette : Hips 0, Spine 1, Head 2 */
    const size_t size = build_glb(glb, GLB_MAX, CHAIN, "{\"joints\":[2,1,0]}", CHAIN_ANIM, accs, 5);
    ASSERT_TRUE(size > 0);
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EXPECT_STREQ(sk.joints[0].name, "Hips");
    EngAnimClip clip;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));
    EXPECT_NE(eng_anim_find_channel(&clip, 0, ENG_ANIM_TRANSLATION), UINT32_MAX); /* Hips */
    EXPECT_NE(eng_anim_find_channel(&clip, 1, ENG_ANIM_ROTATION), UINT32_MAX);    /* Spine */
    EXPECT_EQ(eng_anim_find_channel(&clip, 2, ENG_ANIM_TRANSLATION), UINT32_MAX);
    fx_done(&f);
}

/* ── Articulations à ancêtres repliés (Armature de Mixamo) ── */

#define ARMATURE_NODES                                                                             \
    "{\"name\":\"Armature\",\"scale\":[0.01,0.01,0.01],\"children\":[1]},"                         \
    "{\"name\":\"Hips\",\"translation\":[0,100,0],\"children\":[2]},"                              \
    "{\"name\":\"Spine\",\"translation\":[0,50,0]}"

static const float CM_T[6] = {0, 100, 0, 0, 200, 0};
static const float HIPS_R[12] = {0, 0, 0, 1, 0, 0.70710678F, 0, 0.70710678F, 0, 1, 0, 0};
static const float SPINE_R2[8] = {0, 0, 0, 1, 0.70710678F, 0, 0, 0.70710678F};

UTEST(anim, armature_scale_is_folded_into_the_clip_like_it_is_into_the_skeleton)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const float t_hips_t[2] = {0.0F, 1.0F};
    const float t_hips_r[3] = {0.0F, 0.5F, 1.0F};
    const Acc accs[5] = {{t_hips_t, 2, "SCALAR"},
                         {CM_T, 2, "VEC3"},
                         {t_hips_r, 3, "SCALAR"},
                         {HIPS_R, 3, "VEC4"},
                         {SPINE_R2, 2, "VEC4"}};
    const char *anim = "{\"samplers\":[{\"input\":0,\"output\":1},{\"input\":2,\"output\":3},"
                       "{\"input\":0,\"output\":4}],\"channels\":["
                       "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"translation\"}},"
                       "{\"sampler\":1,\"target\":{\"node\":1,\"path\":\"rotation\"}},"
                       "{\"sampler\":2,\"target\":{\"node\":2,\"path\":\"rotation\"}}]}";
    const size_t size =
        build_glb(glb, GLB_MAX, ARMATURE_NODES, "{\"joints\":[1,2]}", anim, accs, 5);
    ASSERT_TRUE(size > 0);
    EngSkeleton sk;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, NULL));
    EXPECT_NEAR(sk.joints[0].translation[1], 1.0F, 1e-5F); /* 100 cm replié : 1 m */
    EXPECT_NEAR(sk.joints[0].scale[0], 0.01F, 1e-7F);

    EngAnimClip clip;
    EngAnimImportStats st;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, &st));
    EXPECT_EQ(st.rebaked_joints, 1U); /* Hips seulement : Spine a un parent articulation */
    EXPECT_EQ(st.kept_channels, 4U);  /* Hips : T, R, S ; Spine : R */
    const uint32_t ht = eng_anim_find_channel(&clip, 0, ENG_ANIM_TRANSLATION);
    const uint32_t hr = eng_anim_find_channel(&clip, 0, ENG_ANIM_ROTATION);
    const uint32_t hs = eng_anim_find_channel(&clip, 0, ENG_ANIM_SCALE);
    const uint32_t sr = eng_anim_find_channel(&clip, 1, ENG_ANIM_ROTATION);
    ASSERT_NE(ht, UINT32_MAX);
    ASSERT_NE(hr, UINT32_MAX);
    ASSERT_NE(hs, UINT32_MAX);
    ASSERT_NE(sr, UINT32_MAX);
    /* union des instants {0 ; 0,5 ; 1} pour les trois propriétés recalculées */
    EXPECT_EQ(clip.channels[ht].key_count, 3U);
    EXPECT_EQ(clip.channels[hr].key_count, 3U);
    EXPECT_EQ(clip.channels[hs].key_count, 3U);
    EXPECT_EQ(clip.channels[sr].key_count, 2U); /* Spine : clés d'origine */
    float v[4];
    eng_anim_sample(&clip, ht, 0.0F, v); /* = repos du squelette */
    EXPECT_NEAR(v[1], sk.joints[0].translation[1], 1e-5F);
    eng_anim_sample(&clip, ht, 0.5F, v); /* 150 cm -> 1,5 m */
    EXPECT_NEAR(v[0], 0.0F, 1e-5F);
    EXPECT_NEAR(v[1], 1.5F, 1e-5F);
    eng_anim_sample(&clip, ht, 1.0F, v);
    EXPECT_NEAR(v[1], 2.0F, 1e-5F);
    eng_anim_sample(&clip, hs, 0.5F, v);
    EXPECT_NEAR(v[0], 0.01F, 1e-7F);
    EXPECT_NEAR(v[1], 0.01F, 1e-7F);
    EXPECT_NEAR(v[2], 0.01F, 1e-7F);
    eng_anim_sample(&clip, hr, 0.5F, v); /* 90° autour de Y */
    EXPECT_NEAR(v[1], 0.70710678F, 1e-5F);
    EXPECT_NEAR(v[3], 0.70710678F, 1e-5F);
    eng_anim_sample(&clip, hr, 1.0F, v); /* 180° : (0,1,0,0), signe libre */
    EXPECT_NEAR(fabsf(v[1]), 1.0F, 1e-5F);
    fx_done(&f);
}

static void compose(const float *t, const float *r, const float *s, mat4 out)
{
    mat4 mt;
    mat4 mr;
    mat4 ms;
    mat4 rs;
    glm_translate_make(mt, (vec3){t[0], t[1], t[2]});
    glm_quat_mat4((versor){r[0], r[1], r[2], r[3]}, mr);
    glm_scale_make(ms, (vec3){s[0], s[1], s[2]});
    glm_mat4_mul(mr, ms, rs);
    glm_mat4_mul(mt, rs, out);
}

UTEST(anim, rebaked_channels_compose_to_the_same_world_matrix_as_the_original_animation)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    /* Armature : translation (10,0,0), rotation 90° autour de Z, échelle 0,5 ; Hips : seule la
     * translation est animée (la rotation et l'échelle restent celles du repos) */
    const char *nodes =
        "{\"name\":\"Armature\",\"translation\":[10,0,0],\"rotation\":[0,0,0.70710678,0.70710678],"
        "\"scale\":[0.5,0.5,0.5],\"children\":[1]},"
        "{\"name\":\"Hips\",\"translation\":[1,2,3],\"rotation\":[0.70710678,0,0,0.70710678],"
        "\"scale\":[2,2,2]}";
    const float times[3] = {0.0F, 1.0F, 3.0F};
    const float tr[9] = {1, 2, 3, 4, 0, -2, 0, 7, 1};
    const Acc accs[2] = {{times, 3, "SCALAR"}, {tr, 3, "VEC3"}};
    const char *anim = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                       "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"translation\"}}]}";
    const size_t size = build_glb(glb, GLB_MAX, nodes, "{\"joints\":[1]}", anim, accs, 2);
    ASSERT_TRUE(size > 0);
    EngAnimClip clip;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));
    const uint32_t ct = eng_anim_find_channel(&clip, 0, ENG_ANIM_TRANSLATION);
    const uint32_t cr = eng_anim_find_channel(&clip, 0, ENG_ANIM_ROTATION);
    const uint32_t cs = eng_anim_find_channel(&clip, 0, ENG_ANIM_SCALE);
    ASSERT_TRUE(ct != UINT32_MAX && cr != UINT32_MAX && cs != UINT32_MAX);
    mat4 armature;
    const float a_t[3] = {10, 0, 0};
    const float a_r[4] = {0, 0, 0.70710678F, 0.70710678F};
    const float a_s[3] = {0.5F, 0.5F, 0.5F};
    compose(a_t, a_r, a_s, armature);
    const float h_r[4] = {0.70710678F, 0, 0, 0.70710678F};
    const float h_s[3] = {2, 2, 2};
    for (uint32_t k = 0; k < 3; k++) {
        mat4 want_local;
        mat4 want;
        compose(&tr[(size_t)k * 3U], h_r, h_s, want_local);
        glm_mat4_mul(armature, want_local, want);
        float a[4];
        float b[4];
        float c[4];
        eng_anim_sample(&clip, ct, times[k], a);
        eng_anim_sample(&clip, cr, times[k], b);
        eng_anim_sample(&clip, cs, times[k], c);
        mat4 got;
        compose(a, b, c, got);
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                EXPECT_NEAR(got[col][row], want[col][row], 1e-4F);
            }
        }
    }
    fx_done(&f);
}

/* ── Refus à l'import ── */

static bool import_with(Fx *f, const char *interp, const float *times, uint32_t n_times,
                        const float *vals, uint32_t n_vals, const char *path, const char *skeleton)
{
    static uint8_t glb[GLB_MAX];
    const Acc accs[2] = {{times, n_times, "SCALAR"},
                         {vals, n_vals, strcmp(path, "rotation") == 0 ? "VEC4" : "VEC3"}};
    char anim[512];
    snprintf(anim, sizeof anim,
             "{\"samplers\":[{\"input\":0,\"output\":1%s}],\"channels\":["
             "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"%s\"}}]}",
             interp, path);
    const size_t size = build_glb(glb, GLB_MAX, CHAIN, CHAIN_SKIN, anim, accs, 2);
    EngAnimClip clip;
    const bool ok = size > 0 && eng_anim_from_gltf(&f->arena, &f->scratch, glb, size, NULL, 0, 0,
                                                   skeleton, &clip, NULL);
    if (!ok) {
        return false; /* l'appelant vérifie la restitution des arènes */
    }
    return true;
}

UTEST(anim, import_refuses_bad_animations_and_restores_the_arenas)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float ok_t[2] = {0.0F, 1.0F};
    const float ok_v[6] = {0, 1, 0, 1, 1, 0};
    EXPECT_TRUE(import_with(&f, "", ok_t, 2, ok_v, 2, "translation", "p.skel")); /* témoin */

    /* un seul canal : la sortie n'est donc valide que s'il est gardé ; chaque cas doit échouer */
    EXPECT_FALSE(
        import_with(&f, ",\"interpolation\":\"STEP\"", ok_t, 2, ok_v, 2, "translation", "p.skel"));
    EXPECT_FALSE(import_with(&f, ",\"interpolation\":\"CUBICSPLINE\"", ok_t, 2, ok_v, 2,
                             "translation", "p.skel"));
    EXPECT_TRUE(import_with(&f, ",\"interpolation\":\"LINEAR\"", ok_t, 2, ok_v, 2, "translation",
                            "p.skel"));
    const float bad_t[2] = {1.0F, 1.0F}; /* non strictement croissants */
    EXPECT_FALSE(import_with(&f, "", bad_t, 2, ok_v, 2, "translation", "p.skel"));
    const float back_t[2] = {1.0F, 0.5F};
    EXPECT_FALSE(import_with(&f, "", back_t, 2, ok_v, 2, "translation", "p.skel"));
    const float neg_t[2] = {-1.0F, 0.5F};
    EXPECT_FALSE(import_with(&f, "", neg_t, 2, ok_v, 2, "translation", "p.skel"));
    const float nan_v[6] = {0, NAN, 0, 1, 1, 0};
    EXPECT_FALSE(import_with(&f, "", ok_t, 2, nan_v, 2, "translation", "p.skel"));
    EXPECT_FALSE(
        import_with(&f, "", ok_t, 2, ok_v, 1, "translation", "p.skel")); /* comptes différents */
    const float zero_q[8] = {0, 0, 0, 1, 0, 0, 0, 0};
    EXPECT_FALSE(import_with(&f, "", ok_t, 2, zero_q, 2, "rotation", "p.skel"));
    const float big_q[8] = {0, 0, 0, 2, 0, 0, 0, 2}; /* non unitaires : normalisés */
    EXPECT_TRUE(import_with(&f, "", ok_t, 2, big_q, 2, "rotation", "p.skel"));
    EXPECT_FALSE(import_with(&f, "", ok_t, 2, ok_v, 2, "translation", NULL));
    EXPECT_FALSE(import_with(&f, "", ok_t, 2, ok_v, 2, "translation", ""));
    char longp[300];
    memset(longp, 'a', sizeof longp);
    longp[256] = '\0';
    EXPECT_FALSE(import_with(&f, "", ok_t, 2, ok_v, 2, "translation", longp));
    longp[255] = '\0';
    EXPECT_TRUE(import_with(&f, "", ok_t, 2, ok_v, 2, "translation", longp));
    EXPECT_EQ(f.scratch.used, (size_t)0);
    fx_done(&f);
}

UTEST(anim, import_refuses_missing_targets_duplicates_and_empty_results)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    EngAnimClip clip;
    /* animation ou skin inexistants */
    size_t size = chain_glb(glb);
    EXPECT_FALSE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 1, 0, "p.skel", &clip, NULL));
    EXPECT_FALSE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 1, "p.skel", &clip, NULL));
    EXPECT_EQ(f.arena.used, (size_t)0);
    EXPECT_EQ(f.scratch.used, (size_t)0);
    EXPECT_EQ(clip.channel_count, 0U);

    /* deux canaux sur la même cible */
    const Acc accs[2] = {{T2, 2, "SCALAR"}, {PROP_T, 2, "VEC3"}};
    const char *dup = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                      "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}},"
                      "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]}";
    size = build_glb(glb, GLB_MAX, CHAIN, CHAIN_SKIN, dup, accs, 2);
    EXPECT_FALSE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));

    /* aucun canal sur une articulation du skin : rien d'utilisable */
    const char *none = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                       "{\"sampler\":0,\"target\":{\"node\":3,\"path\":\"translation\"}}]}";
    size = build_glb(glb, GLB_MAX, CHAIN, CHAIN_SKIN, none, accs, 2);
    EngAnimImportStats st;
    EXPECT_FALSE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, &st));
    EXPECT_EQ(st.ignored_channels, 1U);
    EXPECT_EQ(f.arena.used, (size_t)0);

    /* données absentes, tronquées, arguments NULL */
    EXPECT_FALSE(
        eng_anim_from_gltf(&f.arena, &f.scratch, NULL, 10, NULL, 0, 0, "p.skel", &clip, NULL));
    size = chain_glb(glb);
    EXPECT_FALSE(eng_anim_from_gltf(&f.arena, &f.scratch, glb, size / 2U, NULL, 0, 0, "p.skel",
                                    &clip, NULL));
    EXPECT_FALSE(
        eng_anim_from_gltf(NULL, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));
    EXPECT_FALSE(eng_anim_from_gltf(&f.arena, NULL, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));
    EXPECT_FALSE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", NULL, NULL));
    EXPECT_EQ(eng_anim_gltf_clip_count(NULL, 10), 0U);
    EXPECT_EQ(eng_anim_gltf_clip_count(glb, 0), 0U);
    fx_done(&f);
}

UTEST(anim, import_memory_exhaustion_fails_cleanly)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const size_t size = chain_glb(glb);
    static _Alignas(16) uint8_t storage[16384];
    bool failed = false;
    for (size_t cap = 0; cap <= 4096U; cap += 16U) {
        EngArena tiny;
        EngAnimClip clip;
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        if (eng_anim_from_gltf(&tiny, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL)) {
            EXPECT_EQ(clip.channel_count, 2U);
        } else {
            failed = true;
            EXPECT_EQ(tiny.used, (size_t)0);
        }
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        if (eng_anim_from_gltf(&f.arena, &tiny, glb, size, NULL, 0, 0, "p.skel", &clip, NULL)) {
            EXPECT_EQ(clip.channel_count, 2U);
        } else {
            failed = true;
            EXPECT_EQ(tiny.used, (size_t)0);
        }
        f.arena.used = 0;
    }
    EXPECT_TRUE(failed);
    fx_done(&f);
}

/* ── Cas limites ajoutés après les mutations ── */

/* Référence indépendante (double précision) : interpolation sphérique, plus court chemin. */
static void slerp_reference(const float *a, const float *b, double f, double *out)
{
    double bb[4] = {b[0], b[1], b[2], b[3]};
    double dot = 0.0;
    for (int i = 0; i < 4; i++) {
        dot += (double)a[i] * bb[i];
    }
    if (dot < 0.0) {
        for (int i = 0; i < 4; i++) {
            bb[i] = -bb[i];
        }
        dot = -dot;
    }
    const double theta = acos(dot > 1.0 ? 1.0 : dot);
    const double sin_theta = sin(theta);
    const double wa = sin_theta < 1e-9 ? 1.0 - f : sin((1.0 - f) * theta) / sin_theta;
    const double wb = sin_theta < 1e-9 ? f : sin(f * theta) / sin_theta;
    for (int i = 0; i < 4; i++) {
        out[i] = (wa * (double)a[i]) + (wb * bb[i]);
    }
}

UTEST(anim, rotation_sampling_matches_an_independent_double_precision_slerp)
{
    const float pairs[][8] = {
        {0.5F, 0.5F, 0.5F, 0.5F, 0.6F, 0.0F, 0.8F, 0.0F},   /* quelconques, dot 0,7 */
        {0.5F, -0.5F, 0.5F, -0.5F, 0.0F, 0.6F, 0.0F, 0.8F}, /* dot négatif : chemin court */
        {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F},   /* orthogonales, dot 0 exact */
        {0.18257419F, 0.36514837F, 0.54772256F, 0.73029674F, 0.18F, 0.37F, 0.55F, 0.7292F}};
    for (size_t p = 0; p < sizeof pairs / sizeof pairs[0]; p++) {
        float times[2] = {0.0F, 1.0F};
        float values[8];
        memcpy(values, pairs[p], sizeof values);
        float len[2] = {0, 0}; /* ramène chaque clé à la norme 1 */
        for (int k = 0; k < 2; k++) {
            for (int i = 0; i < 4; i++) {
                len[k] += values[(k * 4) + i] * values[(k * 4) + i];
            }
            len[k] = sqrtf(len[k]);
            for (int i = 0; i < 4; i++) {
                values[(k * 4) + i] /= len[k];
            }
        }
        EngAnimChannel ch = {0, ENG_ANIM_ROTATION, 2, 0};
        uint32_t first = 0;
        const EngAnimClip clip = {.duration = 1.0F,
                                  .channel_count = 1,
                                  .channels = &ch,
                                  .key_total = 2,
                                  .times = times,
                                  .values = values,
                                  .value_first = &first,
                                  .skeleton_path = "p.skel"};
        for (int step = 1; step <= 9; step += 2) {
            const double f = (double)step / 10.0;
            float got[4];
            double want[4];
            ASSERT_EQ(eng_anim_sample(&clip, 0, (float)f, got), 4U);
            slerp_reference(values, &values[4], (double)(float)f, want);
            double norm = 0.0;
            for (int i = 0; i < 4; i++) {
                norm += want[i] * want[i];
            }
            norm = sqrt(norm);
            for (int i = 0; i < 4; i++) {
                EXPECT_NEAR(got[i], want[i] / norm, 2e-6);
            }
        }
    }
}

UTEST(anim, binary_search_is_exact_on_a_channel_with_many_keys)
{
    enum { N = 9 };
    float times[N];
    float values[N * 3];
    for (int i = 0; i < N; i++) {
        times[i] = (float)(i * i) * 0.25F; /* pas réguliers : 0 ; 0,25 ; 1 ; 2,25 ; 4... */
        values[(i * 3) + 0] = 2.0F * times[i] + 1.0F;
        values[(i * 3) + 1] = -times[i];
        values[(i * 3) + 2] = 7.0F;
    }
    EngAnimChannel ch = {0, ENG_ANIM_TRANSLATION, N, 0};
    uint32_t first = 0;
    const EngAnimClip clip = {.duration = times[N - 1],
                              .channel_count = 1,
                              .channels = &ch,
                              .key_total = N,
                              .times = times,
                              .values = values,
                              .value_first = &first,
                              .skeleton_path = "p.skel"};
    for (int step = 0; step <= 640; step++) { /* fonction affine : l'interpolation est exacte */
        const float t = (float)step * 0.01F;
        float v[4];
        ASSERT_EQ(eng_anim_sample(&clip, 0, t, v), 3U);
        EXPECT_NEAR(v[0], (2.0F * t) + 1.0F, 1e-4F);
        EXPECT_NEAR(v[1], -t, 1e-4F);
        EXPECT_NEAR(v[2], 7.0F, 1e-6F);
    }
}

UTEST(anim, boundary_clips_single_channel_zero_duration_joint_255_and_768_channels)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    /* un canal, une clé à 0, durée 0 */
    float time0 = 0.0F;
    float v0[3] = {1, 2, 3};
    EngAnimChannel one = {0, ENG_ANIM_TRANSLATION, 1, 0};
    uint32_t first = 0;
    EngAnimClip clip = {.duration = 0.0F,
                        .channel_count = 1,
                        .channels = &one,
                        .key_total = 1,
                        .times = &time0,
                        .values = v0,
                        .value_first = &first,
                        .skeleton_path = "p.skel"};
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_anim_encode(&f.arena, &clip, &data, &size));
    EngAnimClip back;
    ASSERT_TRUE(eng_anim_decode(&f.arena, data, size, "un", &back));
    EXPECT_EQ(back.duration, 0.0F);
    EXPECT_EQ(back.channel_count, 1U);
    float v[4];
    EXPECT_EQ(eng_anim_sample(&back, 0, 5.0F, v), 3U);
    EXPECT_NEAR(v[2], 3.0F, 0.0F);

    one.joint = 255; /* dernière articulation permise */
    EXPECT_TRUE(eng_anim_encode(&f.arena, &clip, &data, &size));
    one.joint = 256;
    EXPECT_FALSE(eng_anim_encode(&f.arena, &clip, &data, &size));

    /* 768 canaux (256 articulations x 3 propriétés), puis 769 */
    static EngAnimChannel channels[769];
    static uint32_t firsts[769];
    static float times[769];
    static float values[256 * 10];
    uint32_t key = 0;
    uint32_t value = 0;
    uint32_t n = 0;
    for (uint32_t j = 0; j < 256; j++) {
        for (uint32_t p = 0; p < 3; p++) {
            channels[n] = (EngAnimChannel){j, p, 1, key};
            firsts[n] = value;
            times[key] = 0.0F;
            if (p == ENG_ANIM_ROTATION) {
                values[value + 3U] = 1.0F; /* identité (0, 0, 0, 1) */
            }
            key++;
            value += p == ENG_ANIM_ROTATION ? 4U : 3U;
            n++;
        }
    }
    clip = (EngAnimClip){.duration = 0.0F,
                         .channel_count = 768,
                         .channels = channels,
                         .key_total = 768,
                         .times = times,
                         .values = values,
                         .value_first = firsts,
                         .skeleton_path = "p.skel"};
    ASSERT_TRUE(eng_anim_encode(&f.arena, &clip, &data, &size));
    ASSERT_TRUE(eng_anim_decode(&f.arena, data, size, "768", &back));
    EXPECT_EQ(back.channel_count, 768U);
    EXPECT_EQ(eng_anim_find_channel(&back, 255, ENG_ANIM_SCALE), 767U);
    channels[768] =
        (EngAnimChannel){255, 3, 1, 768}; /* un 769e canal : refusé quoi qu'il contienne */
    clip.channel_count = 769;
    EXPECT_FALSE(eng_anim_encode(&f.arena, &clip, &data, &size));
    fx_done(&f);
}

UTEST(anim, a_channel_of_1048576_keys_is_accepted_and_one_more_refused)
{
    static float times[1048577];
    static float values[3 * 1048577];
    for (uint32_t i = 0; i < 1048577U; i++) {
        times[i] = (float)i * 0.001F;
    }
    EngArena big;
    ASSERT_TRUE(eng_arena_init(&big, "big", (size_t)96 << 20));
    EngAnimChannel ch = {0, ENG_ANIM_TRANSLATION, 1048576, 0};
    uint32_t first = 0;
    EngAnimClip clip = {.duration = times[1048575],
                        .channel_count = 1,
                        .channels = &ch,
                        .key_total = 1048576,
                        .times = times,
                        .values = values,
                        .value_first = &first,
                        .skeleton_path = "p.skel"};
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_anim_encode(&big, &clip, &data, &size));
    EngAnimClip back;
    ASSERT_TRUE(eng_anim_decode(&big, data, size, "grand", &back));
    EXPECT_EQ(back.key_total, 1048576U);
    ch.key_count = 1048577;
    clip.key_total = 1048577;
    clip.duration = times[1048576];
    EXPECT_FALSE(eng_anim_encode(&big, &clip, &data, &size));
    eng_arena_shutdown(&big);
}

/* .anim fabriqué à la main avec eng_asset_build, pour les cas qu'un encodeur correct n'écrit pas.
 */
typedef struct CraftInfo {
    float duration;
    uint32_t channel_count;
    uint32_t path_offset;
    uint32_t reserved;
} CraftInfo;

static size_t craft(EngArena *arena, const CraftInfo *info, uint32_t chan_size, uint32_t chan_count,
                    const char *strings, uint32_t strings_size, const void **out)
{
    const EngAnimClip clip = hand_clip();
    const EngAssetSectionDesc sections[5] = {
        {ENG_ASSET_TAG_INFO, info, sizeof(*info), 1},
        {ENG_ASSET_TAG_CHAN, clip.channels, chan_size, chan_count},
        {ENG_ASSET_TAG_TIME, clip.times, (uint32_t)sizeof g_times, 6},
        {ENG_ASSET_TAG_VALS, clip.values, (uint32_t)sizeof g_values, 6},
        {ENG_ASSET_TAG_STRS, strings, strings_size, strings_size}};
    const size_t total = eng_asset_file_size(sections, 5);
    void *file = eng_arena_alloc(arena, total, ENG_ASSET_ALIGN);
    size_t written = 0;
    if (file == NULL || !eng_asset_build(ENG_ASSET_MAGIC_ANIM, ENG_ASSET_VERSION_ANIM, sections, 5,
                                         file, total, &written)) {
        return 0;
    }
    *out = file;
    return written;
}

UTEST(anim, crafted_files_chan_section_counts_and_a_non_zero_path_offset)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const CraftInfo info = {1.0F, 3, 0, 0};
    const void *data = NULL;
    EngAnimClip out;
    size_t size = craft(&f.arena, &info, 48, 3, "p.skel", 7, &data); /* référence : valide */
    ASSERT_TRUE(size > 0);
    EXPECT_TRUE(eng_anim_decode(&f.arena, data, size, "ref", &out));

    size = craft(&f.arena, &info, 48, 2, "p.skel", 7, &data); /* taille juste, compte faux */
    ASSERT_TRUE(size > 0);
    EXPECT_FALSE(eng_anim_decode(&f.arena, data, size, "compte", &out));
    size = craft(&f.arena, &info, 32, 3, "p.skel", 7, &data); /* compte juste, taille fausse */
    ASSERT_TRUE(size > 0);
    EXPECT_FALSE(eng_anim_decode(&f.arena, data, size, "taille", &out));

    /* chemin placé après un préfixe : l'offset le désigne, la chaîne lue est celle-là */
    const CraftInfo shifted = {1.0F, 3, 5, 0};
    size = craft(&f.arena, &shifted, 48, 3, "xxxx\0real/p.skel", 17, &data);
    ASSERT_TRUE(size > 0);
    ASSERT_TRUE(eng_anim_decode(&f.arena, data, size, "decale", &out));
    EXPECT_STREQ(out.skeleton_path, "real/p.skel");
    const CraftInfo last = {1.0F, 3, 16, 0}; /* sur le zéro final : chemin vide */
    size = craft(&f.arena, &last, 48, 3, "xxxx\0real/p.skel", 17, &data);
    ASSERT_TRUE(size > 0);
    EXPECT_FALSE(eng_anim_decode(&f.arena, data, size, "vide", &out));
    fx_done(&f);
}

/* ── Import : cas limites ajoutés après les mutations ── */

static bool import_chain(Fx *f, const Acc *accs, uint32_t acc_count, const char *anim_json,
                         EngAnimClip *clip, EngAnimImportStats *st)
{
    static uint8_t glb[GLB_MAX];
    const size_t size = build_glb(glb, GLB_MAX, CHAIN, CHAIN_SKIN, anim_json, accs, acc_count);
    return size > 0 &&
           eng_anim_from_gltf(&f->arena, &f->scratch, glb, size, NULL, 0, 0, "p.skel", clip, st);
}

UTEST(anim, import_single_key_channels_and_quaternion_normalisation)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float one_time[1] = {0.5F};
    const float tr[3] = {1, 2, 3};
    const float q[4] = {0, 3, 0, 4}; /* norme 5 */
    const Acc accs[3] = {{one_time, 1, "SCALAR"}, {tr, 1, "VEC3"}, {q, 1, "VEC4"}};
    const char *anim =
        "{\"samplers\":[{\"input\":0,\"output\":1},{\"input\":0,\"output\":2}],\"channels\":["
        "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}},"
        "{\"sampler\":1,\"target\":{\"node\":1,\"path\":\"rotation\"}}]}";
    EngAnimClip clip;
    ASSERT_TRUE(import_chain(&f, accs, 3, anim, &clip, NULL));
    ASSERT_EQ(clip.channel_count, 2U);
    EXPECT_EQ(clip.channels[0].key_count, 1U);
    EXPECT_EQ(clip.duration, 0.5F);
    float v[4];
    ASSERT_EQ(eng_anim_sample(&clip, 1, 0.0F, v), 4U);
    EXPECT_NEAR(v[0], 0.0F, 1e-7F);
    EXPECT_NEAR(v[1], 0.6F, 1e-6F);
    EXPECT_NEAR(v[2], 0.0F, 1e-7F);
    EXPECT_NEAR(v[3], 0.8F, 1e-6F);
    fx_done(&f);
}

UTEST(anim, import_refuses_a_non_finite_value_in_the_last_key)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float times[2] = {0.0F, 1.0F};
    const float tr[6] = {0, 1, 0, 1, 1, NAN};
    const Acc accs[2] = {{times, 2, "SCALAR"}, {tr, 2, "VEC3"}};
    const char *anim = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                       "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]}";
    EngAnimClip clip;
    EXPECT_FALSE(import_chain(&f, accs, 2, anim, &clip, NULL));
    fx_done(&f);
}

UTEST(anim, import_sorts_channels_given_in_any_order)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float times[2] = {0.0F, 1.0F};
    const float v3[6] = {1, 1, 1, 2, 2, 2};
    const float v4[8] = {0, 0, 0, 1, 0, 0, 0, 1};
    const Acc accs[3] = {{times, 2, "SCALAR"}, {v3, 2, "VEC3"}, {v4, 2, "VEC4"}};
    /* ordre fourni : (Hips, échelle), (Hips, rotation), (Hips, translation), (Spine, translation)
     */
    const char *anim =
        "{\"samplers\":[{\"input\":0,\"output\":1},{\"input\":0,\"output\":2}],\"channels\":["
        "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"translation\"}},"
        "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"scale\"}},"
        "{\"sampler\":1,\"target\":{\"node\":0,\"path\":\"rotation\"}},"
        "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]}";
    EngAnimClip clip;
    ASSERT_TRUE(import_chain(&f, accs, 3, anim, &clip, NULL));
    ASSERT_EQ(clip.channel_count, 4U);
    const uint32_t joints[4] = {0, 0, 0, 1};
    const uint32_t paths[4] = {ENG_ANIM_TRANSLATION, ENG_ANIM_ROTATION, ENG_ANIM_SCALE,
                               ENG_ANIM_TRANSLATION};
    for (uint32_t c = 0; c < 4; c++) {
        EXPECT_EQ(clip.channels[c].joint, joints[c]);
        EXPECT_EQ(clip.channels[c].path, paths[c]);
    }
    float v[4];
    eng_anim_sample(&clip, 2, 1.0F, v);
    EXPECT_NEAR(v[0], 2.0F, 1e-6F);
    fx_done(&f);
}

UTEST(anim, two_root_joints_under_the_armature_are_both_recomputed)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const char *nodes = "{\"name\":\"Armature\",\"scale\":[0.01,0.01,0.01],\"children\":[1,2]},"
                        "{\"name\":\"Hips\",\"translation\":[0,100,0]},"
                        "{\"name\":\"Prop\",\"translation\":[50,0,0]}";
    const float times[2] = {0.0F, 1.0F};
    const Acc accs[2] = {{times, 2, "SCALAR"}, {CM_T, 2, "VEC3"}};
    const char *anim = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                       "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"translation\"}},"
                       "{\"sampler\":0,\"target\":{\"node\":2,\"path\":\"translation\"}}]}";
    const size_t size = build_glb(glb, GLB_MAX, nodes, "{\"joints\":[1,2]}", anim, accs, 2);
    ASSERT_TRUE(size > 0);
    EngAnimClip clip;
    EngAnimImportStats st;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, &st));
    EXPECT_EQ(st.rebaked_joints, 2U);
    EXPECT_EQ(st.kept_channels, 6U);
    float v[4];
    eng_anim_sample(&clip, eng_anim_find_channel(&clip, 1, ENG_ANIM_TRANSLATION), 1.0F, v);
    EXPECT_NEAR(v[1], 2.0F, 1e-5F);
    fx_done(&f);
}

UTEST(anim, rebaked_rotation_keys_stay_in_the_same_hemisphere_and_a_single_key_works)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const char *nodes = "{\"name\":\"Armature\",\"scale\":[0.5,0.5,0.5],\"children\":[1]},"
                        "{\"name\":\"Hips\"}";
    const float times[4] = {0.0F, 1.0F, 2.0F, 3.0F};
    /* rotations de 0, 100, 200 et 300 degrés autour de Z : la forme canonique d'une matrice (w >=
     * 0) change de signe aux deux dernières clés */
    const float rot[16] = {0,           0,           0,    1,           0,           0,
                           0.76604444F, 0.64278761F, 0,    0,           0.98480775F, -0.17364818F,
                           0,           0,           0.5F, -0.86602540F};
    const Acc accs[2] = {{times, 4, "SCALAR"}, {rot, 4, "VEC4"}};
    const char *anim = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                       "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"rotation\"}}]}";
    size_t size = build_glb(glb, GLB_MAX, nodes, "{\"joints\":[1]}", anim, accs, 2);
    ASSERT_TRUE(size > 0);
    EngAnimClip clip;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));
    const uint32_t cr = eng_anim_find_channel(&clip, 0, ENG_ANIM_ROTATION);
    const uint32_t cs = eng_anim_find_channel(&clip, 0, ENG_ANIM_SCALE);
    ASSERT_TRUE(cr != UINT32_MAX && cs != UINT32_MAX);
    ASSERT_EQ(clip.channels[cr].key_count, 4U);
    const float *rv = &clip.values[clip.value_first[cr]];
    for (int k = 1; k < 4; k++) {
        const float dot =
            (rv[(k * 4) + 0] * rv[((k - 1) * 4) + 0]) + (rv[(k * 4) + 1] * rv[((k - 1) * 4) + 1]) +
            (rv[(k * 4) + 2] * rv[((k - 1) * 4) + 2]) + (rv[(k * 4) + 3] * rv[((k - 1) * 4) + 3]);
        EXPECT_GT(dot, 0.0F); /* continuité : jamais l'hémisphère opposé */
    }
    for (int k = 0; k < 4; k++) { /* chaque clé représente bien la rotation d'origine (±q) */
        const float dot =
            fabsf((rv[(k * 4) + 2] * rot[(k * 4) + 2]) + (rv[(k * 4) + 3] * rot[(k * 4) + 3]));
        EXPECT_NEAR(dot, 1.0F, 1e-5F);
    }
    const float *sv = &clip.values[clip.value_first[cs]];
    for (int k = 0; k < 12; k++) { /* l'échelle de l'Armature, positive, sur toutes les clés */
        EXPECT_NEAR(sv[k], 0.5F, 1e-6F);
    }

    /* une seule clé : l'union des instants n'a qu'un élément */
    const float one_time[1] = {0.5F};
    const float one_tr[3] = {0, 4, 0};
    const Acc one[2] = {{one_time, 1, "SCALAR"}, {one_tr, 1, "VEC3"}};
    const char *anim1 = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                        "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"translation\"}}]}";
    size = build_glb(glb, GLB_MAX, nodes, "{\"joints\":[1]}", anim1, one, 2);
    ASSERT_TRUE(size > 0);
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));
    const uint32_t ct = eng_anim_find_channel(&clip, 0, ENG_ANIM_TRANSLATION);
    ASSERT_NE(ct, UINT32_MAX);
    EXPECT_EQ(clip.channels[ct].key_count, 1U);
    float v[4];
    eng_anim_sample(&clip, ct, 0.0F, v);
    EXPECT_NEAR(v[1], 2.0F, 1e-6F); /* 4 x 0,5 */
    fx_done(&f);
}

UTEST(anim, rebaked_rotation_continuity_holds_for_an_oblique_axis)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const char *nodes = "{\"name\":\"Armature\",\"scale\":[0.5,0.5,0.5],\"children\":[1]},"
                        "{\"name\":\"Hips\"}";
    const float times[4] = {0.0F, 1.0F, 2.0F, 3.0F};
    /* rotations de 0, 100, 200 et 300 degrés autour de l'axe (1, 2, 2) / 3 : les quatre composantes
     * sont non nulles, et la forme canonique (w >= 0) d'une matrice change de signe */
    const float rot[16] = {0.00000000F, 0.00000000F, 0.00000000F, 1.00000000F,
                           0.25534815F, 0.51069630F, 0.51069630F, 0.64278761F,
                           0.32826925F, 0.65653850F, 0.65653850F, -0.17364818F,
                           0.16666667F, 0.33333333F, 0.33333333F, -0.86602540F};
    const Acc accs[2] = {{times, 4, "SCALAR"}, {rot, 4, "VEC4"}};
    const char *anim = "{\"samplers\":[{\"input\":0,\"output\":1}],\"channels\":["
                       "{\"sampler\":0,\"target\":{\"node\":1,\"path\":\"rotation\"}}]}";
    const size_t size = build_glb(glb, GLB_MAX, nodes, "{\"joints\":[1]}", anim, accs, 2);
    ASSERT_TRUE(size > 0);
    EngAnimClip clip;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, NULL));
    const uint32_t cr = eng_anim_find_channel(&clip, 0, ENG_ANIM_ROTATION);
    ASSERT_NE(cr, UINT32_MAX);
    ASSERT_EQ(clip.channels[cr].key_count, 4U);
    const float *rv = &clip.values[clip.value_first[cr]];
    for (int k = 0; k < 4; k++) {
        float same = 0.0F; /* ressemblance avec la rotation d'origine, au signe près */
        for (int i = 0; i < 4; i++) {
            same += rv[(k * 4) + i] * rot[(k * 4) + i];
        }
        EXPECT_NEAR(fabsf(same), 1.0F, 1e-5F);
        if (k > 0) {
            float dot = 0.0F;
            for (int i = 0; i < 4; i++) {
                dot += rv[(k * 4) + i] * rv[((k - 1) * 4) + i];
            }
            EXPECT_GT(dot, 0.0F);
        }
    }
    fx_done(&f);
}

UTEST(anim, import_accepts_768_channels_on_256_joints)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static char nodes[32768];
    static char skin[2048];
    static char anim[131072];
    static uint8_t glb[1U << 17];
    size_t np = 0;
    size_t sp = (size_t)snprintf(skin, sizeof skin, "{\"joints\":[");
    for (uint32_t i = 0; i < 256; i++) {
        np += (size_t)snprintf(nodes + np, sizeof nodes - np, "%s{\"children\":[%u]}", i ? "," : "",
                               i + 1U);
        sp += (size_t)snprintf(skin + sp, sizeof skin - sp, "%s%u", i ? "," : "", i);
    }
    (void)snprintf(nodes + np, sizeof nodes - np, ",{}");
    (void)snprintf(skin + sp, sizeof skin - sp, "]}");
    const float times[2] = {0.0F, 1.0F};
    const float v3[6] = {0, 0, 0, 1, 1, 1};
    const float v4[8] = {0, 0, 0, 1, 0, 0, 0, 1};
    const Acc accs[3] = {{times, 2, "SCALAR"}, {v3, 2, "VEC3"}, {v4, 2, "VEC4"}};
    size_t ap =
        (size_t)snprintf(anim, sizeof anim,
                         "{\"samplers\":[{\"input\":0,\"output\":1},{\"input\":0,\"output\":2}],"
                         "\"channels\":[");
    const char *const paths[3] = {"translation", "rotation", "scale"};
    for (uint32_t j = 0; j < 256; j++) {
        for (uint32_t p = 0; p < 3; p++) {
            ap += (size_t)snprintf(anim + ap, sizeof anim - ap,
                                   "%s{\"sampler\":%u,\"target\":{\"node\":%u,\"path\":\"%s\"}}",
                                   (j || p) ? "," : "", p == 1 ? 1U : 0U, j, paths[p]);
        }
    }
    (void)snprintf(anim + ap, sizeof anim - ap, "]}");
    const size_t size = build_glb(glb, sizeof glb, nodes, skin, anim, accs, 3);
    ASSERT_TRUE(size > 0);
    EngAnimClip clip;
    EngAnimImportStats st;
    ASSERT_TRUE(
        eng_anim_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, 0, "p.skel", &clip, &st));
    EXPECT_EQ(clip.channel_count, 768U);
    EXPECT_EQ(st.kept_channels, 768U);
    EXPECT_EQ(st.ignored_channels, 0U);
    fx_done(&f);
}

/* ── Outil en ligne de commande ── */

static int run_assetc(const char *a, const char *b, const char *c, const char *d, const char *e,
                      const char *g)
{
    const char *const argv[] = {OW_ASSETC_PATH, a, b, c, d, e, g, NULL};
    SDL_Process *process = SDL_CreateProcess(argv, false);
    if (process == NULL) {
        return -1;
    }
    int code = -1;
    const bool waited = SDL_WaitProcess(process, true, &code);
    SDL_DestroyProcess(process);
    return waited ? code : -1;
}

UTEST(anim, assetc_anim_cooks_a_gltf_into_an_anim_and_refuses_bad_requests)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const size_t size = chain_glb(glb);
    ASSERT_TRUE(size > 0);
    const char *in = "test_anim_cli.glb";
    const char *out = "anim_demo.anim"; /* gardé : lu par d'autres tests */
    const char *dep = "test_anim_cli.d";
    FILE *fp = fopen(in, "wb");
    ASSERT_TRUE(fp != NULL);
    ASSERT_EQ(fwrite(glb, 1, size, fp), size);
    fclose(fp);
    remove(out);
    remove(dep);

    EXPECT_EQ(run_assetc("anim", in, out, "--depfile", dep, NULL), 0);
    EngAnimClip clip;
    ASSERT_TRUE(eng_anim_load(&f.arena, &f.scratch, out, &clip));
    EXPECT_EQ(clip.channel_count, 2U);
    EXPECT_STREQ(clip.skeleton_path, "test_anim_cli.skel"); /* défaut : nom de l'entrée + .skel */
    FILE *d = fopen(dep, "rb");
    EXPECT_TRUE(d != NULL);
    if (d != NULL) {
        fclose(d);
    }
    EXPECT_EQ(run_assetc("anim", in, out, "--skeleton", "dossier/perso.skel", NULL), 0);
    ASSERT_TRUE(eng_anim_load(&f.arena, &f.scratch, out, &clip));
    EXPECT_STREQ(clip.skeleton_path, "dossier/perso.skel");
    /* laissés pour le test launcher.anim (débogueur d'animation) : squelette + clip qui s'y réfère
     */
    EXPECT_EQ(run_assetc("skel", in, "anim_demo.skel", NULL, NULL, NULL), 0);
    EXPECT_EQ(run_assetc("anim", in, out, "--skeleton", "anim_demo.skel", NULL), 0);
    /* et deux sockets pour launcher.sockets (M4-22) */
    {
        const char *sockets = "# points d'attache de démonstration\n"
                              "socket tete Head 0 0.1 0\n"
                              "socket dos Spine 0 0 -0.1 0 180 0\n";
        FILE *sp = fopen("anim_demo.sockets", "wb");
        ASSERT_TRUE(sp != NULL);
        fputs(sockets, sp);
        fclose(sp);
    }

    EXPECT_EQ(run_assetc("anim", in, out, "--anim", "1", NULL), 1); /* une seule animation */
    EXPECT_EQ(run_assetc("anim", in, out, "--skin", "1", NULL), 1); /* un seul skin */
    EXPECT_EQ(run_assetc("anim", in, "mauvaise.mesh", NULL, NULL, NULL), 2);
    EXPECT_EQ(run_assetc("anim", "absent.glb", out, NULL, NULL, NULL), 1);
    EXPECT_EQ(run_assetc("anim", in, out, "--anim", "x", NULL), 2);
    remove(in);
    remove(dep);
    fx_done(&f);
}
