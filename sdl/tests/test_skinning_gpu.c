#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/model_gpu.h"
#include "engine/pose.h"
#include "test_render_util.h"
#include "utest.h"

/* Skinning GPU (M4-15), pour de vrai : un maillage skinné doit donner, à l'arrondi 8 bits près,
 * la même image que le MÊME maillage déformé sur le CPU par un calcul indépendant (ici, dans le
 * test) et dessiné par le pipeline ordinaire. Sans GPU ou sans shaders compilés, ces tests
 * s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 128U

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle plain;
    EngPipelineHandle skinned;
    EngPipelineHandle skinned_shadow;
} Fixture;

static bool open_fixture(Fixture *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "skinning-gpu", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "skinning-level", (size_t)8 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = (uint32_t)1 << 20,
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
    eng_model_skinned_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->skinned = eng_renderer_create_pipeline(&t->r, &pipeline);
    eng_model_skinned_shadow_pipeline_desc(&pipeline);
    t->skinned_shadow = eng_renderer_create_pipeline(&t->r, &pipeline);
    return true;
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_pipeline(&t->r, t->skinned_shadow);
    (void)eng_renderer_destroy_pipeline(&t->r, t->skinned);
    (void)eng_renderer_destroy_pipeline(&t->r, t->plain);
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->level);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

#define CLOSE_FIXTURE(t) EXPECT_EQ(close_fixture(&(t)), (uint32_t)0)

typedef struct DrawContext {
    EngRenderer *renderer;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    (void)eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];
static uint8_t g_other[SIZE * SIZE * 4];

static int lit_pixels(const uint8_t *pixels)
{
    int count = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        count += (pixels[i * 4] + pixels[(i * 4) + 1] + pixels[(i * 4) + 2]) > 6 ? 1 : 0;
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

static void camera(mat4 view_proj, EngLighting *light)
{
    mat4 view;
    mat4 proj;
    glm_lookat((vec3){2.5F, 2.0F, 5.0F}, (vec3){0.0F, 1.0F, 0.0F}, (vec3){0.0F, 1.0F, 0.0F}, view);
    glm_perspective(glm_rad(50.0F), 1.0F, 0.5F, 50.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    eng_lighting_default(light);
    (void)eng_lighting_set_sun_angles(light, 40.0F, 50.0F);
    light->camera_position[0] = 2.5F;
    light->camera_position[1] = 2.0F;
    light->camera_position[2] = 5.0F;
}

/* Personnage de test : une colonne (boîte 0,6 x 2 x 0,6 m, y de 0 à 2) subdivisée en hauteur par
 * la répartition des poids : sous y = 0,8 tout obéit à l'articulation `low`, au-dessus de
 * y = 1,2 à `high`, entre les deux un fondu linéaire. La boîte n'a que 24 sommets : les sommets
 * des grandes faces latérales sont aux extrémités, le fondu ne s'y voit donc qu'aux arêtes. */
typedef struct Column {
    EngModel model;
    EngSkinVertex skin[24];
    float weight_high[24]; /* poids de `high`, pour l'oracle CPU */
    uint8_t low;
    uint8_t high;
} Column;

static bool make_column(Fixture *t, Column *c, uint8_t low, uint8_t high)
{
    const float lo[3] = {-0.3F, 0.0F, -0.3F};
    const float hi[3] = {0.3F, 2.0F, 0.3F};
    if (!eng_model_make_box(&t->level, lo, hi, NULL, &c->model)) {
        return false;
    }
    c->low = low;
    c->high = high;
    for (uint32_t v = 0; v < c->model.vertex_count; v++) {
        const float y = c->model.vertices[v].position[1];
        /* sommets du haut (y = 2) : 100 % high ; du bas (y = 0) : 100 % low ; milieu : n'existe pas
         * dans une boîte, on mélange donc selon x pour que le fondu soit exercé */
        float w = y > 1.0F ? 1.0F : 0.0F;
        if (c->model.vertices[v].position[0] > 0.0F) {
            w = w > 0.5F ? 0.75F : 0.25F; /* côté +x : sommets partiellement liés à l'autre os */
        }
        const int wh = (int)lroundf(w * 255.0F);
        c->skin[v] = (EngSkinVertex){.joints = {high, low, 0, 0},
                                     .weights = {(uint8_t)wh, (uint8_t)(255 - wh), 0, 0}};
        c->weight_high[v] = (float)wh / 255.0F;
    }
    return true;
}

/* Oracle : les sommets déformés par la palette, sur le CPU, avec les mêmes formules que le shader
 * (position : Σ w M p ; normale / tangente : partie linéaire, renormalisée). */
static void skin_on_cpu(const Column *c, const float *palette, EngModel *out, EngModelVertex *dst)
{
    *out = c->model;
    out->vertices = dst;
    for (uint32_t v = 0; v < c->model.vertex_count; v++) {
        const EngModelVertex *src = &c->model.vertices[v];
        dst[v] = *src;
        float pos[3] = {0, 0, 0};
        float nrm[3] = {0, 0, 0};
        float tan[3] = {0, 0, 0};
        for (int i = 0; i < 4; i++) {
            const float w = (float)c->skin[v].weights[i] / 255.0F;
            if (w == 0.0F) {
                continue;
            }
            mat4 m;
            memcpy(m, &palette[(size_t)c->skin[v].joints[i] * 16U], sizeof m);
            vec3 p;
            vec3 n;
            vec3 tg;
            glm_mat4_mulv3(m, (float *)src->position, 1.0F, p);
            glm_mat4_mulv3(m, (float *)src->normal, 0.0F, n);
            glm_mat4_mulv3(m, (float *)src->tangent, 0.0F, tg);
            for (int a = 0; a < 3; a++) {
                pos[a] += w * p[a];
                nrm[a] += w * n[a];
                tan[a] += w * tg[a];
            }
        }
        glm_vec3_normalize(nrm);
        for (int a = 0; a < 3; a++) {
            dst[v].position[a] = pos[a];
            dst[v].normal[a] = nrm[a];
            dst[v].tangent[a] = tan[a];
        }
    }
}

static void dump_ppm(const uint8_t *pixels, const char *name)
{
    const char *dir = SDL_getenv("OW_SKINNING_DUMP");
    if (dir == NULL) {
        return;
    }
    char path[512];
    SDL_snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return;
    }
    (void)fprintf(f, "P6\n%u %u\n255\n", SIZE, SIZE);
    for (uint32_t i = 0; i < SIZE * SIZE; i++) {
        (void)fwrite(&pixels[(size_t)i * 4U], 1, 3, f);
    }
    (void)fclose(f);
}

/* Dessine `gpu` avec le pipeline `pipeline` (palette éventuelle) dans `pixels`. */
static bool render(Fixture *t, const EngModelGpu *gpu, EngPipelineHandle pipeline,
                   const float *palette, uint32_t joints, uint8_t *pixels)
{
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    mat4 model;
    glm_mat4_identity(model);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const EngModelDrawOptions options = {.skin_matrices = palette, .skin_joint_count = joints};
    if (eng_model_gpu_draw_ex(gpu, q, pipeline, model, view_proj, &light, NULL, &options) !=
        gpu->part_count) {
        return false;
    }
    DrawContext c = {&t->r};
    return render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue, &c, SIZE,
                                  pixels);
}

static void identity_palette(float *out, uint32_t joints)
{
    for (uint32_t j = 0; j < joints; j++) {
        mat4 m;
        glm_mat4_identity(m);
        memcpy(&out[(size_t)j * 16U], m, sizeof m);
    }
}

/* Palette identité : la même image que le maillage ordinaire. */
UTEST(eng_skinning_gpu, an_identity_palette_gives_the_plain_image)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Column col;
    ASSERT_TRUE(make_column(&t, &col, 0, 1));
    EngModelGpu plain;
    EngModelGpu skinned;
    ASSERT_TRUE(eng_model_gpu_create(&plain, &t.r, &t.permanent, &col.model, 0.0F));
    ASSERT_TRUE(
        eng_model_gpu_create_skinned(&skinned, &t.r, &t.permanent, &col.model, col.skin, 0.0F));
    EXPECT_TRUE(skinned.skinned);
    EXPECT_FALSE(plain.skinned);
    float palette[2 * 16];
    identity_palette(palette, 2);
    ASSERT_TRUE(render(&t, &plain, t.plain, NULL, 0, g_other));
    ASSERT_TRUE(render(&t, &skinned, t.skinned, palette, 2, g_pixels));
    dump_ppm(g_other, "column_plain");
    EXPECT_GT(lit_pixels(g_pixels), 700);
    EXPECT_EQ(lit_pixels(g_pixels), lit_pixels(g_other));
    EXPECT_LE(worst_difference(g_pixels, g_other), 2);
    eng_model_gpu_destroy(&skinned, &t.r);
    eng_model_gpu_destroy(&plain, &t.r);
    CLOSE_FIXTURE(t);
}

static void make_palette_move(float *out, uint32_t joints, uint32_t moved, const float t[3],
                              float angle_z)
{
    identity_palette(out, joints);
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (float *)t);
    glm_rotate_z(m, angle_z, m);
    memcpy(&out[(size_t)moved * 16U], m, sizeof m);
}

/* Un os translaté / tourné : la même image que le maillage déformé sur le CPU. */
UTEST(eng_skinning_gpu, a_moved_bone_matches_the_cpu_skinned_mesh)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Column col;
    ASSERT_TRUE(make_column(&t, &col, 0, 1));
    EngModelGpu skinned;
    ASSERT_TRUE(
        eng_model_gpu_create_skinned(&skinned, &t.r, &t.permanent, &col.model, col.skin, 0.0F));
    EngModelVertex deformed[24];
    const float shift[3] = {0.9F, 0.0F, 0.0F};
    const float none[3] = {0.0F, 0.0F, 0.0F};
    const float cases[3][4] = {{shift[0], 0, 0, 0.0F}, /* translation pure */
                               {none[0], 0, 0, 0.9F},  /* rotation pure autour de Z (~52°) */
                               {0.3F, 0.4F, 0.0F, 0.5F} /* les deux */};
    for (int k = 0; k < 3; k++) {
        float palette[2 * 16];
        const float tr[3] = {cases[k][0], cases[k][1], cases[k][2]};
        make_palette_move(palette, 2, 1, tr, cases[k][3]);
        EngModel cpu;
        skin_on_cpu(&col, palette, &cpu, deformed);
        EngModelGpu reference;
        ASSERT_TRUE(eng_model_gpu_create(&reference, &t.r, &t.permanent, &cpu, 0.0F));
        ASSERT_TRUE(render(&t, &reference, t.plain, NULL, 0, g_other));
        ASSERT_TRUE(render(&t, &skinned, t.skinned, palette, 2, g_pixels));
        char name[32];
        SDL_snprintf(name, sizeof name, "column_move_%d", k);
        dump_ppm(g_pixels, name);
        EXPECT_GT(lit_pixels(g_pixels), 800);
        EXPECT_EQ(lit_pixels(g_pixels), lit_pixels(g_other));
        EXPECT_LE(worst_difference(g_pixels, g_other), 3);
        eng_model_gpu_destroy(&reference, &t.r);
    }
    /* et le déplacement se voit : la palette identité donne une autre image */
    float ident[2 * 16];
    identity_palette(ident, 2);
    ASSERT_TRUE(render(&t, &skinned, t.skinned, ident, 2, g_other));
    float moved[2 * 16];
    make_palette_move(moved, 2, 1, shift, 0.0F);
    ASSERT_TRUE(render(&t, &skinned, t.skinned, moved, 2, g_pixels));
    EXPECT_GT(worst_difference(g_pixels, g_other), 40);
    eng_model_gpu_destroy(&skinned, &t.r);
    CLOSE_FIXTURE(t);
}

/* Palette complète (256 articulations, ~12 Ko) et articulation d'indice 255 : le décodage de
 * l'indice (octet normalisé x 255) et la taille maximale du bloc d'uniforms. */
UTEST(eng_skinning_gpu, joint_255_and_a_full_palette_work)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Column col;
    ASSERT_TRUE(make_column(&t, &col, 254, 255));
    EngModelGpu skinned;
    ASSERT_TRUE(
        eng_model_gpu_create_skinned(&skinned, &t.r, &t.permanent, &col.model, col.skin, 0.0F));
    static float palette[256 * 16];
    const float tr[3] = {0.0F, 0.0F, 0.0F};
    make_palette_move(palette, ENG_SKELETON_MAX_JOINTS, 255, tr, 0.7F);
    EngModelVertex deformed[24];
    EngModel cpu;
    skin_on_cpu(&col, palette, &cpu, deformed);
    EngModelGpu reference;
    ASSERT_TRUE(eng_model_gpu_create(&reference, &t.r, &t.permanent, &cpu, 0.0F));
    ASSERT_TRUE(render(&t, &reference, t.plain, NULL, 0, g_other));
    ASSERT_TRUE(render(&t, &skinned, t.skinned, palette, ENG_SKELETON_MAX_JOINTS, g_pixels));
    EXPECT_GT(lit_pixels(g_pixels), 800);
    EXPECT_LE(worst_difference(g_pixels, g_other), 3);
    /* un os mal rangé (indice 254 déplacé au lieu de 255) donnerait une autre image */
    make_palette_move(palette, ENG_SKELETON_MAX_JOINTS, 254, tr, 0.7F);
    ASSERT_TRUE(render(&t, &skinned, t.skinned, palette, ENG_SKELETON_MAX_JOINTS, g_pixels));
    EXPECT_GT(worst_difference(g_pixels, g_other), 20);
    eng_model_gpu_destroy(&reference, &t.r);
    eng_model_gpu_destroy(&skinned, &t.r);
    CLOSE_FIXTURE(t);
}

UTEST(eng_skinning_gpu, drawing_without_a_valid_palette_is_refused)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Column col;
    ASSERT_TRUE(make_column(&t, &col, 0, 1));
    EngModelGpu plain;
    EngModelGpu skinned;
    ASSERT_TRUE(eng_model_gpu_create(&plain, &t.r, &t.permanent, &col.model, 0.0F));
    ASSERT_TRUE(
        eng_model_gpu_create_skinned(&skinned, &t.r, &t.permanent, &col.model, col.skin, 0.0F));
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    mat4 model;
    glm_mat4_identity(model);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    float palette[2 * 16];
    identity_palette(palette, 2);
    EngModelDrawOptions o = {0};
    EXPECT_EQ(eng_model_gpu_draw_ex(&skinned, q, t.skinned, model, view_proj, &light, NULL, &o),
              (uint32_t)0); /* pas de palette */
    o = (EngModelDrawOptions){.skin_matrices = palette, .skin_joint_count = 0};
    EXPECT_EQ(eng_model_gpu_draw_ex(&skinned, q, t.skinned, model, view_proj, &light, NULL, &o),
              (uint32_t)0);
    o.skin_joint_count = ENG_SKELETON_MAX_JOINTS + 1U;
    EXPECT_EQ(eng_model_gpu_draw_ex(&skinned, q, t.skinned, model, view_proj, &light, NULL, &o),
              (uint32_t)0);
    o.skin_joint_count = 2;
    palette[5] = NAN;
    EXPECT_EQ(eng_model_gpu_draw_ex(&skinned, q, t.skinned, model, view_proj, &light, NULL, &o),
              (uint32_t)0);
    palette[5] = 0.0F;
    EXPECT_EQ(eng_model_gpu_draw_ex(&plain, q, t.plain, model, view_proj, &light, NULL, &o),
              (uint32_t)0); /* palette sur un modèle ordinaire */
    o = (EngModelDrawOptions){.skin_matrices = NULL, .skin_joint_count = 2};
    EXPECT_EQ(eng_model_gpu_draw_ex(&plain, q, t.plain, model, view_proj, &light, NULL, &o),
              (uint32_t)0); /* nombre d'os seul */
    o = (EngModelDrawOptions){.skin_matrices = palette, .skin_joint_count = 0};
    EXPECT_EQ(eng_model_gpu_draw_ex(&plain, q, t.plain, model, view_proj, &light, NULL, &o),
              (uint32_t)0); /* matrices seules */
    o = (EngModelDrawOptions){.skin_matrices = palette, .skin_joint_count = 2};
    EXPECT_EQ(q->count, 0U);
    EXPECT_EQ(eng_model_gpu_draw_ex(&skinned, q, t.skinned, model, view_proj, &light, NULL, &o),
              skinned.part_count); /* et une palette valide passe */
    /* bloc de vertex : modèle (3 matrices + couleur = 208 octets) + 48 octets par os */
    EXPECT_EQ(q->commands[0].uniform_size, 208U + (2U * 48U));
    EngModelGpu none;
    EXPECT_FALSE(eng_model_gpu_create_skinned(&none, &t.r, &t.permanent, &col.model, NULL, 0.0F));
    eng_model_gpu_destroy(&skinned, &t.r);
    eng_model_gpu_destroy(&plain, &t.r);
    CLOSE_FIXTURE(t);
}

UTEST(eng_skinning_gpu, the_shadow_pass_accepts_a_palette_and_refuses_the_wrong_model)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Column col;
    ASSERT_TRUE(make_column(&t, &col, 0, 1));
    EngModelGpu plain;
    EngModelGpu skinned;
    ASSERT_TRUE(eng_model_gpu_create(&plain, &t.r, &t.permanent, &col.model, 0.0F));
    ASSERT_TRUE(
        eng_model_gpu_create_skinned(&skinned, &t.r, &t.permanent, &col.model, col.skin, 0.0F));
    EXPECT_TRUE(eng_renderer_pipeline_is_valid(&t.r, t.skinned_shadow));
    float palette[2 * 16];
    identity_palette(palette, 2);
    mat4 model;
    mat4 light;
    glm_mat4_identity(model);
    glm_mat4_identity(light);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(
        eng_model_gpu_draw_depth_skinned(&skinned, q, t.skinned_shadow, model, light, palette, 2),
        skinned.part_count);
    EXPECT_EQ(q->commands[0].uniform_size, 64U + (2U * 48U)); /* matrice + 48 octets par os */
    EXPECT_EQ(
        eng_model_gpu_draw_depth_skinned(&skinned, q, t.skinned_shadow, model, light, NULL, 2),
        (uint32_t)0);
    EXPECT_EQ(
        eng_model_gpu_draw_depth_skinned(&plain, q, t.skinned_shadow, model, light, palette, 2),
        (uint32_t)0);
    EXPECT_EQ(eng_model_gpu_draw_depth(&skinned, q, t.skinned_shadow, model, light), (uint32_t)0);
    eng_model_gpu_destroy(&skinned, &t.r);
    eng_model_gpu_destroy(&plain, &t.r);
    CLOSE_FIXTURE(t);
}

#endif /* OW_SHADER_DIR */
