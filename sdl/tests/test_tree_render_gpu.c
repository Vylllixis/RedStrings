#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/atmosphere.h"
#include "engine/fog.h"
#include "engine/image.h"
#include "engine/model_gpu.h"
#include "engine/tree_render.h"
#include "test_render_util.h"
#include "utest.h"

/* Rendu des arbres réalistes (engine/tree_render.h), pour de vrai, sur la carte graphique des
 * tests :
 *   - le vertex shader tree.vert doit donner la même image, à l'arrondi 8 bits près, qu'un maillage
 *     ordinaire dont chaque sommet a été déplacé par la référence CPU eng_tree_vertex_world ; comme
 *     la référence est dessinée par model.frag, cela vérifie aussi qu'une partie opaque sort de
 *     tree.frag comme de model.frag ;
 *   - une partie découpée ne dessine que les pixels opaques de sa texture, des deux côtés ;
 *   - le tramage du seuil garde la densité d'un feuillage à moitié transparent ;
 *   - l'ombre d'une carte est découpée elle aussi, et sa densité se règle.
 * Sans GPU ou sans shaders compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 128U
#define MAP_SIZE 256U

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle plain;       /* model.vert / model.frag */
    EngPipelineHandle instanced;   /* instanced.vert / model.frag : le témoin sans découpe */
    EngPipelineHandle tree;        /* tree.vert / tree.frag */
    EngPipelineHandle shadow;      /* passe d'ombre ordinaire */
    EngPipelineHandle tree_shadow; /* passe d'ombre des arbres */
    EngTextureHandle map;
} Fixture;

static bool open_fixture(Fixture *t)
{
    memset(t, 0, sizeof *t);
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "tree-gpu", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "tree-level", (size_t)8 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 65536,
                                  .max_shadow_commands = 16,
                                  .depth_buffer = true};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->level);
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    EngPipelineDesc pipeline;
    eng_model_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->plain = eng_renderer_create_pipeline(&t->r, &pipeline);
    eng_instanced_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->instanced = eng_renderer_create_pipeline(&t->r, &pipeline);
    eng_tree_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->tree = eng_renderer_create_pipeline(&t->r, &pipeline);
    eng_model_shadow_pipeline_desc(&pipeline);
    t->shadow = eng_renderer_create_pipeline(&t->r, &pipeline);
    eng_tree_shadow_pipeline_desc(&pipeline);
    t->tree_shadow = eng_renderer_create_pipeline(&t->r, &pipeline);
    t->map = eng_renderer_create_shadow_map(&t->r, MAP_SIZE);
    return true;
}

static bool fixture_valid(const Fixture *t)
{
    return eng_renderer_pipeline_is_valid(&t->r, t->plain) &&
           eng_renderer_pipeline_is_valid(&t->r, t->instanced) &&
           eng_renderer_pipeline_is_valid(&t->r, t->tree) &&
           eng_renderer_pipeline_is_valid(&t->r, t->shadow) &&
           eng_renderer_pipeline_is_valid(&t->r, t->tree_shadow) &&
           eng_renderer_texture_is_valid(&t->r, t->map);
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_texture(&t->r, t->map);
    (void)eng_renderer_destroy_pipeline(&t->r, t->tree_shadow);
    (void)eng_renderer_destroy_pipeline(&t->r, t->shadow);
    (void)eng_renderer_destroy_pipeline(&t->r, t->tree);
    (void)eng_renderer_destroy_pipeline(&t->r, t->instanced);
    (void)eng_renderer_destroy_pipeline(&t->r, t->plain);
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->level);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

#define CLOSE_FIXTURE(t) EXPECT_EQ(close_fixture(&(t)), (uint32_t)0)

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    EngRenderer *r = (EngRenderer *)context;
    (void)eng_renderer_execute(r, &r->queue, commands, pass);
}

static void run_shadow_pass(SDL_GPUCommandBuffer *commands, void *context)
{
    Fixture *t = (Fixture *)context;
    (void)eng_renderer_run_shadow_pass(&t->r, commands, t->map, &t->r.shadow_queue);
}

static uint8_t g_pixels[SIZE * SIZE * 4];
static uint8_t g_other[SIZE * SIZE * 4];
static float g_tangent_w = 1.0F;  /* w des tangentes des cartes fabriquées par make_card */
static const float *g_fog = NULL; /* brouillard des dessins de draw_trees */
/* hauteur de l'arbre annoncée aux shaders, en multiple de celle du modèle : 2 met le haut du
 * modèle à mi-hauteur de l'arbre (le pli y vaut le quart de celui de la cime) */
static float g_height_scale = 1.0F;

static EngTreeShape shape_of(const EngModelGpu *model)
{
    EngTreeShape shape = eng_tree_shape_from_bounds(model->bounds_min, model->bounds_max, 0.6F);
    shape.height *= g_height_scale;
    return shape;
}

/* mise au point : OW_TREE_DUMP=<dossier> écrit les images en PPM */
static void dump_ppm(const uint8_t *pixels, const char *name)
{
    const char *dir = SDL_getenv("OW_TREE_DUMP");
    char path[512];
    FILE *f = NULL;
    if (dir != NULL) {
        SDL_snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
        f = fopen(path, "wb");
    }
    if (f == NULL) {
        return;
    }
    (void)fprintf(f, "P6\n%u %u\n255\n", SIZE, SIZE);
    for (uint32_t i = 0; i < SIZE * SIZE; i++) {
        (void)fwrite(&pixels[(size_t)i * 4U], 1, 3, f);
    }
    (void)fclose(f);
}

static const float EYE[3] = {0.0F, 1.0F, 6.0F};

/* caméra devant l'origine, regardant vers -z */
static void camera(mat4 view_proj, EngLighting *light)
{
    mat4 view;
    mat4 proj;
    glm_lookat((vec3){EYE[0], EYE[1], EYE[2]}, (vec3){0.0F, 1.0F, 0.0F}, (vec3){0.0F, 1.0F, 0.0F},
               view);
    glm_perspective(glm_rad(50.0F), 1.0F, 0.5F, 200.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    eng_lighting_default(light);
    (void)eng_lighting_set_sun_angles(light, 40.0F, 60.0F);
    memcpy(light->camera_position, EYE, sizeof EYE);
}

/* pixels non noirs des colonnes [x0, x1[ */
static int lit_columns(const uint8_t *pixels, uint32_t x0, uint32_t x1)
{
    int count = 0;
    for (uint32_t y = 0; y < SIZE; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            const uint8_t *p = &pixels[(((size_t)y * SIZE) + x) * 4U];
            count += (p[0] + p[1] + p[2]) > 6 ? 1 : 0;
        }
    }
    return count;
}

/* nombre de pixels dont un canal diffère de plus de `tolerance` */
static int differing_pixels(const uint8_t *a, const uint8_t *b, int tolerance)
{
    int count = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        bool differs = false;
        for (size_t c = 0; c < 4U; c++) {
            differs = differs || abs((int)a[(i * 4U) + c] - (int)b[(i * 4U) + c]) > tolerance;
        }
        count += differs ? 1 : 0;
    }
    return count;
}

static int worst_difference(const uint8_t *a, const uint8_t *b)
{
    int worst = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE * 4U; i++) {
        const int d = abs((int)a[i] - (int)b[i]);
        worst = d > worst ? d : worst;
    }
    return worst;
}

/* Une carte : un rectangle texturé (image `rgba` de w x h), matière découpée au seuil `cutoff`.
 * Debout (x de -1 à 1, y de 0 à 2, face vers +z) ou couchée (x de -2 à 2, z de -1 à 1, vers +y) ;
 * u croît avec x. */
static bool make_card(Fixture *t, const uint8_t *rgba, uint32_t w, uint32_t h, float cutoff,
                      bool lying, EngModelGpu *out)
{
    const EngArenaMark mark = eng_arena_mark(&t->level);
    EngModel model;
    memset(&model, 0, sizeof model);
    model.vertices = ENG_ARENA_NEW_ARRAY(&t->level, EngModelVertex, 4);
    model.indices = ENG_ARENA_NEW_ARRAY(&t->level, uint32_t, 6);
    model.parts = ENG_ARENA_NEW_ARRAY(&t->level, EngModelPart, 1);
    model.materials = ENG_ARENA_NEW_ARRAY(&t->level, EngModelMaterial, 2);
    model.images = ENG_ARENA_NEW_ARRAY(&t->level, EngImage, 1);
    if (model.vertices == NULL || model.indices == NULL || model.parts == NULL ||
        model.materials == NULL || model.images == NULL ||
        !eng_image_create(w, h, rgba, &model.images[0])) {
        return false;
    }
    static const float standing[4][3] = {
        {-1.0F, 2.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {1.0F, 2.0F, 0.0F}};
    static const float flat[4][3] = {
        {-2.0F, 0.0F, -1.0F}, {-2.0F, 0.0F, 1.0F}, {2.0F, 0.0F, 1.0F}, {2.0F, 0.0F, -1.0F}};
    static const float uv[4][2] = {{0.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 1.0F}, {1.0F, 0.0F}};
    for (int k = 0; k < 4; k++) {
        EngModelVertex *v = &model.vertices[k];
        memset(v, 0, sizeof *v);
        memcpy(v->position, lying ? flat[k] : standing[k], 3U * sizeof(float));
        v->normal[lying ? 1 : 2] = 1.0F;
        memcpy(v->uv, uv[k], sizeof uv[k]);
        v->tangent[0] = 1.0F;
        v->tangent[3] = g_tangent_w;
        for (int a = 0; a < 3; a++) {
            model.bounds_min[a] =
                k == 0 ? v->position[a] : fminf(model.bounds_min[a], v->position[a]);
            model.bounds_max[a] =
                k == 0 ? v->position[a] : fmaxf(model.bounds_max[a], v->position[a]);
        }
    }
    const uint32_t indices[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    memcpy(model.indices, indices, sizeof indices);
    const EngModelMaterial material = {.base_color = {1.0F, 1.0F, 1.0F, 1.0F},
                                       .image = 0,
                                       .metallic = 0.0F,
                                       .roughness = 1.0F,
                                       .metallic_roughness_image = -1,
                                       .normal_scale = 1.0F,
                                       .normal_image = -1,
                                       .occlusion_strength = 1.0F,
                                       .occlusion_image = -1,
                                       .emissive_image = -1,
                                       .alpha_cutoff = cutoff,
                                       .double_sided = true};
    model.materials[0] = material;
    model.materials[0].image = -1;
    model.materials[1] = material;
    model.parts[0] = (EngModelPart){.first_index = 0U, .index_count = 6U, .material = 1U};
    model.vertex_count = 4U;
    model.index_count = 6U;
    model.part_count = 1U;
    model.material_count = 2U;
    model.image_count = 1U;
    const bool ok = eng_model_gpu_create(out, &t->r, &t->permanent, &model, 0.0F);
    eng_model_free(&model);
    eng_arena_reset_to(&t->level, mark);
    return ok;
}

/* 8 x 8 : moitié gauche verte opaque, moitié droite rouge transparente */
static void half_texture(uint8_t rgba[8 * 8 * 4])
{
    for (uint32_t i = 0; i < 64U; i++) {
        const bool left = i % 8U < 4U;
        const uint8_t texel[4] = {left ? 40U : 255U, left ? 200U : 0U, 40U, left ? 255U : 0U};
        memcpy(&rgba[(size_t)i * 4U], texel, 4U);
    }
}

/* un réglage sans tramage ni fondu : la découpe est exacte, les comparaisons aussi */
static EngTreeLook exact_look(void)
{
    EngTreeLook look = eng_tree_look_default();
    look.dither_near = 0.0F;
    look.dither_far = 0.0F;
    look.edge_fade = 0.0F;
    return look;
}

/* dessine `count` copies de `model` avec les shaders des arbres ; pixels dans `out` */
static bool draw_trees(Fixture *t, const EngModelGpu *model, const EngInstance *instances,
                       uint32_t count, const EngTreeWind *wind, const EngTreeLook *look,
                       EngPipelineHandle pipeline, uint8_t *out)
{
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    const EngBufferHandle buffer = eng_instances_upload(&t->r, instances, count);
    if (!eng_renderer_buffer_is_valid(&t->r, buffer)) {
        return false;
    }
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    uint32_t accepted;
    if (pipeline.handle.generation == t->tree.handle.generation &&
        pipeline.handle.index == t->tree.handle.index) {
        const EngTreeDraw draw = {.instances = buffer,
                                  .count = count,
                                  .shape = shape_of(model),
                                  .wind = *wind,
                                  .look = look,
                                  .fog = g_fog};
        accepted = eng_tree_draw(model, q, pipeline, view_proj, &light, NULL, &draw);
    } else { /* le témoin : le même dessin instancié par model.frag */
        mat4 identity;
        glm_mat4_identity(identity);
        const EngModelDrawOptions options = {.instance_buffer = buffer, .instance_count = count};
        accepted =
            eng_model_gpu_draw_ex(model, q, pipeline, identity, view_proj, &light, NULL, &options);
    }
    const bool ok = accepted == model->part_count &&
                    render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &t->r, SIZE, out);
    (void)eng_renderer_destroy_buffer(&t->r, buffer);
    return ok;
}

#define TRUNKS 4U

UTEST(eng_tree_gpu, wind_matches_the_cpu_reference_and_an_opaque_part_matches_model_frag)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(fixture_valid(&t));
    const EngModelMaterial bark = {.base_color = {0.7F, 0.5F, 0.3F, 1.0F},
                                   .image = -1,
                                   .metallic = 0.0F,
                                   .roughness = 0.8F,
                                   .metallic_roughness_image = -1,
                                   .normal_scale = 1.0F,
                                   .normal_image = -1,
                                   .occlusion_strength = 1.0F,
                                   .occlusion_image = -1,
                                   .emissive_image = -1};
    const float lo[3] = {-0.35F, 0.0F, -0.35F};
    const float hi[3] = {0.35F, 3.0F, 0.35F};
    EngModel trunk;
    ASSERT_TRUE(eng_model_make_box(&t.level, lo, hi, &bark, &trunk));
    EngModelGpu gpu;
    ASSERT_TRUE(eng_model_gpu_create(&gpu, &t.r, &t.permanent, &trunk, 0.0F));
    EngInstance instances[TRUNKS];
    for (uint32_t i = 0; i < TRUNKS; i++) {
        const float position[3] = {((float)i - 1.5F) * 1.3F, -0.4F, (float)(i % 2U) * -1.0F};
        eng_instance_make_yaw(&instances[i], position, 0.8F + (0.12F * (float)i), 0.7F * (float)i,
                              NULL);
    }
    const EngTreeWind wind = {
        .direction = {1.0F, 0.5F}, .strength = 16.0F, .flutter = 1.2F, .time_s = 1.37F};
    const EngTreeLook look = exact_look();
    g_height_scale = 2.0F; /* le haut des troncs est à mi-hauteur de l'arbre */
    ASSERT_TRUE(draw_trees(&t, &gpu, instances, TRUNKS, &wind, &look, t.tree, g_pixels));
    EXPECT_GT(lit_columns(g_pixels, 0U, SIZE), 600); /* des troncs à l'écran */

    /* référence : un maillage par tronc, chaque sommet déplacé par eng_tree_vertex_world */
    float packed[ENG_TREE_VERTEX_FLOATS];
    const EngTreeShape shape = shape_of(&gpu);
    eng_tree_pack_vertex(&shape, &wind, packed);
    mat4 view_proj;
    mat4 identity;
    EngLighting light;
    camera(view_proj, &light);
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EngModelGpu displaced[TRUNKS];
    for (uint32_t i = 0; i < TRUNKS; i++) {
        const EngArenaMark mark = eng_arena_mark(&t.level);
        EngModel copy = trunk;
        copy.vertices = ENG_ARENA_NEW_ARRAY(&t.level, EngModelVertex, trunk.vertex_count);
        ASSERT_TRUE(copy.vertices != NULL);
        const float qy = instances[i].rotation[1];
        const float qw = instances[i].rotation[3];
        const float cs = (qw * qw) - (qy * qy);
        const float sn = 2.0F * qw * qy;
        for (uint32_t v = 0; v < trunk.vertex_count; v++) {
            copy.vertices[v] = trunk.vertices[v];
            eng_tree_vertex_world(packed, &instances[i], trunk.vertices[v].position,
                                  copy.vertices[v].position);
            /* normale et tangente tournées par le cap de l'instance (rotation autour de Y) */
            const float *n = trunk.vertices[v].normal;
            const float *tangent = trunk.vertices[v].tangent;
            copy.vertices[v].normal[0] = (cs * n[0]) + (sn * n[2]);
            copy.vertices[v].normal[2] = (-sn * n[0]) + (cs * n[2]);
            copy.vertices[v].tangent[0] = (cs * tangent[0]) + (sn * tangent[2]);
            copy.vertices[v].tangent[2] = (-sn * tangent[0]) + (cs * tangent[2]);
        }
        ASSERT_TRUE(eng_model_gpu_create(&displaced[i], &t.r, &t.permanent, &copy, 0.0F));
        ASSERT_EQ(1U, eng_model_gpu_draw(&displaced[i], q, t.plain, identity, view_proj, &light));
        eng_arena_reset_to(&t.level, mark);
    }
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &t.r,
                                       SIZE, g_other));
    dump_ppm(g_pixels, "tree_wind");
    dump_ppm(g_other, "tree_wind_reference");
    /* même image à l'arrondi 8 bits près ; au plus quelques pixels du contour basculent (le sinus
     * du GPU et celui du processeur ne s'arrondissent pas pareil) */
    EXPECT_LE(differing_pixels(g_pixels, g_other, 3), 4);
    /* le vent se voit : sans lui, l'image est très différente */
    const EngTreeWind calm = {.direction = {1.0F, 0.5F}};
    ASSERT_TRUE(draw_trees(&t, &gpu, instances, TRUNKS, &calm, &look, t.tree, g_other));
    EXPECT_GT(worst_difference(g_pixels, g_other), 40);
    EXPECT_GT(differing_pixels(g_pixels, g_other, 3), 300);
    for (uint32_t i = 0; i < TRUNKS; i++) {
        eng_model_gpu_destroy(&displaced[i], &t.r);
    }
    eng_model_gpu_destroy(&gpu, &t.r);
    g_height_scale = 1.0F;
    CLOSE_FIXTURE(t);
}

UTEST(eng_tree_gpu, a_cut_out_part_draws_only_its_opaque_texels_on_both_faces)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(fixture_valid(&t));
    uint8_t texture[8 * 8 * 4];
    half_texture(texture);
    EngModelGpu card;
    ASSERT_TRUE(make_card(&t, texture, 8U, 8U, 0.5F, false, &card));
    ASSERT_EQ(0.5F, card.materials[1].alpha_cutoff); /* le seuil a suivi jusqu'au GPU */
    ASSERT_TRUE(card.materials[1].double_sided);
    const EngTreeWind calm = {.direction = {1.0F, 0.0F}};
    const EngTreeLook look = exact_look();
    EngInstance instance;
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    eng_instance_make_yaw(&instance, origin, 1.0F, 0.0F, NULL);
    /* de face : la moitié gauche de la carte (opaque) est dessinée, la droite ne l'est pas */
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    const int front_left = lit_columns(g_pixels, 0U, SIZE / 2U);
    EXPECT_GT(front_left, 500);
    EXPECT_EQ(0, lit_columns(g_pixels, SIZE / 2U, SIZE));
    /* le témoin, par model.frag : rien n'est découpé, toute la carte est dessinée */
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.instanced, g_other));
    EXPECT_GT(lit_columns(g_other, SIZE / 2U, SIZE), 500);
    /* de dos (demi-tour) : la carte se voit encore, et son image est retournée */
    eng_instance_make_yaw(&instance, origin, 1.0F, 3.14159265F, NULL);
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    EXPECT_EQ(0, lit_columns(g_pixels, 0U, (SIZE / 2U) - 1U));
    EXPECT_GT(lit_columns(g_pixels, SIZE / 2U, SIZE), 500);
    EXPECT_NEAR((double)front_left, (double)lit_columns(g_pixels, SIZE / 2U, SIZE), 40.0);
    /* une matière opaque (seuil 0) du même modèle n'est pas découpée, même par tree.frag */
    EngModelGpu solid;
    ASSERT_TRUE(make_card(&t, texture, 8U, 8U, 0.0F, false, &solid));
    eng_instance_make_yaw(&instance, origin, 1.0F, 0.0F, NULL);
    ASSERT_TRUE(draw_trees(&t, &solid, &instance, 1U, &calm, &look, t.tree, g_pixels));
    EXPECT_GT(lit_columns(g_pixels, SIZE / 2U, SIZE), 500);
    eng_model_gpu_destroy(&solid, &t.r);
    eng_model_gpu_destroy(&card, &t.r);
    CLOSE_FIXTURE(t);
}

UTEST(eng_tree_gpu, the_dithered_threshold_keeps_the_density_of_a_thin_foliage)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(fixture_valid(&t));
    uint8_t thin[8 * 8 * 4];  /* un feuillage fin : chaque texel couvert à 30 % */
    uint8_t dense[8 * 8 * 4]; /* le même, plein */
    for (uint32_t i = 0; i < 64U; i++) {
        memcpy(&thin[(size_t)i * 4U], (const uint8_t[4]){40U, 200U, 40U, 77U}, 4U);
        memcpy(&dense[(size_t)i * 4U], (const uint8_t[4]){40U, 200U, 40U, 255U}, 4U);
    }
    EngModelGpu thin_card;
    EngModelGpu dense_card;
    ASSERT_TRUE(make_card(&t, thin, 8U, 8U, 0.5F, false, &thin_card));
    ASSERT_TRUE(make_card(&t, dense, 8U, 8U, 0.5F, false, &dense_card));
    const EngTreeWind calm = {.direction = {1.0F, 0.0F}};
    EngInstance instance;
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    eng_instance_make_yaw(&instance, origin, 1.0F, 0.0F, NULL);
    EngTreeLook look = exact_look();
    ASSERT_TRUE(draw_trees(&t, &dense_card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    const int whole = lit_columns(g_pixels, 0U, SIZE);
    EXPECT_GT(whole, 1500);
    /* seuil net : 30 % < 50 %, le feuillage fin disparaît entièrement */
    ASSERT_TRUE(draw_trees(&t, &thin_card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    EXPECT_EQ(0, lit_columns(g_pixels, 0U, SIZE));
    /* seuil tramé sur toute sa largeur : 30 % des pixels sont dessinés, la densité est gardée */
    look.dither_near = 1.0F;
    look.dither_far = 1.0F;
    ASSERT_TRUE(draw_trees(&t, &thin_card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    const double kept = (double)lit_columns(g_pixels, 0U, SIZE) / (double)whole;
    EXPECT_NEAR(77.0 / 255.0, kept, 0.05);
    /* et un feuillage plein reste plein : le tramage ne troue pas ce qui est opaque */
    ASSERT_TRUE(draw_trees(&t, &dense_card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    EXPECT_EQ(whole, lit_columns(g_pixels, 0U, SIZE));
    eng_model_gpu_destroy(&thin_card, &t.r);
    eng_model_gpu_destroy(&dense_card, &t.r);
    CLOSE_FIXTURE(t);
}

/* moyenne du canal vert des pixels dessinés (0 si rien ne l'est) */
static double mean_green(const uint8_t *pixels)
{
    double sum = 0.0;
    int count = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        if (pixels[i * 4U] + pixels[(i * 4U) + 1U] + pixels[(i * 4U) + 2U] > 6) {
            sum += (double)pixels[(i * 4U) + 1U];
            count++;
        }
    }
    return count > 0 ? sum / (double)count : 0.0;
}

UTEST(eng_tree_gpu, the_sky_seen_by_a_vertex_darkens_it_whatever_the_sign_of_the_tangent)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(fixture_valid(&t));
    uint8_t leaf[8 * 8 * 4];
    for (uint32_t i = 0; i < 64U; i++) {
        memcpy(&leaf[(size_t)i * 4U], (const uint8_t[4]){60U, 200U, 60U, 255U}, 4U);
    }
    const EngTreeWind calm = {.direction = {1.0F, 0.0F}};
    const EngTreeLook look = exact_look();
    EngInstance instance;
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    eng_instance_make_yaw(&instance, origin, 1.0F, 0.0F, NULL);
    double green[3];
    const float sky[3] = {1.0F, 0.25F, -0.25F}; /* |w| : part du ciel ; signe : la bitangente */
    for (int k = 0; k < 3; k++) {
        g_tangent_w = sky[k];
        EngModelGpu card;
        ASSERT_TRUE(make_card(&t, leaf, 8U, 8U, 0.5F, false, &card));
        ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.tree, g_pixels));
        green[k] = mean_green(g_pixels);
        eng_model_gpu_destroy(&card, &t.r);
    }
    EXPECT_GT(green[0], 30.0);
    EXPECT_GT(green[0], 1.25 * green[1]); /* au cœur du houppier : nettement plus sombre */
    EXPECT_NEAR(green[1], green[2], 1.0); /* le signe ne compte pas pour l'ombre */
    /* les deux occlusions à zéro : la part du ciel n'a plus d'effet */
    EngTreeLook flat = exact_look();
    flat.sky_occlusion = 0.0F;
    flat.sun_occlusion = 0.0F;
    EngModelGpu card;
    ASSERT_TRUE(make_card(&t, leaf, 8U, 8U, 0.5F, false, &card));
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &flat, t.tree, g_pixels));
    EXPECT_NEAR(green[0], mean_green(g_pixels), 1.0);
    /* l'éclat des feuilles : la même carte, deux fois moins éclatante, est plus sombre */
    EngTreeLook dull = exact_look();
    dull.leaf_gain = 0.5F * look.leaf_gain;
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    const double bright = mean_green(g_pixels);
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &dull, t.tree, g_pixels));
    EXPECT_GT(bright, 1.2 * mean_green(g_pixels));
    eng_model_gpu_destroy(&card, &t.r);
    g_tangent_w = 1.0F;
    CLOSE_FIXTURE(t);
}

UTEST(eng_tree_gpu, the_fog_of_the_terrain_covers_the_trees)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(fixture_valid(&t));
    uint8_t leaf[8 * 8 * 4];
    for (uint32_t i = 0; i < 64U; i++) {
        memcpy(&leaf[(size_t)i * 4U], (const uint8_t[4]){60U, 200U, 60U, 255U}, 4U);
    }
    EngModelGpu card;
    ASSERT_TRUE(make_card(&t, leaf, 8U, 8U, 0.5F, false, &card));
    const EngTreeWind calm = {.direction = {1.0F, 0.0F}};
    const EngTreeLook look = exact_look();
    EngInstance instance;
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    eng_instance_make_yaw(&instance, origin, 1.0F, 0.0F, NULL);
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.tree, g_other)); /* sans */
    EngFog fog;
    EngAtmosphere atmosphere;
    eng_fog_default(&fog);
    eng_atmosphere_default(&atmosphere);
    fog.density = 0.3F; /* très épais : à 6 m, les trois quarts de la couleur sont du brouillard */
    fog.height_falloff = 0.0F;
    float packed[ENG_FOG_UNIFORM_FLOATS];
    eng_fog_pack(&fog, &atmosphere, 1.0F, packed);
    g_fog = packed;
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    EXPECT_EQ(lit_columns(g_other, 0U, SIZE), lit_columns(g_pixels, 0U, SIZE)); /* même découpe */
    EXPECT_GT(differing_pixels(g_pixels, g_other, 12), 1500); /* la carte a changé de couleur */
    /* un brouillard coupé (opacité 0) : la même image que sans brouillard */
    fog.max_opacity = 0.0F;
    eng_fog_pack(&fog, &atmosphere, 1.0F, packed);
    ASSERT_TRUE(draw_trees(&t, &card, &instance, 1U, &calm, &look, t.tree, g_pixels));
    EXPECT_EQ(0, differing_pixels(g_pixels, g_other, 0));
    g_fog = NULL;
    eng_model_gpu_destroy(&card, &t.r);
    CLOSE_FIXTURE(t);
}

/* render_to_pixels_prepared appelle `draw` avec le même contexte que `prepare` : la fixture */
static void execute_fixture_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass,
                                  void *context)
{
    Fixture *t = (Fixture *)context;
    (void)eng_renderer_execute(&t->r, &t->r.queue, commands, pass);
}

/* luminosité moyenne (canal rouge, 0 à 1) du sol autour du point (x, z), vu de dessus */
static float ground_brightness(float x, float z)
{
    const float half = 4.0F;
    const int px = (int)(((x + half) / (2.0F * half)) * (float)SIZE);
    const int py = (int)(((z + half) / (2.0F * half)) * (float)SIZE);
    float sum = 0.0F;
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            sum +=
                (float)g_pixels[((((size_t)(py + dy)) * SIZE) + (size_t)(px + dx)) * 4U] / 255.0F;
        }
    }
    return sum / 25.0F;
}

/* Le sol vu de dessus, éclairé par le seul soleil, avec l'ombre d'une carte couchée à 2 m de haut
 * (moitié x < 0 opaque, moitié x > 0 transparente), d'ombre de densité `density`. La carte n'est
 * dessinée que dans la passe d'ombre : on regarde son ombre, pas elle. */
static bool render_card_shadow(Fixture *t, const EngModelGpu *ground, const EngModelGpu *card,
                               float density)
{
    const float to_sun[3] = {0.0F, 0.8F, 0.6F}; /* l'ombre tombe 1,5 m plus loin vers -z */
    const float center[3] = {0.0F, 1.0F, 0.0F};
    EngShadowMap map;
    if (!eng_shadow_fit(&map, to_sun, center, 8.0F, MAP_SIZE)) {
        return false;
    }
    EngLighting light = {0};
    for (int c = 0; c < 3; c++) {
        light.sun_direction[c] = to_sun[c];
        light.sun_color[c] = 1.0F;
        light.ambient_color[c] = 1.0F;
    }
    light.sun_intensity = 1.0F;
    light.camera_position[1] = 20.0F;
    mat4 identity;
    glm_mat4_identity(identity);
    EngInstance instance;
    const float above[3] = {0.0F, 2.0F, 0.0F};
    eng_instance_make_yaw(&instance, above, 1.0F, 0.0F, NULL);
    const EngBufferHandle buffer = eng_instances_upload(&t->r, &instance, 1U);
    EngTreeLook look = exact_look();
    look.shadow_density = density;
    const EngTreeDraw draw = {.instances = buffer,
                              .count = 1U,
                              .shape = {.height = 1.0F, .crown_y = 0.5F, .radius = 2.0F},
                              .wind = {.direction = {1.0F, 0.0F}},
                              .look = &look};
    EngRenderQueue *sq = eng_renderer_begin_shadow_queue(&t->r);
    bool ok = sq != NULL && eng_renderer_buffer_is_valid(&t->r, buffer) &&
              eng_model_gpu_draw_depth(ground, sq, t->shadow, identity, map.view_proj) == 1U &&
              eng_tree_draw_depth(card, sq, t->tree_shadow, map.view_proj, &draw) == 1U;
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    glm_lookat((vec3){0.0F, 20.0F, 0.0F}, (vec3){0.0F, 0.0F, 0.0F}, (vec3){0.0F, 0.0F, -1.0F},
               view);
    glm_ortho(-4.0F, 4.0F, -4.0F, 4.0F, 0.1F, 50.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    EngModelShadow shadow = {.map = t->map, .shadow = map};
    eng_shadow_params_default(&shadow.params);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    ok = ok && eng_model_gpu_draw_shadowed(ground, q, t->plain, identity, view_proj, &light,
                                           &shadow) == 1U;
    ok = ok && render_to_pixels_prepared(&t->f, eng_renderer_depth_format(&t->r), run_shadow_pass,
                                         execute_fixture_queue, t, SIZE, g_pixels);
    if (eng_renderer_buffer_is_valid(&t->r, buffer)) {
        (void)eng_renderer_destroy_buffer(&t->r, buffer);
    }
    return ok;
}

UTEST(eng_tree_gpu, the_shadow_of_a_card_is_cut_out_and_its_density_is_adjustable)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(fixture_valid(&t));
    const EngModelMaterial white = {.base_color = {1.0F, 1.0F, 1.0F, 1.0F},
                                    .image = -1,
                                    .metallic = 0.0F,
                                    .roughness = 1.0F,
                                    .metallic_roughness_image = -1,
                                    .normal_scale = 1.0F,
                                    .normal_image = -1,
                                    .occlusion_strength = 1.0F,
                                    .occlusion_image = -1,
                                    .emissive_image = -1};
    const float lo[3] = {-8.0F, -0.5F, -8.0F};
    const float hi[3] = {8.0F, 0.0F, 8.0F};
    EngModel slab;
    ASSERT_TRUE(eng_model_make_box(&t.level, lo, hi, &white, &slab));
    EngModelGpu ground;
    ASSERT_TRUE(eng_model_gpu_create(&ground, &t.r, &t.permanent, &slab, 0.0F));
    uint8_t texture[8 * 8 * 4];
    half_texture(texture);
    EngModelGpu card;
    ASSERT_TRUE(make_card(&t, texture, 8U, 8U, 0.5F, true, &card));

    ASSERT_TRUE(render_card_shadow(&t, &ground, &card, 1.0F));
    dump_ppm(g_pixels, "tree_shadow");
    const float open = ground_brightness(-1.0F, 2.5F);        /* loin de la carte : plein soleil */
    const float under_leaf = ground_brightness(-1.0F, -1.5F); /* sous la moitié opaque */
    const float under_hole = ground_brightness(1.0F, -1.5F);  /* sous la moitié transparente */
    EXPECT_GT(open, 0.15F);
    EXPECT_LT(under_leaf, 0.1F * open);          /* l'ombre de la feuille */
    EXPECT_NEAR(open, under_hole, 0.05F * open); /* le soleil passe par le trou */

    /* densité 0 : le feuillage ne fait plus d'ombre du tout */
    ASSERT_TRUE(render_card_shadow(&t, &ground, &card, 0.0F));
    EXPECT_NEAR(open, ground_brightness(-1.0F, -1.5F), 0.05F * open);
    /* densité intermédiaire : une pénombre, entre l'ombre et le plein soleil */
    ASSERT_TRUE(render_card_shadow(&t, &ground, &card, 0.5F));
    const float half_shade = ground_brightness(-1.0F, -1.5F);
    EXPECT_GT(half_shade, 0.25F * open);
    EXPECT_LT(half_shade, 0.75F * open);

    eng_model_gpu_destroy(&card, &t.r);
    eng_model_gpu_destroy(&ground, &t.r);
    CLOSE_FIXTURE(t);
}

#endif /* OW_SHADER_DIR */

UTEST(eng_tree_gpu, the_suite_is_present)
{
    ASSERT_TRUE(true); /* sans shaders compilés, les essais ci-dessus n'existent pas */
}
