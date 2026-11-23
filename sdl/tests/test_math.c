/* Conventions mathématiques (docs/conventions.md, « Maths ») : main droite, Y vers le haut,
 * caméra vers -Z, colonne-majeur, profondeur [0,1]. Les valeurs attendues sont calculées à la main.
 */
#include "engine/math.h"
#include "utest.h"

#define EPS 1e-5F

#define EXPECT_VEC3(v, ex, ey, ez)                                                                 \
    do {                                                                                           \
        EXPECT_NEAR((v)[0], (ex), EPS);                                                            \
        EXPECT_NEAR((v)[1], (ey), EPS);                                                            \
        EXPECT_NEAR((v)[2], (ez), EPS);                                                            \
    } while (0)

UTEST(eng_math, cglm_is_configured_right_handed_with_zero_to_one_depth)
{
    EXPECT_EQ(CGLM_CONFIG_CLIP_CONTROL, CGLM_CLIP_CONTROL_RH_ZO);
}

UTEST(eng_math, axes_form_a_right_handed_frame_with_forward_along_minus_z)
{
    vec3 c;
    glm_vec3_cross(ENG_AXIS_RIGHT, ENG_AXIS_UP, c); /* X × Y = +Z (main droite) */
    EXPECT_VEC3(c, 0.0F, 0.0F, 1.0F);
    EXPECT_VEC3(ENG_AXIS_FORWARD, 0.0F, 0.0F, -1.0F);
    glm_vec3_cross(ENG_AXIS_FORWARD, ENG_AXIS_UP, c); /* avant × haut = droite */
    EXPECT_VEC3(c, 1.0F, 0.0F, 0.0F);
}

UTEST(eng_math, matrices_are_column_major)
{
    mat4 t;
    glm_translate_make(t, (vec3){1.0F, 2.0F, 3.0F});
    /* m[colonne][ligne] : la translation est dans la 4e colonne, donc aux indices mémoire 12..14 */
    EXPECT_NEAR(t[3][0], 1.0F, EPS);
    EXPECT_NEAR(t[3][1], 2.0F, EPS);
    EXPECT_NEAR(t[3][2], 3.0F, EPS);
    const float *flat = &t[0][0];
    EXPECT_NEAR(flat[12], 1.0F, EPS);
    EXPECT_NEAR(flat[13], 2.0F, EPS);
    EXPECT_NEAR(flat[14], 3.0F, EPS);
    EXPECT_NEAR(flat[15], 1.0F, EPS);

    /* a * b applique b d'abord : translation puis échelle ≠ échelle puis translation */
    mat4 s;
    glm_scale_make(s, (vec3){2.0F, 2.0F, 2.0F});
    mat4 m;
    glm_mat4_mul(s, t, m); /* échelle ∘ translation */
    vec3 p;
    glm_mat4_mulv3(m, (vec3){0.0F, 0.0F, 0.0F}, 1.0F, p);
    EXPECT_VEC3(p, 2.0F, 4.0F, 6.0F);
}

UTEST(eng_math, a_positive_rotation_about_y_is_counter_clockwise_seen_from_above)
{
    mat4 r;
    glm_rotate_make(r, glm_rad(90.0F), ENG_AXIS_UP);
    vec3 p;
    glm_mat4_mulv3(r, (vec3){0.0F, 0.0F, 1.0F}, 1.0F, p); /* règle de la main droite : +Z → +X */
    EXPECT_VEC3(p, 1.0F, 0.0F, 0.0F);
    glm_mat4_mulv3(r, (vec3){1.0F, 0.0F, 0.0F}, 1.0F, p); /* +X → -Z */
    EXPECT_VEC3(p, 0.0F, 0.0F, -1.0F);
}

UTEST(eng_math, the_default_camera_looks_down_minus_z)
{
    mat4 view;
    eng_look_at((vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_FORWARD, ENG_AXIS_UP, view);
    mat4 id;
    glm_mat4_identity(id);
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            EXPECT_NEAR(view[c][r], id[c][r], EPS);
        }
    }
}

UTEST(eng_math, the_view_matrix_moves_the_world_in_front_of_the_camera)
{
    mat4 view;
    eng_look_at((vec3){0.0F, 0.0F, 5.0F}, (vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_UP, view);
    vec3 p;
    glm_mat4_mulv3(view, (vec3){0.0F, 0.0F, 0.0F}, 1.0F, p); /* l'origine est à 5 m devant (-Z) */
    EXPECT_VEC3(p, 0.0F, 0.0F, -5.0F);
    glm_mat4_mulv3(view, (vec3){1.0F, 2.0F, 0.0F}, 1.0F, p); /* droite et haut conservés */
    EXPECT_VEC3(p, 1.0F, 2.0F, -5.0F);
}

UTEST(eng_math, turning_the_camera_toward_plus_x_puts_plus_z_on_its_right)
{
    mat4 view;
    eng_look_at((vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_RIGHT, ENG_AXIS_UP, view);
    vec3 p;
    glm_mat4_mulv3(view, (vec3){4.0F, 0.0F, 0.0F}, 1.0F, p); /* +X monde = devant (-Z vue) */
    EXPECT_VEC3(p, 0.0F, 0.0F, -4.0F);
    glm_mat4_mulv3(view, (vec3){0.0F, 0.0F, 3.0F}, 1.0F, p); /* +Z monde = droite (+X vue) */
    EXPECT_VEC3(p, 3.0F, 0.0F, 0.0F);
}

UTEST(eng_math, perspective_maps_near_to_zero_and_far_to_one)
{
    mat4 proj;
    eng_perspective(glm_rad(90.0F), 1.0F, 1.0F, 10.0F, proj);
    vec3 ndc;
    ASSERT_TRUE(eng_project_point(proj, (vec3){0.0F, 0.0F, -1.0F}, ndc));
    EXPECT_VEC3(ndc, 0.0F, 0.0F, 0.0F); /* plan proche → 0 (et non -1 comme OpenGL) */
    ASSERT_TRUE(eng_project_point(proj, (vec3){0.0F, 0.0F, -10.0F}, ndc));
    EXPECT_VEC3(ndc, 0.0F, 0.0F, 1.0F);
    /* distance 2 : z = f(d - n) / (d(f - n)) = 10·1 / (2·9) */
    ASSERT_TRUE(eng_project_point(proj, (vec3){0.0F, 0.0F, -2.0F}, ndc));
    EXPECT_NEAR(ndc[2], 10.0F / 18.0F, EPS);
}

UTEST(eng_math, perspective_x_goes_right_and_y_goes_up)
{
    mat4 proj;
    eng_perspective(glm_rad(90.0F), 1.0F, 1.0F, 10.0F, proj);
    vec3 ndc;
    /* fovy 90° : à 1 m, le champ couvre [-1, 1] en x (aspect 1) et en y */
    ASSERT_TRUE(eng_project_point(proj, (vec3){1.0F, 0.0F, -1.0F}, ndc));
    EXPECT_NEAR(ndc[0], 1.0F, EPS);
    EXPECT_NEAR(ndc[1], 0.0F, EPS);
    ASSERT_TRUE(eng_project_point(proj, (vec3){0.0F, 1.0F, -1.0F}, ndc));
    EXPECT_NEAR(ndc[0], 0.0F, EPS);
    EXPECT_NEAR(ndc[1], 1.0F, EPS); /* +Y monde = haut de l'écran */
    ASSERT_TRUE(eng_project_point(proj, (vec3){-0.5F, -0.5F, -1.0F}, ndc));
    EXPECT_NEAR(ndc[0], -0.5F, EPS);
    EXPECT_NEAR(ndc[1], -0.5F, EPS);
}

UTEST(eng_math, the_aspect_ratio_widens_the_horizontal_field_only)
{
    mat4 proj;
    eng_perspective(glm_rad(90.0F), 2.0F, 1.0F, 10.0F, proj);
    vec3 ndc;
    ASSERT_TRUE(eng_project_point(proj, (vec3){2.0F, 0.0F, -1.0F}, ndc)); /* demi-largeur = 2 m */
    EXPECT_NEAR(ndc[0], 1.0F, EPS);
    ASSERT_TRUE(eng_project_point(proj, (vec3){0.0F, 1.0F, -1.0F}, ndc)); /* demi-hauteur = 1 m */
    EXPECT_NEAR(ndc[1], 1.0F, EPS);
}

UTEST(eng_math, points_behind_the_camera_are_rejected)
{
    mat4 proj;
    eng_perspective(glm_rad(60.0F), 1.0F, 0.1F, 100.0F, proj);
    vec3 ndc;
    EXPECT_FALSE(eng_project_point(proj, (vec3){0.0F, 0.0F, 5.0F}, ndc)); /* +Z = derrière */
    EXPECT_FALSE(eng_project_point(proj, (vec3){0.0F, 0.0F, 0.0F}, ndc));
}

UTEST(eng_math, a_full_projection_view_model_chain_composes_right_to_left)
{
    mat4 proj;
    mat4 view;
    mat4 model;
    mat4 pv;
    mat4 mvp;
    eng_perspective(glm_rad(90.0F), 1.0F, 0.1F, 100.0F, proj);
    eng_look_at((vec3){0.0F, 0.0F, 5.0F}, (vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_UP, view);
    glm_translate_make(model, (vec3){3.0F, 0.0F, 0.0F});
    glm_mat4_mul(proj, view, pv);
    glm_mat4_mul(pv, model, mvp);
    vec3 ndc;
    /* sommet local à l'origine → monde (3,0,0) → vue (3,0,-5) → x = 3/5, z = f(d-n)/(d(f-n)) */
    ASSERT_TRUE(eng_project_point(mvp, (vec3){0.0F, 0.0F, 0.0F}, ndc));
    EXPECT_NEAR(ndc[0], 0.6F, EPS);
    EXPECT_NEAR(ndc[1], 0.0F, EPS);
    EXPECT_NEAR(ndc[2], 100.0F * 4.9F / (5.0F * 99.9F), EPS);
}

UTEST(eng_math, a_counter_clockwise_triangle_stays_counter_clockwise_on_screen)
{
    mat4 proj;
    mat4 view;
    mat4 pv;
    eng_perspective(glm_rad(60.0F), 1.0F, 0.1F, 100.0F, proj);
    eng_look_at((vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_FORWARD, ENG_AXIS_UP, view);
    glm_mat4_mul(proj, view, pv);
    /* triangle dans le plan z = -3, vu de face : haut, bas-gauche, bas-droite = sens antihoraire */
    vec3 a;
    vec3 b;
    vec3 c;
    ASSERT_TRUE(eng_project_point(pv, (vec3){0.0F, 1.0F, -3.0F}, a));
    ASSERT_TRUE(eng_project_point(pv, (vec3){-1.0F, -1.0F, -3.0F}, b));
    ASSERT_TRUE(eng_project_point(pv, (vec3){1.0F, -1.0F, -3.0F}, c));
    const float area2 = ((b[0] - a[0]) * (c[1] - a[1])) - ((b[1] - a[1]) * (c[0] - a[0]));
    EXPECT_TRUE(area2 > 0.0F); /* aire signée > 0 avec y vers le haut = antihoraire */
    /* le même triangle vu de dos (caméra à l'opposé) est horaire */
    mat4 back;
    mat4 pvb;
    eng_look_at((vec3){0.0F, 0.0F, -6.0F}, (vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_UP, back);
    glm_mat4_mul(proj, back, pvb);
    ASSERT_TRUE(eng_project_point(pvb, (vec3){0.0F, 1.0F, -3.0F}, a));
    ASSERT_TRUE(eng_project_point(pvb, (vec3){-1.0F, -1.0F, -3.0F}, b));
    ASSERT_TRUE(eng_project_point(pvb, (vec3){1.0F, -1.0F, -3.0F}, c));
    const float back_area2 = ((b[0] - a[0]) * (c[1] - a[1])) - ((b[1] - a[1]) * (c[0] - a[0]));
    EXPECT_TRUE(back_area2 < 0.0F);
}

UTEST(eng_math, the_inverse_projection_recovers_the_view_space_point)
{
    mat4 proj;
    mat4 inv;
    eng_perspective(glm_rad(70.0F), 16.0F / 9.0F, 0.5F, 200.0F, proj);
    glm_mat4_inv(proj, inv);
    const vec3 p = {1.5F, -0.75F, -12.0F};
    vec3 ndc;
    ASSERT_TRUE(eng_project_point(proj, p, ndc));
    EXPECT_TRUE(ndc[2] > 0.0F && ndc[2] < 1.0F);
    vec4 clip = {ndc[0], ndc[1], ndc[2], 1.0F};
    vec4 back;
    glm_mat4_mulv(inv, clip, back);
    EXPECT_NEAR(back[0] / back[3], p[0], 1e-3F);
    EXPECT_NEAR(back[1] / back[3], p[1], 1e-3F);
    EXPECT_NEAR(back[2] / back[3], p[2], 1e-3F);
}
