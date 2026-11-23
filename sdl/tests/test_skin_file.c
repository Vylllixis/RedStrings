/* Fichiers .skin (M4-21) : poids de skinning cuits à côté du .mesh. Unitaire (encodage, décodage,
 * refus) puis de bout en bout avec assetc (GLB construit en mémoire). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/asset_format.h"
#include "engine/mesh_file.h"
#include "engine/skeleton.h"
#include "engine/skin.h"
#include "engine/skin_file.h"
#include "test_util.h"
#include "utest.h"

typedef struct Piece {
    const void *data;
    uint32_t bytes;
    uint32_t component; /* 5126 float, 5121 ubyte, 5123 ushort */
    uint32_t count;
    const char *type;
} Piece;

#define GLB_MAX 65536U

/* nodes_json / skin_json / mesh_json : fragments tels que décrits dans chaque test. */
static size_t build_glb(uint8_t *out, const char *nodes_json, const char *skins_json,
                        const char *prim_json, const Piece *pieces, uint32_t piece_count)
{
    static char json[16384];
    uint32_t offsets[8];
    uint32_t bin_len = 0;
    for (uint32_t i = 0; i < piece_count; i++) {
        offsets[i] = bin_len;
        bin_len += (pieces[i].bytes + 3U) & ~3U;
    }
    int n = snprintf(json, sizeof json,
                     "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0,2]}],"
                     "\"nodes\":[%s],\"skins\":[%s],\"meshes\":[{\"primitives\":[{%s}]}],"
                     "\"buffers\":[{\"byteLength\":%u}],\"bufferViews\":[",
                     nodes_json, skins_json, prim_json, bin_len);
    for (uint32_t i = 0; i < piece_count; i++) {
        n += snprintf(json + n, sizeof json - (size_t)n,
                      "%s{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}", i ? "," : "",
                      offsets[i], pieces[i].bytes);
    }
    n += snprintf(json + n, sizeof json - (size_t)n, "],\"accessors\":[");
    for (uint32_t i = 0; i < piece_count; i++) {
        n += snprintf(json + n, sizeof json - (size_t)n,
                      "%s{\"bufferView\":%u,\"componentType\":%u,\"count\":%u,\"type\":\"%s\"}",
                      i ? "," : "", i, pieces[i].component, pieces[i].count, pieces[i].type);
    }
    n += snprintf(json + n, sizeof json - (size_t)n, "]}");
    size_t jlen = (size_t)n;
    while ((jlen % 4U) != 0) {
        json[jlen++] = ' ';
    }
    const size_t total = 12U + 8U + jlen + 8U + bin_len;
    uint8_t *p = out;
    const uint32_t head[3] = {0x46546C67U, 2U, (uint32_t)total};
    memcpy(p, head, 12);
    p += 12;
    const uint32_t jhead[2] = {(uint32_t)jlen, 0x4E4F534AU};
    memcpy(p, jhead, 8);
    memcpy(p + 8, json, jlen);
    p += 8 + jlen;
    const uint32_t bhead[2] = {bin_len, 0x004E4942U};
    memcpy(p, bhead, 8);
    p += 8;
    memset(p, 0, bin_len);
    for (uint32_t i = 0; i < piece_count; i++) {
        memcpy(p + offsets[i], pieces[i].data, pieces[i].bytes);
    }
    return total;
}

static const float POS[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
static const uint8_t JNT[12] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0};
static const float WGT[12] = {1, 0, 0, 0, 1, 0, 0, 0, 0.5F, 0.5F, 0, 0};
static const uint16_t IDX[3] = {0, 1, 2};
#define NODES                                                                                      \
    "{\"name\":\"Hips\",\"translation\":[0,1,0],\"children\":[1]},"                                \
    "{\"name\":\"Spine\",\"translation\":[0,0.5,0]},"                                              \
    "{\"mesh\":0,\"skin\":0,\"translation\":[100,0,0]}"
#define PRIM "\"attributes\":{\"POSITION\":0,\"JOINTS_0\":1,\"WEIGHTS_0\":2},\"indices\":3"

static size_t good_glb(uint8_t *glb, const char *skin_json)
{
    const Piece pieces[4] = {{POS, sizeof POS, 5126, 3, "VEC3"},
                             {JNT, sizeof JNT, 5121, 3, "VEC4"},
                             {WGT, sizeof WGT, 5126, 3, "VEC4"},
                             {IDX, sizeof IDX, 5123, 3, "SCALAR"}};
    return build_glb(glb, NODES, skin_json, PRIM, pieces, 4);
}

typedef struct Fx {
    EngArena arena;
    EngArena scratch;
} Fx;

static bool fx_init(Fx *f)
{
    return eng_arena_init(&f->arena, "skf", (size_t)1 << 20) &&
           eng_arena_init(&f->scratch, "skf-scratch", (size_t)1 << 20);
}

static void fx_done(Fx *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_arena_shutdown(&f->arena);
}

/* eng_*_load lit relativement à l'exécutable, assetc et SDL_SaveFile relativement au dossier
 * courant : les chemins des fichiers de test sont donc rendus absolus. */
static const char *abs_path(char *out, size_t size, const char *relative)
{
    char *cwd = SDL_GetCurrentDirectory();
    if (cwd == NULL) {
        out[0] = '\0';
        return out;
    }
    SDL_snprintf(out, size, "%s%s", cwd, relative);
    SDL_free(cwd);
    return out;
}

static const EngSkinVertex GOOD_SKIN[3] = {{{0, 0, 0, 0}, {255, 0, 0, 0}},
                                           {{1, 0, 0, 0}, {255, 0, 0, 0}},
                                           {{0, 1, 0, 0}, {127, 128, 0, 0}}};

/* encode puis modifie un octet, et vérifie que le décodage refuse (helper booléen) */
static bool decode_refused_after_patch(size_t offset, uint8_t value, uint32_t vertices)
{
    EngArena arena;
    if (!eng_arena_init(&arena, "patch", (size_t)1 << 16)) {
        return false;
    }
    const void *file = NULL;
    size_t size = 0;
    bool ok = eng_skin_encode(&arena, GOOD_SKIN, 3, 2, &file, &size);
    uint8_t copy[256];
    ok = ok && size <= sizeof copy && offset < size;
    bool refused = false;
    if (ok) {
        memcpy(copy, file, size);
        copy[offset] = value;
        EngSkinVertex *out = NULL;
        uint32_t joints = 99;
        refused = !eng_skin_decode(&arena, copy, size, "patch", vertices, &out, &joints) &&
                  out == NULL && joints == 0U;
    }
    eng_arena_shutdown(&arena);
    return ok && refused;
}

static size_t section_offset(const void *file, size_t size, uint32_t tag)
{
    EngAssetView view;
    EngAssetSection s;
    if (eng_asset_check(file, size, ENG_ASSET_MAGIC_SKIN, ENG_ASSET_VERSION_SKIN, &view) !=
            ENG_ASSET_OK ||
        !eng_asset_find_section(&view, tag, &s)) {
        return 0;
    }
    return (size_t)(s.data - (const uint8_t *)file);
}

UTEST(skin_file, round_trip_keeps_every_influence)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 2, &file, &size));
    EngSkinVertex *out = NULL;
    uint32_t joints = 0;
    ASSERT_TRUE(eng_skin_decode(&f.arena, file, size, "mem", 3, &out, &joints));
    EXPECT_EQ(joints, 2U);
    ASSERT_TRUE(out != NULL);
    EXPECT_EQ(memcmp(out, GOOD_SKIN, sizeof GOOD_SKIN), 0);
    /* déterministe : mêmes entrées, mêmes octets */
    const void *again = NULL;
    size_t again_size = 0;
    ASSERT_TRUE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 2, &again, &again_size));
    ASSERT_EQ(again_size, size);
    EXPECT_EQ(memcmp(file, again, size), 0);
    fx_done(&f);
}

UTEST(skin_file, encoder_refuses_invalid_input)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const void *file = NULL;
    size_t size = 0;
    EXPECT_FALSE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 0, &file, &size)); /* 0 articulation */
    EXPECT_FALSE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 257, &file, &size));
    EXPECT_FALSE(eng_skin_encode(&f.arena, GOOD_SKIN, 0, 2, &file, &size));
    EXPECT_FALSE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 1, &file, &size)); /* indice 1 >= 1 */
    EXPECT_FALSE(eng_skin_encode(&f.arena, NULL, 3, 2, &file, &size));
    EXPECT_FALSE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 2, NULL, &size));
    EXPECT_FALSE(eng_skin_encode(NULL, GOOD_SKIN, 3, 2, &file, &size));
    EXPECT_TRUE(file == NULL);
    EXPECT_EQ(size, (size_t)0);
    const EngSkinVertex bad_sum[1] = {{{0, 0, 0, 0}, {255, 1, 0, 0}}};
    EXPECT_FALSE(eng_skin_encode(&f.arena, bad_sum, 1, 2, &file, &size));
    const EngSkinVertex low_sum[1] = {{{0, 0, 0, 0}, {254, 0, 0, 0}}};
    EXPECT_FALSE(eng_skin_encode(&f.arena, low_sum, 1, 2, &file, &size));
    const EngSkinVertex ghost[1] = {{{0, 1, 0, 0}, {255, 0, 0, 0}}}; /* poids nul, indice 1 */
    EXPECT_FALSE(eng_skin_encode(&f.arena, ghost, 1, 2, &file, &size));
    const EngSkinVertex last_ok[1] = {{{0, 255, 0, 0}, {1, 254, 0, 0}}};
    EXPECT_FALSE(eng_skin_encode(&f.arena, last_ok, 1, 255, &file, &size)); /* 255 >= 255 */
    EXPECT_TRUE(eng_skin_encode(&f.arena, last_ok, 1, 256, &file, &size));  /* 255 < 256 */
    EXPECT_EQ(f.arena.used > 0 ? 1 : 0, 1);
    fx_done(&f);
}

UTEST(skin_file, the_smallest_and_largest_valid_files_round_trip)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const EngSkinVertex one[1] = {{{0, 0, 0, 0}, {255, 0, 0, 0}}};
    const EngSkinVertex big[1] = {{{0, 255, 0, 0}, {1, 254, 0, 0}}};
    const void *file = NULL;
    size_t size = 0;
    EngSkinVertex *out = NULL;
    uint32_t joints = 0;
    ASSERT_TRUE(eng_skin_encode(&f.arena, one, 1, 1, &file, &size)); /* 1 sommet, 1 os */
    ASSERT_TRUE(eng_skin_decode(&f.arena, file, size, "un", 1, &out, &joints));
    EXPECT_EQ(joints, 1U);
    ASSERT_TRUE(eng_skin_encode(&f.arena, big, 1, 256, &file, &size)); /* 256 os */
    ASSERT_TRUE(eng_skin_decode(&f.arena, file, size, "max", 1, &out, &joints));
    EXPECT_EQ(joints, 256U);
    EXPECT_EQ(out[0].joints[1], 255U);
    fx_done(&f);
}

UTEST(skin_file, decoder_refuses_corrupted_files_and_restores_the_arena)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 2, &file, &size));
    const size_t info = section_offset(file, size, ENG_ASSET_TAG_INFO);
    const size_t wght = section_offset(file, size, ENG_ASSET_TAG_WGHT);
    ASSERT_TRUE(info > 0 && wght > 0);
    const size_t before = f.arena.used;
    EngSkinVertex *out = NULL;
    uint32_t joints = 7;
    /* nombre de sommets attendu différent */
    EXPECT_FALSE(eng_skin_decode(&f.arena, file, size, "mem", 4, &out, &joints));
    EXPECT_TRUE(out == NULL);
    EXPECT_EQ(joints, 0U);
    EXPECT_EQ(f.arena.used, before);
    /* tronqué, mauvais magic, vide */
    EXPECT_FALSE(eng_skin_decode(&f.arena, file, size - 16U, "mem", 3, &out, &joints));
    EXPECT_FALSE(eng_skin_decode(&f.arena, file, 0, "mem", 3, &out, &joints));
    uint8_t copy[256];
    ASSERT_TRUE(size <= sizeof copy);
    memcpy(copy, file, size);
    copy[0] ^= 0x20;
    EXPECT_FALSE(eng_skin_decode(&f.arena, copy, size, "mem", 3, &out, &joints));
    EXPECT_FALSE(eng_skin_decode(NULL, file, size, "mem", 3, &out, &joints));
    EXPECT_FALSE(eng_skin_decode(&f.arena, file, size, "mem", 3, NULL, &joints));
    EXPECT_EQ(f.arena.used, before);
    /* INFO : sommets, articulations, réservés */
    EXPECT_TRUE(decode_refused_after_patch(info + 0, 4, 3)); /* 4 sommets déclarés, WGHT en a 3 */
    EXPECT_TRUE(decode_refused_after_patch(info + 4, 0, 3)); /* 0 articulation */
    EXPECT_TRUE(decode_refused_after_patch(info + 5, 1, 3)); /* 258 articulations */
    EXPECT_TRUE(
        decode_refused_after_patch(info + 4, 1, 3)); /* 1 articulation : indice 1 hors squelette */
    EXPECT_TRUE(decode_refused_after_patch(info + 8, 1, 3));  /* réservé */
    EXPECT_TRUE(decode_refused_after_patch(info + 12, 1, 3)); /* réservé */
    /* WGHT : somme, indice fantôme */
    EXPECT_TRUE(decode_refused_after_patch(wght + 4, 254, 3)); /* somme 254 */
    EXPECT_TRUE(decode_refused_after_patch(wght + 23, 1, 3));  /* v2 : somme 256 */
    EXPECT_TRUE(decode_refused_after_patch(wght + 1, 1, 3));   /* v0 : influence nulle, indice 1 */
    EXPECT_TRUE(decode_refused_after_patch(wght + 8, 2, 3)); /* v1 : indice 2 >= 2 articulations */
    /* une modification inoffensive reste acceptée (le helper ne refuse pas tout) */
    EXPECT_FALSE(decode_refused_after_patch(wght + 4, 255, 3));
    fx_done(&f);
}

UTEST(skin_file, load_reads_a_file_and_refuses_a_missing_one)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skin_encode(&f.arena, GOOD_SKIN, 3, 2, &file, &size));
    ASSERT_TRUE(SDL_CreateDirectory(TEST_TMP_DIR));
    char path[512];
    abs_path(path, sizeof path, TEST_TMP_DIR "/skin_file_unit.skin");
    ASSERT_TRUE(SDL_SaveFile(path, file, size));
    EngSkinVertex *out = NULL;
    uint32_t joints = 0;
    EngArena dest;
    ASSERT_TRUE(eng_arena_init(&dest, "dest", (size_t)1 << 16));
    EXPECT_TRUE(eng_skin_load(&dest, &f.scratch, path, 3, &out, &joints));
    EXPECT_EQ(joints, 2U);
    EXPECT_EQ(f.scratch.used, (size_t)0);
    EXPECT_FALSE(eng_skin_load(&dest, &f.scratch, "absent_dir/absent.skin", 3, &out, &joints));
    EXPECT_TRUE(out == NULL);
    EXPECT_FALSE(eng_skin_load(&dest, &f.scratch, path, 5, &out, &joints));
    EXPECT_FALSE(eng_skin_load(&dest, &f.scratch, NULL, 3, &out, &joints));
    eng_arena_shutdown(&dest);
    fx_done(&f);
}

/* ── De bout en bout : assetc mesh --skin ── */

static int run_assetc(const char *a, const char *b, const char *c, const char *d, const char *e,
                      const char *g, const char *h)
{
    const char *const argv[] = {OW_ASSETC_PATH, a, b, c, d, e, g, h, NULL};
    SDL_Process *process = SDL_CreateProcess(argv, false);
    if (process == NULL) {
        return -1;
    }
    int code = -1;
    const bool waited = SDL_WaitProcess(process, true, &code);
    SDL_DestroyProcess(process);
    return waited ? code : -1;
}

static bool write_file(const char *path, const void *data, size_t size)
{
    return SDL_CreateDirectory(TEST_TMP_DIR) && SDL_SaveFile(path, data, size);
}

UTEST(skin_file, assetc_cooks_a_skinned_character_whose_weights_match_the_skeleton)
{
    static uint8_t glb[GLB_MAX];
    /* le skin liste Spine avant Hips : la permutation doit être appliquée aux poids ET au .skel */
    const size_t gsize = good_glb(glb, "{\"joints\":[1,0]}");
    ASSERT_TRUE(write_file(TEST_TMP_DIR "/perso.glb", glb, gsize));
    char glb_path[512];
    char mesh[512];
    char skin[512];
    char skel[512];
    abs_path(glb_path, sizeof glb_path, TEST_TMP_DIR "/perso.glb");
    abs_path(mesh, sizeof mesh, TEST_TMP_DIR "/perso_cook.mesh");
    abs_path(skin, sizeof skin, TEST_TMP_DIR "/perso_cook.skin");
    abs_path(skel, sizeof skel, TEST_TMP_DIR "/perso_cook.skel");
    remove(mesh);
    remove(skin);
    ASSERT_EQ(run_assetc("skel", glb_path, skel, NULL, NULL, NULL, NULL), 0);
    ASSERT_EQ(run_assetc("mesh", glb_path, mesh, "--skin", "0", NULL, NULL), 0);
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngModel model;
    ASSERT_TRUE(eng_mesh_load(&f.arena, &f.scratch, mesh, &model));
    ASSERT_EQ(model.vertex_count, 3U);
    EXPECT_NEAR(model.vertices[1].position[0], 1.0F, 1e-6F); /* pas de décalage du nœud */
    EngSkeleton sk;
    ASSERT_TRUE(eng_skeleton_load(&f.arena, &f.scratch, skel, &sk));
    EngSkinVertex *w = NULL;
    uint32_t joints = 0;
    ASSERT_TRUE(eng_skin_load(&f.arena, &f.scratch, skin, model.vertex_count, &w, &joints));
    EXPECT_EQ(joints, sk.joint_count);
    /* v0 : indice 0 du SKIN = Spine = articulation 1 du squelette cuit */
    EXPECT_STREQ(sk.joints[w[0].joints[0]].name, "Spine");
    EXPECT_STREQ(sk.joints[w[1].joints[0]].name, "Hips");
    EXPECT_EQ(w[0].weights[0], 255U);
    EXPECT_EQ((unsigned)w[2].weights[0] + w[2].weights[1], 255U);
    /* identique au chargement direct du glTF (même permutation, mêmes poids) */
    EngSkeleton sk2;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, gsize, NULL, 0, &sk2, &remap));
    EngModel direct;
    EngSkinVertex *dw = NULL;
    ASSERT_TRUE(eng_model_parse_skinned(&f.arena, &f.scratch, glb, gsize, NULL, 0, remap,
                                        sk2.joint_count, &direct, &dw));
    ASSERT_EQ(direct.vertex_count, model.vertex_count);
    EXPECT_EQ(memcmp(dw, w, sizeof(EngSkinVertex) * model.vertex_count), 0);
    for (uint32_t v = 0; v < model.vertex_count; v++) {
        EXPECT_NEAR(direct.vertices[v].position[0], model.vertices[v].position[0], 1e-6F);
        EXPECT_NEAR(direct.vertices[v].position[1], model.vertices[v].position[1], 1e-6F);
    }
    /* info reconnaît le .skin */
    EXPECT_EQ(run_assetc("info", skin, NULL, NULL, NULL, NULL, NULL), 0);
    /* sans --skin : le .skin périmé est supprimé */
    ASSERT_EQ(run_assetc("mesh", glb_path, mesh, NULL, NULL, NULL, NULL), 0);
    FILE *fp = fopen(skin, "rb");
    EXPECT_TRUE(fp == NULL);
    if (fp != NULL) {
        fclose(fp);
    }
    fx_done(&f);
}

UTEST(skin_file, assetc_refuses_bad_skin_requests)
{
    static uint8_t glb[GLB_MAX];
    const size_t gsize = good_glb(glb, "{\"joints\":[0,1]}");
    ASSERT_TRUE(write_file(TEST_TMP_DIR "/perso2.glb", glb, gsize));
    const char *in = TEST_TMP_DIR "/perso2.glb";
    const char *mesh = TEST_TMP_DIR "/perso2.mesh";
    EXPECT_EQ(run_assetc("mesh", in, mesh, "--skin", "1", NULL, NULL), 1); /* un seul skin */
    EXPECT_EQ(run_assetc("mesh", in, mesh, "--skin", "x", NULL, NULL), 2); /* pas un nombre */
    EXPECT_EQ(run_assetc("mesh", in, mesh, "--skin", "-1", NULL, NULL), 2);
    EXPECT_EQ(run_assetc("mesh", in, mesh, "--skin", NULL, NULL, NULL), 2); /* valeur manquante */
    EXPECT_EQ(run_assetc("mesh", in, mesh, "--skin", "0", "--bench", "2"), 2);
    EXPECT_EQ(run_assetc("mesh", "absent.glb", mesh, "--skin", "0", NULL, NULL), 1);
    /* un glTF sans skin */
    const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const uint16_t idx[3] = {0, 1, 2};
    const Piece pieces[2] = {{pos, sizeof pos, 5126, 3, "VEC3"},
                             {idx, sizeof idx, 5123, 3, "SCALAR"}};
    const size_t plain = build_glb(glb, "{},{},{\"mesh\":0}", "",
                                   "\"attributes\":{\"POSITION\":0},\"indices\":1", pieces, 2);
    ASSERT_TRUE(write_file(TEST_TMP_DIR "/sans_skin.glb", glb, plain));
    EXPECT_EQ(run_assetc("mesh", TEST_TMP_DIR "/sans_skin.glb", mesh, "--skin", "0", NULL, NULL),
              1);
    EXPECT_EQ(run_assetc("mesh", TEST_TMP_DIR "/sans_skin.glb", mesh, NULL, NULL, NULL, NULL), 0);
    /* mauvaise extension de sortie */
    EXPECT_EQ(run_assetc("mesh", in, TEST_TMP_DIR "/x.skin", "--skin", "0", NULL, NULL), 2);
}
