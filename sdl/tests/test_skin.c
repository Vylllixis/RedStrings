/* Poids de skinning (M4-15, partie CPU) : JOINTS_0 / WEIGHTS_0 d'un glTF recodés avec la
 * permutation du squelette. Les GLB sont construits en mémoire ; valeurs calculées à la main. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/model_gpu.h"
#include "engine/skeleton.h"
#include "engine/skin.h"
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

/* Un triangle, trois sommets :
 *   v0 (0,0,0) : joint 0, poids 1          v1 (1,0,0) : joint 1, poids 1
 *   v2 (0,1,0) : joints 0 et 1, poids 0,5 chacun */
static const float POS[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
static const uint8_t JNT[12] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0};
static const float WGT[12] = {1, 0, 0, 0, 1, 0, 0, 0, 0.5F, 0.5F, 0, 0};
static const uint16_t IDX[3] = {0, 1, 2};

/* Nœuds : 0 Hips (articulation), 1 Spine (articulation), 2 maillage skinné décalé de 100 m en X
 * (sa matrice doit être IGNORÉE). */
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
    return eng_arena_init(&f->arena, "skin", (size_t)1 << 20) &&
           eng_arena_init(&f->scratch, "skin-scratch", (size_t)1 << 20);
}

static void fx_done(Fx *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_arena_shutdown(&f->arena);
}

UTEST(skin, weights_and_joints_are_read_and_the_node_matrix_is_ignored)
{
    static uint8_t glb[GLB_MAX];
    const size_t size = good_glb(glb, "{\"joints\":[0,1]}");
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EngModel model;
    EngSkinVertex *skin = NULL;
    ASSERT_TRUE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model, &skin));
    ASSERT_TRUE(skin != NULL);
    ASSERT_EQ(model.vertex_count, 3U);
    ASSERT_EQ(model.index_count, 3U);
    /* le décalage de 100 m du nœud n'est pas appliqué */
    EXPECT_NEAR(model.vertices[1].position[0], 1.0F, 1e-6F);
    EXPECT_NEAR(model.vertices[2].position[1], 1.0F, 1e-6F);
    EXPECT_EQ(skin[0].joints[0], 0U);
    EXPECT_EQ(skin[0].weights[0], 255U);
    EXPECT_EQ(skin[0].weights[1], 0U);
    EXPECT_EQ(skin[1].joints[0], 1U);
    EXPECT_EQ(skin[1].weights[0], 255U);
    EXPECT_EQ(skin[2].joints[0], 0U);
    EXPECT_EQ(skin[2].joints[1], 1U);
    EXPECT_EQ((unsigned)skin[2].weights[0] + skin[2].weights[1], 255U); /* 127 + 128 */
    EXPECT_TRUE(skin[2].weights[0] >= 127U && skin[2].weights[0] <= 128U);
    EXPECT_EQ(f.scratch.used, (size_t)0);
    fx_done(&f);
}

UTEST(skin, joint_indices_follow_the_skeleton_permutation)
{
    static uint8_t glb[GLB_MAX];
    /* le skin liste Spine (1) AVANT Hips (0) : le squelette range Hips d'abord */
    const size_t size = good_glb(glb, "{\"joints\":[1,0]}");
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EXPECT_EQ(remap[0], 1U); /* indice 0 du skin = Spine = articulation 1 du squelette */
    EXPECT_EQ(remap[1], 0U);
    EngModel model;
    EngSkinVertex *skin = NULL;
    ASSERT_TRUE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model, &skin));
    /* JOINTS_0 vaut 0 pour v0 : l'indice 0 du SKIN, donc Spine = 1 dans le squelette */
    EXPECT_EQ(skin[0].joints[0], 1U);
    EXPECT_EQ(skin[1].joints[0], 0U); /* indice 1 du skin = Hips = 0 */
    EXPECT_EQ(skin[2].joints[0], 1U);
    EXPECT_EQ(skin[2].joints[1], 0U);
    fx_done(&f);
}

UTEST(skin, weights_are_normalised_to_a_sum_of_exactly_255)
{
    static uint8_t glb[GLB_MAX];
    const float wgt[12] = {0.2F, 0.2F, 0,    0, /* somme 0,4 : renormalisée en 50 % / 50 % */
                           1,    1,    1,    0, /* trois influences égales : 85 + 85 + 85 */
                           0.7F, 0.2F, 0.1F, 0.0F /* 178,5 + 51 + 25,5 : l'arrondi est absorbé */};
    const uint8_t jnt[12] = {0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0};
    const Piece pieces[4] = {{POS, sizeof POS, 5126, 3, "VEC3"},
                             {jnt, sizeof jnt, 5121, 3, "VEC4"},
                             {wgt, sizeof wgt, 5126, 3, "VEC4"},
                             {IDX, sizeof IDX, 5123, 3, "SCALAR"}};
    const size_t size = build_glb(glb, NODES, "{\"joints\":[0,1]}", PRIM, pieces, 4);
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EngModel model;
    EngSkinVertex *skin = NULL;
    ASSERT_TRUE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model, &skin));
    for (uint32_t v = 0; v < 3; v++) {
        unsigned sum = 0;
        for (int i = 0; i < 4; i++) {
            sum += skin[v].weights[i];
        }
        EXPECT_EQ(sum, 255U);
    }
    /* égalité parfaite 127,5 / 127,5 : l'arrondi donne 128 + 128, la PREMIÈRE influence cède 1 */
    EXPECT_EQ(skin[0].weights[0], 127U);
    EXPECT_EQ(skin[0].weights[1], 128U);
    EXPECT_EQ(skin[0].weights[2], 0U);
    EXPECT_EQ(skin[1].weights[3], 0U);
    EXPECT_TRUE(skin[1].weights[0] >= 85U && skin[1].weights[0] <= 86U);
    EXPECT_TRUE(skin[2].weights[0] >= 178U && skin[2].weights[0] <= 179U);
    EXPECT_EQ(skin[2].weights[3], 0U);
    EXPECT_EQ(skin[2].joints[3], 0U); /* influence nulle : indice 0 */
    fx_done(&f);
}

typedef struct BadCase {
    const char *what;
    Piece pieces[4];
    uint32_t piece_count;
    const char *prim;
} BadCase;

static bool rejected(const BadCase *c)
{
    static uint8_t glb[GLB_MAX];
    const size_t size =
        build_glb(glb, NODES, "{\"joints\":[0,1]}", c->prim, c->pieces, c->piece_count);
    Fx f;
    if (!fx_init(&f)) {
        return false;
    }
    EngSkeleton sk;
    uint32_t *remap = NULL;
    bool ok = eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap);
    EngModel model = {0};
    EngSkinVertex *skin = (EngSkinVertex *)&f; /* doit être remis à NULL */
    const size_t used = f.arena.used;
    ok = ok && !eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model,
                                        &skin);
    ok = ok && skin == NULL && model.vertex_count == 0 && f.arena.used == used &&
         f.scratch.used == 0;
    fx_done(&f);
    return ok;
}

UTEST(skin, unusable_weights_are_refused_and_everything_is_given_back)
{
    const float zero_w[12] = {0};
    const float neg_w[12] = {1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0};
    const float nan_w[12] = {1, 0, 0, 0, NAN, 0, 0, 0, 1, 0, 0, 0};
    const uint8_t far_j[12] = {0, 0, 0, 0, 2, 0,
                               0, 0, 0, 0, 0, 0}; /* le skin n'a que 2 articulations */
    const Piece pos = {POS, sizeof POS, 5126, 3, "VEC3"};
    const Piece idx = {IDX, sizeof IDX, 5123, 3, "SCALAR"};
    const Piece jnt = {JNT, sizeof JNT, 5121, 3, "VEC4"};
    const Piece wgt = {WGT, sizeof WGT, 5126, 3, "VEC4"};

    BadCase no_skin_attrs = {
        "sans JOINTS/WEIGHTS", {pos, idx}, 2, "\"attributes\":{\"POSITION\":0},\"indices\":1"};
    EXPECT_TRUE(rejected(&no_skin_attrs));
    BadCase no_weights = {"sans WEIGHTS",
                          {pos, jnt, idx},
                          3,
                          "\"attributes\":{\"POSITION\":0,\"JOINTS_0\":1},\"indices\":2"};
    EXPECT_TRUE(rejected(&no_weights));
    BadCase zero = {
        "poids nuls", {pos, jnt, {zero_w, sizeof zero_w, 5126, 3, "VEC4"}, idx}, 4, PRIM};
    EXPECT_TRUE(rejected(&zero));
    BadCase neg = {
        "poids négatif", {pos, jnt, {neg_w, sizeof neg_w, 5126, 3, "VEC4"}, idx}, 4, PRIM};
    EXPECT_TRUE(rejected(&neg));
    const float neg_ok_sum[12] = {1, -0.5F, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}; /* somme 0,5 > 0 */
    BadCase neg_sum = {"poids négatif, somme positive",
                       {pos, jnt, {neg_ok_sum, sizeof neg_ok_sum, 5126, 3, "VEC4"}, idx},
                       4,
                       PRIM};
    EXPECT_TRUE(rejected(&neg_sum));
    BadCase nan = {"poids NaN", {pos, jnt, {nan_w, sizeof nan_w, 5126, 3, "VEC4"}, idx}, 4, PRIM};
    EXPECT_TRUE(rejected(&nan));
    BadCase far = {"articulation hors du skin",
                   {pos, {far_j, sizeof far_j, 5121, 3, "VEC4"}, wgt, idx},
                   4,
                   PRIM};
    EXPECT_TRUE(rejected(&far));
    /* un indice hors du skin mais de poids nul n'est pas lu : accepté (pas de faux refus) */
    static uint8_t glb[GLB_MAX];
    const float w1[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    const uint8_t j_zero_weight[12] = {0, 9, 9, 9, 1, 9, 9, 9, 0, 9, 9, 9};
    const Piece pieces[4] = {pos,
                             {j_zero_weight, sizeof j_zero_weight, 5121, 3, "VEC4"},
                             {w1, sizeof w1, 5126, 3, "VEC4"},
                             idx};
    const size_t size = build_glb(glb, NODES, "{\"joints\":[0,1]}", PRIM, pieces, 4);
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EngModel model;
    EngSkinVertex *skin = NULL;
    EXPECT_TRUE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model, &skin));
    fx_done(&f);
}

UTEST(skin, bad_requests_are_refused)
{
    static uint8_t glb[GLB_MAX];
    const size_t size = good_glb(glb, "{\"joints\":[0,1]}");
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EngModel model;
    EngSkinVertex *skin = (EngSkinVertex *)&f;
    EXPECT_FALSE(eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 1, remap, 2, &model,
                                         &skin)); /* un seul skin : l'indice 1 n'existe pas */
    EXPECT_TRUE(skin == NULL);
    EXPECT_FALSE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, NULL, 2, &model, &skin));
    EXPECT_FALSE(eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 1, &model,
                                         &skin)); /* le skin a 2 articulations */
    EXPECT_FALSE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 3, &model, &skin));
    EXPECT_FALSE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model, NULL));
    EXPECT_FALSE(
        eng_model_parse_skinned(&f.arena, &f.scratch, NULL, 0, NULL, 0, remap, 2, &model, &skin));
    EXPECT_EQ(f.arena.used > 0, 1); /* l'import du squelette est toujours là */
    fx_done(&f);
}

UTEST(skin, an_ordinary_parse_of_a_skinned_file_still_works_and_applies_the_node_matrix)
{
    static uint8_t glb[GLB_MAX];
    const size_t size = good_glb(glb, "{\"joints\":[0,1]}");
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngModel model;
    ASSERT_TRUE(eng_model_parse(&f.arena, &f.scratch, glb, size, NULL, &model));
    EXPECT_NEAR(model.vertices[1].position[0], 101.0F, 1e-4F); /* le comportement historique */
    fx_done(&f);
}

/* L'ordre des attributs dans le fichier ne compte pas, et un second jeu JOINTS_1 / WEIGHTS_1 est
 * ignoré (les quatre premières influences font foi). */
UTEST(skin, attribute_order_is_irrelevant_and_a_second_influence_set_is_ignored)
{
    static uint8_t glb[GLB_MAX];
    const Piece pieces[6] = {
        {POS, sizeof POS, 5126, 3, "VEC3"}, {JNT, sizeof JNT, 5121, 3, "VEC4"},
        {WGT, sizeof WGT, 5126, 3, "VEC4"}, {IDX, sizeof IDX, 5123, 3, "SCALAR"},
        {JNT, sizeof JNT, 5121, 3, "VEC4"}, {WGT, sizeof WGT, 5126, 3, "VEC4"}};
    const size_t size = build_glb(glb, NODES, "{\"joints\":[0,1]}",
                                  "\"attributes\":{\"WEIGHTS_1\":5,\"WEIGHTS_0\":2,\"JOINTS_1\":4,"
                                  "\"JOINTS_0\":1,\"POSITION\":0},\"indices\":3",
                                  pieces, 6);
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EngModel model;
    EngSkinVertex *skin = NULL;
    ASSERT_TRUE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model, &skin));
    EXPECT_EQ(skin[1].joints[0], 1U);
    EXPECT_EQ(skin[1].weights[0], 255U);
    EXPECT_EQ(skin[2].joints[1], 1U);
    fx_done(&f);
}

/* Maillage rigide rattaché à un os (M4, personnages KayKit : casque, cape) : un nœud sans skin
 * rangé sous l'articulation Spine suit Spine. Il est inclus avec un poids 255 sur Spine et ses
 * sommets sont placés en pose de LIAISON. Ici la liaison diffère du repos : inverse_bind(Spine) =
 * T(0, -1, 0) (Spine liée à y = 1) alors que Spine est au repos à y = 1,5 ; le casque est à T(0,
 * 0,25, 0) x rotation de 90° autour de Z sous Spine. Attendu : sommet = T(0, 1,25, 0) x R(90° Z) x
 * p. Un maillage rigide qui n'est sous AUCUNE articulation reste exclu. */
UTEST(skin, a_rigid_mesh_under_a_joint_follows_that_joint_in_the_bind_pose)
{
    static uint8_t glb[GLB_MAX];
    static const float ibm[32] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1,
                                  1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1};
    const Piece pieces[5] = {{POS, sizeof POS, 5126, 3, "VEC3"},
                             {JNT, sizeof JNT, 5121, 3, "VEC4"},
                             {WGT, sizeof WGT, 5126, 3, "VEC4"},
                             {IDX, sizeof IDX, 5123, 3, "SCALAR"},
                             {ibm, sizeof ibm, 5126, 2, "MAT4"}};
    /* 0 Hips, 1 Spine (enfant 3), 2 maillage skinné, 3 casque rigide sous Spine, 4 maillage rigide
     * hors du squelette ; le maillage 1 (sans poids) est fermé dans la chaîne de la primitive. */
    const char *nodes = "{\"name\":\"Hips\",\"translation\":[0,1,0],\"children\":[1]},"
                        "{\"name\":\"Spine\",\"translation\":[0,0.5,0],\"children\":[3]},"
                        "{\"mesh\":0,\"skin\":0},"
                        "{\"name\":\"Helmet\",\"mesh\":1,\"translation\":[0,0.25,0],"
                        "\"rotation\":[0,0,0.70710678,0.70710678]},"
                        "{\"name\":\"Loose\",\"mesh\":1,\"translation\":[5,0,0]}";
    const char *prims = PRIM "}]},{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":3";
    const size_t size =
        build_glb(glb, nodes, "{\"joints\":[0,1],\"inverseBindMatrices\":4}", prims, pieces, 5);
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, &sk, &remap));
    EngModel model;
    EngSkinVertex *skin = NULL;
    ASSERT_TRUE(
        eng_model_parse_skinned(&f.arena, &f.scratch, glb, size, NULL, 0, remap, 2, &model, &skin));
    ASSERT_EQ(model.vertex_count, 6U); /* 3 skinnés + 3 du casque ; « Loose » exclu */
    const float expect[3][3] = {{0.0F, 1.25F, 0.0F}, {0.0F, 2.25F, 0.0F}, {-1.0F, 1.25F, 0.0F}};
    for (uint32_t v = 0; v < 3U; v++) {
        for (int a = 0; a < 3; a++) {
            EXPECT_NEAR(model.vertices[3U + v].position[a], expect[v][a], 1e-5F);
        }
        EXPECT_EQ(skin[3U + v].joints[0], (uint8_t)remap[1]); /* Spine */
        EXPECT_EQ(skin[3U + v].weights[0], 255U);
        EXPECT_EQ(skin[3U + v].weights[1], 0U);
    }
    /* les sommets skinnés ne sont pas touchés */
    EXPECT_NEAR(model.vertices[1].position[0], 1.0F, 1e-6F);
    EXPECT_EQ(f.scratch.used, (size_t)0);
    fx_done(&f);
}

/* Les entrées de sommets d'un pipeline doivent avoir des locations 0, 1, 2... dans l'ordre : la
 * compilation SPIR-V (Vulkan) les numérote dans l'ordre de déclaration du shader, D3D12 lit
 * l'indice de TEXCOORD. Le pipeline d'ombre skinné décrivait 0, 4, 5 : sous Vulkan, la validation
 * le refusait et l'ombre d'un personnage lisait ses poids au mauvais endroit. */
static bool contiguous(const EngPipelineDesc *d)
{
    for (uint32_t i = 0; i < d->attribute_count; i++) {
        if (d->attributes[i].location != i) {
            return false;
        }
    }
    return d->attribute_count > 0U;
}

UTEST(skin, every_model_pipeline_declares_contiguous_vertex_locations)
{
    EngPipelineDesc d;
    eng_model_pipeline_desc(&d);
    EXPECT_TRUE(contiguous(&d));
    eng_model_shadow_pipeline_desc(&d);
    EXPECT_TRUE(contiguous(&d));
    eng_model_skinned_pipeline_desc(&d);
    EXPECT_TRUE(contiguous(&d));
    eng_model_skinned_shadow_pipeline_desc(&d);
    EXPECT_TRUE(contiguous(&d));
}
