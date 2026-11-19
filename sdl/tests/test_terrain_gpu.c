#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/atmosphere.h"
#include "engine/fog.h"
#include "engine/heightmap.h"
#include "engine/model_gpu.h"
#include "engine/splatmap.h"
#include "engine/terrain.h"
#include "engine/terrain_gpu.h"
#include "test_render_util.h"
#include "utest.h"

/* Terrain dessiné pour de vrai (M3-02) : un champ 129 x 129 (2 x 2 chunks de 64 cellules) est
 * maillé, envoyé au GPU, dessiné vu d'en haut en projection orthographique, puis les pixels sont
 * lus. Deux critères : AUCUN pixel de fond (le fond est noir, le terrain ne l'est jamais) dans
 * l'emprise, donc pas de trou aux raccords ; et pas de marche d'éclairage de part et d'autre de la
 * jonction entre chunks (la différence entre deux pixels voisins y reste du même ordre que
 * partout ailleurs). Sans GPU ou sans shaders compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 128U
#define SAMPLES 129U
#define MARGIN 2.0F /* mètres rognés de chaque côté : les pixels du bord ne sont pas du terrain */

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle pipeline;
    EngPipelineHandle terrain_pipeline;
} Fixture;

static bool open_fixture(Fixture *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "terrain-gpu", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "terrain-level", (size_t)16 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 16384,
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
    t->pipeline = eng_renderer_create_pipeline(&t->r, &pipeline);
    eng_terrain_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->terrain_pipeline = eng_renderer_create_pipeline(&t->r, &pipeline);
    return true;
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_pipeline(&t->r, t->terrain_pipeline);
    (void)eng_renderer_destroy_pipeline(&t->r, t->pipeline);
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->level);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

typedef struct DrawContext {
    EngRenderer *renderer;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    (void)eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];

static EngLighting sun_light(void)
{
    EngLighting l;
    eng_lighting_default(&l);
    (void)eng_lighting_set_sun_angles(&l, 40.0F, 50.0F);
    l.camera_position[0] = 64.0F;
    l.camera_position[1] = 500.0F;
    l.camera_position[2] = 64.0F;
    return l;
}

/* Fabrique le champ, le terrain, le dessine vu d'en haut et lit les pixels dans g_pixels. Un
 * terrain d'un seul chunk (chunk_cells == 128) ou de 2 x 2 (64) : le rendu doit être le même. */
static bool draw_terrain(Fixture *t, uint32_t chunk_cells, float amplitude, uint32_t octaves)
{
    EngHeightmapParams hp = eng_heightmap_default_params();
    hp.size = SAMPLES;
    hp.altitude_max = amplitude;
    hp.wavelength = 128.0F;
    hp.octaves = octaves;
    EngHeightmap map;
    if (!eng_heightmap_generate(&t->level, &hp, &map)) {
        return false;
    }
    const EngTerrainParams tp = {.chunk_cells = chunk_cells};
    EngTerrain terrain;
    if (!eng_terrain_build(&t->level, &map, &tp, &terrain)) {
        return false;
    }
    EngModelGpu gpu;
    if (!eng_model_gpu_create(&gpu, &t->r, &t->permanent, &terrain.model, 0.0F)) {
        return false;
    }
    mat4 identity;
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    glm_mat4_identity(identity);
    /* Caméra à la verticale du centre, « haut » de l'écran = -Z ; la boîte orthographique couvre
     * l'emprise (0..128 m) rognée de MARGIN. */
    glm_lookat((vec3){64.0F, 500.0F, 64.0F}, (vec3){64.0F, 0.0F, 64.0F}, (vec3){0.0F, 0.0F, -1.0F},
               view);
    const float half = 64.0F - MARGIN;
    glm_ortho(-half, half, -half, half, 1.0F, 1000.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    const EngLighting light = sun_light();
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const uint32_t accepted = eng_model_gpu_draw(&gpu, q, t->pipeline, identity, view_proj, &light);
    DrawContext c = {&t->r};
    const bool ok = accepted == gpu.part_count &&
                    render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, SIZE, g_pixels);
    eng_model_gpu_destroy(&gpu, &t->r);
    return ok;
}

static int luma(uint32_t x, uint32_t y)
{
    const uint8_t *p = &g_pixels[(((size_t)y * SIZE) + x) * 4U];
    return (int)p[0] + (int)p[1] + (int)p[2];
}

static void dump_ppm(const char *name)
{
    const char *dir = SDL_getenv("OW_TERRAIN_DUMP");
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
        (void)fwrite(&g_pixels[(size_t)i * 4U], 1, 3, f);
    }
    (void)fclose(f);
}

#define CLOSE_FIXTURE(t) EXPECT_EQ(close_fixture(&(t)), (uint32_t)0)

UTEST(eng_terrain_gpu, no_background_pixel_between_chunks)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(draw_terrain(&t, 64U, 30.0F, 6U));
    dump_ppm("terrain_2x2");
    uint32_t background = 0;
    for (uint32_t y = 0; y < SIZE; y++) {
        for (uint32_t x = 0; x < SIZE; x++) {
            if (luma(x, y) < 12) {
                background++;
            }
        }
    }
    EXPECT_EQ(background, (uint32_t)0);
    CLOSE_FIXTURE(t);
}

UTEST(eng_terrain_gpu, chunking_does_not_change_the_picture)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    static uint8_t one[SIZE * SIZE * 4];
    ASSERT_TRUE(draw_terrain(&t, 128U, 30.0F, 6U));
    memcpy(one, g_pixels, sizeof one);
    eng_arena_reset(&t.level);
    ASSERT_TRUE(draw_terrain(&t, 64U, 30.0F, 6U));
    dump_ppm("terrain_2x2_b");
    uint32_t different = 0;
    for (uint32_t i = 0; i < SIZE * SIZE * 4U; i++) {
        if (one[i] != g_pixels[i]) {
            different++;
        }
    }
    EXPECT_EQ(different, (uint32_t)0);
    CLOSE_FIXTURE(t);
}

UTEST(eng_terrain_gpu, no_lighting_step_across_the_chunk_joint)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(draw_terrain(&t, 64U, 30.0F, 1U));
    /* Le raccord est à x = z = 64 m : colonne et ligne centrales de l'image. */
    int worst_elsewhere = 0;
    int worst_seam = 0;
    for (uint32_t y = 1; y < SIZE - 1U; y++) {
        for (uint32_t x = 1; x < SIZE - 1U; x++) {
            const int dx = abs(luma(x + 1U, y) - luma(x, y));
            const int dy = abs(luma(x, y + 1U) - luma(x, y));
            const bool seam_x = x + 1U == SIZE / 2U || x == SIZE / 2U;
            const bool seam_y = y + 1U == SIZE / 2U || y == SIZE / 2U;
            if (x + 1U == SIZE / 2U) {
                worst_seam = dx > worst_seam ? dx : worst_seam;
            } else if (!seam_x) {
                worst_elsewhere = dx > worst_elsewhere ? dx : worst_elsewhere;
            }
            if (y + 1U == SIZE / 2U) {
                worst_seam = dy > worst_seam ? dy : worst_seam;
            } else if (!seam_y) {
                worst_elsewhere = dy > worst_elsewhere ? dy : worst_elsewhere;
            }
        }
    }
    EXPECT_LT(worst_elsewhere, 40); /* le relief lisse n'a pas de forte marche : le test est net */
    EXPECT_LE(worst_seam, worst_elsewhere + 2);
    CLOSE_FIXTURE(t);
}

/* ── Terrain mélangé par splatmap (M3-03) ── */

typedef uint16_t (*HeightFn)(uint32_t x, uint32_t z);
typedef void (*SplatFn)(uint32_t x, uint32_t z, uint8_t out[4]);

typedef struct Layers {
    uint8_t rgba[ENG_SPLAT_LAYERS][16 * 16 * 4];
} Layers;

static void fill_layer(Layers *l, uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    for (size_t i = 0; i < (size_t)16 * 16; i++) {
        l->rgba[index][(i * 4)] = r;
        l->rgba[index][(i * 4) + 1] = g;
        l->rgba[index][(i * 4) + 2] = b;
        l->rgba[index][(i * 4) + 3] = 255;
    }
}

/* Vue du dessus sur l'emprise [0, extent] rognée de MARGIN. */
static void top_view(float extent, mat4 out)
{
    mat4 view;
    mat4 proj;
    const float c = extent * 0.5F;
    glm_lookat((vec3){c, 500.0F, c}, (vec3){c, 0.0F, c}, (vec3){0.0F, 0.0F, -1.0F}, view);
    const float half = c - MARGIN;
    glm_ortho(-half, half, -half, half, 1.0F, 1000.0F, proj);
    glm_mat4_mul(proj, view, out);
}

typedef struct SplatScene {
    uint32_t size;
    HeightFn height;
    SplatFn splat;
    const Layers *layers;
    float tile;
    float sharpness;
    const uint32_t *chunks; /* NULL : tous */
    uint32_t chunk_count;
    uint32_t chunk_cells;
    bool use_model_pipeline; /* comparaison : même terrain, matériau uni, pipeline des modèles */
    float model_color[3];    /* facteur de couleur linéaire (use_model_pipeline) */
    uint32_t accepted;       /* sortie : dessins acceptés */
    const EngFog *fog;       /* NULL : pas de brouillard */
    const EngAtmosphere *atmosphere;
    float sky_scale;
} SplatScene;

/* Fabrique champ, terrain, splatmap, ressources GPU, dessine avec `view_proj` et lit g_pixels. */
static bool draw_splat(Fixture *t, SplatScene *sc, mat4 view_proj, const EngLighting *light)
{
    eng_arena_reset(&t->level);
    uint16_t *samples = eng_arena_alloc(&t->level, (size_t)sc->size * sc->size * 2U, 16);
    uint8_t *splat_pixels = eng_arena_alloc(&t->level, (size_t)sc->size * sc->size * 4U, 16);
    if (samples == NULL || splat_pixels == NULL) {
        return false;
    }
    for (uint32_t z = 0; z < sc->size; z++) {
        for (uint32_t x = 0; x < sc->size; x++) {
            samples[((size_t)z * sc->size) + x] = sc->height(x, z);
            sc->splat(x, z, &splat_pixels[(((size_t)z * sc->size) + x) * 4U]);
        }
    }
    const EngHeightmap map = {.size = sc->size,
                              .spacing = 1.0F,
                              .altitude_min = 0.0F,
                              .altitude_max = 65535.0F,
                              .samples = samples};
    EngModelMaterial material = eng_terrain_splat_material();
    if (sc->use_model_pipeline) {
        for (int c = 0; c < 3; c++) {
            material.base_color[c] = sc->model_color[c];
        }
    }
    const EngTerrainParams tp = {.chunk_cells = sc->chunk_cells, .material = &material};
    EngTerrain terrain;
    if (!eng_terrain_build(&t->level, &map, &tp, &terrain)) {
        return false;
    }
    bool ok = false;
    DrawContext c = {&t->r};
    if (sc->use_model_pipeline) {
        EngModelGpu gpu;
        if (!eng_model_gpu_create(&gpu, &t->r, &t->permanent, &terrain.model, 0.0F)) {
            return false;
        }
        EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
        mat4 identity;
        glm_mat4_identity(identity);
        sc->accepted = eng_model_gpu_draw(&gpu, q, t->pipeline, identity, view_proj, light);
        ok = render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue, &c,
                                    SIZE, g_pixels);
        eng_model_gpu_destroy(&gpu, &t->r);
        return ok;
    }
    const EngSplatmap splat = {.size = sc->size, .pixels = splat_pixels};
    EngTerrainGpuParams gp = {.splat = &splat,
                              .layer_size = 16,
                              .tile_meters = sc->tile,
                              .triplanar_sharpness = sc->sharpness};
    for (uint32_t i = 0; i < ENG_SPLAT_LAYERS; i++) {
        gp.layer_pixels[i] = sc->layers->rgba[i];
    }
    EngTerrainGpu gpu;
    if (!eng_terrain_gpu_create(&gpu, &t->r, &t->permanent, &t->level, &terrain, &gp)) {
        return false;
    }
    if (sc->fog != NULL) {
        (void)eng_terrain_gpu_set_fog(&gpu, sc->fog, sc->atmosphere, sc->sky_scale);
    }
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    sc->accepted = eng_terrain_gpu_draw(&gpu, q, t->terrain_pipeline, view_proj, light, NULL,
                                        sc->chunks, sc->chunk_count);
    ok = render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue, &c, SIZE,
                                g_pixels);
    eng_terrain_gpu_destroy(&gpu, &t->r);
    return ok;
}

static uint16_t flat_ground(uint32_t x, uint32_t z)
{
    (void)x;
    (void)z;
    return 100U;
}

static void splat_layer0(uint32_t x, uint32_t z, uint8_t out[4])
{
    (void)x;
    (void)z;
    out[0] = 255;
    out[1] = out[2] = out[3] = 0;
}

static void pixel_rgb(uint32_t x, uint32_t y, int out[3])
{
    const uint8_t *p = &g_pixels[(((size_t)y * SIZE) + x) * 4U];
    out[0] = p[0];
    out[1] = p[1];
    out[2] = p[2];
}

static float srgb_to_linear(uint8_t v)
{
    const float c = (float)v / 255.0F;
    return c <= 0.04045F ? c / 12.92F : powf((c + 0.055F) / 1.055F, 2.4F);
}

/* Le fragment shader du terrain égale celui des modèles : un terrain dont les quatre couches sont
 * la même couleur unie donne, pixel pour pixel (à l'arrondi 8 bits près), le même rendu que le
 * même terrain dessiné par model.frag avec cette couleur comme facteur de base. */
UTEST(eng_terrain_gpu, a_uniform_layer_is_lit_exactly_like_a_plain_model)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Layers layers;
    for (uint32_t i = 0; i < 4; i++) {
        fill_layer(&layers, i, 120, 170, 70);
    }
    mat4 view_proj;
    top_view(64.0F, view_proj);
    const EngLighting light = sun_light();
    SplatScene sc = {.size = 65,
                     .height = flat_ground,
                     .splat = splat_layer0,
                     .layers = &layers,
                     .chunk_cells = 32};
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    static uint8_t terrain_pixels[SIZE * SIZE * 4];
    memcpy(terrain_pixels, g_pixels, sizeof terrain_pixels);
    sc.use_model_pipeline = true;
    sc.model_color[0] = srgb_to_linear(120);
    sc.model_color[1] = srgb_to_linear(170);
    sc.model_color[2] = srgb_to_linear(70);
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    int worst = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        for (size_t c = 0; c < 3; c++) {
            const int d = abs((int)terrain_pixels[(i * 4) + c] - (int)g_pixels[(i * 4) + c]);
            worst = d > worst ? d : worst;
        }
    }
    EXPECT_LE(worst, 2);
    /* et ce n'est pas un rendu vide : le sol est vert (g > r > b) */
    int mid[3];
    pixel_rgb(SIZE / 2, SIZE / 2, mid);
    EXPECT_GT(mid[1], mid[0]);
    EXPECT_GT(mid[0], mid[2]);
    CLOSE_FIXTURE(t);
}

static void splat_pure(uint32_t layer, uint8_t out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0;
    out[layer] = 255;
}

static uint32_t g_layer;
static void splat_chosen(uint32_t x, uint32_t z, uint8_t out[4])
{
    (void)x;
    (void)z;
    splat_pure(g_layer, out);
}

static void splat_chosen_half(uint32_t x, uint32_t z, uint8_t out[4])
{
    (void)z;
    out[0] = out[1] = out[2] = out[3] = 0;
    out[x < 32U ? 0 : 3] = 255; /* gauche : couche 0 ; droite : couche 3 */
}

static void splat_mix_0_2(uint32_t x, uint32_t z, uint8_t out[4])
{
    (void)x;
    (void)z;
    out[0] = 100;
    out[1] = 0;
    out[2] = 155;
    out[3] = 0;
}

/* Chaque canal de la splatmap commande SA couche (ordre herbe, roche, terre, neige) et un mélange
 * est une moyenne pondérée des couleurs d'albédo, avant l'éclairage. */
UTEST(eng_terrain_gpu, each_splat_channel_selects_its_layer_and_mixing_is_a_weighted_average)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Layers layers;
    fill_layer(&layers, 0, 230, 20, 20);  /* herbe : rouge */
    fill_layer(&layers, 1, 20, 230, 20);  /* roche : vert */
    fill_layer(&layers, 2, 20, 20, 230);  /* terre : bleu */
    fill_layer(&layers, 3, 235, 235, 20); /* neige : jaune */
    mat4 view_proj;
    top_view(64.0F, view_proj);
    const EngLighting light = sun_light();
    SplatScene sc = {.size = 65,
                     .height = flat_ground,
                     .splat = splat_chosen,
                     .layers = &layers,
                     .chunk_cells = 32};
    int rgb[4][3];
    for (g_layer = 0; g_layer < 4; g_layer++) {
        ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
        pixel_rgb(SIZE / 2, SIZE / 2, rgb[g_layer]);
    }
    EXPECT_GT(rgb[0][0], rgb[0][1] + 40);
    EXPECT_GT(rgb[0][0], rgb[0][2] + 40);
    EXPECT_GT(rgb[1][1], rgb[1][0] + 40);
    EXPECT_GT(rgb[1][1], rgb[1][2] + 40);
    EXPECT_GT(rgb[2][2], rgb[2][0] + 40);
    EXPECT_GT(rgb[2][2], rgb[2][1] + 40);
    EXPECT_GT(rgb[3][0], rgb[3][2] + 40);
    EXPECT_GT(rgb[3][1], rgb[3][2] + 40);
    EXPECT_NEAR((float)rgb[3][0], (float)rgb[3][1], 6.0F);
    /* mélange 100/255 de la couche 0 et 155/255 de la couche 2 : la moyenne pondérée des couleurs
     * LINÉAIRES de base (l'éclairage est linéaire en l'albédo, à la réflexion spéculaire près,
     * qui ne dépend pas de la couleur d'un diélectrique) est ENTRE les deux rendus purs, au
     * rapport des poids */
    sc.splat = splat_mix_0_2;
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    int mixed[3];
    pixel_rgb(SIZE / 2, SIZE / 2, mixed);
    const float w0 = 100.0F / 255.0F;
    const float w2 = 155.0F / 255.0F;
    for (int c = 0; c < 3; c++) {
        /* la cible est UNORM (pas sRGB) : les octets sont l'éclairage linéaire */
        const float a = (float)rgb[0][c] / 255.0F;
        const float b = (float)rgb[2][c] / 255.0F;
        const float expected = (w0 * a) + (w2 * b);
        EXPECT_NEAR((float)mixed[c] / 255.0F, expected, 0.012F);
    }
    CLOSE_FIXTURE(t);
}

/* La splatmap est alignée sur les sommets : le texel i est au sommet i, donc la transition entre
 * la couche 0 (x < 32) et la couche 3 (x >= 32) est entre les mètres 31 et 32, pas décalée. */
UTEST(eng_terrain_gpu, the_splatmap_texel_is_aligned_with_the_vertex)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Layers layers;
    fill_layer(&layers, 0, 230, 20, 20);
    fill_layer(&layers, 3, 20, 20, 230);
    fill_layer(&layers, 1, 20, 230, 20);
    fill_layer(&layers, 2, 20, 230, 20);
    mat4 view_proj;
    top_view(64.0F, view_proj);
    const EngLighting light = sun_light();
    SplatScene sc = {.size = 65,
                     .height = flat_ground,
                     .splat = splat_chosen_half,
                     .layers = &layers,
                     .chunk_cells = 32};
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    dump_ppm("splat_half");
    /* L'écran couvre x de 2 à 62 m sur 128 pixels : le pixel p est à x = 2 + (p + 0,5) * 60/128.
     * La transition (mélange 0 -> 3) est entre x = 31 et x = 32 : pixels 61,9 à 64,0. */
    const float m_per_px = 60.0F / (float)SIZE;
    for (uint32_t px = 0; px < SIZE; px++) {
        const float x = 2.0F + (((float)px + 0.5F) * m_per_px);
        int rgb[3];
        pixel_rgb(px, SIZE / 2, rgb);
        if (x < 30.9F) {
            ASSERT_GT(rgb[0], rgb[2] + 40); /* rouge */
        } else if (x > 32.1F) {
            ASSERT_GT(rgb[2], rgb[0] + 40); /* bleu */
        }
    }
    /* dans la transition (x de 31 à 32 m, interpolation bilinéaire entre les texels 31 et 32) la
     * part de la couche 3 vaut x - 31 : les canaux rouge et bleu suivent, à la précision près */
    int left[3];
    int right[3];
    pixel_rgb(10, SIZE / 2, left);
    pixel_rgb(SIZE - 10, SIZE / 2, right);
    uint32_t checked = 0;
    for (uint32_t px = 0; px < SIZE; px++) {
        const float x = 2.0F + (((float)px + 0.5F) * m_per_px);
        if (x > 31.05F && x < 31.95F) {
            const float part = x - 31.0F;
            int rgb[3];
            pixel_rgb(px, SIZE / 2, rgb);
            ASSERT_NEAR((float)rgb[0], (float)left[0] * (1.0F - part), 12.0F);
            ASSERT_NEAR((float)rgb[2], (float)right[2] * part, 12.0F);
            checked++;
        }
    }
    EXPECT_GE(checked, 2U); /* la transition de 1 m couvre au moins 2 pixels de 0,47 m */
    CLOSE_FIXTURE(t);
}

/* Falaise : marche de 40 m sur une cellule (x = 31 -> 32), vue de côté. Les textures de couche sont
 * des BANDES horizontales : un mapping (x, z) ferait des traînées verticales (la couleur ne change
 * pas avec la hauteur) ; le triplanaire reprend les bandes sur la paroi : 40 m / 4 m = 10 bandes.
 */
static uint16_t cliff(uint32_t x, uint32_t z)
{
    (void)z;
    return x < 32U ? 0U : 40U;
}

static void splat_rock(uint32_t x, uint32_t z, uint8_t out[4])
{
    (void)x;
    (void)z;
    out[0] = out[2] = out[3] = 0;
    out[1] = 255;
}

static void fill_stripes(Layers *l, uint32_t index)
{
    /* 16 x 16 : les huit premières lignes claires, les huit suivantes sombres (v : lignes) */
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            const uint8_t v = y < 8 ? 235U : 25U;
            uint8_t *p = &l->rgba[index][(((size_t)y * 16) + x) * 4];
            p[0] = p[1] = p[2] = v;
            p[3] = 255;
        }
    }
}

static uint32_t count_bands_on_wall(void)
{
    /* La paroi est à x = 31..32 ; la caméra regarde vers +X, centrée sur (32, 20, 32). Écran : z en
     * horizontal, y en vertical. On suit la colonne centrale entre y = 2 et y = 38 m et on compte
     * les passages clair/sombre. */
    const float half = 30.0F;
    const float m_per_px = (2.0F * half) / (float)SIZE;
    uint32_t transitions = 0;
    int previous = -1;
    int lo = 255;
    int hi = 0;
    for (uint32_t py = 0; py < SIZE; py++) {
        const float y = 20.0F + half - (((float)py + 0.5F) * m_per_px);
        if (y < 2.0F || y > 38.0F) {
            continue;
        }
        const int v = g_pixels[(((size_t)py * SIZE) + (SIZE / 2)) * 4U];
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    const int mid = (lo + hi) / 2;
    for (uint32_t py = 0; py < SIZE; py++) {
        const float y = 20.0F + half - (((float)py + 0.5F) * m_per_px);
        if (y < 2.0F || y > 38.0F) {
            continue;
        }
        const int bright = g_pixels[(((size_t)py * SIZE) + (SIZE / 2)) * 4U] > mid ? 1 : 0;
        if (previous >= 0 && bright != previous) {
            transitions++;
        }
        previous = bright;
    }
    return hi - lo > 60 ? transitions : 9999U;
}

UTEST(eng_terrain_gpu, a_cliff_shows_the_texture_bands_instead_of_vertical_streaks)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Layers layers;
    for (uint32_t i = 0; i < 4; i++) {
        fill_stripes(&layers, i);
    }
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    glm_lookat((vec3){-300.0F, 20.0F, 32.0F}, (vec3){32.0F, 20.0F, 32.0F}, (vec3){0.0F, 1.0F, 0.0F},
               view);
    glm_ortho(-30.0F, 30.0F, -30.0F, 30.0F, 1.0F, 1000.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    EngLighting light = sun_light();
    (void)eng_lighting_set_sun_angles(&light, 270.0F, 35.0F); /* le soleil éclaire la paroi (-X) */
    SplatScene sc = {.size = 65,
                     .height = cliff,
                     .splat = splat_rock,
                     .layers = &layers,
                     .tile = 8.0F, /* 2 bandes par répétition : une bande = 4 m */
                     .sharpness = 4.0F,
                     .chunk_cells = 32};
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    dump_ppm("cliff");
    const uint32_t bands = count_bands_on_wall();
    /* 36 m de paroi visibles / 4 m par bande : 9 passages, à un près */
    EXPECT_GE(bands, 8U);
    EXPECT_LE(bands, 10U);
    CLOSE_FIXTURE(t);
}

/* Dessiner un sous-ensemble de chunks (culling, M3-04) : les autres ne sont pas dessinés. */
UTEST(eng_terrain_gpu, a_chunk_list_draws_only_those_chunks)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Layers layers;
    for (uint32_t i = 0; i < 4; i++) {
        fill_layer(&layers, i, 120, 170, 70);
    }
    mat4 view_proj;
    top_view(64.0F, view_proj);
    const EngLighting light = sun_light();
    /* 2 x 2 chunks de 32 m : l'indice (cz * 2 + cx) ; chunk 1 = (cx 1, cz 0) : x > 32, z < 32 */
    const uint32_t only_one[1] = {1};
    SplatScene sc = {.size = 65,
                     .height = flat_ground,
                     .splat = splat_layer0,
                     .layers = &layers,
                     .chunk_cells = 32,
                     .chunks = only_one,
                     .chunk_count = 1};
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    EXPECT_EQ(sc.accepted, (uint32_t)1);
    /* z croît vers le BAS de l'image (haut de l'écran = -Z) : z < 32 : moitié haute ; x > 32 :
     * moitié droite. Seul le quadrant haut-droit est dessiné. */
    EXPECT_GT(luma(SIZE * 3 / 4, SIZE / 4), 100);
    EXPECT_LT(luma(SIZE / 4, SIZE / 4), 12);
    EXPECT_LT(luma(SIZE / 4, SIZE * 3 / 4), 12);
    EXPECT_LT(luma(SIZE * 3 / 4, SIZE * 3 / 4), 12);
    /* liste vide + indice hors limites : rien */
    const uint32_t nothing[2] = {7, 99};
    sc.chunks = nothing;
    sc.chunk_count = 2;
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    EXPECT_EQ(sc.accepted, (uint32_t)0);
    EXPECT_LT(luma(SIZE * 3 / 4, SIZE / 4), 12);
    /* tous les chunks, dans le désordre : le même dessin que la liste NULL */
    const uint32_t all[4] = {3, 0, 2, 1};
    sc.chunks = all;
    sc.chunk_count = 4;
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    EXPECT_EQ(sc.accepted, (uint32_t)4);
    static uint8_t listed[SIZE * SIZE * 4];
    memcpy(listed, g_pixels, sizeof listed);
    sc.chunks = NULL;
    ASSERT_TRUE(draw_splat(&t, &sc, view_proj, &light));
    EXPECT_EQ(sc.accepted, (uint32_t)4);
    EXPECT_EQ(memcmp(listed, g_pixels, sizeof listed), 0);
    CLOSE_FIXTURE(t);
}

/* Développement : OW_TERRAIN_DUMP=<dossier> écrit terrain_view.ppm, un terrain généré (reliefs
 * marqués) avec la splatmap par défaut et les textures de couche générées, vu en perspective. */
UTEST(eng_terrain_gpu, dump_a_generated_terrain_for_the_eye)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngHeightmapParams hp = eng_heightmap_default_params();
    hp.size = 257;
    hp.altitude_max = 70.0F;
    hp.wavelength = 220.0F;
    hp.octaves = 6;
    EngHeightmap map;
    ASSERT_TRUE(eng_heightmap_generate(&t.level, &hp, &map));
    EngSplatmap splat;
    const EngSplatParams sp = eng_splat_default_params();
    ASSERT_TRUE(eng_splatmap_generate(&t.level, &map, &sp, &splat));
    const EngModelMaterial material = eng_terrain_splat_material();
    const EngTerrainParams tp = {.material = &material};
    EngTerrain terrain;
    ASSERT_TRUE(eng_terrain_build(&t.level, &map, &tp, &terrain));
    const EngTerrainGpuParams gp = {.splat = &splat, .tile_meters = 6.0F};
    EngTerrainGpu gpu;
    ASSERT_TRUE(eng_terrain_gpu_create(&gpu, &t.r, &t.permanent, &t.level, &terrain, &gp));
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    glm_lookat((vec3){128.0F, 150.0F, 330.0F}, (vec3){128.0F, 30.0F, 128.0F},
               (vec3){0.0F, 1.0F, 0.0F}, view);
    glm_perspective(glm_rad(55.0F), 1.0F, 1.0F, 2000.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    EngLighting light = sun_light();
    (void)eng_lighting_set_sun_angles(&light, 35.0F, 40.0F);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_terrain_gpu_draw(&gpu, q, t.terrain_pipeline, view_proj, &light, NULL, NULL, 0),
              (uint32_t)16);
    DrawContext c = {&t.r};
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    dump_ppm("terrain_view");
    /* le même terrain dans le brouillard (M3-06) : le fond de la vue se fond dans l'horizon */
    EngAtmosphere atmosphere;
    eng_atmosphere_default(&atmosphere);
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 0.006F;
    (void)eng_terrain_gpu_set_fog(&gpu, &fog, &atmosphere, 0.35F);
    q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_terrain_gpu_draw(&gpu, q, t.terrain_pipeline, view_proj, &light, NULL, NULL, 0),
              (uint32_t)16);
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    dump_ppm("terrain_fog");
    eng_terrain_gpu_destroy(&gpu, &t.r);
    CLOSE_FIXTURE(t);
}

/* ── Brouillard (M3-06) ── */

#define FOG_EXTENT 64.0F
#define FOG_GROUND 100.0F

/* point du sol vu par le pixel (x, y) de top_view(FOG_EXTENT) : le haut de l'image est -Z */
static void fog_ground_point(uint32_t x, uint32_t y, float out[3])
{
    const float span = FOG_EXTENT - (2.0F * MARGIN);
    out[0] = MARGIN + (((float)x + 0.5F) / (float)SIZE * span);
    out[1] = FOG_GROUND;
    out[2] = MARGIN + (((float)y + 0.5F) / (float)SIZE * span);
}

static bool draw_fog_scene(Fixture *t, const EngFog *fog, const EngAtmosphere *atmosphere,
                           float sky_scale)
{
    static Layers layers;
    for (uint32_t i = 0; i < 4; i++) {
        fill_layer(&layers, i, 120, 170, 70);
    }
    mat4 view_proj;
    top_view(FOG_EXTENT, view_proj);
    const EngLighting light = sun_light();
    SplatScene sc = {.size = 65,
                     .height = flat_ground,
                     .splat = splat_layer0,
                     .layers = &layers,
                     .chunk_cells = 32,
                     .fog = fog,
                     .atmosphere = atmosphere,
                     .sky_scale = sky_scale};
    return draw_splat(t, &sc, view_proj, &light);
}

/* Le shader recopie eng_fog_apply : pixel pour pixel, le rendu brouillardé égale la couleur du
 * rendu sans brouillard passée par eng_fog_apply (même caméra, même soleil), à l'arrondi 8 bits
 * près. Le point de vue est à 400 m au-dessus du sol : la densité décroît avec l'altitude, donc le
 * facteur dépend de la hauteur, de la distance et (par le ciel) de l'azimut. */
UTEST(eng_terrain_gpu, the_fog_matches_the_cpu_reference_pixel_for_pixel)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngAtmosphere atmosphere;
    eng_atmosphere_default(&atmosphere);
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 0.01F;
    ASSERT_TRUE(draw_fog_scene(&t, NULL, NULL, 1.0F));
    static uint8_t plain[SIZE * SIZE * 4];
    memcpy(plain, g_pixels, sizeof plain);
    ASSERT_TRUE(draw_fog_scene(&t, &fog, &atmosphere, 1.0F));
    const EngLighting light = sun_light();
    const float camera[3] = {light.camera_position[0], light.camera_position[1],
                             light.camera_position[2]};
    int worst = 0;
    int changed = 0;
    for (uint32_t y = 0; y < SIZE; y += 3) {
        for (uint32_t x = 0; x < SIZE; x += 3) {
            const size_t i = (((size_t)y * SIZE) + x) * 4U;
            float point[3];
            fog_ground_point(x, y, point);
            const float lit[3] = {(float)plain[i] / 255.0F, (float)plain[i + 1] / 255.0F,
                                  (float)plain[i + 2] / 255.0F};
            float expected[3];
            ASSERT_TRUE(eng_fog_apply(&fog, &atmosphere, light.sun_direction, 1.0F, camera, point,
                                      lit, expected));
            for (size_t c = 0; c < 3; c++) {
                const int e = (int)lroundf(fminf(fmaxf(expected[c], 0.0F), 1.0F) * 255.0F);
                const int d = abs(e - (int)g_pixels[i + c]);
                worst = d > worst ? d : worst;
                if (abs((int)plain[i + c] - (int)g_pixels[i + c]) > 20) {
                    changed++;
                }
            }
        }
    }
    EXPECT_LE(worst, 3);
    EXPECT_GT(changed, 100); /* le brouillard change réellement l'image */
    CLOSE_FIXTURE(t);
}

/* Densité nulle, opacité maximale nulle ou brouillard refusé : l'image est celle sans brouillard.
 */
UTEST(eng_terrain_gpu, no_fog_when_density_or_opacity_is_zero_or_the_fog_is_invalid)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngAtmosphere atmosphere;
    eng_atmosphere_default(&atmosphere);
    ASSERT_TRUE(draw_fog_scene(&t, NULL, NULL, 1.0F));
    static uint8_t plain[SIZE * SIZE * 4];
    memcpy(plain, g_pixels, sizeof plain);
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 0.0F;
    ASSERT_TRUE(draw_fog_scene(&t, &fog, &atmosphere, 1.0F));
    EXPECT_EQ(memcmp(plain, g_pixels, sizeof plain), 0);
    eng_fog_default(&fog);
    fog.density = 0.05F;
    fog.max_opacity = 0.0F;
    ASSERT_TRUE(draw_fog_scene(&t, &fog, &atmosphere, 1.0F));
    EXPECT_EQ(memcmp(plain, g_pixels, sizeof plain), 0);
    eng_fog_default(&fog);
    fog.density = NAN;
    ASSERT_TRUE(draw_fog_scene(&t, &fog, &atmosphere, 1.0F));
    EXPECT_EQ(memcmp(plain, g_pixels, sizeof plain), 0);
    CLOSE_FIXTURE(t);
}

/* Brouillard épais : le sol prend la couleur du ciel à l'horizon (c'est ce qui fait fondre les
 * chunks lointains dans le ciel), à l'échelle d'exposition près. */
UTEST(eng_terrain_gpu, a_thick_fog_turns_the_ground_into_the_horizon_sky_colour)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngAtmosphere atmosphere;
    eng_atmosphere_default(&atmosphere);
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 1.0F;
    fog.height_falloff = 0.0F;
    const float scale = 0.5F;
    ASSERT_TRUE(draw_fog_scene(&t, &fog, &atmosphere, scale));
    const EngLighting light = sun_light();
    int worst = 0;
    int bright = 0;
    for (uint32_t y = 4; y < SIZE; y += 17) {
        for (uint32_t x = 4; x < SIZE; x += 17) {
            float point[3];
            fog_ground_point(x, y, point);
            const float to_point[3] = {point[0] - light.camera_position[0], 0.0F,
                                       point[2] - light.camera_position[2]};
            float horizon[3];
            ASSERT_TRUE(
                eng_atmosphere_horizon(&atmosphere, light.sun_direction, to_point, horizon));
            const size_t i = (((size_t)y * SIZE) + x) * 4U;
            for (size_t c = 0; c < 3; c++) {
                const int e = (int)lroundf(fminf(horizon[c] * scale, 1.0F) * 255.0F);
                const int d = abs(e - (int)g_pixels[i + c]);
                worst = d > worst ? d : worst;
            }
            bright += g_pixels[i + 2] > 40 ? 1 : 0;
        }
    }
    EXPECT_LE(worst, 3);
    EXPECT_GT(bright, 20); /* ce n'est pas un rendu noir : le ciel de l'horizon est bleu clair */
    CLOSE_FIXTURE(t);
}

/* eng_fog_pack : tout à zéro pour un brouillard invalide (le shader le lit comme « coupé »). */
UTEST(eng_terrain_gpu, the_fog_pack_is_zero_for_an_invalid_fog)
{
    EngAtmosphere atmosphere;
    eng_atmosphere_default(&atmosphere);
    EngFog fog;
    eng_fog_default(&fog);
    float out[ENG_FOG_UNIFORM_FLOATS];
    eng_fog_pack(&fog, &atmosphere, 1.0F, out);
    EXPECT_GT(out[0], 0.0F);
    EXPECT_NEAR(out[15], 1.0F, 1e-6F);
    fog.density = -1.0F;
    eng_fog_pack(&fog, &atmosphere, 1.0F, out);
    for (int i = 0; i < (int)ENG_FOG_UNIFORM_FLOATS; i++) {
        EXPECT_EQ(out[i], 0.0F);
    }
}

#endif
