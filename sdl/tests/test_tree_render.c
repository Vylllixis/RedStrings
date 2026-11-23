/* Rendu des arbres réalistes (engine/tree_render.h), partie sans carte graphique : uniforms du vent
 * et du feuillage, référence CPU du vertex shader, tramage, pipelines. Les shaders eux-mêmes sont
 * comparés à ces références dans test_tree_render_gpu.c. */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "engine/model.h"
#include "engine/tree_render.h"
#include "utest.h"

UTEST(eng_tree, the_default_look_is_packed_in_the_order_of_the_shader)
{
    float out[ENG_TREE_FRAGMENT_FLOATS];
    memset(out, 0x7F, sizeof out);
    eng_tree_pack_fragment(NULL, NULL, out);
    const EngTreeLook look = eng_tree_look_default();
    ASSERT_NEAR(look.translucency, out[0], 1e-6F);
    ASSERT_NEAR(look.crown_normal, out[1], 1e-6F);
    ASSERT_NEAR(look.sky_occlusion, out[2], 1e-6F);
    ASSERT_NEAR(look.sun_occlusion, out[3], 1e-6F);
    ASSERT_NEAR(look.dither_near, out[4], 1e-6F);
    ASSERT_NEAR(look.dither_far, out[5], 1e-6F);
    ASSERT_NEAR(look.dither_distance, out[6], 1e-6F);
    ASSERT_NEAR(look.shadow_offset, out[7], 1e-6F);
    ASSERT_NEAR(look.edge_fade, out[8], 1e-6F);
    ASSERT_NEAR(look.wrap, out[9], 1e-6F);
    ASSERT_NEAR(look.leaf_gain, out[10], 1e-6F);
    ASSERT_NEAR(look.leaf_reflection, out[11], 1e-6F);
    for (uint32_t i = ENG_TREE_LOOK_FLOATS; i < ENG_TREE_FRAGMENT_FLOATS; i++) {
        ASSERT_EQ(0.0F, out[i]); /* sans brouillard : tout à zéro, le shader le coupe */
    }
    /* chaque réglage a une valeur distincte dans son domaine : un échange se verrait */
    ASSERT_NEAR(0.6F, look.translucency, 1e-6F);
    ASSERT_NEAR(0.45F, look.crown_normal, 1e-6F);
    ASSERT_NEAR(0.35F, look.wrap, 1e-6F);
    ASSERT_NEAR(0.75F, look.sky_occlusion, 1e-6F);
    ASSERT_NEAR(0.4F, look.sun_occlusion, 1e-6F);
    ASSERT_NEAR(0.15F, look.dither_near, 1e-6F);
    ASSERT_NEAR(0.9F, look.dither_far, 1e-6F);
    ASSERT_NEAR(60.0F, look.dither_distance, 1e-6F);
    ASSERT_NEAR(0.3F, look.shadow_offset, 1e-6F);
    ASSERT_NEAR(0.25F, look.edge_fade, 1e-6F);
    ASSERT_NEAR(1.45F, look.leaf_gain, 1e-6F);
    ASSERT_NEAR(0.35F, look.leaf_reflection, 1e-6F);
    ASSERT_NEAR(0.72F, look.shadow_density, 1e-6F);
}

UTEST(eng_tree, look_values_are_brought_back_into_their_domain_and_the_fog_follows)
{
    EngTreeLook wild = {.translucency = 3.0F,
                        .crown_normal = -1.0F,
                        .wrap = NAN,
                        .sky_occlusion = INFINITY,
                        .sun_occlusion = 0.25F,
                        .dither_near = 2.0F,
                        .dither_far = -0.5F,
                        .dither_distance = 0.0F,
                        .shadow_offset = -2.0F,
                        .edge_fade = 7.0F,
                        .leaf_gain = -1.0F,
                        .leaf_reflection = 1.5F};
    float fog[ENG_FOG_UNIFORM_FLOATS];
    for (uint32_t i = 0; i < ENG_FOG_UNIFORM_FLOATS; i++) {
        fog[i] = 100.0F + (float)i;
    }
    float out[ENG_TREE_FRAGMENT_FLOATS];
    eng_tree_pack_fragment(&wild, fog, out);
    ASSERT_EQ(1.0F, out[0]);  /* translucidité plafonnée */
    ASSERT_EQ(0.0F, out[1]);  /* négatif : 0 */
    ASSERT_EQ(0.0F, out[2]);  /* infini : 0 (non fini) */
    ASSERT_EQ(0.25F, out[3]); /* une valeur valable passe telle quelle */
    ASSERT_EQ(1.0F, out[4]);
    ASSERT_EQ(0.0F, out[5]);
    ASSERT_EQ(1.0F, out[6]); /* distance nulle : 1 m (le shader divise par elle) */
    ASSERT_EQ(0.0F, out[7]);
    ASSERT_EQ(1.0F, out[8]);
    ASSERT_EQ(0.0F, out[9]);  /* enveloppe non finie : 0 */
    ASSERT_EQ(1.0F, out[10]); /* éclat négatif : 1 (neutre) */
    ASSERT_EQ(1.0F, out[11]);
    for (uint32_t i = 0; i < ENG_FOG_UNIFORM_FLOATS; i++) {
        ASSERT_EQ(100.0F + (float)i, out[ENG_TREE_LOOK_FLOATS + i]);
    }
}

UTEST(eng_tree, the_wind_is_normalised_and_bad_values_leave_a_still_unit_tree)
{
    const EngTreeShape shape = {.height = 9.0F, .crown_y = 5.5F, .radius = 4.0F};
    const EngTreeWind wind = {
        .direction = {3.0F, 4.0F}, .strength = 0.5F, .flutter = 0.04F, .time_s = 12.5F};
    float out[ENG_TREE_VERTEX_FLOATS];
    eng_tree_pack_vertex(&shape, &wind, out);
    ASSERT_NEAR(0.6F, out[0], 1e-6F);
    ASSERT_NEAR(0.8F, out[1], 1e-6F);
    ASSERT_NEAR(0.5F, out[2], 1e-6F);
    ASSERT_NEAR(12.5F, out[3], 1e-6F);
    ASSERT_NEAR(9.0F, out[4], 1e-6F);
    ASSERT_NEAR(5.5F, out[5], 1e-6F);
    ASSERT_NEAR(4.0F, out[6], 1e-6F);
    ASSERT_NEAR(0.04F, out[7], 1e-6F);
    /* direction nulle : pas de vent du tout, mais le temps passe */
    const EngTreeWind none = {
        .direction = {0.0F, 0.0F}, .strength = 0.5F, .flutter = 0.04F, .time_s = 3.0F};
    eng_tree_pack_vertex(&shape, &none, out);
    ASSERT_EQ(1.0F, out[0]);
    ASSERT_EQ(0.0F, out[1]);
    ASSERT_EQ(0.0F, out[2]);
    ASSERT_EQ(0.0F, out[7]);
    ASSERT_EQ(3.0F, out[3]);
    /* valeurs invalides : vent négatif ignoré, forme d'un mètre (le shader divise par elles) */
    const EngTreeShape bad_shape = {.height = 0.0F, .crown_y = NAN, .radius = -2.0F};
    const EngTreeWind bad_wind = {
        .direction = {1.0F, 0.0F}, .strength = -1.0F, .flutter = NAN, .time_s = INFINITY};
    eng_tree_pack_vertex(&bad_shape, &bad_wind, out);
    ASSERT_EQ(0.0F, out[2]);
    ASSERT_EQ(0.0F, out[7]);
    ASSERT_EQ(0.0F, out[3]);
    ASSERT_EQ(1.0F, out[4]);
    ASSERT_EQ(0.5F, out[5]);
    ASSERT_EQ(1.0F, out[6]);
    eng_tree_pack_vertex(NULL, NULL, out);
    ASSERT_EQ(1.0F, out[0]);
    ASSERT_EQ(0.0F, out[2]);
    ASSERT_EQ(1.0F, out[4]);
    ASSERT_EQ(1.0F, out[6]);
    const EngTreeWind standard = eng_tree_wind_default();
    ASSERT_NEAR(0.22F, standard.strength, 1e-6F);
    ASSERT_NEAR(0.035F, standard.flutter, 1e-6F);
    ASSERT_EQ(1.0F, standard.direction[0]);
}

UTEST(eng_tree, the_shape_comes_from_the_bounding_box)
{
    const float lo[3] = {-3.0F, -0.2F, -5.0F};
    const float hi[3] = {4.0F, 9.0F, 2.0F};
    const EngTreeShape shape = eng_tree_shape_from_bounds(lo, hi, 0.6F);
    ASSERT_NEAR(9.0F, shape.height, 1e-6F);
    ASSERT_NEAR(5.4F, shape.crown_y, 1e-5F);
    ASSERT_NEAR(5.0F, shape.radius, 1e-6F); /* le plus grand écart à l'axe : |z| = 5 */
    const float wide[3] = {6.5F, 9.0F, 2.0F};
    ASSERT_NEAR(6.5F, eng_tree_shape_from_bounds(lo, wide, 0.6F).radius, 1e-6F);
    ASSERT_NEAR(9.0F, eng_tree_shape_from_bounds(lo, hi, 4.0F).crown_y, 1e-5F); /* part plafonnée */
    const EngTreeShape unit = eng_tree_shape_from_bounds(NULL, hi, 0.6F);
    ASSERT_EQ(1.0F, unit.height);
    ASSERT_EQ(1.0F, unit.radius);
    const float flat[3] = {0.0F, -1.0F, 0.0F}; /* boîte sous le sol, sans largeur */
    const EngTreeShape degenerate = eng_tree_shape_from_bounds(flat, flat, 0.5F);
    ASSERT_EQ(1.0F, degenerate.height);
    ASSERT_EQ(1.0F, degenerate.radius);
}

static void packed_for(float strength, float flutter, float time_s,
                       float out[ENG_TREE_VERTEX_FLOATS])
{
    const EngTreeShape shape = {.height = 10.0F, .crown_y = 6.0F, .radius = 4.0F};
    const EngTreeWind wind = {
        .direction = {0.6F, 0.8F}, .strength = strength, .flutter = flutter, .time_s = time_s};
    eng_tree_pack_vertex(&shape, &wind, out);
}

UTEST(eng_tree, without_wind_a_vertex_is_just_placed_by_its_instance)
{
    float packed[ENG_TREE_VERTEX_FLOATS];
    packed_for(0.0F, 0.0F, 5.0F, packed);
    EngInstance instance;
    const float position[3] = {12.0F, 3.0F, -7.0F};
    eng_instance_make_yaw(&instance, position, 1.5F, 0.8F, NULL);
    const float local[3] = {2.0F, 7.0F, -1.0F};
    float world[3];
    eng_tree_vertex_world(packed, &instance, local, world);
    mat4 matrix;
    eng_instance_matrix(&instance, matrix);
    vec3 expected;
    glm_mat4_mulv3(matrix, (float *)local, 1.0F, expected);
    for (int a = 0; a < 3; a++) {
        ASSERT_NEAR(expected[a], world[a], 1e-4F);
    }
}

UTEST(eng_tree, the_trunk_bends_with_the_square_of_the_height_and_the_foot_never_moves)
{
    float packed[ENG_TREE_VERTEX_FLOATS];
    packed_for(0.5F, 0.0F, 0.0F, packed); /* t = 0 et arbre à l'origine : rafale = 0,6 */
    EngInstance instance;
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    eng_instance_make_yaw(&instance, origin, 1.0F, 0.0F, NULL);
    float world[3];
    eng_tree_vertex_world(packed, &instance, (const float[3]){0.0F, 10.0F, 0.0F}, world);
    ASSERT_NEAR(0.5F * 0.6F * 0.6F, world[0], 1e-5F); /* force x rafale, le long de la direction */
    ASSERT_NEAR(0.5F * 0.6F * 0.8F, world[2], 1e-5F);
    ASSERT_NEAR(10.0F, world[1], 1e-5F); /* le pli ne soulève rien */
    eng_tree_vertex_world(packed, &instance, (const float[3]){0.0F, 5.0F, 0.0F}, world);
    ASSERT_NEAR(0.25F * 0.5F * 0.6F * 0.6F, world[0], 1e-5F); /* à mi-hauteur : le quart */
    eng_tree_vertex_world(packed, &instance, (const float[3]){1.0F, 0.0F, 2.0F}, world);
    ASSERT_NEAR(1.0F, world[0], 1e-6F); /* le pied : rien */
    ASSERT_NEAR(2.0F, world[2], 1e-6F);
    eng_tree_vertex_world(packed, &instance, (const float[3]){0.0F, 25.0F, 0.0F}, world);
    ASSERT_NEAR(0.5F * 0.6F * 0.6F, world[0], 1e-5F); /* au-dessus de la cime : plafonné */
    /* un arbre deux fois plus grand (échelle 2) plie deux fois plus ; sa cime est à 20 m */
    eng_instance_make_yaw(&instance, origin, 2.0F, 0.0F, NULL);
    eng_tree_vertex_world(packed, &instance, (const float[3]){0.0F, 10.0F, 0.0F}, world);
    ASSERT_NEAR(2.0F * 0.5F * 0.6F * 0.6F, world[0], 1e-5F);
    ASSERT_NEAR(20.0F, world[1], 1e-5F);
    /* chaque arbre a son rythme : ailleurs, au même instant, la rafale diffère */
    const float elsewhere[3] = {4.0F, 0.0F, 1.0F};
    eng_instance_make_yaw(&instance, elsewhere, 1.0F, 0.0F, NULL);
    eng_tree_vertex_world(packed, &instance, (const float[3]){0.0F, 10.0F, 0.0F}, world);
    const float gust = 0.6F + (0.4F * sinf((0.37F * 4.0F) + 0.23F));
    ASSERT_NEAR(4.0F + (0.5F * gust * 0.6F), world[0], 1e-5F);
    ASSERT_GT(fabsf(gust - 0.6F), 0.1F);
}

UTEST(eng_tree, branch_tips_flutter_and_the_axis_does_not)
{
    float packed[ENG_TREE_VERTEX_FLOATS];
    packed_for(0.0F, 0.1F, 1.0F, packed);
    EngInstance instance;
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    eng_instance_make_yaw(&instance, origin, 1.0F, 0.0F, NULL);
    float world[3];
    eng_tree_vertex_world(packed, &instance, (const float[3]){0.0F, 8.0F, 0.0F}, world);
    ASSERT_NEAR(0.0F, world[0], 1e-6F); /* sur l'axe du tronc : pas de frémissement */
    ASSERT_NEAR(8.0F, world[1], 1e-6F);
    const float tip[3] = {4.0F, 8.0F, 0.0F}; /* à un rayon de houppier de l'axe : portée 1 */
    eng_tree_vertex_world(packed, &instance, tip, world);
    const float wave = sinf((2.3F * 1.0F) + (1.7F * 4.0F) + (2.9F * 8.0F));
    const float flap = 0.1F * 1.0F * 0.8F * wave;
    ASSERT_NEAR(8.0F + (0.6F * flap), world[1], 1e-5F);
    ASSERT_NEAR(4.0F + (0.6F * 0.4F * flap), world[0], 1e-5F);
    ASSERT_NEAR(0.8F * 0.4F * flap, world[2], 1e-5F);
    ASSERT_GT(fabsf(flap), 0.01F);
    const float half_way[3] = {2.0F, 8.0F, 0.0F}; /* à mi-portée : moitié moins (à l'onde près) */
    eng_tree_vertex_world(packed, &instance, half_way, world);
    const float wave_half = sinf((2.3F * 1.0F) + (1.7F * 2.0F) + (2.9F * 8.0F));
    ASSERT_NEAR(8.0F + (0.6F * 0.1F * 0.5F * 0.8F * wave_half), world[1], 1e-5F);
    const float beyond[3] = {0.0F, 8.0F, 9.0F}; /* plus loin que le rayon : portée plafonnée à 1 */
    eng_tree_vertex_world(packed, &instance, beyond, world);
    const float wave_far = sinf((2.3F * 1.0F) + (2.9F * 8.0F) + (1.3F * 9.0F));
    ASSERT_NEAR(8.0F + (0.6F * 0.1F * 0.8F * wave_far), world[1], 1e-5F);
}

UTEST(eng_tree, the_dither_is_a_fixed_noise_that_fills_zero_to_one_evenly)
{
    /* le pixel (0,5 ; 0,5) : frac(52,9829189 x frac(0,5 x 0,06711056 + 0,5 x 0,00583715)) */
    ASSERT_NEAR(0.93249F, eng_tree_dither(0.5F, 0.5F), 2e-3F);
    ASSERT_EQ(eng_tree_dither(17.5F, 40.5F), eng_tree_dither(17.5F, 40.5F));
    double sum = 0.0;
    uint32_t low = 0;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            const float d = eng_tree_dither((float)x + 0.5F, (float)y + 0.5F);
            ASSERT_GE(d, 0.0F);
            ASSERT_LT(d, 1.0F);
            sum += (double)d;
            low += d < 0.3F ? 1U : 0U;
        }
    }
    ASSERT_NEAR(0.5, sum / 4096.0, 0.02);         /* moyenne : la densité est conservée */
    ASSERT_NEAR(0.3, (double)low / 4096.0, 0.03); /* 30 % des pixels sous 0,3 */
    /* deux voisins n'ont pas le même seuil : pas d'aplat */
    ASSERT_GT(fabsf(eng_tree_dither(10.5F, 10.5F) - eng_tree_dither(11.5F, 10.5F)), 0.2F);
    ASSERT_GT(fabsf(eng_tree_dither(10.5F, 10.5F) - eng_tree_dither(10.5F, 11.5F)), 0.2F);
}

UTEST(eng_tree, the_pipelines_read_the_vertices_the_shaders_expect)
{
    EngPipelineDesc desc;
    eng_tree_pipeline_desc(&desc);
    ASSERT_STREQ("tree.vert", desc.vertex_shader);
    ASSERT_STREQ("tree.frag", desc.fragment_shader);
    ASSERT_EQ((int)ENG_CULL_NONE, (int)desc.cull_mode); /* les cartes ont deux faces */
    ASSERT_TRUE(desc.depth_test);
    ASSERT_TRUE(desc.depth_write);
    ASSERT_FALSE(desc.depth_only);
    ASSERT_EQ(4U, desc.attribute_count);
    ASSERT_EQ(3U, desc.instance_attribute_count);
    ASSERT_EQ((uint32_t)ENG_INSTANCE_STRIDE, desc.instance_stride);
    ASSERT_EQ((uint32_t)sizeof(EngModelVertex), desc.vertex_stride);

    eng_tree_shadow_pipeline_desc(&desc);
    ASSERT_STREQ("tree_shadow.vert", desc.vertex_shader);
    ASSERT_STREQ("tree_shadow.frag", desc.fragment_shader);
    ASSERT_TRUE(desc.depth_only);
    ASSERT_EQ((int)ENG_CULL_NONE, (int)desc.cull_mode);
    ASSERT_EQ(2U, desc.attribute_count);
    ASSERT_EQ(0U, desc.attributes[0].location);
    ASSERT_EQ((uint32_t)offsetof(EngModelVertex, position), desc.attributes[0].offset);
    ASSERT_EQ(1U, desc.attributes[1].location);
    ASSERT_EQ((uint32_t)offsetof(EngModelVertex, uv), desc.attributes[1].offset);
    ASSERT_EQ((int)ENG_VERTEX_FLOAT2, (int)desc.attributes[1].format);
    ASSERT_EQ(2U, desc.instance_attribute_count);
    ASSERT_EQ(2U, desc.instance_attributes[0].location);
    ASSERT_EQ((uint32_t)offsetof(EngInstance, position), desc.instance_attributes[0].offset);
    ASSERT_EQ(3U, desc.instance_attributes[1].location);
    ASSERT_EQ((uint32_t)offsetof(EngInstance, rotation), desc.instance_attributes[1].offset);
    ASSERT_EQ((uint32_t)ENG_INSTANCE_STRIDE, desc.instance_stride);
}

UTEST(eng_tree, an_empty_draw_queues_nothing)
{
    EngModelGpu gpu;
    memset(&gpu, 0, sizeof gpu);
    EngRenderQueue queue;
    memset(&queue, 0, sizeof queue);
    mat4 identity;
    glm_mat4_identity(identity);
    EngTreeDraw draw = {.count = 0U};
    ASSERT_EQ(0U, eng_tree_draw(&gpu, &queue, (EngPipelineHandle){0}, identity, NULL, NULL, NULL));
    ASSERT_EQ(0U, eng_tree_draw(&gpu, &queue, (EngPipelineHandle){0}, identity, NULL, NULL, &draw));
    draw.count = 4U; /* des instances annoncées mais pas de buffer */
    ASSERT_EQ(0U, eng_tree_draw(&gpu, &queue, (EngPipelineHandle){0}, identity, NULL, NULL, &draw));
    ASSERT_EQ(0U, eng_tree_draw_depth(&gpu, &queue, (EngPipelineHandle){0}, identity, &draw));
    ASSERT_EQ(0U, eng_tree_draw_depth(NULL, &queue, (EngPipelineHandle){0}, identity, &draw));
    ASSERT_EQ(0U, eng_tree_draw(&gpu, NULL, (EngPipelineHandle){0}, identity, NULL, NULL, &draw));
    ASSERT_EQ(0U, queue.count);
    ASSERT_EQ(0U, queue.dropped);
}

/* ── La file de dessin (sans GPU) : ce que eng_tree_draw et eng_tree_draw_depth y mettent ── */

typedef struct DrawQueue {
    EngRenderQueue queue;
    EngDrawCmd commands[8];
    uint8_t uniforms[8192];
} DrawQueue;

static void init_draw_queue(DrawQueue *d)
{
    memset(d, 0, sizeof *d);
    d->queue.commands = d->commands;
    d->queue.capacity = 8U;
    d->queue.uniforms = d->uniforms;
    d->queue.uniform_capacity = sizeof d->uniforms;
}

#define FRAGMENT_FLOATS 128U /* les uniforms de model.frag : 32 float4 */

UTEST(eng_tree, a_draw_queues_the_chosen_parts_with_wind_look_and_fog)
{
    EngModelPart parts[3] = {{.first_index = 0U, .index_count = 30U, .material = 0U},
                             {.first_index = 30U, .index_count = 12U, .material = 1U},
                             {.first_index = 42U, .index_count = 6U, .material = 1U}};
    EngModelGpuMaterial materials[2] = {
        {.base_color = {1.0F, 1.0F, 1.0F, 1.0F}},
        {.base_color = {1.0F, 1.0F, 1.0F, 1.0F}, .alpha_cutoff = 0.5F, .double_sided = true}};
    materials[1].texture.handle.generation = 4U; /* la texture de couleur du feuillage */
    EngModelGpu gpu = {
        .parts = parts, .part_count = 3U, .materials = materials, .material_count = 2U};
    gpu.sampler.handle.generation = 6U;
    const uint32_t chosen[3] = {2U, 0U, 9U}; /* la vue lointaine, puis le tronc ; 9 n'existe pas */
    float fog[ENG_FOG_UNIFORM_FLOATS];
    for (uint32_t i = 0; i < ENG_FOG_UNIFORM_FLOATS; i++) {
        fog[i] = 50.0F + (float)i;
    }
    EngTreeLook look = eng_tree_look_default();
    look.translucency = 0.25F;
    look.shadow_density = 0.4F;
    EngTreeDraw draw = {
        .count = 7U,
        .shape = {.height = 9.0F, .crown_y = 5.5F, .radius = 4.0F},
        .wind = {.direction = {0.0F, 2.0F}, .strength = 0.5F, .flutter = 0.04F, .time_s = 2.0F},
        .look = &look,
        .fog = fog,
        .parts = chosen,
        .part_count = 3U};
    draw.instances.handle.generation = 1U;
    mat4 identity;
    glm_mat4_identity(identity);
    DrawQueue d;
    init_draw_queue(&d);
    ASSERT_EQ(2U,
              eng_tree_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, NULL, NULL, &draw));
    ASSERT_EQ(2U, d.queue.count);
    ASSERT_EQ(42U, d.queue.commands[0].first); /* dans l'ordre demandé */
    ASSERT_EQ(6U, d.queue.commands[0].count);
    ASSERT_EQ(7U, d.queue.commands[0].instance_count);
    ASSERT_EQ(0U, d.queue.commands[1].first);
    ASSERT_EQ(30U, d.queue.commands[1].count);
    /* vertex : vue-projection (16), couleur (4), puis le vent et la forme (8) */
    ASSERT_EQ((16U + 4U + ENG_TREE_VERTEX_FLOATS) * 4U, d.queue.commands[0].uniform_size);
    float vertex[16U + 4U + ENG_TREE_VERTEX_FLOATS];
    memcpy(vertex, &d.uniforms[d.queue.commands[0].uniform_offset], sizeof vertex);
    ASSERT_EQ(0.0F, vertex[20]);
    ASSERT_EQ(1.0F, vertex[21]); /* direction normalisée */
    ASSERT_EQ(0.5F, vertex[22]);
    ASSERT_EQ(2.0F, vertex[23]);
    ASSERT_EQ(9.0F, vertex[24]);
    ASSERT_EQ(5.5F, vertex[25]);
    ASSERT_EQ(4.0F, vertex[26]);
    ASSERT_EQ(0.04F, vertex[27]);
    /* fragment : les uniforms de model.frag, puis le feuillage (12), puis le brouillard (20) */
    ASSERT_EQ((FRAGMENT_FLOATS + ENG_TREE_FRAGMENT_FLOATS) * 4U,
              d.queue.commands[0].fragment_uniform_size);
    float fragment[FRAGMENT_FLOATS + ENG_TREE_FRAGMENT_FLOATS];
    memcpy(fragment, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof fragment);
    ASSERT_EQ(0.5F, fragment[3]); /* le seuil de découpe de la partie (sun_direction.w) */
    ASSERT_EQ(1.0F, fragment[7]); /* deux faces (sun_radiance.w) */
    ASSERT_EQ(0.25F, fragment[FRAGMENT_FLOATS]);
    ASSERT_EQ(look.crown_normal, fragment[FRAGMENT_FLOATS + 1U]);
    ASSERT_EQ(50.0F, fragment[FRAGMENT_FLOATS + ENG_TREE_LOOK_FLOATS]);
    ASSERT_EQ(69.0F, fragment[FRAGMENT_FLOATS + ENG_TREE_FRAGMENT_FLOATS - 1U]);
    memcpy(fragment, &d.uniforms[d.queue.commands[1].fragment_uniform_offset], sizeof fragment);
    ASSERT_EQ(0.0F, fragment[3]); /* le tronc est opaque */
    ASSERT_EQ(0.25F, fragment[FRAGMENT_FLOATS]);

    /* la passe d'ombre : mêmes parties, même vent, la texture de couleur et le seuil de la partie
     */
    init_draw_queue(&d);
    ASSERT_EQ(2U, eng_tree_draw_depth(&gpu, &d.queue, (EngPipelineHandle){0}, identity, &draw));
    ASSERT_EQ(42U, d.queue.commands[0].first);
    ASSERT_EQ(7U, d.queue.commands[0].instance_count);
    ASSERT_EQ(4U, d.queue.commands[0].texture.handle.generation);
    ASSERT_EQ(6U, d.queue.commands[0].sampler.handle.generation);
    ASSERT_EQ((16U + ENG_TREE_VERTEX_FLOATS) * 4U, d.queue.commands[0].uniform_size);
    float shadow_vertex[16U + ENG_TREE_VERTEX_FLOATS];
    memcpy(shadow_vertex, &d.uniforms[d.queue.commands[0].uniform_offset], sizeof shadow_vertex);
    ASSERT_EQ(1.0F, shadow_vertex[0]); /* la matrice du soleil */
    ASSERT_EQ(1.0F, shadow_vertex[17]);
    ASSERT_EQ(0.5F, shadow_vertex[18]);
    ASSERT_EQ(9.0F, shadow_vertex[20]);
    ASSERT_EQ(0.04F, shadow_vertex[23]);
    float cut[4];
    ASSERT_EQ(16U, d.queue.commands[0].fragment_uniform_size);
    memcpy(cut, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof cut);
    ASSERT_EQ(0.5F, cut[0]);
    ASSERT_EQ(0.4F, cut[1]); /* la densité de l'ombre du feuillage */
    memcpy(cut, &d.uniforms[d.queue.commands[1].fragment_uniform_offset], sizeof cut);
    ASSERT_EQ(0.0F, cut[0]);

    /* sans liste de parties : toutes ; un modèle skinné n'a pas d'ombre d'arbre */
    draw.parts = NULL;
    init_draw_queue(&d);
    ASSERT_EQ(3U,
              eng_tree_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, NULL, NULL, &draw));
    init_draw_queue(&d);
    ASSERT_EQ(3U, eng_tree_draw_depth(&gpu, &d.queue, (EngPipelineHandle){0}, identity, &draw));
    gpu.skinned = true;
    init_draw_queue(&d);
    ASSERT_EQ(0U, eng_tree_draw_depth(&gpu, &d.queue, (EngPipelineHandle){0}, identity, &draw));
    /* densité d'ombre hors domaine : ramenée entre 0 et 1 ; sans réglage : celle par défaut */
    gpu.skinned = false;
    look.shadow_density = 5.0F;
    init_draw_queue(&d);
    ASSERT_EQ(3U, eng_tree_draw_depth(&gpu, &d.queue, (EngPipelineHandle){0}, identity, &draw));
    memcpy(cut, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof cut);
    ASSERT_EQ(1.0F, cut[1]);
    draw.look = NULL;
    init_draw_queue(&d);
    ASSERT_EQ(3U, eng_tree_draw_depth(&gpu, &d.queue, (EngPipelineHandle){0}, identity, &draw));
    memcpy(cut, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof cut);
    ASSERT_EQ(0.72F, cut[1]);
}
