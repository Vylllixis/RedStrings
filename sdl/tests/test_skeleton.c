/* Squelettes (M4-12) : import glTF -> EngSkeleton, format .skel, poses repos. Le GLB est construit
 * en mémoire (aucun fichier Mixamo réel dans le dépôt de test). Géométrie de référence :
 *
 *   Armature (non articulation, translation (10,0,0))
 *     └ Hips   (0,1,0)
 *         └ Spine (0,0.5,0), rotation 90° autour de X
 *             ├ Mid (non articulation, (0,0.25,0))
 *             │   └ Head (0,0.25,0)
 *             └ Arm (0.3,0.2,0)
 *
 * Positions monde attendues (calculées à la main) : Hips (10,1,0), Spine (10,1.5,0),
 * Head (10,1.5,0.5) (le +Y local de Spine devient +Z), Arm (10.3,1.5,0.2).
 * Ordre des articulations dans le skin : Head, Arm, Spine, Hips (parents APRÈS les enfants). */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/asset_format.h"
#include "engine/skeleton.h"
#include "utest.h"

#define ARENA_SIZE ((size_t)1 << 20)

typedef struct Fx {
    EngArena arena;
    EngArena scratch;
} Fx;

static bool fx_init(Fx *f)
{
    return eng_arena_init(&f->arena, "skel-test", ARENA_SIZE) &&
           eng_arena_init(&f->scratch, "skel-scratch", ARENA_SIZE);
}

static void fx_done(Fx *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_arena_shutdown(&f->arena);
}

/* ── GLB en mémoire ── */

#define GLB_MAX 65536U

typedef struct GlbSpec {
    const char *nodes_json;  /* contenu du tableau "nodes" */
    const char *skin_joints; /* "4,5,2,1" */
    uint32_t ibm_count;      /* matrices écrites dans le tampon */
    bool ibm_accessor;       /* le skin référence l'accesseur de matrices */
    uint32_t accessor_count; /* 0 : ibm_count */
    const float *ibm;        /* ibm_count * 16 flottants, NULL : identité */
    bool second_skin;
} GlbSpec;

static size_t build_glb(uint8_t *out, size_t cap, const GlbSpec *s)
{
    char json[8192];
    const uint32_t ibm_count = s->ibm_count;
    const uint32_t acc_count = s->accessor_count != 0 ? s->accessor_count : ibm_count;
    const uint32_t bin_len = ibm_count * 64U;
    int n = snprintf(json, sizeof json,
                     "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
                     "\"nodes\":[%s],"
                     "\"skins\":[{%s\"joints\":[%s]}%s]",
                     s->nodes_json, s->ibm_accessor ? "\"inverseBindMatrices\":0," : "",
                     s->skin_joints, s->second_skin ? ",{\"joints\":[1]}" : "");
    if (ibm_count > 0) {
        n += snprintf(json + n, sizeof json - (size_t)n,
                      ",\"buffers\":[{\"byteLength\":%u}],"
                      "\"bufferViews\":[{\"buffer\":0,\"byteLength\":%u}],"
                      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":%u,"
                      "\"type\":\"MAT4\"}]",
                      bin_len, bin_len, acc_count);
    }
    n += snprintf(json + n, sizeof json - (size_t)n, "}");
    size_t jlen = (size_t)n;
    while ((jlen % 4U) != 0) {
        json[jlen++] = ' ';
    }
    const size_t total = 12U + 8U + jlen + (bin_len > 0 ? 8U + bin_len : 0U);
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
    if (bin_len > 0) {
        const uint32_t bt = 0x004E4942U;
        memcpy(p, &bin_len, 4);
        memcpy(p + 4, &bt, 4);
        if (s->ibm != NULL) {
            memcpy(p + 8, s->ibm, bin_len);
        } else {
            for (uint32_t i = 0; i < ibm_count; i++) {
                mat4 id;
                glm_mat4_identity(id);
                memcpy(p + 8 + ((size_t)i * 64U), id, 64);
            }
        }
    }
    return total;
}

#define NODES_REF                                                                                  \
    "{\"name\":\"Armature\",\"translation\":[10,0,0],\"children\":[1]},"                           \
    "{\"name\":\"Hips\",\"translation\":[0,1,0],\"children\":[2]},"                                \
    "{\"name\":\"Spine\",\"translation\":[0,0.5,0],\"rotation\":[0.70710678,0,0,0.70710678],"      \
    "\"children\":[3,5]},"                                                                         \
    "{\"name\":\"Mid\",\"translation\":[0,0.25,0],\"children\":[4]},"                              \
    "{\"name\":\"Head\",\"translation\":[0,0.25,0]},"                                              \
    "{\"name\":\"Arm\",\"translation\":[0.3,0.2,0]}"

/* Matrices inverses de la pose repos, dans l'ordre du skin Head, Arm, Spine, Hips. */
static void reference_ibms(float out[4][16])
{
    const float pos[4][3] = {
        {10.0F, 1.5F, 0.5F}, {10.3F, 1.5F, 0.2F}, {10.0F, 1.5F, 0.0F}, {10.0F, 1.0F, 0.0F}};
    /* Spine et ses enfants ont la rotation de 90° autour de X ; Hips non. Head et Arm la
     * héritent de Spine. */
    const bool rotated[4] = {true, true, true, false};
    for (int i = 0; i < 4; i++) {
        mat4 world;
        glm_translate_make(world, (vec3){pos[i][0], pos[i][1], pos[i][2]});
        if (rotated[i]) {
            mat4 r;
            glm_rotate_make(r, GLM_PI_2f, (vec3){1.0F, 0.0F, 0.0F});
            glm_mat4_mul(world, r, world);
        }
        mat4 inv;
        glm_mat4_inv(world, inv);
        memcpy(out[i], inv, sizeof inv);
    }
}

static size_t reference_glb(uint8_t *buf, bool with_ibm_accessor, const float *ibm)
{
    GlbSpec s = {.nodes_json = NODES_REF,
                 .skin_joints = "4,5,2,1",
                 .ibm_count = with_ibm_accessor ? 4U : 0U,
                 .ibm_accessor = with_ibm_accessor,
                 .ibm = ibm};
    return build_glb(buf, GLB_MAX, &s);
}

static bool import(Fx *f, const uint8_t *glb, size_t size, uint32_t skin, EngSkeleton *sk,
                   uint32_t **remap)
{
    return eng_skeleton_from_gltf(&f->arena, &f->scratch, glb, size, NULL, skin, sk, remap);
}

static int find_joint(const EngSkeleton *sk, const char *name)
{
    for (uint32_t i = 0; i < sk->joint_count; i++) {
        if (sk->joints[i].name != NULL && strcmp(sk->joints[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ── Import ── */

UTEST(skeleton, import_orders_parents_first_and_keeps_the_hierarchy)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    float ibm[4][16];
    reference_ibms(ibm);
    uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, true, &ibm[0][0]);
    ASSERT_TRUE(size > 0);
    EngSkeleton sk;
    uint32_t *remap = NULL;
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, &remap));
    ASSERT_EQ(sk.joint_count, 4U);
    EXPECT_STREQ(sk.joints[0].name, "Hips");
    EXPECT_STREQ(sk.joints[1].name, "Spine");
    EXPECT_STREQ(sk.joints[2].name, "Head");
    EXPECT_STREQ(sk.joints[3].name, "Arm");
    EXPECT_EQ(sk.joints[0].parent, -1);
    EXPECT_EQ(sk.joints[1].parent, 0);
    EXPECT_EQ(sk.joints[2].parent, 1);
    EXPECT_EQ(sk.joints[3].parent, 1);
    /* permutation : skin Head, Arm, Spine, Hips -> 2, 3, 1, 0 */
    ASSERT_TRUE(remap != NULL);
    EXPECT_EQ(remap[0], 2U);
    EXPECT_EQ(remap[1], 3U);
    EXPECT_EQ(remap[2], 1U);
    EXPECT_EQ(remap[3], 0U);
    fx_done(&f);
}

UTEST(skeleton, import_folds_non_joint_ancestors_into_the_local_pose)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    float ibm[4][16];
    reference_ibms(ibm);
    uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, true, &ibm[0][0]);
    EngSkeleton sk;
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, NULL));
    const EngJoint *hips = &sk.joints[find_joint(&sk, "Hips")];
    const EngJoint *spine = &sk.joints[find_joint(&sk, "Spine")];
    const EngJoint *head = &sk.joints[find_joint(&sk, "Head")];
    /* Hips : Armature (10,0,0) + (0,1,0) repliés dans la racine */
    EXPECT_NEAR(hips->translation[0], 10.0F, 1e-5F);
    EXPECT_NEAR(hips->translation[1], 1.0F, 1e-5F);
    /* Head : Mid (0,0.25,0) + (0,0.25,0) = (0,0.5,0) dans le repère de Spine */
    EXPECT_NEAR(head->translation[0], 0.0F, 1e-5F);
    EXPECT_NEAR(head->translation[1], 0.5F, 1e-5F);
    EXPECT_NEAR(head->translation[2], 0.0F, 1e-5F);
    /* Spine : rotation 90° autour de X, échelle 1 */
    EXPECT_NEAR(spine->rotation[0], 0.70710678F, 1e-5F);
    EXPECT_NEAR(spine->rotation[3], 0.70710678F, 1e-5F);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(spine->scale[i], 1.0F, 1e-5F);
    }
    fx_done(&f);
}

UTEST(skeleton, rest_world_positions_match_the_hand_computation_and_the_bind_pose)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    float ibm[4][16];
    reference_ibms(ibm);
    uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, true, &ibm[0][0]);
    EngSkeleton sk;
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, NULL));
    mat4 world[ENG_SKELETON_MAX_JOINTS];
    ASSERT_TRUE(eng_skeleton_rest_world(&sk, world));
    const struct {
        const char *name;
        float p[3];
    } want[] = {{"Hips", {10.0F, 1.0F, 0.0F}},
                {"Spine", {10.0F, 1.5F, 0.0F}},
                {"Head", {10.0F, 1.5F, 0.5F}},
                {"Arm", {10.3F, 1.5F, 0.2F}}};
    for (size_t i = 0; i < 4; i++) {
        const int j = find_joint(&sk, want[i].name);
        ASSERT_TRUE(j >= 0);
        EXPECT_NEAR(world[j][3][0], want[i].p[0], 1e-4F);
        EXPECT_NEAR(world[j][3][1], want[i].p[1], 1e-4F);
        EXPECT_NEAR(world[j][3][2], want[i].p[2], 1e-4F);
    }
    EXPECT_LT(eng_skeleton_bind_mismatch(&sk), 1e-4F);
    mat4 bind[ENG_SKELETON_MAX_JOINTS];
    ASSERT_TRUE(eng_skeleton_bind_world(&sk, bind));
    EXPECT_NEAR(bind[find_joint(&sk, "Head")][3][2], 0.5F, 1e-4F);
    fx_done(&f);
}

UTEST(skeleton, a_shifted_bind_pose_is_measured_as_a_mismatch)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    float ibm[4][16];
    reference_ibms(ibm);
    /* Hips (4e matrice du skin) : on décale la pose de liaison de 0,25 m en Y */
    mat4 shift;
    glm_translate_make(shift, (vec3){0.0F, -0.25F, 0.0F});
    mat4 old;
    memcpy(old, ibm[3], sizeof old);
    mat4 moved;
    glm_mat4_mul(shift, old, moved);
    memcpy(ibm[3], moved, sizeof moved);
    uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, true, &ibm[0][0]);
    EngSkeleton sk;
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, NULL));
    EXPECT_NEAR(eng_skeleton_bind_mismatch(&sk), 0.25F, 1e-3F);
    fx_done(&f);
}

UTEST(skeleton, without_inverse_bind_matrices_they_are_the_identity)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, false, NULL);
    EngSkeleton sk;
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, NULL));
    for (uint32_t i = 0; i < sk.joint_count; i++) {
        for (int k = 0; k < 16; k++) {
            EXPECT_EQ(sk.joints[i].inverse_bind[k], (k % 5 == 0) ? 1.0F : 0.0F);
        }
    }
    fx_done(&f);
}

UTEST(skeleton, skin_count_and_invalid_skin_index)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    uint8_t glb[GLB_MAX];
    GlbSpec s = {.nodes_json = NODES_REF, .skin_joints = "4,5,2,1", .second_skin = true};
    const size_t size = build_glb(glb, GLB_MAX, &s);
    ASSERT_TRUE(size > 0);
    EXPECT_EQ(eng_skeleton_gltf_skin_count(glb, size), 2U);
    EngSkeleton sk;
    uint32_t *remap = (uint32_t *)&sk;
    EXPECT_TRUE(import(&f, glb, size, 1, &sk, &remap)); /* le second skin : 1 articulation */
    EXPECT_EQ(sk.joint_count, 1U);
    EXPECT_FALSE(import(&f, glb, size, 2, &sk, &remap));
    EXPECT_EQ(sk.joint_count, 0U);
    EXPECT_TRUE(remap == NULL);
    EXPECT_EQ(eng_skeleton_gltf_skin_count(glb, 10), 0U);
    EXPECT_EQ(eng_skeleton_gltf_skin_count(NULL, 0), 0U);
    fx_done(&f);
}

UTEST(skeleton, import_rejects_bad_inputs_and_rolls_the_arenas_back)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    float ibm[4][16];
    reference_ibms(ibm);
    uint8_t glb[GLB_MAX];
    EngSkeleton sk;

    /* nombre de matrices différent du nombre d'articulations */
    GlbSpec few = {.nodes_json = NODES_REF,
                   .skin_joints = "4,5,2,1",
                   .ibm_count = 4,
                   .ibm_accessor = true,
                   .accessor_count = 3};
    size_t size = build_glb(glb, GLB_MAX, &few);
    const size_t used_before = f.arena.used;
    EXPECT_FALSE(import(&f, glb, size, 0, &sk, NULL));
    EXPECT_EQ(sk.joint_count, 0U);
    EXPECT_EQ(f.arena.used, used_before);
    EXPECT_EQ(f.scratch.used, (size_t)0);

    /* articulation listée deux fois */
    GlbSpec dup = {.nodes_json = NODES_REF, .skin_joints = "4,4,2,1"};
    size = build_glb(glb, GLB_MAX, &dup);
    EXPECT_FALSE(import(&f, glb, size, 0, &sk, NULL));

    /* échelle négative (miroir) et échelle nulle */
    GlbSpec mirror = {.nodes_json = "{\"children\":[1]},{\"scale\":[-1,1,1]}", .skin_joints = "1"};
    size = build_glb(glb, GLB_MAX, &mirror);
    EXPECT_FALSE(import(&f, glb, size, 0, &sk, NULL));
    GlbSpec zero = {.nodes_json = "{\"children\":[1]},{\"scale\":[0,1,1]}", .skin_joints = "1"};
    size = build_glb(glb, GLB_MAX, &zero);
    EXPECT_FALSE(import(&f, glb, size, 0, &sk, NULL));

    /* nom trop long (64 octets) */
    char long_name[200];
    snprintf(long_name, sizeof long_name, "{\"children\":[1]},{\"name\":\"%065d\"}",
             0); /* 65 caractères */
    GlbSpec longn = {.nodes_json = long_name, .skin_joints = "1"};
    size = build_glb(glb, GLB_MAX, &longn);
    EXPECT_FALSE(import(&f, glb, size, 0, &sk, NULL));

    /* aucune articulation */
    GlbSpec none = {.nodes_json = "{}", .skin_joints = ""};
    size = build_glb(glb, GLB_MAX, &none);
    EXPECT_FALSE(import(&f, glb, size, 0, &sk, NULL));

    /* données absentes ou tronquées */
    EXPECT_FALSE(import(&f, NULL, 0, 0, &sk, NULL));
    size = reference_glb(glb, true, &ibm[0][0]);
    EXPECT_FALSE(import(&f, glb, size / 2, 0, &sk, NULL));
    EXPECT_FALSE(eng_skeleton_from_gltf(NULL, &f.scratch, glb, size, NULL, 0, &sk, NULL));
    EXPECT_FALSE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, size, NULL, 0, NULL, NULL));
    fx_done(&f);
}

UTEST(skeleton, the_256_joint_limit_is_enforced_on_import)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static char nodes[32768];
    static char joints[4096];
    static uint8_t glb[GLB_MAX];
    EngSkeleton sk;
    for (uint32_t count = 256; count <= 257; count++) {
        size_t np = 0;
        size_t jp = 0;
        for (uint32_t i = 0; i < count; i++) {
            np += (size_t)snprintf(nodes + np, sizeof nodes - np, "%s{\"children\":[%u]}",
                                   i == 0 ? "" : ",", i + 1U);
            jp += (size_t)snprintf(joints + jp, sizeof joints - jp, "%s%u", i == 0 ? "" : ",", i);
        }
        /* dernier nœud : sans enfant (on retire "children" du dernier) */
        (void)snprintf(nodes + np, sizeof nodes - np, ",{}");
        GlbSpec s = {.nodes_json = nodes, .skin_joints = joints};
        const size_t size = build_glb(glb, GLB_MAX, &s);
        ASSERT_TRUE(size > 0);
        EXPECT_EQ(import(&f, glb, size, 0, &sk, NULL), count == 256);
        if (count == 256) {
            EXPECT_EQ(sk.joint_count, 256U);
            EXPECT_EQ(sk.joints[255].parent, 254);
        }
    }
    fx_done(&f);
}

/* ── Format .skel ── */

static bool same_bytes(const float *a, const float *b, size_t bytes)
{
    return memcmp((const uint8_t *)a, (const uint8_t *)b, bytes) == 0;
}

static bool make_reference(Fx *f, EngSkeleton *sk)
{
    float ibm[4][16];
    reference_ibms(ibm);
    uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, true, &ibm[0][0]);
    return size > 0 && import(f, glb, size, 0, sk, NULL);
}

UTEST(skeleton, encode_decode_roundtrip_is_exact_and_deterministic)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    ASSERT_TRUE(make_reference(&f, &sk));
    const void *a = NULL;
    const void *b = NULL;
    size_t sa = 0;
    size_t sb = 0;
    ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &a, &sa));
    ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &b, &sb));
    ASSERT_EQ(sa, sb);
    EXPECT_EQ(memcmp(a, b, sa), 0);

    EngSkeleton back;
    ASSERT_TRUE(eng_skeleton_decode(&f.arena, a, sa, "test", &back));
    ASSERT_EQ(back.joint_count, sk.joint_count);
    for (uint32_t i = 0; i < sk.joint_count; i++) {
        EXPECT_EQ(back.joints[i].parent, sk.joints[i].parent);
        EXPECT_TRUE(same_bytes(back.joints[i].translation, sk.joints[i].translation, 12));
        EXPECT_TRUE(same_bytes(back.joints[i].rotation, sk.joints[i].rotation, 16));
        EXPECT_TRUE(same_bytes(back.joints[i].scale, sk.joints[i].scale, 12));
        EXPECT_TRUE(same_bytes(back.joints[i].inverse_bind, sk.joints[i].inverse_bind, 64));
        EXPECT_STREQ(back.joints[i].name, sk.joints[i].name);
    }
    fx_done(&f);
}

UTEST(skeleton, unnamed_joints_roundtrip_without_a_string_table)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngJoint j[2];
    memset(j, 0, sizeof j);
    for (int i = 0; i < 2; i++) {
        j[i].rotation[3] = 1.0F;
        j[i].scale[0] = j[i].scale[1] = j[i].scale[2] = 1.0F;
        j[i].inverse_bind[0] = j[i].inverse_bind[5] = j[i].inverse_bind[10] =
            j[i].inverse_bind[15] = 1.0F;
    }
    j[0].parent = -1;
    j[1].parent = 0;
    const EngSkeleton sk = {.joint_count = 2, .joints = j};
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
    EngSkeleton back;
    ASSERT_TRUE(eng_skeleton_decode(&f.arena, data, size, "anon", &back));
    EXPECT_TRUE(back.joints[0].name == NULL);
    EXPECT_TRUE(back.joints[1].name == NULL);
    EXPECT_EQ(back.joints[1].parent, 0);
    fx_done(&f);
}

static EngJoint good_joint(int32_t parent)
{
    EngJoint j;
    memset(&j, 0, sizeof j);
    j.parent = parent;
    j.rotation[3] = 1.0F;
    j.scale[0] = j.scale[1] = j.scale[2] = 1.0F;
    j.inverse_bind[0] = j.inverse_bind[5] = j.inverse_bind[10] = j.inverse_bind[15] = 1.0F;
    return j;
}

static bool encode_two(Fx *f, EngJoint a, EngJoint b)
{
    EngJoint j[2] = {a, b};
    const EngSkeleton sk = {.joint_count = 2, .joints = j};
    const void *data;
    size_t size;
    return eng_skeleton_encode(&f->arena, &sk, &data, &size);
}

UTEST(skeleton, encode_refuses_invalid_skeletons)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EXPECT_TRUE(encode_two(&f, good_joint(-1), good_joint(0)));
    EXPECT_FALSE(encode_two(&f, good_joint(0), good_joint(0)));  /* parent = soi-même */
    EXPECT_FALSE(encode_two(&f, good_joint(-1), good_joint(1))); /* parent = soi-même (indice 1) */
    EXPECT_FALSE(encode_two(&f, good_joint(1), good_joint(-1))); /* parent après l'enfant */
    EXPECT_FALSE(encode_two(&f, good_joint(-2), good_joint(0))); /* parent < -1 */

    EngJoint bad = good_joint(0);
    bad.rotation[3] = 0.5F; /* quaternion non unitaire */
    EXPECT_FALSE(encode_two(&f, good_joint(-1), bad));
    bad = good_joint(0);
    bad.translation[1] = NAN;
    EXPECT_FALSE(encode_two(&f, good_joint(-1), bad));
    bad = good_joint(0);
    bad.inverse_bind[15] = INFINITY;
    EXPECT_FALSE(encode_two(&f, good_joint(-1), bad));
    bad = good_joint(0);
    bad.scale[2] = 0.0F;
    EXPECT_FALSE(encode_two(&f, good_joint(-1), bad));
    char name[80];
    memset(name, 'a', sizeof name - 1);
    name[sizeof name - 1] = '\0';
    bad = good_joint(0);
    bad.name = name;
    EXPECT_FALSE(encode_two(&f, good_joint(-1), bad));
    name[ENG_SKELETON_MAX_NAME] = '\0'; /* 63 octets : accepté */
    EXPECT_TRUE(encode_two(&f, good_joint(-1), bad));

    const EngSkeleton empty = {0};
    const void *data;
    size_t size;
    EXPECT_FALSE(eng_skeleton_encode(&f.arena, &empty, &data, &size));
    EXPECT_FALSE(eng_skeleton_encode(NULL, &empty, &data, &size));
    fx_done(&f);
}

UTEST(skeleton, decode_rejects_corrupted_files)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    ASSERT_TRUE(make_reference(&f, &sk));
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
    uint8_t *copy = eng_arena_alloc(&f.arena, size, 16);
    ASSERT_TRUE(copy != NULL);
    EngSkeleton out;

    memcpy(copy, data, size);
    EXPECT_TRUE(eng_skeleton_decode(&f.arena, copy, size, "ok", &out));
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size - 1, "tronque", &out));
    EXPECT_EQ(out.joint_count, 0U);
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, 8, "court", &out));
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, NULL, 0, "vide", &out));

    memcpy(copy, data, size);
    copy[0] ^= 0xFFU; /* magie */
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "magie", &out));

    EngAssetView view;
    EngAssetSection jnts;
    EngAssetSection info;
    EngAssetSection strs;
    memcpy(copy, data, size);
    ASSERT_EQ(eng_asset_open(copy, size, ENG_ASSET_MAGIC_SKEL, ENG_ASSET_VERSION_SKEL, "x", &view),
              ENG_ASSET_OK);
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_JNTS, &jnts));
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_INFO, &info));
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_STRS, &strs));
    uint8_t *j_bytes = copy + (jnts.data - (const uint8_t *)copy);
    uint8_t *i_bytes = copy + (info.data - (const uint8_t *)copy);
    uint8_t *s_bytes = copy + (strs.data - (const uint8_t *)copy);

    /* parent de la 2e articulation = 5 (hors table) */
    const int32_t five = 5;
    memcpy(j_bytes + 48, &five, 4);
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "parent", &out));
    memcpy(copy, data, size);

    /* quaternion de la racine mis à zéro : rotation[0..3] à l'offset 8+4+12 = 24 */
    memset(j_bytes + 24, 0, 16);
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "quat", &out));
    memcpy(copy, data, size);

    /* décalage de nom hors de STRS */
    const uint32_t far = 100000;
    memcpy(j_bytes + 4, &far, 4);
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "nom", &out));
    memcpy(copy, data, size);

    /* STRS non terminée par zéro */
    s_bytes[strs.size - 1U] = 'x';
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "strs", &out));
    memcpy(copy, data, size);

    /* nombre d'articulations de INFO incohérent avec JNTS, puis nul, puis > 256 */
    const uint32_t counts[3] = {3, 0, 257};
    for (int i = 0; i < 3; i++) {
        memcpy(i_bytes, &counts[i], 4);
        EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "n", &out));
    }
    memcpy(copy, data, size);

    /* champ réservé non nul */
    i_bytes[4] = 1;
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "reserve", &out));
    memcpy(copy, data, size);
    EXPECT_TRUE(eng_skeleton_decode(&f.arena, copy, size, "retour", &out));
    fx_done(&f);
}

/* ── Dessin ── */

UTEST(skeleton, debug_draw_emits_one_bone_per_non_root_joint_in_world_space)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    ASSERT_TRUE(make_reference(&f, &sk));
    mat4 world[ENG_SKELETON_MAX_JOINTS];
    ASSERT_TRUE(eng_skeleton_rest_world(&sk, world));
    EngDebugLines lines;
    ASSERT_TRUE(eng_debug_lines_init(&lines, &f.arena, 256));
    mat4 root;
    glm_mat4_identity(root);
    EXPECT_EQ(eng_skeleton_debug_draw(&sk, world, root, &lines, ENG_DEBUG_RGB(255, 255, 0)), 3U);
    /* 4 croix de 3 segments + 3 os */
    EXPECT_EQ(eng_debug_lines_count(&lines), 15U);

    /* la matrice `root` s'applique aux deux extrémités : décalage de 100 m en X */
    eng_debug_lines_clear(&lines);
    glm_translate_make(root, (vec3){100.0F, 0.0F, 0.0F});
    EXPECT_EQ(eng_skeleton_debug_draw(&sk, world, root, &lines, ENG_DEBUG_RGB(255, 0, 0)), 3U);
    bool all_far = true;
    for (uint32_t i = 0; i < lines.depth_count; i++) {
        all_far = all_far && lines.depth[i].position[0] > 105.0F;
    }
    EXPECT_TRUE(all_far);

    EXPECT_EQ(eng_skeleton_debug_draw(NULL, world, root, &lines, ENG_DEBUG_RGB(0, 0, 0)), 0U);
    EXPECT_EQ(eng_skeleton_debug_draw(&sk, NULL, root, &lines, ENG_DEBUG_RGB(0, 0, 0)), 0U);
    fx_done(&f);
}

UTEST(skeleton, load_reads_a_skel_file_and_refuses_a_missing_one)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    ASSERT_TRUE(make_reference(&f, &sk));
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
    const char *path = "test_skeleton_tmp.skel";
    FILE *fp = fopen(path, "wb");
    ASSERT_TRUE(fp != NULL);
    ASSERT_EQ(fwrite(data, 1, size, fp), size);
    fclose(fp);
    EngSkeleton back;
    EXPECT_TRUE(eng_skeleton_load(&f.arena, &f.scratch, path, &back));
    EXPECT_EQ(back.joint_count, 4U);
    EXPECT_EQ(f.scratch.used, (size_t)0);
    remove(path);
    EXPECT_FALSE(eng_skeleton_load(&f.arena, &f.scratch, "n_existe_pas.skel", &back));
    EXPECT_EQ(back.joint_count, 0U);
    fx_done(&f);
}

/* ── Cas limites ajoutés après les mutations ── */

static void fill_unit(EngJoint *j, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        j[i] = good_joint(i == 0 ? -1 : (int32_t)i - 1);
    }
}

UTEST(skeleton, joint_count_limits_on_encode_and_decode)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static EngJoint joints[ENG_SKELETON_MAX_JOINTS + 1U];
    fill_unit(joints, ENG_SKELETON_MAX_JOINTS + 1U);
    const void *data = NULL;
    size_t size = 0;
    EngSkeleton out;

    EngSkeleton sk = {.joint_count = 0, .joints = joints}; /* zéro, pointeur non NULL */
    EXPECT_FALSE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
    sk.joint_count = ENG_SKELETON_MAX_JOINTS + 1U;
    EXPECT_FALSE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
    EXPECT_TRUE(data == NULL); /* sorties remises à zéro en cas d'échec */
    EXPECT_EQ(size, (size_t)0);
    EXPECT_FALSE(eng_skeleton_encode(&f.arena, &sk, NULL, &size));
    EXPECT_FALSE(eng_skeleton_encode(&f.arena, &sk, &data, NULL));

    for (uint32_t n = 1; n <= ENG_SKELETON_MAX_JOINTS; n += (n == 1U ? 254U : 1U)) {
        sk.joint_count = n; /* 1 puis 255, 256 */
        ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
        ASSERT_TRUE(eng_skeleton_decode(&f.arena, data, size, "n", &out));
        EXPECT_EQ(out.joint_count, n);
    }
    fx_done(&f);
}

UTEST(skeleton, any_unit_quaternion_is_accepted_whatever_its_non_zero_components)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float quats[][4] = {{0.6F, 0.8F, 0.0F, 0.0F},
                              {0.0F, 0.6F, 0.8F, 0.0F},
                              {0.5F, 0.5F, 0.5F, 0.5F},
                              {0.0F, 0.0F, 0.6F, 0.8F},
                              {-0.5F, 0.5F, -0.5F, 0.5F}};
    for (size_t i = 0; i < sizeof quats / sizeof quats[0]; i++) {
        EngJoint j = good_joint(0);
        memcpy(j.rotation, quats[i], sizeof j.rotation);
        EXPECT_TRUE(encode_two(&f, good_joint(-1), j));
    }
    EngJoint bad = good_joint(0);
    const float off[4] = {0.6F, 0.8F, 0.0F, 0.2F}; /* norme² = 1,04 */
    memcpy(bad.rotation, off, sizeof bad.rotation);
    EXPECT_FALSE(encode_two(&f, good_joint(-1), bad));
    fx_done(&f);
}

UTEST(skeleton, decode_rejects_a_name_offset_just_past_the_string_table)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    ASSERT_TRUE(make_reference(&f, &sk));
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
    uint8_t *copy = eng_arena_alloc(&f.arena, size, 16);
    ASSERT_TRUE(copy != NULL);
    memcpy(copy, data, size);
    EngAssetView view;
    EngAssetSection jnts;
    EngAssetSection strs;
    ASSERT_EQ(eng_asset_open(copy, size, ENG_ASSET_MAGIC_SKEL, ENG_ASSET_VERSION_SKEL, "x", &view),
              ENG_ASSET_OK);
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_JNTS, &jnts));
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_STRS, &strs));
    uint8_t *j_bytes = copy + (jnts.data - (const uint8_t *)copy);
    EngSkeleton out;
    uint32_t offset = strs.size - 1U; /* le dernier octet (le zéro) : nom vide, accepté */
    memcpy(j_bytes + 4, &offset, 4);
    EXPECT_TRUE(eng_skeleton_decode(&f.arena, copy, size, "dernier", &out));
    offset = strs.size; /* un octet après la fin */
    memcpy(j_bytes + 4, &offset, 4);
    EXPECT_FALSE(eng_skeleton_decode(&f.arena, copy, size, "apres", &out));
    fx_done(&f);
}

UTEST(skeleton, memory_exhaustion_fails_cleanly_and_restores_the_arena)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    ASSERT_TRUE(make_reference(&f, &sk));
    const void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_skeleton_encode(&f.arena, &sk, &data, &size));
    static _Alignas(16) uint8_t storage[8192];
    bool decode_failed_once = false;
    bool encode_failed_once = false;
    for (size_t cap = 0; cap <= 2048U; cap += 4U) {
        EngArena tiny;
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        EngSkeleton out;
        if (eng_skeleton_decode(&tiny, data, size, "tiny", &out)) {
            EXPECT_EQ(out.joint_count, 4U);
            EXPECT_STREQ(out.joints[3].name, "Arm");
        } else {
            decode_failed_once = true;
            EXPECT_EQ(tiny.used, (size_t)0);
            EXPECT_EQ(out.joint_count, 0U);
        }
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        const void *again = NULL;
        size_t again_size = 0;
        if (eng_skeleton_encode(&tiny, &sk, &again, &again_size)) {
            EXPECT_EQ(again_size, size);
            EXPECT_EQ(memcmp(again, data, size), 0);
        } else {
            encode_failed_once = true;
            EXPECT_EQ(tiny.used, (size_t)0);
        }
    }
    EXPECT_TRUE(decode_failed_once);
    EXPECT_TRUE(encode_failed_once);

    /* import : arène de sortie, puis arène de travail, trop petites */
    float ibm[4][16];
    reference_ibms(ibm);
    static uint8_t glb[GLB_MAX];
    const size_t glb_size = reference_glb(glb, true, &ibm[0][0]);
    bool import_failed_once = false;
    for (size_t cap = 0; cap <= 1024U; cap += 4U) {
        EngArena tiny;
        EngSkeleton out;
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        if (eng_skeleton_from_gltf(&tiny, &f.scratch, glb, glb_size, NULL, 0, &out, NULL)) {
            EXPECT_EQ(out.joint_count, 4U);
        } else {
            import_failed_once = true;
            EXPECT_EQ(tiny.used, (size_t)0);
        }
        eng_arena_init_buffer(&tiny, "tiny", storage, cap);
        if (eng_skeleton_from_gltf(&f.arena, &tiny, glb, glb_size, NULL, 0, &out, NULL)) {
            EXPECT_EQ(out.joint_count, 4U);
        } else {
            import_failed_once = true;
            EXPECT_EQ(tiny.used, (size_t)0);
        }
    }
    EXPECT_TRUE(import_failed_once);
    fx_done(&f);
}

UTEST(skeleton, gltf_input_checks_and_skin_count_on_degenerate_data)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, false, NULL);
    EngSkeleton sk;
    EXPECT_EQ(eng_skeleton_gltf_skin_count(NULL, 10), 0U);
    EXPECT_EQ(eng_skeleton_gltf_skin_count(glb, 0), 0U);
    EXPECT_FALSE(eng_skeleton_from_gltf(&f.arena, &f.scratch, NULL, 10, NULL, 0, &sk, NULL));
    EXPECT_FALSE(eng_skeleton_from_gltf(&f.arena, &f.scratch, glb, 0, NULL, 0, &sk, NULL));
    EXPECT_FALSE(eng_skeleton_from_gltf(&f.arena, NULL, glb, size, NULL, 0, &sk, NULL));
    EXPECT_EQ(eng_skeleton_gltf_skin_count(glb, size), 1U);
    fx_done(&f);
}

UTEST(skeleton, a_skin_already_in_topological_order_keeps_it_and_joint_zero_may_have_children)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    const char *chain = "{\"name\":\"Armature\",\"children\":[1]},"
                        "{\"name\":\"Hips\",\"translation\":[0,1,0],\"children\":[2]},"
                        "{\"name\":\"Spine\",\"translation\":[0,1,0],\"children\":[3]},"
                        "{\"name\":\"Head\",\"translation\":[0,1,0]}";
    EngSkeleton sk;
    uint32_t *remap = NULL;

    GlbSpec ordered = {.nodes_json = chain, .skin_joints = "1,2,3"};
    size_t size = build_glb(glb, GLB_MAX, &ordered);
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, &remap));
    EXPECT_EQ(remap[0], 0U);
    EXPECT_EQ(remap[1], 1U);
    EXPECT_EQ(remap[2], 2U);
    EXPECT_EQ(sk.joints[1].parent, 0);
    EXPECT_EQ(sk.joints[2].parent, 1);

    /* Spine en indice 0 (avec un enfant), son parent Hips après lui, puis Head */
    GlbSpec shuffled = {.nodes_json = chain, .skin_joints = "2,1,3"};
    size = build_glb(glb, GLB_MAX, &shuffled);
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, &remap));
    EXPECT_EQ(remap[0], 1U); /* Spine -> 1 */
    EXPECT_EQ(remap[1], 0U); /* Hips -> 0 */
    EXPECT_EQ(remap[2], 2U); /* Head -> 2 */
    EXPECT_STREQ(sk.joints[0].name, "Hips");
    EXPECT_EQ(sk.joints[1].parent, 0);
    EXPECT_EQ(sk.joints[2].parent, 1);

    /* Head listée avant Spine : le parent d'un nœud non articulation intermédiaire reste juste */
    GlbSpec reversed = {.nodes_json = chain, .skin_joints = "3,2,1"};
    size = build_glb(glb, GLB_MAX, &reversed);
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, &remap));
    EXPECT_EQ(remap[0], 2U);
    EXPECT_EQ(remap[2], 0U);
    EXPECT_STREQ(sk.joints[2].name, "Head");
    fx_done(&f);
}

UTEST(skeleton, joint_names_of_63_bytes_are_kept_and_64_bytes_refused)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static uint8_t glb[GLB_MAX];
    char nodes[256];
    EngSkeleton sk;
    snprintf(nodes, sizeof nodes, "{\"children\":[1]},{\"name\":\"%063d\"}", 0);
    GlbSpec ok = {.nodes_json = nodes, .skin_joints = "1"};
    size_t size = build_glb(glb, GLB_MAX, &ok);
    ASSERT_TRUE(import(&f, glb, size, 0, &sk, NULL));
    EXPECT_EQ(strlen(sk.joints[0].name), (size_t)ENG_SKELETON_MAX_NAME);
    snprintf(nodes, sizeof nodes, "{\"children\":[1]},{\"name\":\"%064d\"}", 0);
    size = build_glb(glb, GLB_MAX, &ok);
    EXPECT_FALSE(import(&f, glb, size, 0, &sk, NULL));
    fx_done(&f);
}

UTEST(skeleton, pose_functions_refuse_null_outputs_and_invalid_skeletons)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngSkeleton sk;
    ASSERT_TRUE(make_reference(&f, &sk));
    mat4 out[ENG_SKELETON_MAX_JOINTS];
    EXPECT_FALSE(eng_skeleton_rest_world(&sk, NULL));
    EXPECT_FALSE(eng_skeleton_bind_world(&sk, NULL));
    const EngSkeleton empty = {0};
    EXPECT_FALSE(eng_skeleton_rest_world(&empty, out));
    EXPECT_FALSE(eng_skeleton_bind_world(&empty, out));
    EXPECT_LT(eng_skeleton_bind_mismatch(&empty), 0.0F);
    EXPECT_LT(eng_skeleton_bind_mismatch(NULL), 0.0F);
    fx_done(&f);
}

/* ── Outil en ligne de commande ── */

static int run_assetc(const char *a, const char *b, const char *c, const char *d, const char *e)
{
    const char *const argv[] = {OW_ASSETC_PATH, a, b, c, d, e, NULL};
    SDL_Process *process = SDL_CreateProcess(argv, false);
    if (process == NULL) {
        return -1;
    }
    int code = -1;
    const bool waited = SDL_WaitProcess(process, true, &code);
    SDL_DestroyProcess(process);
    return waited ? code : -1;
}

UTEST(skeleton, assetc_skel_cooks_a_gltf_into_a_skel_and_refuses_bad_requests)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    float ibm[4][16];
    reference_ibms(ibm);
    static uint8_t glb[GLB_MAX];
    const size_t size = reference_glb(glb, true, &ibm[0][0]);
    ASSERT_TRUE(size > 0);
    const char *in = "test_skeleton_cli.glb";
    const char *out = "skeleton_demo.skel"; /* gardé : lu par le test launcher.skeleton */
    const char *dep = "test_skeleton_cli.d";
    FILE *fp = fopen(in, "wb");
    ASSERT_TRUE(fp != NULL);
    ASSERT_EQ(fwrite(glb, 1, size, fp), size);
    fclose(fp);
    remove(out);
    remove(dep);

    EXPECT_EQ(run_assetc("skel", in, out, "--depfile", dep), 0);
    EngSkeleton sk;
    ASSERT_TRUE(eng_skeleton_load(&f.arena, &f.scratch, out, &sk));
    EXPECT_EQ(sk.joint_count, 4U);
    EXPECT_STREQ(sk.joints[0].name, "Hips");
    EXPECT_EQ(sk.joints[2].parent, 1);
    FILE *d = fopen(dep, "rb");
    EXPECT_TRUE(d != NULL);
    if (d != NULL) {
        fclose(d);
    }

    EXPECT_EQ(run_assetc("skel", in, out, "--skin", "1"), 1); /* un seul skin */
    EXPECT_EQ(run_assetc("skel", in, "mauvaise_extension.mesh", NULL, NULL), 2);
    EXPECT_EQ(run_assetc("skel", "absent.glb", out, NULL, NULL), 1);
    EXPECT_EQ(run_assetc("skel", in, out, "--skin", "x"), 2);
    remove(in);
    remove(dep);
    fx_done(&f);
}
