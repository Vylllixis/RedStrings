#include <math.h>
#include <string.h>

#include "engine/debug_lines.h"
#include "engine/editor.h"
#include "engine/memory.h"
#include "utest.h"

static bool eng_entity_equal(EngEntity a, EngEntity b)
{
    return a.index == b.index && a.generation == b.generation;
}

/* Éditeur (M2-10a) : rayons, sélection, poignées de translation. Tout est analytique. */

static bool unit_bounds(void *user, EngEntity entity, float min[3], float max[3])
{
    (void)user;
    (void)entity;
    for (int i = 0; i < 3; i++) {
        min[i] = -1.0F;
        max[i] = 1.0F;
    }
    return true;
}

static bool no_bounds(void *user, EngEntity entity, float min[3], float max[3])
{
    (void)user;
    (void)entity;
    (void)min;
    (void)max;
    return false;
}

static EngRay make_ray(float ox, float oy, float oz, float dx, float dy, float dz)
{
    EngRay ray = {{ox, oy, oz}, {dx, dy, dz}};
    const float n = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    for (int i = 0; i < 3; i++) {
        ray.dir[i] /= n;
    }
    return ray;
}

static EngEntity spawn(EngWorld *world, const char *name, float x, float y, float z)
{
    const EngEntity e = eng_world_create(world, name);
    EngTransform *t = eng_world_add(world, e, ENG_COMPONENT_TRANSFORM);
    t->position[0] = x;
    t->position[1] = y;
    t->position[2] = z;
    EngMeshRenderer *r = eng_world_add(world, e, ENG_COMPONENT_MESH_RENDERER);
    r->visible = true;
    return e;
}

UTEST(editor, ray_center_is_forward)
{
    EngCamera camera;
    const float pos[3] = {1, 2, 3};
    eng_camera_init(&camera, pos, 0.7F, -0.2F);
    EngRay ray;
    ASSERT_TRUE(eng_ray_from_screen(&camera, 16.0F / 9.0F, 800, 450, 1600, 900, &ray));
    float forward[3];
    eng_camera_forward(&camera, forward);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(ray.origin[i], pos[i], 1e-6);
        EXPECT_NEAR(ray.dir[i], forward[i], 1e-5);
    }
}

UTEST(editor, ray_corners_match_fov)
{
    EngCamera camera;
    const float pos[3] = {0, 0, 0};
    eng_camera_init(&camera, pos, 0.0F, 0.0F);
    float forward[3];
    float right[3];
    eng_camera_forward(&camera, forward);
    eng_camera_right(&camera, right);
    const float aspect = 2.0F;
    EngRay ray;
    /* Bord droit, milieu vertical : tan(angle horizontal) = tan(fov/2) * aspect. */
    ASSERT_TRUE(eng_ray_from_screen(&camera, aspect, 200, 50, 200, 100, &ray));
    const float f =
        (ray.dir[0] * forward[0]) + (ray.dir[1] * forward[1]) + (ray.dir[2] * forward[2]);
    const float r = (ray.dir[0] * right[0]) + (ray.dir[1] * right[1]) + (ray.dir[2] * right[2]);
    EXPECT_NEAR(r / f, tanf(0.5F * camera.fov_y) * aspect, 1e-4);
    /* Haut de l'écran : monte de tan(fov/2). */
    ASSERT_TRUE(eng_ray_from_screen(&camera, aspect, 100, 0, 200, 100, &ray));
    const float f2 =
        (ray.dir[0] * forward[0]) + (ray.dir[1] * forward[1]) + (ray.dir[2] * forward[2]);
    EXPECT_NEAR(ray.dir[1] / f2, tanf(0.5F * camera.fov_y), 1e-4);
    float len =
        sqrtf((ray.dir[0] * ray.dir[0]) + (ray.dir[1] * ray.dir[1]) + (ray.dir[2] * ray.dir[2]));
    EXPECT_NEAR(len, 1.0F, 1e-5);
}

UTEST(editor, ray_rejects_invalid)
{
    EngCamera camera;
    const float pos[3] = {0, 0, 0};
    eng_camera_init(&camera, pos, 0, 0);
    EngRay ray;
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, -1, 5, 100, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 5, -1, 100, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 101, 5, 100, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 5, 101, 100, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 5, 5, 0, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 5, 5, 100, 0, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 0.0F, 5, 5, 100, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, NAN, 5, 5, 100, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, NAN, 5, 100, 100, &ray));
    EXPECT_TRUE(eng_ray_from_screen(&camera, 1.0F, 0, 0, 100, 100, &ray));
    EXPECT_TRUE(eng_ray_from_screen(&camera, 1.0F, 100, 100, 100, 100, &ray));
}

UTEST(editor, box_hit_distance)
{
    mat4 id;
    glm_mat4_identity(id);
    const float mn[3] = {-1, -1, -1};
    const float mx[3] = {1, 1, 1};
    float t = -1;
    EngRay ray = make_ray(0, 0, 5, 0, 0, -1);
    ASSERT_TRUE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    EXPECT_NEAR(t, 4.0F, 1e-5);
    /* Rayon qui part de l'intérieur : t = 0. */
    ray = make_ray(0, 0, 0, 1, 0, 0);
    ASSERT_TRUE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    EXPECT_NEAR(t, 0.0F, 1e-6);
    /* Boîte derrière le rayon. */
    ray = make_ray(0, 0, 5, 0, 0, 1);
    EXPECT_FALSE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    /* Raté de côté. */
    ray = make_ray(1.5F, 0, 5, 0, 0, -1);
    EXPECT_FALSE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    /* Rayon parallèle à un axe, à l'intérieur de la dalle. */
    ray = make_ray(0.5F, 0.5F, 5, 0, 0, -1);
    EXPECT_TRUE(eng_ray_vs_box(&ray, mn, mx, id, NULL));
    /* Rayon parallèle à un axe, à l'extérieur de la dalle sur cet axe. */
    ray = make_ray(0, 1.01F, 0, 1, 0, 0);
    EXPECT_FALSE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    /* Boîte invalide. */
    const float bad_min[3] = {2, -1, -1};
    ray = make_ray(0, 0, 5, 0, 0, -1);
    EXPECT_FALSE(eng_ray_vs_box(&ray, bad_min, mx, id, &t));
}

UTEST(editor, box_hit_scaled_and_rotated)
{
    /* Échelle 2 en x : la boîte s'étend de -2 à 2 en monde. */
    mat4 m;
    glm_mat4_identity(m);
    glm_scale(m, (vec3){2, 1, 1});
    const float mn[3] = {-1, -1, -1};
    const float mx[3] = {1, 1, 1};
    float t = 0;
    EngRay ray = make_ray(-5, 0, 0, 1, 0, 0);
    ASSERT_TRUE(eng_ray_vs_box(&ray, mn, mx, m, &t));
    EXPECT_NEAR(t, 3.0F, 1e-5); /* distance monde, pas distance locale */
    /* Rotation de 45° autour de y : la diagonale (√2) est atteinte à x = √2. */
    glm_mat4_identity(m);
    glm_rotate(m, GLM_PI_4f, (vec3){0, 1, 0});
    ray = make_ray(-5, 0, 0, 1, 0, 0);
    ASSERT_TRUE(eng_ray_vs_box(&ray, mn, mx, m, &t));
    EXPECT_NEAR(t, 5.0F - sqrtf(2.0F), 1e-4);
    /* Un point à x=1.2, z=0 est hors de la boîte à l'identité mais dedans une fois tournée... */
    ray = make_ray(1.2F, 0, 5, 0, 0, -1);
    EXPECT_TRUE(eng_ray_vs_box(&ray, mn, mx, m, &t));
    glm_mat4_identity(m);
    EXPECT_FALSE(eng_ray_vs_box(&ray, mn, mx, m, &t));
    /* Matrice singulière. */
    glm_mat4_identity(m);
    glm_scale(m, (vec3){0, 1, 1});
    EXPECT_FALSE(eng_ray_vs_box(&ray, mn, mx, m, &t));
}

UTEST(editor, closest_to_line)
{
    const float point[3] = {0, 0, 0};
    const float dir[3] = {1, 0, 0};
    float s = 0;
    float t = 0;
    float d = 0;
    /* Rayon vertical passant à x=2, z=0, y de 5 vers le bas : il croise l'axe x en (2,0,0). */
    EngRay ray = make_ray(2, 5, 0, 0, -1, 0);
    ASSERT_TRUE(eng_ray_closest_to_line(&ray, point, dir, &s, &t, &d));
    EXPECT_NEAR(s, 2.0F, 1e-5);
    EXPECT_NEAR(t, 5.0F, 1e-5);
    EXPECT_NEAR(d, 0.0F, 1e-5);
    /* Décalé de 0.5 en z : distance 0.5. */
    ray = make_ray(2, 5, 0.5F, 0, -1, 0);
    ASSERT_TRUE(eng_ray_closest_to_line(&ray, point, dir, &s, &t, &d));
    EXPECT_NEAR(s, 2.0F, 1e-5);
    EXPECT_NEAR(d, 0.5F, 1e-5);
    /* Parallèle : refusé. */
    ray = make_ray(0, 1, 0, 1, 0, 0);
    EXPECT_FALSE(eng_ray_closest_to_line(&ray, point, dir, &s, &t, &d));
    /* Presque parallèle (en deçà du seuil) : refusé ; au-delà : accepté. */
    ray = make_ray(0, 1, 0, 1, 0.04F, 0);
    EXPECT_FALSE(eng_ray_closest_to_line(&ray, point, dir, &s, &t, &d));
    ray = make_ray(0, 1, 0, 1, 0.06F, 0);
    EXPECT_TRUE(eng_ray_closest_to_line(&ray, point, dir, &s, &t, &d));
}

UTEST(editor, pick_nearest_visible)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 64));
    const EngEntity far_one = spawn(&world, "loin", 0, 0, -10);
    const EngEntity near_one = spawn(&world, "pres", 0, 0, -3);
    const EngEntity side = spawn(&world, "cote", 5, 0, 0);
    (void)side;
    EngRay ray = make_ray(0, 0, 5, 0, 0, -1);
    float t = 0;
    EngEntity hit = eng_editor_pick(&world, unit_bounds, NULL, &ray, &t);
    EXPECT_TRUE(eng_entity_equal(hit, near_one));
    EXPECT_NEAR(t, 7.0F, 1e-5);
    /* Le plus proche devient invisible : on prend le suivant. */
    EngMeshRenderer *r = eng_world_get(&world, near_one, ENG_COMPONENT_MESH_RENDERER);
    r->visible = false;
    hit = eng_editor_pick(&world, unit_bounds, NULL, &ray, &t);
    EXPECT_TRUE(eng_entity_equal(hit, far_one));
    EXPECT_NEAR(t, 14.0F, 1e-5);
    /* Sans bornes, rien. */
    hit = eng_editor_pick(&world, no_bounds, NULL, &ray, &t);
    EXPECT_EQ(hit.generation, 0U);
    /* Rayon dans le vide. */
    ray = make_ray(0, 10, 5, 0, 0, -1);
    hit = eng_editor_pick(&world, unit_bounds, NULL, &ray, &t);
    EXPECT_EQ(hit.generation, 0U);
    eng_world_shutdown(&world);
}

UTEST(editor, pick_uses_parent_transform)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    const EngEntity parent = spawn(&world, "parent", 10, 0, 0);
    const EngEntity child = spawn(&world, "enfant", 0, 0, 0);
    ASSERT_TRUE(eng_world_set_parent(&world, child, parent));
    EngRay ray = make_ray(10, 0, 5, 0, 0, -1);
    const EngEntity hit = eng_editor_pick(&world, unit_bounds, NULL, &ray, NULL);
    EXPECT_NE(hit.generation, 0U);
    eng_world_shutdown(&world);
}

UTEST(editor, set_world_position_with_parent)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    const EngEntity parent = spawn(&world, "parent", 10, 0, 0);
    EngTransform *pt = eng_world_get(&world, parent, ENG_COMPONENT_TRANSFORM);
    pt->scale[0] = 2.0F;
    pt->scale[1] = 2.0F;
    pt->scale[2] = 2.0F;
    /* Rotation de 90° autour de y : (x,y,z) local → (z, y, -x). */
    pt->rotation[0] = 0.0F;
    pt->rotation[1] = sinf(GLM_PI_4f);
    pt->rotation[2] = 0.0F;
    pt->rotation[3] = cosf(GLM_PI_4f);
    const EngEntity child = spawn(&world, "enfant", 0, 0, 0);
    ASSERT_TRUE(eng_world_set_parent(&world, child, parent));
    const float target[3] = {10, 3, -4};
    ASSERT_TRUE(eng_editor_set_world_position(&world, child, target));
    float out[3];
    ASSERT_TRUE(eng_editor_world_position(&world, child, out));
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(out[i], target[i], 1e-4);
    }
    /* Local attendu : (target - parent) / 2 puis rotation inverse : (0,3,-4)/2 = (0,1.5,-2) →
     * inverse de y90 : (x,y,z) → (-z, y, x) = (2, 1.5, 0). */
    const EngTransform *ct = eng_world_get(&world, child, ENG_COMPONENT_TRANSFORM);
    EXPECT_NEAR(ct->position[0], 2.0F, 1e-4);
    EXPECT_NEAR(ct->position[1], 1.5F, 1e-4);
    EXPECT_NEAR(ct->position[2], 0.0F, 1e-4);
    eng_world_shutdown(&world);
}

UTEST(editor, set_world_position_edge_cases)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    /* Entité sans Transform : le composant est ajouté. */
    const EngEntity bare = eng_world_create(&world, "nu");
    const float p[3] = {1, 2, 3};
    ASSERT_TRUE(eng_editor_set_world_position(&world, bare, p));
    EXPECT_NE(eng_world_get(&world, bare, ENG_COMPONENT_TRANSFORM), NULL);
    float out[3];
    ASSERT_TRUE(eng_editor_world_position(&world, bare, out));
    EXPECT_NEAR(out[2], 3.0F, 1e-6);
    /* Valeurs non finies. */
    const float bad[3] = {1, NAN, 3};
    EXPECT_FALSE(eng_editor_set_world_position(&world, bare, bad));
    const float inf[3] = {INFINITY, 0, 0};
    EXPECT_FALSE(eng_editor_set_world_position(&world, bare, inf));
    ASSERT_TRUE(eng_editor_world_position(&world, bare, out));
    EXPECT_NEAR(out[0], 1.0F, 1e-6); /* inchangé */
    /* Parent à échelle nulle : non inversible. */
    const EngEntity parent = spawn(&world, "p", 0, 0, 0);
    EngTransform *pt = eng_world_get(&world, parent, ENG_COMPONENT_TRANSFORM);
    pt->scale[0] = 0.0F;
    pt->scale[1] = 1.0F;
    pt->scale[2] = 1.0F;
    ASSERT_TRUE(eng_world_set_parent(&world, bare, parent));
    EXPECT_FALSE(eng_editor_set_world_position(&world, bare, p));
    /* Entité détruite. */
    eng_world_destroy(&world, bare);
    EXPECT_FALSE(eng_editor_set_world_position(&world, bare, p));
    EXPECT_FALSE(eng_editor_world_position(&world, bare, out));
    eng_world_shutdown(&world);
}

UTEST(editor, selection_goes_stale)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    EngEditor editor;
    eng_editor_init(&editor);
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    const EngEntity e = spawn(&world, "a", 0, 0, 0);
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    EXPECT_TRUE(eng_entity_equal(eng_editor_selected(&editor, &world), e));
    ASSERT_TRUE(eng_editor_select(&editor, &world, ENG_ENTITY_NULL)); /* désélection */
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    editor.drag_axis = ENG_GIZMO_X;
    eng_world_destroy(&world, e);
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_NONE);
    /* Sélectionner une entité détruite est refusé. */
    EXPECT_FALSE(eng_editor_select(&editor, &world, e));
    eng_world_shutdown(&world);
}

UTEST(editor, gizmo_length_scales_with_distance)
{
    const float o[3] = {0, 0, 0};
    const float c1[3] = {0, 0, 10};
    EXPECT_NEAR(eng_editor_gizmo_length(c1, o), 2.0F, 1e-5);
    const float c2[3] = {0, 0, 0.1F};
    EXPECT_NEAR(eng_editor_gizmo_length(c2, o), ENG_GIZMO_MIN_LENGTH, 1e-6);
    EXPECT_NEAR(eng_editor_gizmo_length(o, o), ENG_GIZMO_MIN_LENGTH, 1e-6);
}

UTEST(editor, gizmo_hit_axes)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity e = spawn(&world, "a", 0, 0, 0);
    const float cam[3] = {0, 0, 10}; /* longueur = 2 */
    /* Pas de sélection : rien. */
    EngRay ray = make_ray(1, 5, 0, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_NONE);
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    /* Rayon vertical coupant l'axe X à x=1. */
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_X);
    /* Au-delà de la longueur de la poignée. */
    ray = make_ray(2.5F, 5, 0, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_NONE);
    /* Derrière l'origine (s < 0). */
    ray = make_ray(-0.5F, 5, 0, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_NONE);
    /* Tolérance : rayon à 0.08*2 = 0.16 de l'axe. Dedans à 0.15, dehors à 0.17. */
    ray = make_ray(1, 5, 0.15F, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_X);
    ray = make_ray(1, 5, 0.17F, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_NONE);
    /* Axe Y et axe Z, vus de côté. */
    ray = make_ray(5, 1, 0, -1, 0, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_Y);
    ray = make_ray(5, 0, 1, -1, 0, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_Z);
    /* Le rayon doit aller vers l'avant : la poignée derrière la caméra n'est pas touchée. */
    ray = make_ray(1, -5, 0, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_NONE);
    eng_world_shutdown(&world);
}

UTEST(editor, drag_moves_along_axis_only)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity e = spawn(&world, "a", 1, 2, 3);
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    /* Caméra au-dessus, rayons verticaux vers le bas, drag de l'axe X. */
    EngRay ray = make_ray(1.5F, 10, 3, 0, -1, 0);
    ASSERT_TRUE(eng_editor_drag_begin(&editor, &world, ENG_GIZMO_X, &ray));
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_X);
    /* Même rayon : pas de mouvement. */
    ASSERT_TRUE(eng_editor_drag_update(&editor, &world, &ray));
    float pos[3];
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[0], 1.0F, 1e-5);
    /* Souris déplacée de +2 en x et de +7 en z : seul x change (axe contraint). */
    ray = make_ray(3.5F, 10, 10, 0, -1, 0);
    ASSERT_TRUE(eng_editor_drag_update(&editor, &world, &ray));
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[0], 3.0F, 1e-4);
    EXPECT_NEAR(pos[1], 2.0F, 1e-6);
    EXPECT_NEAR(pos[2], 3.0F, 1e-6);
    /* Fin du drag : plus de mise à jour. */
    eng_editor_drag_end(&editor);
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_NONE);
    EXPECT_FALSE(eng_editor_drag_update(&editor, &world, &ray));
    /* Axes invalides. */
    EXPECT_FALSE(eng_editor_drag_begin(&editor, &world, ENG_GIZMO_NONE, &ray));
    EXPECT_FALSE(eng_editor_drag_begin(&editor, &world, 4, &ray));
    eng_world_shutdown(&world);
}

UTEST(editor, drag_refuses_parallel_and_unselected)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity e = spawn(&world, "a", 0, 0, 0);
    EngRay vertical = make_ray(0, 10, 0, 0, -1, 0);
    EXPECT_FALSE(
        eng_editor_drag_begin(&editor, &world, ENG_GIZMO_X, &vertical)); /* rien de sélectionné */
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    EngRay parallel = make_ray(0, 1, 0, 1, 0, 0); /* parallèle à X */
    EXPECT_FALSE(eng_editor_drag_begin(&editor, &world, ENG_GIZMO_X, &parallel));
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_NONE);
    ASSERT_TRUE(eng_editor_drag_begin(&editor, &world, ENG_GIZMO_X, &vertical));
    EXPECT_FALSE(eng_editor_drag_update(&editor, &world, &parallel));
    float pos[3];
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[0], 0.0F, 1e-6); /* inchangé après le refus */
    eng_world_shutdown(&world);
}

UTEST(editor, drag_with_rotated_parent_moves_in_world_axes)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    const EngEntity parent = spawn(&world, "p", 0, 0, 0);
    EngTransform *pt = eng_world_get(&world, parent, ENG_COMPONENT_TRANSFORM);
    pt->rotation[1] = sinf(GLM_PI_4f);
    pt->rotation[3] = cosf(GLM_PI_4f);
    const EngEntity child = spawn(&world, "c", 0, 0, 0);
    ASSERT_TRUE(eng_world_set_parent(&world, child, parent));
    EngEditor editor;
    eng_editor_init(&editor);
    ASSERT_TRUE(eng_editor_select(&editor, &world, child));
    EngRay ray = make_ray(0, 10, 0, 0, -1, 0);
    ASSERT_TRUE(eng_editor_drag_begin(&editor, &world, ENG_GIZMO_X, &ray));
    ray = make_ray(4, 10, 0, 0, -1, 0);
    ASSERT_TRUE(eng_editor_drag_update(&editor, &world, &ray));
    float pos[3];
    ASSERT_TRUE(eng_editor_world_position(&world, child, pos));
    EXPECT_NEAR(pos[0], 4.0F, 1e-4);
    EXPECT_NEAR(pos[1], 0.0F, 1e-4);
    EXPECT_NEAR(pos[2], 0.0F, 1e-4);
    eng_world_shutdown(&world);
}

UTEST(editor, euler_round_trip_and_known_values)
{
    float q[4];
    const float zero[3] = {0, 0, 0};
    eng_editor_euler_to_quat(zero, q);
    EXPECT_NEAR(q[0], 0.0F, 1e-6);
    EXPECT_NEAR(q[3], 1.0F, 1e-6);
    /* 90° autour de y : (0, sin45, 0, cos45). */
    const float y90[3] = {0, 90, 0};
    eng_editor_euler_to_quat(y90, q);
    EXPECT_NEAR(q[1], sinf(GLM_PI_4f), 1e-5);
    EXPECT_NEAR(q[3], cosf(GLM_PI_4f), 1e-5);
    EXPECT_NEAR(q[0], 0.0F, 1e-6);
    float back[3];
    eng_editor_quat_to_euler(q, back);
    EXPECT_NEAR(back[1], 90.0F, 1e-2);
    /* Aller-retour sur des angles quelconques hors cardan. */
    const float angles[][3] = {{30, 20, -40}, {-75, 10, 120}, {10, -60, 5}, {0, 0, 179}};
    for (size_t i = 0; i < sizeof angles / sizeof angles[0]; i++) {
        eng_editor_euler_to_quat(angles[i], q);
        EXPECT_NEAR(sqrtf((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3])), 1.0F,
                    1e-5);
        eng_editor_quat_to_euler(q, back);
        for (int k = 0; k < 3; k++) {
            EXPECT_NEAR(back[k], angles[i][k], 1e-2);
        }
    }
    /* Un quaternion non normalisé est accepté (normalisé en interne). */
    const float scaled[4] = {0, 2 * sinf(GLM_PI_4f), 0, 2 * cosf(GLM_PI_4f)};
    eng_editor_quat_to_euler(scaled, back);
    EXPECT_NEAR(back[1], 90.0F, 1e-2);
}

UTEST(editor, new_entity_names_and_parenting)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 4));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity a = eng_editor_new_entity(&editor, &world, false);
    ASSERT_NE(a.generation, 0U);
    EXPECT_TRUE(eng_entity_equal(eng_editor_selected(&editor, &world), a));
    EXPECT_NE(eng_world_get(&world, a, ENG_COMPONENT_TRANSFORM), NULL);
    EXPECT_TRUE(eng_entity_equal(eng_world_find_by_name(&world, "entité 1"), a));
    /* Enfant de la sélection. */
    const EngEntity b = eng_editor_new_entity(&editor, &world, true);
    ASSERT_NE(b.generation, 0U);
    EXPECT_TRUE(eng_entity_equal(eng_world_parent(&world, b), a));
    EXPECT_TRUE(eng_entity_equal(eng_world_find_by_name(&world, "entité 2"), b));
    /* as_child sans sélection : racine. */
    ASSERT_TRUE(eng_editor_select(&editor, &world, ENG_ENTITY_NULL));
    const EngEntity c = eng_editor_new_entity(&editor, &world, true);
    EXPECT_EQ(eng_world_parent(&world, c).generation, 0U);
    /* Un nom libéré est réutilisé : on supprime « entité 1 » (et son enfant). */
    ASSERT_TRUE(eng_editor_select(&editor, &world, a));
    ASSERT_TRUE(eng_editor_delete_selected(&editor, &world));
    EXPECT_FALSE(eng_world_alive(&world, a));
    EXPECT_FALSE(eng_world_alive(&world, b));
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    const EngEntity d = eng_editor_new_entity(&editor, &world, false);
    EXPECT_TRUE(eng_entity_equal(eng_world_find_by_name(&world, "entité 1"), d));
    /* Rien à supprimer. */
    ASSERT_TRUE(eng_editor_select(&editor, &world, ENG_ENTITY_NULL));
    EXPECT_FALSE(eng_editor_delete_selected(&editor, &world));
    /* Monde plein (capacité 4 : c, d + 2) : la création échoue proprement. */
    (void)eng_editor_new_entity(&editor, &world, false);
    (void)eng_editor_new_entity(&editor, &world, false);
    const EngEntity full = eng_editor_new_entity(&editor, &world, false);
    EXPECT_EQ(full.generation, 0U);
    eng_world_shutdown(&world);
}

static uint32_t count_color(const EngDebugVertex *list, uint32_t vertices, EngDebugColor c)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < vertices; i++) {
        n += (list[i].color.r == c.r && list[i].color.g == c.g && list[i].color.b == c.b) ? 1U : 0U;
    }
    return n;
}

UTEST(editor, draw_box_and_three_handles_in_overlay)
{
    EngArena arena;
    EngDebugLines lines;
    ASSERT_TRUE(eng_arena_init(&arena, "editor-test", (size_t)1 << 20));
    ASSERT_TRUE(eng_debug_lines_init(&lines, &arena, 256));
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 16));
    EngEditor editor;
    eng_editor_init(&editor);
    const float cam[3] = {0, 0, 10};
    /* Rien de sélectionné : rien de dessiné. */
    eng_editor_draw(&editor, &world, unit_bounds, NULL, &lines, cam);
    EXPECT_EQ(eng_debug_lines_count(&lines), 0U);
    const EngEntity e = spawn(&world, "a", 4, 0, 0);
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    lines.overlay_mode = false;
    eng_editor_draw(&editor, &world, unit_bounds, NULL, &lines, cam);
    /* boîte (12) + 3 flèches (3 × 5) dans la liste superposée, rien en profondeur */
    EXPECT_EQ(eng_debug_lines_overlay_count(&lines), 27U);
    EXPECT_EQ(eng_debug_lines_depth_count(&lines), 0U);
    EXPECT_FALSE(lines.overlay_mode); /* mode restauré */
    const uint32_t v = lines.overlay_count;
    EXPECT_EQ(count_color(lines.overlay, v, ENG_DEBUG_CYAN), 24U);
    EXPECT_EQ(count_color(lines.overlay, v, ENG_DEBUG_RED), 10U);
    EXPECT_EQ(count_color(lines.overlay, v, ENG_DEBUG_GREEN), 10U);
    EXPECT_EQ(count_color(lines.overlay, v, ENG_DEBUG_BLUE), 10U);
    /* Les sommets de la boîte s'étendent de 3 à 5 en x (centrée sur l'objet). */
    float lo = 1e9F;
    float hi = -1e9F;
    for (uint32_t i = 0; i < v; i++) {
        if (lines.overlay[i].color.r == ENG_DEBUG_CYAN.r &&
            lines.overlay[i].color.g == ENG_DEBUG_CYAN.g) {
            lo = fminf(lo, lines.overlay[i].position[0]);
            hi = fmaxf(hi, lines.overlay[i].position[0]);
        }
    }
    EXPECT_NEAR(lo, 3.0F, 1e-5);
    EXPECT_NEAR(hi, 5.0F, 1e-5);
    /* L'axe X part de l'objet et mesure 20 % de la distance caméra (≈ 2.16 ici). */
    float far_x = -1e9F;
    for (uint32_t i = 0; i < v; i++) {
        if (lines.overlay[i].color.r == ENG_DEBUG_RED.r &&
            lines.overlay[i].color.g == ENG_DEBUG_RED.g) {
            far_x = fmaxf(far_x, lines.overlay[i].position[0]);
        }
    }
    const float expected = 4.0F + (ENG_GIZMO_SCREEN_FRACTION * sqrtf((16.0F) + 100.0F));
    EXPECT_NEAR(far_x, expected, 1e-4);
    /* Axe survolé : jaune à la place de sa couleur. */
    eng_debug_lines_clear(&lines);
    editor.hover_axis = ENG_GIZMO_Y;
    eng_editor_draw(&editor, &world, unit_bounds, NULL, &lines, cam);
    EXPECT_EQ(count_color(lines.overlay, lines.overlay_count, ENG_DEBUG_GREEN), 0U);
    EXPECT_EQ(count_color(lines.overlay, lines.overlay_count, ENG_DEBUG_YELLOW), 10U);
    /* Axe glissé aussi. */
    eng_debug_lines_clear(&lines);
    editor.hover_axis = ENG_GIZMO_NONE;
    editor.drag_axis = ENG_GIZMO_Z;
    eng_editor_draw(&editor, &world, unit_bounds, NULL, &lines, cam);
    EXPECT_EQ(count_color(lines.overlay, lines.overlay_count, ENG_DEBUG_BLUE), 0U);
    EXPECT_EQ(count_color(lines.overlay, lines.overlay_count, ENG_DEBUG_YELLOW), 10U);
    /* Sans bornes : seulement les poignées ; mode superposé initial conservé. */
    eng_debug_lines_clear(&lines);
    editor.drag_axis = ENG_GIZMO_NONE;
    lines.overlay_mode = true;
    eng_editor_draw(&editor, &world, no_bounds, NULL, &lines, cam);
    EXPECT_EQ(eng_debug_lines_overlay_count(&lines), 15U);
    EXPECT_TRUE(lines.overlay_mode);
    eng_world_shutdown(&world);
    eng_arena_shutdown(&arena);
}

/* ── Tests ajoutés après la campagne de mutations ── */

UTEST(editor, ray_from_screen_degenerate_sizes_and_camera)
{
    EngCamera camera;
    const float pos[3] = {0, 0, 0};
    eng_camera_init(&camera, pos, 0, 0);
    EngRay ray;
    /* largeur / hauteur nulles : refusées même quand le pixel vaut 0 */
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 0, 5, 0, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 5, 0, 100, 0, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 1.0F, 0, 0, 0, 0, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, 0.0F, 0, 0, 100, 100, &ray));
    EXPECT_FALSE(eng_ray_from_screen(&camera, INFINITY, 5, 5, 100, 100, &ray));
    EngCamera bad = camera;
    bad.fov_y = 0.0F;
    EXPECT_FALSE(eng_ray_from_screen(&bad, 1.0F, 5, 5, 100, 100, &ray));
    bad.fov_y = NAN;
    EXPECT_FALSE(eng_ray_from_screen(&bad, 1.0F, 5, 5, 100, 100, &ray));
    bad = camera;
    bad.position[1] = NAN;
    EXPECT_FALSE(eng_ray_from_screen(&bad, 1.0F, 5, 5, 100, 100, &ray));
}

UTEST(editor, box_touching_cases_are_hits)
{
    mat4 id;
    glm_mat4_identity(id);
    const float mn[3] = {-1, -1, -1};
    const float mx[3] = {1, 1, 1};
    float t = -1;
    /* Rayon parallèle à x, posé exactement sur la face haute / basse : touche. */
    EngRay ray = make_ray(-5, 1, 0, 1, 0, 0);
    EXPECT_TRUE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    ray = make_ray(-5, -1, 0, 1, 0, 0);
    EXPECT_TRUE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    /* Boîte plate (min == max sur z) : on la touche dans son plan. */
    const float flat_min[3] = {-1, -1, 0};
    const float flat_max[3] = {1, 1, 0};
    ray = make_ray(0, 0, 5, 0, 0, -1);
    ASSERT_TRUE(eng_ray_vs_box(&ray, flat_min, flat_max, id, &t));
    EXPECT_NEAR(t, 5.0F, 1e-5);
    /* Rayon qui n'effleure que l'arête x=1,y=1 : un seul point commun, compté comme touché. */
    ray = make_ray(2, 0, 0, -1, 1, 0);
    ASSERT_TRUE(eng_ray_vs_box(&ray, mn, mx, id, &t));
    EXPECT_NEAR(t, sqrtf(2.0F), 1e-5);
}

UTEST(editor, closest_to_line_oblique_and_offset)
{
    const float origin[3] = {0, 0, 0};
    const float dir[3] = {1, 0, 0};
    float s = 0;
    float t = 0;
    float d = 0;
    /* Rayon oblique qui croise l'axe x en (5, 0, 0). */
    EngRay ray = make_ray(0, 0, 5, 1, 0, -1);
    ASSERT_TRUE(eng_ray_closest_to_line(&ray, origin, dir, &s, &t, &d));
    EXPECT_NEAR(s, 5.0F, 1e-4);
    EXPECT_NEAR(t, 5.0F * sqrtf(2.0F), 1e-4);
    EXPECT_NEAR(d, 0.0F, 1e-4);
    /* Même rayon décalé de 1 en y : droites gauches, distance 1. */
    ray = make_ray(0, 1, 5, 1, 0, -1);
    ASSERT_TRUE(eng_ray_closest_to_line(&ray, origin, dir, &s, &t, &d));
    EXPECT_NEAR(s, 5.0F, 1e-4);
    EXPECT_NEAR(d, 1.0F, 1e-4);
    /* Droite qui ne passe pas par l'origine : x = s + 3, y = 2. */
    const float point[3] = {3, 2, 0};
    ray = make_ray(1, 2, 5, 1, 0, -1); /* croise z = 0 en x = 6, y = 2 */
    ASSERT_TRUE(eng_ray_closest_to_line(&ray, point, dir, &s, &t, &d));
    EXPECT_NEAR(s, 3.0F, 1e-4); /* 6 - 3 */
    EXPECT_NEAR(d, 0.0F, 1e-4);
    /* Direction de droite inversée : s change de signe. */
    const float back[3] = {-1, 0, 0};
    ASSERT_TRUE(eng_ray_closest_to_line(&ray, point, back, &s, &t, &d));
    EXPECT_NEAR(s, -3.0F, 1e-4);
}

UTEST(editor, pick_tie_goes_to_the_first_in_table_order)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 8));
    const EngEntity first = spawn(&world, "a", 0, 0, 0);
    const EngEntity second = spawn(&world, "b", 0, 0, 0);
    (void)second;
    const EngRay ray = make_ray(0, 0, 5, 0, 0, -1);
    const EngEntity hit = eng_editor_pick(&world, unit_bounds, NULL, &ray, NULL);
    EXPECT_TRUE(eng_entity_equal(hit, first));
    eng_world_shutdown(&world);
}

UTEST(editor, gizmo_hit_boundaries)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 8));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity e = spawn(&world, "a", 0, 0, 0);
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    const float cam[3] = {0, 0, 10};
    const float length = eng_editor_gizmo_length(cam, (const float[3]){0, 0, 0});
    /* Exactement à l'extrémité de la poignée : touchée. */
    EngRay ray = make_ray(length, 5, 0, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_X);
    /* Exactement à l'origine : X et Z sont à égalité, la première (X) gagne. */
    ray = make_ray(0, 5, 0, 0, -1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_X);
    /* Rayon qui NAÎT sur la poignée (t = 0) : pas touchée. */
    ray = make_ray(1, 0, 0, 0, 1, 0);
    EXPECT_EQ(eng_editor_gizmo_hit(&editor, &world, &ray, cam), (uint32_t)ENG_GIZMO_NONE);
    /* Glisser sur l'axe Z fonctionne (dernier axe valide). */
    ray = make_ray(0, 5, 1, 0, -1, 0);
    ASSERT_TRUE(eng_editor_drag_begin(&editor, &world, ENG_GIZMO_Z, &ray));
    ray = make_ray(0, 5, 3, 0, -1, 0);
    ASSERT_TRUE(eng_editor_drag_update(&editor, &world, &ray));
    float pos[3];
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[2], 2.0F, 1e-4);
    EXPECT_NEAR(pos[0], 0.0F, 1e-6);
    eng_world_shutdown(&world);
}

/* ── Machine à états de la souris ── */

UTEST(editor, pointer_selects_drags_and_releases)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 8));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity a = spawn(&world, "a", 0, 0, 0);
    const EngEntity b = spawn(&world, "b", 10, 0, 0);
    const float cam[3] = {0, 20, 0};
    /* Le pointeur est au-dessus de rien : rien n'est sélectionné, le survol est vide. */
    EngRay ray = make_ray(5, 20, 0, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    EXPECT_EQ(editor.hover_axis, (uint32_t)ENG_GIZMO_NONE);
    /* Appui sur a (rayon vertical) : sélection. */
    ray = make_ray(0, 20, 0, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    EXPECT_TRUE(eng_entity_equal(eng_editor_selected(&editor, &world), a));
    /* Relâché puis appui sur b : la sélection change. */
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    ray = make_ray(10, 20, 0, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    EXPECT_TRUE(eng_entity_equal(eng_editor_selected(&editor, &world), b));
    /* Appui dans le vide : désélection. */
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    ray = make_ray(50, 20, 50, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    eng_world_shutdown(&world);
}

UTEST(editor, pointer_hover_then_drag_the_handle)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 8));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity e = spawn(&world, "e", 0, 0, 0);
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    const float cam[3] = {0, 10, 0}; /* poignées de 2 m, vues d'en haut */
    /* Sans bouton : le survol suit le rayon (axe X à x = 1). */
    EngRay ray = make_ray(1, 10, 0, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    EXPECT_EQ(editor.hover_axis, (uint32_t)ENG_GIZMO_X);
    /* Appui sur la poignée : glissement, sans changer la sélection ; pas de déplacement encore. */
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_X);
    EXPECT_TRUE(eng_entity_equal(eng_editor_selected(&editor, &world), e));
    /* Maintenu et déplacé de +3 en x (et +4 en z, ignoré) : l'objet bouge, retour true. */
    ray = make_ray(4, 10, 4, 0, -1, 0);
    EXPECT_TRUE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    float pos[3];
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[0], 3.0F, 1e-4);
    EXPECT_NEAR(pos[2], 0.0F, 1e-6);
    /* Même position : modifié (le déplacement est réécrit) mais identique. */
    EXPECT_TRUE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    /* Relâché : fin du glissement ; l'objet reste où il est, le survol est recalculé. */
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_NONE);
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[0], 3.0F, 1e-4);
    /* Un nouvel appui ailleurs ne ramène pas l'objet. */
    ray = make_ray(30, 10, 30, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[0], 3.0F, 1e-4);
    eng_world_shutdown(&world);
}

UTEST(editor, pointer_over_the_interface_is_ignored)
{
    EngWorld world;
    ASSERT_TRUE(eng_world_init(&world, 8));
    EngEditor editor;
    eng_editor_init(&editor);
    const EngEntity e = spawn(&world, "e", 0, 0, 0);
    const float cam[3] = {0, 10, 0};
    EngRay ray = make_ray(0, 10, 0, 0, -1, 0);
    /* Appui sur l'interface : aucune sélection. */
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, true));
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    /* Le bouton est resté enfoncé en quittant l'interface : ce n'est pas un nouvel appui. */
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    /* Glissement interrompu quand le pointeur passe sur l'interface. */
    ASSERT_TRUE(eng_editor_select(&editor, &world, e));
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    ray = make_ray(1, 10, 0, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_X);
    ray = make_ray(5, 10, 0, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, true));
    EXPECT_EQ(editor.drag_axis, (uint32_t)ENG_GIZMO_NONE);
    float pos[3];
    ASSERT_TRUE(eng_editor_world_position(&world, e, pos));
    EXPECT_NEAR(pos[0], 0.0F, 1e-6);
    /* Survol sur l'interface : effacé. */
    editor.hover_axis = ENG_GIZMO_Y;
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, true));
    EXPECT_EQ(editor.hover_axis, (uint32_t)ENG_GIZMO_NONE);
    /* Une sélection détruite pendant un glissement est purgée sans déplacer personne. */
    ray = make_ray(1, 10, 0, 0, -1, 0);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, false, false));
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    eng_world_destroy(&world, e);
    EXPECT_FALSE(eng_editor_pointer(&editor, &world, unit_bounds, NULL, &ray, cam, true, false));
    EXPECT_EQ(editor.drag_axis,
              (uint32_t)ENG_GIZMO_NONE); /* avant tout autre appel : purgé par pointer */
    EXPECT_EQ(editor.selected.generation, 0U);
    EXPECT_EQ(eng_editor_selected(&editor, &world).generation, 0U);
    eng_world_shutdown(&world);
}
