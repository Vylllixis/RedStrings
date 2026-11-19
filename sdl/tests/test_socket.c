/* Sockets (M4-22) : points d'attache sur les articulations. Valeurs calculées à la main. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/debug_lines.h"
#include "engine/pose.h"
#include "engine/socket.h"
#include "utest.h"

/* Armature (échelle 0,01, comme un import Mixamo en cm) > Hips (0, 100, 0) cm > Hand (30, 0, 0) cm
 * tournée de 90° autour de Z (son axe x pointe vers +Y du personnage). */
#define H 0.70710678F

static EngJoint g_joints[3] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {0.01F, 0.01F, 0.01F}, {0}, "Armature"},
    {0, {0, 100, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "Hips"},
    {1, {30, 0, 0}, {0, 0, H, H}, {1, 1, 1}, {0}, "Hand"},
};

typedef struct Fx {
    EngSkeleton sk;
    EngArena arena;
    EngPose pose;
    mat4 world[3];
    EngSocketSet set;
} Fx;

static bool fx_init(Fx *f)
{
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    if (!eng_arena_init(&f->arena, "socket", (size_t)1 << 14) ||
        !eng_pose_init(&f->arena, &f->sk, &f->pose) ||
        !eng_pose_world(&f->pose, &f->sk, f->world)) {
        return false;
    }
    eng_socketset_init(&f->set);
    return true;
}

static void identity(mat4 m)
{
    glm_mat4_identity(m);
}

static bool near3(const float *a, float x, float y, float z, float tol)
{
    return fabsf(a[0] - x) <= tol && fabsf(a[1] - y) <= tol && fabsf(a[2] - z) <= tol;
}

#define EXPECT_COL(m, col, x, y, z) EXPECT_TRUE(near3((m)[col], x, y, z, 1e-5F))

UTEST(socket, an_attached_object_keeps_its_size_despite_the_scaled_armature)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    /* le monde brut de la main porte bien l'échelle 0,01 */
    EXPECT_NEAR(glm_vec3_norm(f.world[2][0]), 0.01F, 1e-6F);
    const float t[3] = {0.0F, 0.1F, 0.0F};
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "main", "Hand", t, NULL, 1.0F));
    mat4 model;
    identity(model);
    mat4 out;
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[0], f.world, 3, model, out));
    /* main = (0,3 ; 1 ; 0), axes tournés de 90° autour de Z : x -> +Y, y -> -X */
    EXPECT_COL(out, 0, 0, 1, 0);
    EXPECT_COL(out, 1, -1, 0, 0);
    EXPECT_COL(out, 2, 0, 0, 1);
    /* décalage de 0,1 m selon le y local = -X : (0,3 - 0,1 ; 1 ; 0) */
    EXPECT_COL(out, 3, 0.2F, 1.0F, 0.0F);
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, the_character_model_places_the_socket_in_the_world)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "main", "Hand", NULL, NULL, 1.0F));
    mat4 model;
    identity(model);
    glm_translate(model, (vec3){10.0F, 0.0F, 0.0F});
    glm_rotate_y(model, GLM_PI_2f, model); /* le personnage regarde +X : x -> -Z, z -> +X */
    mat4 out;
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[0], f.world, 3, model, out));
    /* position locale (0,3 ; 1 ; 0) -> (10 + 0, 1, -0,3) après rotation Y de 90° (x -> -Z) */
    EXPECT_COL(out, 3, 10.0F, 1.0F, -0.3F);
    EXPECT_NEAR(glm_vec3_norm(out[0]), 1.0F, 1e-5F);
    /* sortie confondue avec le modèle : même résultat */
    mat4 same;
    glm_mat4_copy(model, same);
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[0], f.world, 3, same, same));
    EXPECT_TRUE(near3(same[3], 10.0F, 1.0F, -0.3F, 1e-5F));
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, euler_angles_apply_x_then_y_then_z_about_fixed_axes)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    /* racine : cadre identique au monde (échelle retirée) */
    const float euler[3] = {90.0F, 90.0F, 0.0F};
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "a", "Armature", NULL, euler, 1.0F));
    mat4 model;
    identity(model);
    mat4 out;
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[0], f.world, 3, model, out));
    /* X 90° : y -> z, z -> -y ; puis Y 90° : z -> x, x -> -z. x local : X(x)=x, Y(x)=-z */
    EXPECT_COL(out, 0, 0, 0, -1);
    EXPECT_COL(out, 1, 1, 0, 0);  /* y -> z -> x */
    EXPECT_COL(out, 2, 0, -1, 0); /* z -> -y -> -y (Y ne change pas y) */
    const float z_only[3] = {0.0F, 0.0F, 90.0F};
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "b", "Armature", NULL, z_only, 1.0F));
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[1], f.world, 3, model, out));
    EXPECT_COL(out, 0, 0, 1, 0);
    EXPECT_COL(out, 1, -1, 0, 0);
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, own_scale_applies_to_the_attached_object_only)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "grand", "Hand", NULL, NULL, 2.5F));
    mat4 model;
    identity(model);
    mat4 out;
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[0], f.world, 3, model, out));
    EXPECT_NEAR(glm_vec3_norm(out[0]), 2.5F, 1e-5F);
    EXPECT_NEAR(glm_vec3_norm(out[1]), 2.5F, 1e-5F);
    EXPECT_NEAR(glm_vec3_norm(out[2]), 2.5F, 1e-5F);
    EXPECT_COL(out, 3, 0.3F, 1.0F, 0.0F); /* la translation ne dépend pas de l'échelle propre */
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, the_socket_follows_the_animated_pose)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "main", "Hand", NULL, NULL, 1.0F));
    mat4 model;
    identity(model);
    mat4 before;
    mat4 after;
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[0], f.world, 3, model, before));
    /* la main monte de 20 cm (dans le repère de Hips, en cm) : 0,2 m dans le monde */
    f.pose.translation[(3U * 2U) + 1U] += 20.0F;
    ASSERT_TRUE(eng_pose_world(&f.pose, &f.sk, f.world));
    ASSERT_TRUE(eng_socket_world(&f.set.sockets[0], f.world, 3, model, after));
    EXPECT_NEAR(after[3][1] - before[3][1], 0.2F, 1e-5F);
    EXPECT_NEAR(after[3][0], before[3][0], 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, world_matrix_refuses_bad_input_and_leaves_the_output_alone)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "main", "Hand", NULL, NULL, 1.0F));
    const EngSocket *s = &f.set.sockets[0];
    mat4 model;
    identity(model);
    mat4 out;
    glm_mat4_copy(model, out);
    out[3][0] = 42.0F;
    EXPECT_FALSE(eng_socket_world(NULL, f.world, 3, model, out));
    EXPECT_FALSE(eng_socket_world(s, NULL, 3, model, out));
    EXPECT_FALSE(eng_socket_world(s, f.world, 3, NULL, out));
    EXPECT_FALSE(eng_socket_world(s, f.world, 3, model, NULL));
    EXPECT_FALSE(eng_socket_world(s, f.world, 2, model, out)); /* joint 2 hors de [0 ; 2[ */
    mat4 bad[3];
    memcpy(bad, f.world, sizeof bad);
    bad[2][1][1] = NAN;
    EXPECT_FALSE(eng_socket_world(s, bad, 3, model, out));
    memcpy(bad, f.world, sizeof bad);
    memset(bad[2][2], 0, sizeof bad[2][2]); /* axe z de longueur nulle */
    EXPECT_FALSE(eng_socket_world(s, bad, 3, model, out));
    model[2][2] = INFINITY;
    EXPECT_FALSE(eng_socket_world(s, f.world, 3, model, out));
    EXPECT_NEAR(out[3][0], 42.0F, 0.0F);
    /* limite : joint_count == joint + 1 passe */
    identity(model);
    EXPECT_TRUE(eng_socket_world(s, f.world, 3, model, out));
    eng_arena_shutdown(&f.arena);
}

static bool add_refused(Fx *f, const char *name, const char *joint, float scale)
{
    const uint32_t before = f->set.count;
    return !eng_socketset_add(&f->set, &f->sk, name, joint, NULL, NULL, scale) &&
           f->set.count == before;
}

UTEST(socket, add_refuses_invalid_sockets)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EXPECT_TRUE(add_refused(&f, "", "Hand", 1.0F));
    EXPECT_TRUE(add_refused(&f, "mauvais nom", "Hand", 1.0F));
    EXPECT_TRUE(add_refused(&f, "a-b", "Hand", 1.0F));
    EXPECT_TRUE(add_refused(&f, "accentué", "Hand", 1.0F));
    EXPECT_TRUE(add_refused(&f, NULL, "Hand", 1.0F));
    EXPECT_TRUE(add_refused(&f, "ok", "Inconnu", 1.0F));
    EXPECT_TRUE(add_refused(&f, "ok", "hand", 1.0F)); /* comparaison exacte */
    EXPECT_TRUE(add_refused(&f, "ok", NULL, 1.0F));
    EXPECT_TRUE(add_refused(&f, "ok", "Hand", 0.0F));
    EXPECT_TRUE(add_refused(&f, "ok", "Hand", -1.0F));
    EXPECT_TRUE(add_refused(&f, "ok", "Hand", NAN));
    EXPECT_TRUE(add_refused(&f, "ok", "Hand", INFINITY));
    const float bad[3] = {0.0F, NAN, 0.0F};
    EXPECT_FALSE(eng_socketset_add(&f.set, &f.sk, "ok", "Hand", bad, NULL, 1.0F));
    EXPECT_FALSE(eng_socketset_add(&f.set, &f.sk, "ok", "Hand", NULL, bad, 1.0F));
    EXPECT_FALSE(eng_socketset_add(NULL, &f.sk, "ok", "Hand", NULL, NULL, 1.0F));
    EXPECT_FALSE(eng_socketset_add(&f.set, NULL, "ok", "Hand", NULL, NULL, 1.0F));
    /* nom de 31 caractères accepté, de 32 refusé */
    char longest[40];
    memset(longest, 'a', 31);
    longest[31] = '\0';
    EXPECT_TRUE(eng_socketset_add(&f.set, &f.sk, longest, "Hand", NULL, NULL, 1.0F));
    longest[31] = 'a';
    longest[32] = '\0';
    EXPECT_TRUE(add_refused(&f, longest, "Hand", 1.0F));
    /* doublon */
    EXPECT_TRUE(add_refused(&f, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "Hips", 1.0F));
    EXPECT_EQ(eng_socketset_find(&f.set, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 0U);
    EXPECT_EQ(eng_socketset_find(&f.set, "absent"), UINT32_MAX);
    EXPECT_EQ(eng_socketset_find(NULL, "x"), UINT32_MAX);
    EXPECT_EQ(eng_socketset_find(&f.set, NULL), UINT32_MAX);
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, a_set_holds_sixteen_sockets)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    for (uint32_t i = 0; i < ENG_SOCKET_MAX; i++) {
        char name[16];
        snprintf(name, sizeof name, "s%u", (unsigned)i);
        ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, name, "Hips", NULL, NULL, 1.0F));
    }
    EXPECT_EQ(f.set.count, ENG_SOCKET_MAX);
    EXPECT_TRUE(add_refused(&f, "de_trop", "Hips", 1.0F));
    EXPECT_EQ(eng_socketset_find(&f.set, "s15"), 15U);
    eng_arena_shutdown(&f.arena);
}

static bool same_sockets(const EngSocketSet *a, const EngSocketSet *b)
{
    if (a->count != b->count) {
        return false;
    }
    for (uint32_t i = 0; i < a->count; i++) {
        if (strcmp(a->sockets[i].name, b->sockets[i].name) != 0 ||
            a->sockets[i].joint != b->sockets[i].joint) {
            return false;
        }
    }
    return true;
}

static bool parse_refused_with(Fx *f, const char *text, const char *expected)
{
    char err[160];
    const EngSocketSet before = f->set;
    const bool refused = !eng_socketset_parse(text, &f->sk, &f->set, err, sizeof err);
    return refused && strstr(err, expected) != NULL && same_sockets(&before, &f->set);
}

UTEST(socket, a_text_file_defines_sockets)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    char err[160];
    const char *text = "# armes\n"
                       "socket main Hand 0 0.1 0   # dans la paume\n"
                       "\n"
                       "socket dos Hips 0 0.2 -0.1 0 180 0 0.5\n"
                       "socket tete Hips 0 0 0 10 20 30\n";
    ASSERT_TRUE(eng_socketset_parse(text, &f.sk, &f.set, err, sizeof err));
    ASSERT_EQ(f.set.count, 3U);
    EXPECT_STREQ(f.set.sockets[0].name, "main");
    EXPECT_EQ(f.set.sockets[0].joint, 2U);
    EXPECT_NEAR(f.set.sockets[0].translation[1], 0.1F, 1e-6F);
    EXPECT_NEAR(f.set.sockets[0].scale, 1.0F, 0.0F);
    EXPECT_NEAR(f.set.sockets[1].scale, 0.5F, 0.0F);
    EXPECT_NEAR(f.set.sockets[1].rotation[1], 1.0F, 1e-6F); /* 180° autour de Y */
    EXPECT_EQ(f.set.sockets[2].joint, 1U);
    /* dernière ligne sans retour à la ligne, fins de ligne Windows */
    ASSERT_TRUE(eng_socketset_parse("socket a Hand 1 2 3\r\nsocket b Hips 0 0 0", &f.sk, &f.set,
                                    err, sizeof err));
    EXPECT_EQ(f.set.count, 2U);
    EXPECT_NEAR(f.set.sockets[0].translation[2], 3.0F, 0.0F);
    /* texte vide : ensemble vide */
    ASSERT_TRUE(eng_socketset_parse("# rien\n", &f.sk, &f.set, err, sizeof err));
    EXPECT_EQ(f.set.count, 0U);
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, a_bad_text_file_is_refused_with_its_line_and_keeps_the_previous_set)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "garde", "Hand", NULL, NULL, 1.0F));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0 0\nbidule x\n", "ligne 2 : instruction"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0\n", "ligne 1 : socket NOM"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0 0 1\n", "ligne 1 : socket NOM"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0 0 1 2\n", "ligne 1 : socket NOM"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0 0 1 2 3 4 5\n", "ligne 1 : socket NOM"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 x 0\n", "ligne 1 : nombre invalide"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 nan 0\n", "ligne 1 : nombre invalide"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0 0 0 0 0 abc\n", "nombre invalide"));
    EXPECT_TRUE(
        parse_refused_with(&f, "\n\nsocket a Nulle 0 0 0\n", "ligne 3 : socket refusé : a"));
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0 0\nsocket a Hips 0 0 0\n", "ligne 2"));
    EXPECT_TRUE(
        parse_refused_with(&f, "socket a Hand 0 0 0 0 0 0 -1\n", "ligne 1 : socket refusé"));
    EXPECT_EQ(f.set.count, 1U);
    EXPECT_STREQ(f.set.sockets[0].name, "garde");
    /* trop de jetons, ligne trop longue */
    EXPECT_TRUE(parse_refused_with(&f, "socket a Hand 0 0 0 0 0 0 1 2 3 4 5 6 7\n", "ligne 1"));
    char big[400];
    memset(big, 'x', sizeof big - 1U);
    big[sizeof big - 1U] = '\0';
    EXPECT_TRUE(parse_refused_with(&f, big, "ligne 1 : ligne trop longue"));
    /* arguments NULL */
    char err[64];
    EXPECT_FALSE(eng_socketset_parse(NULL, &f.sk, &f.set, err, sizeof err));
    EXPECT_FALSE(eng_socketset_parse("", NULL, &f.set, err, sizeof err));
    EXPECT_FALSE(eng_socketset_parse("", &f.sk, NULL, err, sizeof err));
    /* message tronqué proprement, sans tampon d'erreur */
    char tiny[8];
    EXPECT_FALSE(eng_socketset_parse("bidule\n", &f.sk, &f.set, tiny, sizeof tiny));
    EXPECT_EQ(strlen(tiny) < sizeof tiny ? 1 : 0, 1);
    EXPECT_FALSE(eng_socketset_parse("bidule\n", &f.sk, &f.set, NULL, 0));
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, names_accept_the_edge_characters_of_each_range)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    static const char *const names[] = {"a", "z", "A", "Z", "0", "9", "_", "aZ09_"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        EXPECT_TRUE(eng_socketset_add(&f.set, &f.sk, names[i], "Hips", NULL, NULL, 1.0F));
    }
    static const char *const bad[] = {"`", "{", "@",
                                      "[", "/", ":"}; /* juste hors de chaque plage */
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EXPECT_TRUE(add_refused(&f, bad[i], "Hips", 1.0F));
    }
    eng_arena_shutdown(&f.arena);
}

UTEST(socket, debug_draw_draws_one_frame_per_socket_in_the_overlay_list)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "lines", (size_t)1 << 16));
    EngDebugLines lines;
    ASSERT_TRUE(eng_debug_lines_init(&lines, &arena, 256));
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "a", "Hand", NULL, NULL, 1.0F));
    ASSERT_TRUE(eng_socketset_add(&f.set, &f.sk, "b", "Hips", NULL, NULL, 1.0F));
    mat4 model;
    identity(model);
    EXPECT_EQ(eng_socketset_debug_draw(&f.set, f.world, 3, model, &lines, 0.1F), 2U);
    EXPECT_EQ(eng_debug_lines_overlay_count(&lines), 6U); /* 3 axes par repère */
    EXPECT_EQ(eng_debug_lines_depth_count(&lines), 0U);
    EXPECT_FALSE(lines.overlay_mode); /* mode rétabli */
    lines.overlay_mode = true;
    (void)eng_socketset_debug_draw(&f.set, f.world, 3, model, &lines, 0.1F);
    EXPECT_TRUE(lines.overlay_mode);
    /* un socket dégénéré est sauté, les autres sont dessinés */
    eng_debug_lines_clear(&lines);
    lines.overlay_mode = false;
    mat4 bad[3];
    memcpy(bad, f.world, sizeof bad);
    memset(bad[2][2], 0, sizeof bad[2][2]);
    EXPECT_EQ(eng_socketset_debug_draw(&f.set, bad, 3, model, &lines, 0.1F), 1U);
    EXPECT_EQ(eng_socketset_debug_draw(NULL, f.world, 3, model, &lines, 0.1F), 0U);
    EXPECT_EQ(eng_socketset_debug_draw(&f.set, NULL, 3, model, &lines, 0.1F), 0U);
    EXPECT_EQ(eng_socketset_debug_draw(&f.set, f.world, 3, NULL, &lines, 0.1F), 0U);
    EXPECT_EQ(eng_socketset_debug_draw(&f.set, f.world, 3, model, NULL, 0.1F), 0U);
    eng_arena_shutdown(&arena);
    eng_arena_shutdown(&f.arena);
}
