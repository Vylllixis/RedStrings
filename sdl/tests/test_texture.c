#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/image.h"
#include "engine/math.h"
#include "engine/renderer.h"
#include "test_render_util.h"
#include "test_util.h"
#include "utest.h"

/* Textures et samplers (M1-08), de bout en bout : les pixels sont lus après un vrai rendu (pilote
 * Vulkan logiciel en CI). Sans GPU ou sans shaders compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SMALL 64U
#define LARGE 128U

typedef struct Vertex {
    float position[3];
    float tint[3];
    float uv[2];
} Vertex;

typedef struct TestTex {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
} TestTex;

static bool open_tex(TestTex *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "texture-test", (size_t)8 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 8192,
                                  .depth_buffer = true};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    return true;
}

static uint32_t close_tex(TestTex *t)
{
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

/* Fermeture + vérification : tout a été détruit par le test, aucune fuite. */
#define CLOSE_TEX(t) EXPECT_EQ(close_tex(&(t)), (uint32_t)0)

static EngPipelineHandle make_pipeline(TestTex *t, bool depth)
{
    EngPipelineDesc d = {.name = "textured",
                         .vertex_shader = "textured.vert",
                         .fragment_shader = "textured.frag",
                         .vertex_stride = sizeof(Vertex),
                         .attribute_count = 3,
                         .cull_mode = ENG_CULL_NONE,
                         .color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                         .depth_test = depth,
                         .depth_write = depth};
    d.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, position)};
    d.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, tint)};
    d.attributes[2] = (EngVertexAttribute){
        .location = 2, .format = ENG_VERTEX_FLOAT2, .offset = offsetof(Vertex, uv)};
    return eng_renderer_create_pipeline(&t->r, &d);
}

static EngBufferHandle make_buffer(TestTex *t, EngBufferUsage usage, const void *data,
                                   uint32_t size)
{
    const EngBufferDesc d = {.usage = usage, .size = size, .data = data};
    return eng_renderer_create_buffer(&t->r, &d);
}

/* Deux triangles : le rectangle [x0, x1] x [y0, y1] (NDC, y vers le haut) avec les coordonnées de
 * texture (u0, v0) en haut à gauche et (u1, v1) en bas à droite. */
static void make_quad(Vertex out[6], float x0, float y0, float x1, float y1, float u0, float v0,
                      float u1, float v1)
{
    const float white[3] = {1.0F, 1.0F, 1.0F};
    const Vertex tl = {{x0, y1, 0.0F}, {1, 1, 1}, {u0, v0}};
    const Vertex tr = {{x1, y1, 0.0F}, {1, 1, 1}, {u1, v0}};
    const Vertex bl = {{x0, y0, 0.0F}, {1, 1, 1}, {u0, v1}};
    const Vertex br = {{x1, y0, 0.0F}, {1, 1, 1}, {u1, v1}};
    (void)white;
    out[0] = tl;
    out[1] = bl;
    out[2] = br;
    out[3] = tl;
    out[4] = br;
    out[5] = tr;
}

static EngTextureHandle make_texture(TestTex *t, uint32_t w, uint32_t h, EngTextureFormat format,
                                     const uint8_t *pixels, uint32_t mip_levels)
{
    const EngTextureDesc d = {.name = "test",
                              .width = w,
                              .height = h,
                              .format = format,
                              .pixels = pixels,
                              .mip_levels = mip_levels};
    return eng_renderer_create_texture(&t->r, &d);
}

static EngSamplerHandle make_sampler(TestTex *t, EngFilter filter, EngMipFilter mip,
                                     EngAddressMode address, float anisotropy)
{
    const EngSamplerDesc d = {.name = "test",
                              .min_filter = filter,
                              .mag_filter = filter,
                              .mip_filter = mip,
                              .address_u = address,
                              .address_v = address,
                              .max_anisotropy = anisotropy};
    return eng_renderer_create_sampler(&t->r, &d);
}

typedef struct DrawContext {
    EngRenderer *renderer;
    uint32_t drawn;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    c->drawn = eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

static uint8_t g_pixels[LARGE * LARGE * 4];

static bool render_queue(TestTex *t, uint32_t size, uint32_t *drawn)
{
    DrawContext c = {&t->r, 0};
    const bool ok = render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, size, g_pixels);
    if (drawn != NULL) {
        *drawn = c.drawn;
    }
    return ok;
}

/* Dessine un seul quad texturé avec une matrice identité. */
static bool draw_quad(TestTex *t, uint32_t size, EngPipelineHandle pipe, EngBufferHandle vb,
                      EngTextureHandle tex, EngSamplerHandle smp)
{
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const EngDrawCmd cmd = {
        .pipeline = pipe, .vertex_buffer = vb, .count = 6, .texture = tex, .sampler = smp};
    return eng_render_draw_uniforms(q, &cmd, identity, sizeof identity) &&
           render_queue(t, size, NULL);
}

/* Luminance (canal rouge, les textures des tests sont grises) du pixel (x, y). */
static int gray_at(uint32_t size, uint32_t x, uint32_t y)
{
    return g_pixels[(((size_t)y * size) + x) * 4];
}

/* ── Échantillonnage de base ── */

UTEST(eng_texture, texels_land_where_the_uv_convention_says)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* 2x2 : rouge, vert / bleu, blanc ; (0, 0) est le coin HAUT-GAUCHE de l'image */
    const uint8_t pixels[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle tex = make_texture(&t, 2, 2, ENG_TEXTURE_RGBA8, pixels, 1);
    const EngSamplerHandle smp =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    ASSERT_TRUE(eng_renderer_texture_is_valid(&t.r, tex));
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, tex, smp));
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 16, 16, 255, 0, 0, 2));     /* haut-gauche : rouge */
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 48, 16, 0, 255, 0, 2));     /* haut-droite : vert */
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 16, 48, 0, 0, 255, 2));     /* bas-gauche : bleu */
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 48, 48, 255, 255, 255, 2)); /* bas-droite : blanc */
    (void)eng_renderer_destroy_sampler(&t.r, smp);
    (void)eng_renderer_destroy_texture(&t.r, tex);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, linear_filtering_blends_texels_and_nearest_does_not)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const uint8_t pixels[8] = {0, 0, 0, 255, 255, 255, 255, 255}; /* 2x1 : noir, blanc */
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle tex = make_texture(&t, 2, 1, ENG_TEXTURE_RGBA8, pixels, 1);
    const EngSamplerHandle linear =
        make_sampler(&t, ENG_FILTER_LINEAR, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    const EngSamplerHandle nearest =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, tex, linear));
    /* centres des texels en u = 0,25 (noir) et 0,75 (blanc) : dégradé entre les colonnes 16 et
     * 48, gris moyen au milieu */
    EXPECT_EQ(gray_at(SMALL, 8, 32), 0);
    EXPECT_EQ(gray_at(SMALL, 56, 32), 255);
    EXPECT_TRUE(abs(gray_at(SMALL, 32, 32) - 128) <= 8);
    EXPECT_TRUE(gray_at(SMALL, 24, 32) > 40 && gray_at(SMALL, 24, 32) < 90);
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, tex, nearest));
    for (uint32_t x = 0; x < SMALL; x++) { /* uniquement noir ou blanc, coupure au milieu */
        EXPECT_EQ(gray_at(SMALL, x, 32), x < 32 ? 0 : 255);
    }
    (void)eng_renderer_destroy_sampler(&t.r, linear);
    (void)eng_renderer_destroy_sampler(&t.r, nearest);
    (void)eng_renderer_destroy_texture(&t.r, tex);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, address_modes_repeat_clamp_and_mirror_differ_outside_zero_to_one)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const uint8_t pixels[8] = {255, 0, 0, 255, 0, 0, 255, 255}; /* 2x1 : rouge, bleu */
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 2, 1); /* u va de 0 à 2 : la texture est lue deux fois */
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle tex = make_texture(&t, 2, 1, ENG_TEXTURE_RGBA8, pixels, 1);
    const struct {
        EngAddressMode mode;
        bool expect_red[4]; /* colonnes 8, 24, 40, 56 : u = 0,25 ; 0,75 ; 1,25 ; 1,75 */
    } cases[] = {
        {ENG_ADDRESS_REPEAT, {true, false, true, false}},
        {ENG_ADDRESS_CLAMP, {true, false, false, false}},
        {ENG_ADDRESS_MIRROR, {true, false, false, true}},
    };
    const uint32_t columns[4] = {8, 24, 40, 56};
    for (size_t c = 0; c < 3; c++) {
        const EngSamplerHandle smp =
            make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, cases[c].mode, 0);
        ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, tex, smp));
        for (int i = 0; i < 4; i++) {
            const bool red = pixel_is(g_pixels, SMALL, columns[i], 32, 255, 0, 0, 2);
            const bool blue = pixel_is(g_pixels, SMALL, columns[i], 32, 0, 0, 255, 2);
            EXPECT_TRUE(cases[c].expect_red[i] ? red : blue);
        }
        (void)eng_renderer_destroy_sampler(&t.r, smp);
    }
    (void)eng_renderer_destroy_texture(&t.r, tex);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, each_axis_has_its_own_address_mode)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* 2x2 : rouge, bleu / vert, blanc ; u et v vont de 0 à 2. u répété, v prolongé : la moitié
     * basse de l'écran (v > 1) répète la ligne du BAS de la texture. */
    const uint8_t pixels[16] = {255, 0, 0, 255, 0, 0, 255, 255, 0, 255, 0, 255, 255, 255, 255, 255};
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 2, 2);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle tex = make_texture(&t, 2, 2, ENG_TEXTURE_RGBA8, pixels, 1);
    const EngSamplerDesc desc = {.name = "uv",
                                 .min_filter = ENG_FILTER_NEAREST,
                                 .mag_filter = ENG_FILTER_NEAREST,
                                 .mip_filter = ENG_MIP_OFF,
                                 .address_u = ENG_ADDRESS_REPEAT,
                                 .address_v = ENG_ADDRESS_CLAMP};
    const EngSamplerHandle smp = eng_renderer_create_sampler(&t.r, &desc);
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, tex, smp));
    /* colonnes 8 (u 0,25), 24 (0,75), 40 (1,25 : répété), 56 (1,75) ; lignes 8 (v 0,25 : haut),
     * 24 (v 0,75 : bas), 40 et 56 (v > 1 : prolongé, donc encore la ligne du bas) */
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 8, 8, 255, 0, 0, 2));
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 40, 8, 255, 0, 0, 2)); /* u répété */
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 8, 24, 0, 255, 0, 2));
    EXPECT_TRUE(
        pixel_is(g_pixels, SMALL, 8, 40, 0, 255, 0, 2)); /* v prolongé : pas de retour au haut */
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 40, 56, 0, 255, 0, 2));
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 24, 56, 255, 255, 255, 2));
    (void)eng_renderer_destroy_sampler(&t.r, smp);
    (void)eng_renderer_destroy_texture(&t.r, tex);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

/* ── Mipmaps : le cœur du critère « sans aliasing au loin » ── */

/* Damier 1 pixel noir / blanc, `size` x `size`. Le pire cas pour l'aliasing. */
static void fill_checker(uint8_t *pixels, uint32_t size)
{
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint8_t *p = pixels + (((size_t)y * size) + x) * 4;
            p[0] = p[1] = p[2] = (uint8_t)(((x + y) & 1) != 0 ? 255 : 0);
            p[3] = 255;
        }
    }
}

static uint8_t g_checker[256 * 256 * 4];

/* Plus petit et plus grand gris de la zone [from, to) des deux axes. */
static void gray_range(uint32_t size, uint32_t from, uint32_t to, int *lo, int *hi)
{
    *lo = 255;
    *hi = 0;
    for (uint32_t y = from; y < to; y++) {
        for (uint32_t x = from; x < to; x++) {
            const int v = gray_at(size, x, y);
            *lo = v < *lo ? v : *lo;
            *hi = v > *hi ? v : *hi;
        }
    }
}

UTEST(eng_texture, mipmaps_remove_the_aliasing_of_a_minified_texture)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    fill_checker(g_checker, 256);
    /* Un damier 256 x 256 affiché sur environ 40 pixels : réduction d'un facteur 6,4. */
    Vertex quad[6];
    make_quad(quad, -0.625F, -0.625F, 0.625F, 0.625F, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle with_mips = make_texture(&t, 256, 256, ENG_TEXTURE_RGBA8, g_checker, 0);
    const EngTextureHandle no_mips = make_texture(&t, 256, 256, ENG_TEXTURE_RGBA8, g_checker, 1);
    EXPECT_EQ(eng_renderer_texture_mip_levels(&t.r, with_mips), (uint32_t)9);
    EXPECT_EQ(eng_renderer_texture_mip_levels(&t.r, no_mips), (uint32_t)1);
    const EngSamplerHandle trilinear =
        make_sampler(&t, ENG_FILTER_LINEAR, ENG_MIP_LINEAR, ENG_ADDRESS_CLAMP, 0);
    const EngSamplerHandle base_only =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    int lo;
    int hi;

    /* Avec mipmaps : le damier fin se fond en un gris uniforme, sans scintillement. */
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, with_mips, trilinear));
    gray_range(SMALL, 20, 44, &lo, &hi);
    EXPECT_TRUE(lo >= 124 && hi <= 132);

    /* Sans mipmaps (niveau 0 seul, texel le plus proche) : aliasing, noir et blanc purs en
     * motif irrégulier. Ce test prouve que le test précédent peut échouer. */
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, no_mips, base_only));
    gray_range(SMALL, 20, 44, &lo, &hi);
    EXPECT_TRUE(lo <= 5 && hi >= 250);

    /* Même texture à mipmaps, mais sampler qui les ignore : l'aliasing revient. C'est le
     * sampler qui choisit le niveau. */
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, with_mips, base_only));
    gray_range(SMALL, 20, 44, &lo, &hi);
    EXPECT_TRUE(lo <= 5 && hi >= 250);

    (void)eng_renderer_destroy_sampler(&t.r, trilinear);
    (void)eng_renderer_destroy_sampler(&t.r, base_only);
    (void)eng_renderer_destroy_texture(&t.r, with_mips);
    (void)eng_renderer_destroy_texture(&t.r, no_mips);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, srgb_textures_are_decoded_and_their_mipmaps_average_in_linear_light)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    fill_checker(g_checker, 256);
    uint8_t gray[4 * 4 * 4]; /* 4x4 gris sRGB 128 */
    for (int i = 0; i < 16; i++) {
        gray[(i * 4) + 0] = gray[(i * 4) + 1] = gray[(i * 4) + 2] = 128;
        gray[(i * 4) + 3] = 255;
    }
    Vertex big[6];
    make_quad(big, -1, -1, 1, 1, 0, 0, 1, 1);
    Vertex small[6];
    make_quad(small, -0.625F, -0.625F, 0.625F, 0.625F, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb_big = make_buffer(&t, ENG_BUFFER_VERTEX, big, sizeof big);
    const EngBufferHandle vb_small = make_buffer(&t, ENG_BUFFER_VERTEX, small, sizeof small);
    const EngSamplerHandle trilinear =
        make_sampler(&t, ENG_FILTER_LINEAR, ENG_MIP_LINEAR, ENG_ADDRESS_CLAMP, 0);

    /* Gris sRGB 128 lu en lumière linéaire : 0,2158 -> 55 dans la cible (qui n'est pas sRGB).
     * Le même gris en format linéaire reste 128 : le format fait bien la différence. */
    const EngTextureHandle srgb_gray = make_texture(&t, 4, 4, ENG_TEXTURE_RGBA8_SRGB, gray, 1);
    const EngTextureHandle lin_gray = make_texture(&t, 4, 4, ENG_TEXTURE_RGBA8, gray, 1);
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb_big, srgb_gray, trilinear));
    EXPECT_TRUE(abs(gray_at(SMALL, 32, 32) - 55) <= 3);
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb_big, lin_gray, trilinear));
    EXPECT_TRUE(abs(gray_at(SMALL, 32, 32) - 128) <= 2);

    /* Damier noir/blanc réduit, en sRGB : le gris du niveau de mipmap est 188 (lumière 0,5), que
     * le GPU relit comme 0,5 linéaire soit 128. Moyenner naïvement les valeurs encodées (128)
     * donnerait 55 : trop sombre. */
    const EngTextureHandle srgb_checker =
        make_texture(&t, 256, 256, ENG_TEXTURE_RGBA8_SRGB, g_checker, 0);
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb_small, srgb_checker, trilinear));
    int lo;
    int hi;
    gray_range(SMALL, 20, 44, &lo, &hi);
    EXPECT_TRUE(lo >= 122 && hi <= 134);

    (void)eng_renderer_destroy_texture(&t.r, srgb_checker);
    (void)eng_renderer_destroy_texture(&t.r, srgb_gray);
    (void)eng_renderer_destroy_texture(&t.r, lin_gray);
    (void)eng_renderer_destroy_sampler(&t.r, trilinear);
    (void)eng_renderer_destroy_buffer(&t.r, vb_big);
    (void)eng_renderer_destroy_buffer(&t.r, vb_small);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, nearest_mip_filtering_reads_one_level_and_linear_blends_two)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Bandes verticales de 4 pixels : niveau 0 = bandes de 4, niveau 1 = bandes de 2, niveau 2 =
     * bandes de 1 (plein contraste), niveau 3 et suivants = gris uniforme. Réduction de 6,4 : le
     * GPU vise le niveau 2,68. « Plus proche » lit le niveau 3 (gris) ; « linéaire » mélange le
     * niveau 2 (bandes) et le 3 : il reste du contraste. */
    static uint8_t stripes[256 * 256 * 4];
    for (uint32_t y = 0; y < 256; y++) {
        for (uint32_t x = 0; x < 256; x++) {
            uint8_t *p = stripes + (((size_t)y * 256) + x) * 4;
            p[0] = p[1] = p[2] = (uint8_t)((((x / 4) & 1) != 0) ? 255 : 0);
            p[3] = 255;
        }
    }
    Vertex quad[6];
    make_quad(quad, -0.625F, -0.625F, 0.625F, 0.625F, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle tex = make_texture(&t, 256, 256, ENG_TEXTURE_RGBA8, stripes, 0);
    const EngSamplerHandle nearest_level =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_NEAREST, ENG_ADDRESS_CLAMP, 0);
    const EngSamplerHandle blended =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_LINEAR, ENG_ADDRESS_CLAMP, 0);
    int lo;
    int hi;
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, tex, nearest_level));
    gray_range(SMALL, 20, 44, &lo, &hi);
    EXPECT_TRUE(hi - lo <= 6);
    ASSERT_TRUE(draw_quad(&t, SMALL, pipe, vb, tex, blended));
    gray_range(SMALL, 20, 44, &lo, &hi);
    EXPECT_TRUE(hi - lo >= 30);
    (void)eng_renderer_destroy_sampler(&t.r, nearest_level);
    (void)eng_renderer_destroy_sampler(&t.r, blended);
    (void)eng_renderer_destroy_texture(&t.r, tex);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

/* ── Anisotropie : sol vu de biais ── */

/* Damier de grosses cases (32 pixels sur 256), noir et blanc. */
static void fill_big_checker(uint8_t *pixels, uint32_t size, uint32_t square)
{
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint8_t *p = pixels + (((size_t)y * size) + x) * 4;
            p[0] = p[1] = p[2] = (uint8_t)((((x / square) + (y / square)) & 1) != 0 ? 255 : 0);
            p[3] = 255;
        }
    }
}

/* Contraste moyen sur une ligne d'écran : somme des écarts entre pixels voisins. Plus la texture
 * est floue dans le sens horizontal, plus il est faible. */
static int row_contrast(uint32_t size, uint32_t y)
{
    int total = 0;
    for (uint32_t x = 1; x < size; x++) {
        total += abs(gray_at(size, x, y) - gray_at(size, x - 1, y));
    }
    return total;
}

/* Dessine un sol de damier vu presque à l'horizontale (perspective) avec le sampler donné, et
 * retourne le contraste cumulé de quelques lignes lointaines. */
static int ground_contrast(TestTex *t, EngPipelineHandle pipe, EngTextureHandle tex,
                           EngSamplerHandle smp, EngBufferHandle vb)
{
    mat4 view;
    mat4 proj;
    mat4 mvp;
    eng_look_at((vec3){0.0F, 1.0F, 2.0F}, (vec3){0.0F, 0.85F, -10.0F}, ENG_AXIS_UP, view);
    eng_perspective(glm_rad(60.0F), 1.0F, 0.1F, 500.0F, proj);
    glm_mat4_mul(proj, view, mvp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const EngDrawCmd cmd = {
        .pipeline = pipe, .vertex_buffer = vb, .count = 6, .texture = tex, .sampler = smp};
    if (!eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp) || !render_queue(t, LARGE, NULL)) {
        return -1;
    }
    int total = 0;
    for (uint32_t y = 66; y < 76; y++) { /* sous l'horizon : le sol, loin */
        total += row_contrast(LARGE, y);
    }
    return total;
}

UTEST(eng_texture, anisotropic_filtering_keeps_a_distant_slanted_floor_sharper)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    static uint8_t checker[256 * 256 * 4];
    fill_big_checker(checker, 256, 32);
    /* Sol : x dans [-200, 200], z de -400 à 0 ; une case de la texture = 1 m (u = x, v = z) */
    const Vertex floor_vertices[6] = {
        {{-200, 0, -400}, {1, 1, 1}, {-200, -400}}, {{-200, 0, 0}, {1, 1, 1}, {-200, 0}},
        {{200, 0, 0}, {1, 1, 1}, {200, 0}},         {{-200, 0, -400}, {1, 1, 1}, {-200, -400}},
        {{200, 0, 0}, {1, 1, 1}, {200, 0}},         {{200, 0, -400}, {1, 1, 1}, {200, -400}},
    };
    const EngPipelineHandle pipe = make_pipeline(&t, true);
    const EngBufferHandle vb =
        make_buffer(&t, ENG_BUFFER_VERTEX, floor_vertices, sizeof floor_vertices);
    const EngTextureHandle tex = make_texture(&t, 256, 256, ENG_TEXTURE_RGBA8, checker, 0);
    const EngSamplerHandle isotropic =
        make_sampler(&t, ENG_FILTER_LINEAR, ENG_MIP_LINEAR, ENG_ADDRESS_REPEAT, 0);
    const EngSamplerHandle anisotropic =
        make_sampler(&t, ENG_FILTER_LINEAR, ENG_MIP_LINEAR, ENG_ADDRESS_REPEAT, 16.0F);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    const int iso = ground_contrast(&t, pipe, tex, isotropic, vb);
    const int aniso = ground_contrast(&t, pipe, tex, anisotropic, vb);
    ASSERT_TRUE(iso >= 0 && aniso >= 0);
    EXPECT_TRUE(aniso > iso + iso / 4); /* au moins 25 % de contraste en plus */
    (void)eng_renderer_destroy_sampler(&t.r, isotropic);
    (void)eng_renderer_destroy_sampler(&t.r, anisotropic);
    (void)eng_renderer_destroy_texture(&t.r, tex);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

/* ── Validation, handles, ressources ── */

/* Écrit une valeur hors énumération dans un champ enum (sans cast, que l'analyseur refuse). */
static void set_raw_enum(void *field, int value)
{
    memcpy(field, &value, sizeof value);
}

UTEST(eng_texture, anisotropy_is_capped_at_sixteen)
{
    EXPECT_EQ(eng_clamp_anisotropy(0.0F), 0.0F);
    EXPECT_EQ(eng_clamp_anisotropy(1.0F), 1.0F);
    EXPECT_EQ(eng_clamp_anisotropy(8.0F), 8.0F);
    EXPECT_EQ(eng_clamp_anisotropy(16.0F), 16.0F);
    EXPECT_EQ(eng_clamp_anisotropy(16.5F), 16.0F);
    EXPECT_EQ(eng_clamp_anisotropy(1000.0F), 16.0F);
}

UTEST(eng_texture, invalid_texture_and_sampler_descriptions_are_refused_without_leaking)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    static uint8_t pixels[8 * 8 * 4];
    const EngTextureDesc good = {
        .name = "ok", .width = 8, .height = 8, .format = ENG_TEXTURE_RGBA8, .pixels = pixels};
    EngTextureDesc bad = good;
    bad.width = 0;
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &bad)));
    bad = good;
    bad.height = 0;
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &bad)));
    bad = good;
    bad.pixels = NULL;
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &bad)));
    bad = good;
    bad.format = ENG_TEXTURE_NONE;
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &bad)));
    bad = good;
    bad.mip_levels = 5; /* chaîne complète de 8 x 8 : 4 niveaux */
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &bad)));
    bad = good;
    bad.width = ENG_IMAGE_MAX_SIZE + 1;
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &bad)));
    bad = good;
    bad.height = ENG_IMAGE_MAX_SIZE + 1;
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &bad)));
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, NULL)));
    EXPECT_EQ(eng_pool_count(&t.r.textures), (uint32_t)0); /* aucun emplacement consommé */

    const EngTextureHandle ok = eng_renderer_create_texture(&t.r, &good);
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, ok));
    EXPECT_EQ(eng_renderer_texture_mip_levels(&t.r, ok), (uint32_t)4);
    EXPECT_TRUE(eng_renderer_destroy_texture(&t.r, ok));
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, ok)); /* handle périmé */
    EXPECT_FALSE(eng_renderer_destroy_texture(&t.r, ok));  /* double destruction refusée */
    EXPECT_EQ(eng_renderer_texture_mip_levels(&t.r, ok), (uint32_t)0);

    EngSamplerDesc sampler = {.name = "ko", .max_anisotropy = -1.0F};
    EXPECT_FALSE(eng_renderer_sampler_is_valid(&t.r, eng_renderer_create_sampler(&t.r, &sampler)));
    sampler.max_anisotropy = NAN;
    EXPECT_FALSE(eng_renderer_sampler_is_valid(&t.r, eng_renderer_create_sampler(&t.r, &sampler)));
    sampler.max_anisotropy = 0.0F;
    set_raw_enum(&sampler.address_u, 99);
    EXPECT_FALSE(eng_renderer_sampler_is_valid(&t.r, eng_renderer_create_sampler(&t.r, &sampler)));
    sampler.address_u = ENG_ADDRESS_REPEAT;
    set_raw_enum(&sampler.mip_filter, 9);
    EXPECT_FALSE(eng_renderer_sampler_is_valid(&t.r, eng_renderer_create_sampler(&t.r, &sampler)));
    EXPECT_FALSE(eng_renderer_sampler_is_valid(&t.r, eng_renderer_create_sampler(&t.r, NULL)));
    EXPECT_EQ(eng_pool_count(&t.r.samplers), (uint32_t)0);
    sampler.mip_filter = ENG_MIP_LINEAR;
    sampler.max_anisotropy = 1000.0F; /* plafonné à 16 plutôt que refusé */
    const EngSamplerHandle capped = eng_renderer_create_sampler(&t.r, &sampler);
    EXPECT_TRUE(eng_renderer_sampler_is_valid(&t.r, capped));
    EXPECT_EQ(eng_renderer_sampler_anisotropy(&t.r, capped), 16.0F);
    sampler.max_anisotropy = 4.0F;
    const EngSamplerHandle four = eng_renderer_create_sampler(&t.r, &sampler);
    EXPECT_EQ(eng_renderer_sampler_anisotropy(&t.r, four), 4.0F);
    sampler.max_anisotropy = 1.0F; /* 1 = désactivée */
    const EngSamplerHandle one = eng_renderer_create_sampler(&t.r, &sampler);
    EXPECT_EQ(eng_renderer_sampler_anisotropy(&t.r, one), 0.0F);
    EXPECT_TRUE(eng_renderer_destroy_sampler(&t.r, four));
    EXPECT_TRUE(eng_renderer_destroy_sampler(&t.r, one));
    EXPECT_EQ(eng_renderer_sampler_anisotropy(&t.r, four), 0.0F); /* handle périmé */
    EXPECT_TRUE(eng_renderer_destroy_sampler(&t.r, capped));
    EXPECT_FALSE(eng_renderer_destroy_sampler(&t.r, capped));
    CLOSE_TEX(t);
}

UTEST(eng_texture, the_pool_limits_are_enforced_and_slots_are_reused)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    static uint8_t px[4] = {255, 255, 255, 255};
    const EngTextureDesc desc = {
        .name = "1x1", .width = 1, .height = 1, .format = ENG_TEXTURE_RGBA8, .pixels = px};
    /* la limite par défaut est 256 ; on la dépasse, le refus est propre et rien ne fuit */
    EngTextureHandle handles[257];
    uint32_t created = 0;
    for (uint32_t i = 0; i < 257; i++) {
        handles[i] = eng_renderer_create_texture(&t.r, &desc);
        created += eng_renderer_texture_is_valid(&t.r, handles[i]) ? 1U : 0U;
    }
    EXPECT_EQ(created, (uint32_t)256);
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, handles[256]));
    EXPECT_TRUE(eng_renderer_destroy_texture(&t.r, handles[0]));
    const EngTextureHandle again = eng_renderer_create_texture(&t.r, &desc);
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, again));
    EXPECT_FALSE(
        eng_renderer_texture_is_valid(&t.r, handles[0])); /* l'ancien handle reste périmé */
    for (uint32_t i = 1; i < 256; i++) {
        (void)eng_renderer_destroy_texture(&t.r, handles[i]);
    }
    (void)eng_renderer_destroy_texture(&t.r, again);
    CLOSE_TEX(t);
}

UTEST(eng_texture, draws_with_inconsistent_textures_are_rejected_and_the_others_still_drawn)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const uint8_t pixels[4] = {255, 0, 0, 255}; /* 1 pixel rouge */
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle tex = make_texture(&t, 1, 1, ENG_TEXTURE_RGBA8, pixels, 1);
    const EngTextureHandle stale = make_texture(&t, 1, 1, ENG_TEXTURE_RGBA8, pixels, 1);
    const EngSamplerHandle smp =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    ASSERT_TRUE(eng_renderer_destroy_texture(&t.r, stale));
    mat4 identity;
    glm_mat4_identity(identity);

    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngDrawCmd texture_only = {
        .pipeline = pipe, .vertex_buffer = vb, .count = 6, .texture = tex};
    const EngDrawCmd sampler_only = {
        .pipeline = pipe, .vertex_buffer = vb, .count = 6, .sampler = smp};
    const EngDrawCmd stale_texture = {
        .pipeline = pipe, .vertex_buffer = vb, .count = 6, .texture = stale, .sampler = smp};
    const EngDrawCmd good = {
        .pipeline = pipe, .vertex_buffer = vb, .count = 6, .texture = tex, .sampler = smp};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &texture_only, identity, sizeof identity));
    ASSERT_TRUE(eng_render_draw_uniforms(q, &sampler_only, identity, sizeof identity));
    ASSERT_TRUE(eng_render_draw_uniforms(q, &stale_texture, identity, sizeof identity));
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    EXPECT_EQ(drawn, (uint32_t)0);
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)3);
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 32, 32, 0, 0, 0, 0)); /* rien n'a été dessiné */

    q = eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(eng_render_draw_uniforms(q, &stale_texture, identity, sizeof identity));
    ASSERT_TRUE(eng_render_draw_uniforms(q, &good, identity, sizeof identity));
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1); /* la commande valide passe malgré la mauvaise qui la précède */
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 32, 32, 255, 0, 0, 0));

    (void)eng_renderer_destroy_sampler(&t.r, smp);
    (void)eng_renderer_destroy_texture(&t.r, tex);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, redundant_texture_binds_are_skipped_and_changes_are_counted)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const uint8_t red[4] = {255, 0, 0, 255};
    const uint8_t blue[4] = {0, 0, 255, 255};
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_pipeline(&t, false);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle a = make_texture(&t, 1, 1, ENG_TEXTURE_RGBA8, red, 1);
    const EngTextureHandle b = make_texture(&t, 1, 1, ENG_TEXTURE_RGBA8, blue, 1);
    const EngSamplerHandle s1 =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    const EngSamplerHandle s2 =
        make_sampler(&t, ENG_FILTER_LINEAR, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngTextureHandle textures[5] = {a, a, a, b, b};
    const EngSamplerHandle samplers[5] = {s1, s1, s2, s2, s2};
    for (int i = 0; i < 5; i++) {
        const EngDrawCmd cmd = {.pipeline = pipe,
                                .vertex_buffer = vb,
                                .count = 6,
                                .texture = textures[i],
                                .sampler = samplers[i]};
        ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, identity, sizeof identity));
    }
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    EXPECT_EQ(drawn, (uint32_t)5);
    /* (a,s1) x2 : 1 liaison ; (a,s2) : 2 ; (b,s2) x2 : 3 */
    EXPECT_EQ(t.r.stats.sampler_binds, (uint64_t)3);
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 32, 32, 0, 0, 255, 0)); /* le dernier dessin : bleu */
    (void)eng_renderer_destroy_sampler(&t.r, s1);
    (void)eng_renderer_destroy_sampler(&t.r, s2);
    (void)eng_renderer_destroy_texture(&t.r, a);
    (void)eng_renderer_destroy_texture(&t.r, b);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

/* ── Plusieurs textures par dessin (M1-15) ── */

/* Vrai si le pixel (x, y) vaut (r, g, b, a) à `tolerance` près par canal (alpha compris : le
 * shader multitex y écrit une valeur). */
static bool rgba_near(const uint8_t *rgba, uint32_t size, uint32_t x, uint32_t y, int r, int g,
                      int b, int a, int tolerance)
{
    const uint8_t *p = rgba + (((size_t)y * size) + x) * 4;
    return abs((int)p[0] - r) <= tolerance && abs((int)p[1] - g) <= tolerance &&
           abs((int)p[2] - b) <= tolerance && abs((int)p[3] - a) <= tolerance;
}

static EngPipelineHandle make_multitex_pipeline(TestTex *t)
{
    EngPipelineDesc d = {.name = "multitex",
                         .vertex_shader = "textured.vert",
                         .fragment_shader = "multitex.frag",
                         .vertex_stride = sizeof(Vertex),
                         .attribute_count = 3,
                         .cull_mode = ENG_CULL_NONE,
                         .color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                         .depth_test = false,
                         .depth_write = false};
    d.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, position)};
    d.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, tint)};
    d.attributes[2] = (EngVertexAttribute){
        .location = 2, .format = ENG_VERTEX_FLOAT2, .offset = offsetof(Vertex, uv)};
    return eng_renderer_create_pipeline(&t->r, &d);
}

static EngTextureHandle make_gray(TestTex *t, uint8_t value)
{
    const uint8_t pixel[4] = {value, value, value, 255};
    return make_texture(t, 1, 1, ENG_TEXTURE_RGBA8, pixel, 1);
}

UTEST(eng_texture, five_textures_of_one_draw_land_in_their_own_slots)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_multitex_pipeline(&t);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const uint8_t values[5] = {10, 70, 130, 190, 250};
    EngTextureHandle tex[5];
    for (int i = 0; i < 5; i++) {
        tex[i] = make_gray(&t, values[i]);
    }
    const EngSamplerHandle smp =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngDrawCmd cmd = {.pipeline = pipe,
                            .vertex_buffer = vb,
                            .count = 6,
                            .texture = tex[0],
                            .sampler = smp,
                            .extra_textures = {tex[1], tex[2], tex[3], tex[4]}};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, identity, sizeof identity));
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1);
    /* A = 0,25 * 190 + 0,75 * 250 = 235 */
    EXPECT_TRUE(rgba_near(g_pixels, SMALL, 32, 32, 10, 70, 130, 235, 1));
    /* échanger deux emplacements change le résultat : le test ne passe pas par hasard */
    q = eng_renderer_begin_frame(&t.r);
    const EngDrawCmd swapped = {.pipeline = pipe,
                                .vertex_buffer = vb,
                                .count = 6,
                                .texture = tex[1],
                                .sampler = smp,
                                .extra_textures = {tex[0], tex[2], tex[4], tex[3]}};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &swapped, identity, sizeof identity));
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    /* A = 0,25 * 250 + 0,75 * 190 = 205 */
    EXPECT_TRUE(rgba_near(g_pixels, SMALL, 32, 32, 70, 10, 130, 205, 1));
    for (int i = 0; i < 5; i++) {
        (void)eng_renderer_destroy_texture(&t.r, tex[i]);
    }
    (void)eng_renderer_destroy_sampler(&t.r, smp);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, extra_textures_with_a_gap_a_stale_handle_or_no_sampler_are_rejected)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_multitex_pipeline(&t);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    const EngTextureHandle a = make_gray(&t, 200);
    const EngTextureHandle stale = make_gray(&t, 100);
    const EngSamplerHandle smp =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    ASSERT_TRUE(eng_renderer_destroy_texture(&t.r, stale));
    mat4 identity;
    glm_mat4_identity(identity);
    const EngDrawCmd base = {
        .pipeline = pipe, .vertex_buffer = vb, .count = 6, .texture = a, .sampler = smp};
    EngDrawCmd gap = base;
    gap.extra_textures[1] = a; /* l'emplacement 1 est vide, le 2 est occupé */
    EngDrawCmd no_first = base;
    no_first.texture = ENG_TEXTURE_NULL;
    no_first.sampler = ENG_SAMPLER_NULL;
    no_first.extra_textures[0] = a;
    EngDrawCmd stale_extra = base;
    stale_extra.extra_textures[0] = stale;
    EngDrawCmd no_sampler = base;
    no_sampler.sampler = ENG_SAMPLER_NULL;
    no_sampler.extra_textures[0] = a;
    EngDrawCmd last_only = base;
    last_only.extra_textures[3] = a; /* trou de trois emplacements */
    const EngDrawCmd bad[5] = {gap, no_first, stale_extra, no_sampler, last_only};
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(eng_render_draw_uniforms(q, &bad[i], identity, sizeof identity));
    }
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    EXPECT_EQ(drawn, (uint32_t)0);
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)5);
    EXPECT_TRUE(pixel_is(g_pixels, SMALL, 32, 32, 0, 0, 0, 0));

    /* la même commande, avec des emplacements consécutifs, est dessinée */
    EngDrawCmd ok = base;
    ok.extra_textures[0] = a;
    ok.extra_textures[1] = a;
    ok.extra_textures[2] = a;
    ok.extra_textures[3] = a;
    q = eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(eng_render_draw_uniforms(q, &ok, identity, sizeof identity));
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1);
    EXPECT_TRUE(rgba_near(g_pixels, SMALL, 32, 32, 200, 200, 200, 200, 1));
    (void)eng_renderer_destroy_texture(&t.r, a);
    (void)eng_renderer_destroy_sampler(&t.r, smp);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, texture_set_changes_are_counted_per_bind_not_per_slot)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Vertex quad[6];
    make_quad(quad, -1, -1, 1, 1, 0, 0, 1, 1);
    const EngPipelineHandle pipe = make_multitex_pipeline(&t);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, quad, sizeof quad);
    EngTextureHandle tex[6];
    for (int i = 0; i < 6; i++) {
        tex[i] = make_gray(&t, (uint8_t)(40 * (i + 1)));
    }
    const EngSamplerHandle smp =
        make_sampler(&t, ENG_FILTER_NEAREST, ENG_MIP_OFF, ENG_ADDRESS_CLAMP, 0);
    mat4 identity;
    glm_mat4_identity(identity);
    EngDrawCmd cmds[4];
    for (int i = 0; i < 4; i++) {
        cmds[i] = (EngDrawCmd){.pipeline = pipe,
                               .vertex_buffer = vb,
                               .count = 6,
                               .texture = tex[0],
                               .sampler = smp,
                               .extra_textures = {tex[1], tex[2], tex[3], tex[4]}};
    }
    /* 0 : référence ; 1 : identique (aucune liaison) ; 2 : seul le dernier emplacement change ;
     * 3 : mêmes quatre premières textures que 2, mais une de moins (le nombre de liaisons change)
     */
    cmds[2].extra_textures[3] = tex[5];
    cmds[3].extra_textures[3] = ENG_TEXTURE_NULL;
    cmds[3].extra_textures[2] = tex[3];
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(eng_render_draw_uniforms(q, &cmds[i], identity, sizeof identity));
    }
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, SMALL, &drawn));
    EXPECT_EQ(drawn, (uint32_t)4);
    EXPECT_EQ(t.r.stats.sampler_binds, (uint64_t)3);
    for (int i = 0; i < 6; i++) {
        (void)eng_renderer_destroy_texture(&t.r, tex[i]);
    }
    (void)eng_renderer_destroy_sampler(&t.r, smp);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_TEX(t);
}

UTEST(eng_texture, shutdown_frees_leaked_textures_and_samplers_and_reports_them)
{
    TestTex t;
    if (!open_tex(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const uint8_t px[4] = {1, 2, 3, 255};
    (void)make_texture(&t, 1, 1, ENG_TEXTURE_RGBA8, px, 1);
    (void)make_texture(&t, 1, 1, ENG_TEXTURE_RGBA8_SRGB, px, 1);
    (void)make_sampler(&t, ENG_FILTER_LINEAR, ENG_MIP_LINEAR, ENG_ADDRESS_REPEAT, 4.0F);
    EXPECT_EQ(close_tex(&t), (uint32_t)3);
}

#endif /* OW_SHADER_DIR */
