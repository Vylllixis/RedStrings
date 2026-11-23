/* Hurtboxes (M5-05) : capsules attachées aux os, suivant la pose. Valeurs calculées à la main avec
 * le squelette de test des sockets (Armature à l'échelle 0,01 comme un import Mixamo). */
#include <math.h>
#include <string.h>

#include "engine/debug_lines.h"
#include "engine/hurtbox.h"
#include "engine/pose.h"
#include "utest.h"

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
    EngHurtboxSet set;
} Fx;

static bool fx_init(Fx *f)
{
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    if (!eng_arena_init(&f->arena, "hurtbox", (size_t)1 << 16) ||
        !eng_pose_init(&f->arena, &f->sk, &f->pose) ||
        !eng_pose_world(&f->pose, &f->sk, f->world)) {
        return false;
    }
    eng_hurtboxset_init(&f->set);
    return true;
}

static void model_identity(mat4 m)
{
    glm_mat4_identity(m);
}

UTEST(hurtbox, a_capsule_on_the_hand_has_the_right_place_and_keeps_its_size_despite_scaled_armature)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float a[3] = {0, 0, 0};
    const float b[3] = {0.4F, 0, 0}; /* le long du x local de la main = +Y monde */
    ASSERT_TRUE(eng_hurtboxset_add(&f.set, &f.sk, "main", ENG_ZONE_LIMB, "Hand", a, b, 0.07F));
    mat4 model;
    model_identity(model);
    EngCapsule c;
    ASSERT_TRUE(eng_hurtboxset_world(&f.set, f.world, 3, model, &c));
    EXPECT_NEAR(c.a[0], 0.3F, 1e-5F);
    EXPECT_NEAR(c.a[1], 1.0F, 1e-5F);
    EXPECT_NEAR(c.b[0], 0.3F, 1e-5F);
    EXPECT_NEAR(c.b[1], 1.4F, 1e-5F);
    EXPECT_NEAR(c.radius, 0.07F, 1e-6F); /* pas 0,0007 : l'échelle de l'Armature est retirée */
    eng_arena_shutdown(&f.arena);
}

UTEST(hurtbox, the_capsule_follows_the_animation_every_tick)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float a[3] = {0, 0, 0};
    const float b[3] = {0.4F, 0, 0};
    ASSERT_TRUE(eng_hurtboxset_add(&f.set, &f.sk, "main", ENG_ZONE_LIMB, "Hand", a, b, 0.07F));
    mat4 model;
    model_identity(model);
    EngCapsule c;
    /* la main tourne : rotation identité -> son x local = +X monde */
    f.pose.rotation[2 * 4 + 0] = 0.0F;
    f.pose.rotation[2 * 4 + 1] = 0.0F;
    f.pose.rotation[2 * 4 + 2] = 0.0F;
    f.pose.rotation[2 * 4 + 3] = 1.0F;
    ASSERT_TRUE(eng_pose_world(&f.pose, &f.sk, f.world));
    ASSERT_TRUE(eng_hurtboxset_world(&f.set, f.world, 3, model, &c));
    EXPECT_NEAR(c.b[0], 0.7F, 1e-5F);
    EXPECT_NEAR(c.b[1], 1.0F, 1e-5F);
    /* le bassin monte de 1 m (translation locale en cm sous l'Armature) */
    f.pose.translation[1 * 3 + 1] = 200.0F;
    ASSERT_TRUE(eng_pose_world(&f.pose, &f.sk, f.world));
    ASSERT_TRUE(eng_hurtboxset_world(&f.set, f.world, 3, model, &c));
    EXPECT_NEAR(c.a[1], 2.0F, 1e-5F);
    EXPECT_NEAR(c.b[1], 2.0F, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(hurtbox, the_model_placement_moves_scales_and_rotates_the_capsule)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float a[3] = {0, 0, 0};
    const float b[3] = {0.4F, 0, 0};
    ASSERT_TRUE(eng_hurtboxset_add(&f.set, &f.sk, "main", ENG_ZONE_LIMB, "Hand", a, b, 0.07F));
    mat4 model;
    model_identity(model);
    glm_translate(model, (vec3){10.0F, 0.0F, 0.0F});
    glm_scale_uni(model, 2.0F);
    EngCapsule c;
    ASSERT_TRUE(eng_hurtboxset_world(&f.set, f.world, 3, model, &c));
    EXPECT_NEAR(c.a[0], 10.6F, 1e-4F); /* (0,3 ; 1 ; 0) x 2 + (10 ; 0 ; 0) */
    EXPECT_NEAR(c.a[1], 2.0F, 1e-4F);
    EXPECT_NEAR(c.b[1], 2.8F, 1e-4F); /* longueur 0,4 -> 0,8 */
    EXPECT_NEAR(c.radius, 0.14F, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(hurtbox, invalid_adds_are_refused_and_leave_the_set_unchanged)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float a[3] = {0, 0, 0};
    const float b[3] = {0, 1, 0};
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Nope", a, b, 0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "", ENG_ZONE_HEAD, "Hips", a, b, 0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "a b", ENG_ZONE_HEAD, "Hips", a, b, 0.1F));
    EXPECT_FALSE(
        eng_hurtboxset_add(&f.set, &f.sk, "x", (EngHitZone)ENG_ZONE_COUNT, "Hips", a, b, 0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Hips", a, b, 0.0F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Hips", a, b, -0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Hips", a, b, 10.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Hips", a, b, NAN));
    const float nan3[3] = {NAN, 0, 0};
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Hips", nan3, b, 0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Hips", a, nan3, 0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(NULL, &f.sk, "x", ENG_ZONE_HEAD, "Hips", a, b, 0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, NULL, "x", ENG_ZONE_HEAD, "Hips", a, b, 0.1F));
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, NULL, a, b, 0.1F));
    EXPECT_EQ(f.set.count, 0U);
    EXPECT_TRUE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_HEAD, "Hips", a, b, 10.0F));
    EXPECT_FALSE(
        eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_TORSO, "Hips", a, b, 0.1F)); /* nom pris */
    EXPECT_EQ(f.set.count, 1U);
    EXPECT_EQ(eng_hurtboxset_find(&f.set, "x"), 0U);
    EXPECT_EQ(eng_hurtboxset_find(&f.set, "y"), UINT32_MAX);
    EXPECT_EQ(eng_hurtboxset_find(NULL, "x"), UINT32_MAX);
    /* ensemble plein : 16 au plus */
    for (int i = 1; i < 16; i++) {
        char n[8];
        (void)snprintf(n, sizeof n, "h%d", i);
        EXPECT_TRUE(eng_hurtboxset_add(&f.set, &f.sk, n, ENG_ZONE_LIMB, "Hips", a, b, 0.1F));
    }
    EXPECT_FALSE(eng_hurtboxset_add(&f.set, &f.sk, "h99", ENG_ZONE_LIMB, "Hips", a, b, 0.1F));
    eng_arena_shutdown(&f.arena);
}

UTEST(hurtbox, a_text_file_defines_zones_and_refuses_every_error_with_its_line)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    char err[200];
    const char *good = "# personnage\n"
                       "hurtbox head head Hips 0 0.1 0  0 0.3 0  0.11   # tête\n"
                       "hurtbox torso torso Hips 0 0 0  0 0.35 0  0.18\n"
                       "hurtbox arm limb Hand 0 0 0  0.28 0 0  0.07\n";
    ASSERT_TRUE(eng_hurtboxset_parse(good, &f.sk, &f.set, err, sizeof err));
    ASSERT_EQ(f.set.count, 3U);
    EXPECT_EQ(f.set.boxes[0].zone, ENG_ZONE_HEAD);
    EXPECT_EQ(f.set.boxes[1].zone, ENG_ZONE_TORSO);
    EXPECT_EQ(f.set.boxes[2].zone, ENG_ZONE_LIMB);
    EXPECT_EQ(f.set.boxes[2].joint, 2U);
    EXPECT_EQ(f.set.boxes[2].b[0], 0.28F);
    EXPECT_EQ(f.set.boxes[2].radius, 0.07F);

    const EngHurtboxSet before = f.set;
    const struct {
        const char *text;
        const char *expect;
    } bad[] = {
        {"coucou\n", "ligne 1 : instruction inconnue : coucou"},
        {"hurtbox a head Hips 0 0 0 0 1 0\n", "hurtbox NOM ZONE ARTICULATION"},
        {"hurtbox a head Hips 0 0 0 0 1 0 0.1 9\n", "hurtbox NOM ZONE ARTICULATION"},
        {"hurtbox a arm Hips 0 0 0 0 1 0 0.1\n", "zone inconnue (head, torso ou limb) : arm"},
        {"hurtbox a head Hips 0 0 x 0 1 0 0.1\n", "nombre invalide"},
        {"hurtbox a head Hips 0 0 0 0 1 0 nan\n", "nombre invalide"},
        {"hurtbox a head Nope 0 0 0 0 1 0 0.1\n", "hurtbox refusée"},
        {"hurtbox a head Hips 0 0 0 0 1 0 0\n", "hurtbox refusée"},
        {"hurtbox a head Hips 0 0 0 0 1 0 0.1\nhurtbox a limb Hips 0 0 0 0 1 0 0.1\n", "ligne 2 :"},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EXPECT_FALSE(eng_hurtboxset_parse(bad[i].text, &f.sk, &f.set, err, sizeof err));
        EXPECT_NE(strstr(err, bad[i].expect), NULL);
        EXPECT_EQ(f.set.count, before.count); /* inchangé */
    }
    EXPECT_FALSE(eng_hurtboxset_parse(NULL, &f.sk, &f.set, err, sizeof err));
    EXPECT_FALSE(eng_hurtboxset_parse(good, NULL, &f.set, err, sizeof err));
    EXPECT_FALSE(eng_hurtboxset_parse(good, &f.sk, NULL, err, sizeof err));
    EXPECT_FALSE(eng_hurtboxset_parse("coucou\n", &f.sk, &f.set, NULL, 0));
    char tiny[1];
    EXPECT_FALSE(eng_hurtboxset_parse("coucou\n", &f.sk, &f.set, tiny, sizeof tiny));
    char longline[300];
    memset(longline, 'x', sizeof longline - 1);
    longline[sizeof longline - 1] = '\0';
    EXPECT_FALSE(eng_hurtboxset_parse(longline, &f.sk, &f.set, err, sizeof err));
    EXPECT_NE(strstr(err, "trop longue"), NULL);
    eng_arena_shutdown(&f.arena);
}

UTEST(hurtbox, a_failing_joint_gives_an_invalid_capsule_that_is_never_hit)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const float a[3] = {0, 0, 0};
    const float b[3] = {0, 1, 0};
    ASSERT_TRUE(eng_hurtboxset_add(&f.set, &f.sk, "x", ENG_ZONE_LIMB, "Hand", a, b, 0.1F));
    mat4 model;
    model_identity(model);
    EngCapsule c[1];
    EXPECT_FALSE(eng_hurtboxset_world(&f.set, f.world, 2, model, c)); /* joint_count trop petit */
    EXPECT_LT(c[0].radius, 0.0F);
    EngCapsule probe = {{0, 0, 0}, {0, 1, 0}, 5.0F};
    EXPECT_FALSE(eng_capsule_overlap(&probe, &c[0], NULL));
    EngCapsule out = {{9, 9, 9}, {9, 9, 9}, 9.0F};
    EXPECT_FALSE(eng_capsule_on_joint(7, a, b, 0.1F, f.world, 3, model, &out));
    EXPECT_EQ(out.radius, 9.0F); /* inchangé */
    EXPECT_FALSE(eng_capsule_on_joint(2, NULL, b, 0.1F, f.world, 3, model, &out));
    EXPECT_FALSE(eng_capsule_on_joint(2, a, NULL, 0.1F, f.world, 3, model, &out));
    EXPECT_FALSE(eng_capsule_on_joint(2, a, b, NAN, f.world, 3, model, &out));
    EXPECT_FALSE(eng_capsule_on_joint(2, a, b, 0.1F, NULL, 3, model, &out));
    EXPECT_FALSE(eng_capsule_on_joint(2, a, b, 0.1F, f.world, 3, model, NULL));
    EXPECT_FALSE(eng_hurtboxset_world(NULL, f.world, 3, model, c));
    EXPECT_FALSE(eng_hurtboxset_world(&f.set, f.world, 3, model, NULL));
    eng_arena_shutdown(&f.arena);
}

static bool same_color(EngDebugColor a, EngDebugColor b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

UTEST(hurtbox, zones_have_names_and_distinct_debug_colours_and_capsules_can_be_drawn)
{
    EXPECT_STREQ(eng_zone_name(ENG_ZONE_HEAD), "head");
    EXPECT_STREQ(eng_zone_name(ENG_ZONE_TORSO), "torso");
    EXPECT_STREQ(eng_zone_name(ENG_ZONE_LIMB), "limb");
    EXPECT_STREQ(eng_zone_name((EngHitZone)ENG_ZONE_COUNT), "?");
    EXPECT_FALSE(same_color(eng_zone_color(ENG_ZONE_HEAD), eng_zone_color(ENG_ZONE_TORSO)));
    EXPECT_FALSE(same_color(eng_zone_color(ENG_ZONE_TORSO), eng_zone_color(ENG_ZONE_LIMB)));
    EXPECT_FALSE(same_color(eng_zone_color(ENG_ZONE_HEAD), eng_zone_color(ENG_ZONE_LIMB)));
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "dbg", (size_t)1 << 20));
    EngDebugLines lines;
    ASSERT_TRUE(eng_debug_lines_init(&lines, &arena, 1000));
    const EngCapsule c = {{0, 0, 0}, {0, 1, 0}, 0.1F};
    EXPECT_TRUE(eng_capsule_debug_draw(&lines, &c, ENG_DEBUG_RED));
    EXPECT_EQ(eng_debug_lines_count(&lines), 2U * 36U + 4U); /* 2 sphères (3 x 12) + 4 lignes */
    EngCapsule bad = c;
    bad.radius = -1.0F;
    EXPECT_FALSE(eng_capsule_debug_draw(&lines, &bad, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_capsule_debug_draw(NULL, &c, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_capsule_debug_draw(&lines, NULL, ENG_DEBUG_RED));
    eng_arena_shutdown(&arena);
}
