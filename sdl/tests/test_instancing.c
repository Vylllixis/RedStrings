#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/instancing.h"
#include "utest.h"

UTEST(eng_instancing, the_instance_is_36_bytes_with_the_attribute_offsets_the_shader_reads)
{
    EXPECT_EQ(sizeof(EngInstance), (size_t)ENG_INSTANCE_STRIDE);
    EXPECT_EQ(offsetof(EngInstance, position), (size_t)0);
    EXPECT_EQ(offsetof(EngInstance, scale), (size_t)12);
    EXPECT_EQ(offsetof(EngInstance, rotation), (size_t)16);
    EXPECT_EQ(offsetof(EngInstance, tint), (size_t)32);
    EngPipelineDesc d;
    eng_instanced_pipeline_desc(&d);
    EXPECT_EQ(d.instance_stride, 36U);
    ASSERT_EQ(d.instance_attribute_count, 3U);
    EXPECT_EQ(d.instance_attributes[0].location, 4U);
    EXPECT_EQ(d.instance_attributes[0].format, ENG_VERTEX_FLOAT4);
    EXPECT_EQ(d.instance_attributes[0].offset, 0U);
    EXPECT_EQ(d.instance_attributes[1].location, 5U);
    EXPECT_EQ(d.instance_attributes[1].format, ENG_VERTEX_FLOAT4);
    EXPECT_EQ(d.instance_attributes[1].offset, 16U);
    EXPECT_EQ(d.instance_attributes[2].location, 6U);
    EXPECT_EQ(d.instance_attributes[2].format, ENG_VERTEX_UBYTE4_NORM);
    EXPECT_EQ(d.instance_attributes[2].offset, 32U);
    EXPECT_TRUE(d.depth_test);
    EXPECT_TRUE(d.depth_write);
    /* le flux 0 est celui des modèles : les emplacements 0 à 3 */
    EXPECT_EQ(d.attribute_count, 4U);
}

UTEST(eng_instancing, a_yaw_instance_rotates_about_y_with_x_towards_minus_z)
{
    const float position[3] = {10.0F, 2.0F, -4.0F};
    EngInstance inst;
    eng_instance_make_yaw(&inst, position, 2.0F, 3.14159265F * 0.5F, NULL);
    EXPECT_TRUE(eng_instance_valid(&inst));
    EXPECT_EQ(inst.tint[0], 255);
    EXPECT_EQ(inst.tint[3], 255);
    mat4 m;
    eng_instance_matrix(&inst, m);
    vec4 p = {1.0F, 0.0F, 0.0F, 1.0F};
    vec4 out;
    glm_mat4_mulv(m, p, out);
    /* +X tourné de 90 degrés autour de +Y (main droite) donne -Z ; échelle 2 ; puis la translation
     */
    EXPECT_NEAR(out[0], 10.0F, 1e-5F);
    EXPECT_NEAR(out[1], 2.0F, 1e-5F);
    EXPECT_NEAR(out[2], -4.0F - 2.0F, 1e-5F);
    vec4 up = {0.0F, 1.0F, 0.0F, 1.0F};
    glm_mat4_mulv(m, up, out);
    EXPECT_NEAR(out[1], 2.0F + 2.0F, 1e-5F);
    const uint8_t tint[4] = {10, 20, 30, 40};
    eng_instance_make_yaw(&inst, position, 1.0F, 0.0F, tint);
    EXPECT_EQ(inst.tint[0], 10);
    EXPECT_EQ(inst.tint[3], 40);
    EXPECT_NEAR(inst.rotation[3], 1.0F, 1e-6F); /* pas de rotation : quaternion identité */
}

UTEST(eng_instancing, the_matrix_equals_translation_rotation_scale_for_a_general_quaternion)
{
    EngInstance inst = {
        .position = {1.0F, 2.0F, 3.0F}, .scale = 0.5F, .tint = {255, 255, 255, 255}};
    versor q;
    glm_quatv(q, 1.1F,
              (vec3){0.3F, 0.5F, 0.81240384F}); /* axe (presque) unitaire, normalisé ci-dessous */
    glm_quat_normalize(q);
    for (int c = 0; c < 4; c++) {
        inst.rotation[c] = q[c];
    }
    ASSERT_TRUE(eng_instance_valid(&inst));
    mat4 m;
    eng_instance_matrix(&inst, m);
    const vec3 v = {0.7F, -1.2F, 2.3F};
    vec3 expected;
    glm_quat_rotatev(q, (float *)v, expected);
    for (int c = 0; c < 3; c++) {
        expected[c] = (expected[c] * 0.5F) + inst.position[c];
    }
    vec4 in = {v[0], v[1], v[2], 1.0F};
    vec4 out;
    glm_mat4_mulv(m, in, out);
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(out[c], expected[c], 1e-5F);
    }
    EXPECT_EQ(out[3], 1.0F);
}

UTEST(eng_instancing, validity_rejects_non_finite_non_positive_scale_and_non_unit_quaternions)
{
    EngInstance ok = {.position = {0, 0, 0},
                      .scale = 1.0F,
                      .rotation = {0, 0, 0, 1},
                      .tint = {255, 255, 255, 255}};
    EXPECT_TRUE(eng_instance_valid(&ok));
    EngInstance bad = ok;
    bad.scale = 0.0F;
    EXPECT_FALSE(eng_instance_valid(&bad));
    bad.scale = -1.0F;
    EXPECT_FALSE(eng_instance_valid(&bad));
    bad.scale = NAN;
    EXPECT_FALSE(eng_instance_valid(&bad));
    bad = ok;
    bad.position[1] = INFINITY;
    EXPECT_FALSE(eng_instance_valid(&bad));
    bad = ok;
    bad.rotation[3] = 1.01F;
    EXPECT_FALSE(eng_instance_valid(&bad));
    bad.rotation[3] = 1.0005F; /* sous la tolérance de 1e-3 */
    EXPECT_TRUE(eng_instance_valid(&bad));
    bad = ok;
    bad.rotation[0] = NAN;
    EXPECT_FALSE(eng_instance_valid(&bad));
    bad = ok;
    bad.rotation[3] = 0.0F; /* quaternion nul */
    EXPECT_FALSE(eng_instance_valid(&bad));
}

UTEST(eng_instancing, clearing_a_disc_removes_the_instances_inside_and_keeps_the_order)
{
    EngInstance list[5];
    const float xs[5] = {0.0F, 9.9F, 10.0F, 30.0F, -7.0F};
    const float zs[5] = {0.0F, 0.0F, 0.0F, 30.0F, 7.5F};
    for (int i = 0; i < 5; i++) {
        const float position[3] = {xs[i] + 5.0F, 2.0F, zs[i] - 3.0F};
        const uint8_t tint[4] = {(uint8_t)i, 255, 255, 255};
        eng_instance_make_yaw(&list[i], position, 1.0F, 0.0F, tint);
    }
    /* disque de 10 m autour de (5, -3) : 0 et 1 dedans, 2 sur le bord (gardé), 4 à 10,25 m */
    const uint32_t kept = eng_instances_clear_disc(list, 5, 5.0F, -3.0F, 10.0F);
    ASSERT_EQ(3U, kept);
    EXPECT_EQ(2, list[0].tint[0]);
    EXPECT_EQ(3, list[1].tint[0]);
    EXPECT_EQ(4, list[2].tint[0]);
    EXPECT_EQ(5U, eng_instances_clear_disc(list, 5, 0.0F, 0.0F, 0.0F));
    EXPECT_EQ(5U, eng_instances_clear_disc(list, 5, 0.0F, 0.0F, NAN));
    EXPECT_EQ(0U, eng_instances_clear_disc(NULL, 0, 0.0F, 0.0F, 3.0F));
    EXPECT_EQ(0U, eng_instances_clear_disc(list, 3, 5.0F, -3.0F, 1000.0F));
}
