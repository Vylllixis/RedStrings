#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/model.h"
#include "engine/model_gpu.h"
#include "engine/shadow.h"
#include "test_render_util.h"
#include "utest.h"

/* Ombre du soleil de bout en bout (M1-17) : une vraie passe d'ombre (shadow.*), la carte lue par
 * model.frag (PCF), les pixels relus. La scène : un sol (dessus à y = 0) et une boîte de 2 m de
 * côté posée dessus (x, z dans [-1, 1], y dans [0, 2]), vue du dessus par une caméra orthographique
 * (un pixel = un carré du sol). Pas de lumière ambiante : un pixel à l'ombre est NOIR, un pixel
 * éclairé a la couleur du soleil.
 *
 * La référence est GÉOMÉTRIQUE et indépendante du shader : le point P du sol est à l'ombre si le
 * rayon P + t * vers_le_soleil traverse la boîte (test de dalles). On ne compare que les pixels
 * « certains », ceux dont tous les voisins à 3 texels (de la carte) donnent la même réponse : près
 * du bord de l'ombre, la discrétisation de la carte et le filtre rendent le résultat légitimement
 * flou. */

#ifdef OW_SHADER_DIR

#define SIZE 128U
#define MAP_SIZE 256U
#define SPHERE_RADIUS 8.0F
#define VIEW_HALF 6.0F /* la caméra voit [-6, 6] x [-6, 6] mètres du sol */

typedef struct Scene {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle pipeline;
    EngPipelineHandle shadow_pipeline;
    EngTextureHandle map;
    EngModelGpu ground;
    EngModelGpu box;
    bool ok;
} Scene;

static bool make_gpu_box(Scene *s, const float lo[3], const float hi[3], EngModelGpu *out)
{
    EngModelMaterial material = {.base_color = {1.0F, 1.0F, 1.0F, 1.0F},
                                 .image = -1,
                                 .metallic = 0.0F,
                                 .roughness = 1.0F,
                                 .metallic_roughness_image = -1,
                                 .normal_scale = 1.0F,
                                 .normal_image = -1,
                                 .occlusion_strength = 1.0F,
                                 .occlusion_image = -1,
                                 .emissive_image = -1};
    EngModel model;
    if (!eng_model_make_box(&s->level, lo, hi, &material, &model)) {
        return false;
    }
    return eng_model_gpu_create(out, &s->r, &s->permanent, &model, 0.0F);
}

static bool open_scene(Scene *s)
{
    memset(s, 0, sizeof *s);
    if (!open_render_fixture(&s->f)) {
        return false;
    }
    if (!eng_arena_init(&s->permanent, "shadow-test", (size_t)8 << 20) ||
        !eng_arena_init(&s->level, "shadow-level", (size_t)1 << 20)) {
        close_render_fixture(&s->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 16384,
                                  .max_shadow_commands = 64,
                                  .depth_buffer = true};
    if (!eng_renderer_init(&s->r, &s->f.gpu, &s->permanent, &s->f.scratch, &desc)) {
        return false;
    }
    EngPipelineDesc pipeline;
    eng_model_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    s->pipeline = eng_renderer_create_pipeline(&s->r, &pipeline);
    eng_model_shadow_pipeline_desc(&pipeline);
    s->shadow_pipeline = eng_renderer_create_pipeline(&s->r, &pipeline);
    s->map = eng_renderer_create_shadow_map(&s->r, MAP_SIZE);
    const float ground_lo[3] = {-10.0F, -0.5F, -10.0F};
    const float ground_hi[3] = {10.0F, 0.0F, 10.0F};
    const float box_lo[3] = {-1.0F, 0.0F, -1.0F};
    const float box_hi[3] = {1.0F, 2.0F, 1.0F};
    s->ok = eng_renderer_pipeline_is_valid(&s->r, s->pipeline) &&
            eng_renderer_pipeline_is_valid(&s->r, s->shadow_pipeline) &&
            eng_renderer_texture_is_valid(&s->r, s->map) &&
            make_gpu_box(s, ground_lo, ground_hi, &s->ground) &&
            make_gpu_box(s, box_lo, box_hi, &s->box);
    return true;
}

static uint32_t close_scene(Scene *s)
{
    eng_model_gpu_destroy(&s->box, &s->r);
    eng_model_gpu_destroy(&s->ground, &s->r);
    (void)eng_renderer_destroy_texture(&s->r, s->map);
    (void)eng_renderer_destroy_pipeline(&s->r, s->shadow_pipeline);
    (void)eng_renderer_destroy_pipeline(&s->r, s->pipeline);
    const uint32_t leaks = eng_renderer_shutdown(&s->r);
    eng_arena_shutdown(&s->level);
    eng_arena_shutdown(&s->permanent);
    close_render_fixture(&s->f);
    return leaks;
}

/* Un rendu : le soleil, la caméra, les réglages. */
typedef struct Shot {
    float elevation_deg;
    float azimuth_deg; /* 0 : le soleil est du côté +x */
    EngShadowParams params;
    bool shadows;
    float view_half;  /* demi-côté vu par la caméra (m) */
    float view_x;     /* centre de la vue, en x */
    float box_x;      /* décalage de la boîte par sa matrice de modèle (m) */
    EngShadowMap map; /* rempli par render_shot */
    float to_sun[3];
} Shot;

typedef struct ShotContext {
    Scene *scene;
    const Shot *shot;
} ShotContext;

static void shot_prepare(SDL_GPUCommandBuffer *commands, void *context)
{
    ShotContext *c = (ShotContext *)context;
    (void)eng_renderer_run_shadow_pass(&c->scene->r, commands, c->scene->map,
                                       &c->scene->r.shadow_queue);
}

static void shot_draw(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    ShotContext *c = (ShotContext *)context;
    (void)eng_renderer_execute(&c->scene->r, &c->scene->r.queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];

static void sun_vector(float elevation_deg, float azimuth_deg, float out[3])
{
    const float e = glm_rad(elevation_deg);
    const float a = glm_rad(azimuth_deg);
    out[0] = cosf(e) * cosf(a);
    out[1] = sinf(e);
    out[2] = cosf(e) * sinf(a);
}

/* Dessine la scène ; les pixels sont dans g_pixels. */
static bool render_shot(Scene *s, Shot *shot)
{
    sun_vector(shot->elevation_deg, shot->azimuth_deg, shot->to_sun);
    const float center[3] = {0.0F, 1.0F, 0.0F};
    if (!eng_shadow_fit(&shot->map, shot->to_sun, center, SPHERE_RADIUS, MAP_SIZE)) {
        return false;
    }
    EngLighting light = {0};
    for (int c = 0; c < 3; c++) {
        light.sun_direction[c] = shot->to_sun[c];
        light.sun_color[c] = 1.0F;
        light.ambient_color[c] = 1.0F;
    }
    light.sun_intensity = 1.0F;
    light.ambient_intensity = 0.0F;
    light.camera_position[0] = shot->view_x;
    light.camera_position[1] = 20.0F;

    mat4 identity;
    glm_mat4_identity(identity);
    mat4 box_model;
    glm_translate_make(box_model, (vec3){shot->box_x, 0.0F, 0.0F});

    /* passe d'ombre */
    EngRenderQueue *sq = eng_renderer_begin_shadow_queue(&s->r);
    if (sq == NULL ||
        eng_model_gpu_draw_depth(&s->ground, sq, s->shadow_pipeline, identity,
                                 shot->map.view_proj) != 1U ||
        eng_model_gpu_draw_depth(&s->box, sq, s->shadow_pipeline, box_model, shot->map.view_proj) !=
            1U) {
        return false;
    }

    /* image : vue du dessus, x vers la droite, -z vers le haut de l'image */
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    const vec3 eye = {shot->view_x, 20.0F, 0.0F};
    const vec3 target = {shot->view_x, 0.0F, 0.0F};
    const vec3 up = {0.0F, 0.0F, -1.0F};
    glm_lookat((float *)eye, (float *)target, (float *)up, view);
    glm_ortho(-shot->view_half, shot->view_half, -shot->view_half, shot->view_half, 0.1F, 50.0F,
              proj);
    glm_mat4_mul(proj, view, view_proj);

    EngModelShadow shadow = {.map = s->map, .shadow = shot->map, .params = shot->params};
    EngRenderQueue *q = eng_renderer_begin_frame(&s->r);
    const EngModelShadow *used = shot->shadows ? &shadow : NULL;
    if (eng_model_gpu_draw_shadowed(&s->ground, q, s->pipeline, identity, view_proj, &light,
                                    used) != 1U ||
        eng_model_gpu_draw_shadowed(&s->box, q, s->pipeline, box_model, view_proj, &light, used) !=
            1U) {
        return false;
    }
    ShotContext context = {s, shot};
    return render_to_pixels_prepared(&s->f, eng_renderer_depth_format(&s->r), shot_prepare,
                                     shot_draw, &context, SIZE, g_pixels);
}

/* Monde (x, z) du centre du pixel (px, py) sur le sol. */
static void pixel_world(const Shot *shot, uint32_t px, uint32_t py, float *x, float *z)
{
    const float pixel = 2.0F * shot->view_half / (float)SIZE;
    *x = shot->view_x - shot->view_half + (((float)px + 0.5F) * pixel);
    *z = -shot->view_half + (((float)py + 0.5F) * pixel);
}

static float brightness(uint32_t px, uint32_t py)
{
    return (float)g_pixels[(((size_t)py * SIZE) + px) * 4U] / 255.0F;
}

/* Vrai si le rayon de (x, 0, z) vers le soleil traverse la boîte [-1, 1] x [0, 2] x [-1, 1]. */
static bool ground_in_shadow(const float to_sun[3], float x, float z)
{
    const float origin[3] = {x, 0.0F, z};
    const float lo[3] = {-1.0F, 0.0F, -1.0F};
    const float hi[3] = {1.0F, 2.0F, 1.0F};
    float t_near = 1e-3F; /* un rayon qui part du bord de la boîte ne la traverse pas */
    float t_far = 1e9F;
    for (int a = 0; a < 3; a++) {
        if (fabsf(to_sun[a]) < 1e-9F) {
            if (origin[a] < lo[a] || origin[a] > hi[a]) {
                return false;
            }
            continue;
        }
        float t0 = (lo[a] - origin[a]) / to_sun[a];
        float t1 = (hi[a] - origin[a]) / to_sun[a];
        if (t0 > t1) {
            const float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        t_near = t0 > t_near ? t0 : t_near;
        t_far = t1 < t_far ? t1 : t_far;
    }
    return t_near < t_far - 1e-4F;
}

/* Le pixel est-il « certain » : la boîte est-elle au-dessus (pixel de la boîte, pas du sol), et
 * tous les points à `margin` mètres autour donnent-ils la même réponse ? */
static bool certain_ground(const float to_sun[3], float box_x, float x_world, float z, float margin,
                           bool *shadowed)
{
    const float x = x_world - box_x; /* dans le repère de la boîte */
    if (fabsf(x) < 1.0F + margin && fabsf(z) < 1.0F + margin) {
        return false; /* sous la boîte (vue du dessus : le toit de la boîte) ou contre elle */
    }
    *shadowed = ground_in_shadow(to_sun, x, z);
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            if (ground_in_shadow(to_sun, x + ((float)i * margin), z + ((float)j * margin)) !=
                *shadowed) {
                return false;
            }
        }
    }
    return true;
}

typedef struct Stats {
    uint32_t certain_lit;
    uint32_t certain_shadow;
    uint32_t wrong_lit;    /* éclairé attendu, noirci : acné ou ombre fantôme */
    uint32_t wrong_shadow; /* ombre attendue, éclairée : l'ombre manque ou est décollée */
    float lit_reference;   /* luminosité d'un pixel éclairé loin de la boîte */
} Stats;

static Stats compare_with_geometry(const Shot *shot)
{
    Stats st = {0};
    const float margin = 3.0F * shot->map.texel_world;
    float x;
    float z;
    pixel_world(shot, SIZE - 2U, 1U, &x, &z); /* coin loin de la boîte (x > 0, z < 0) */
    st.lit_reference = brightness(SIZE - 2U, 1U);
    for (uint32_t py = 0; py < SIZE; py++) {
        for (uint32_t px = 0; px < SIZE; px++) {
            pixel_world(shot, px, py, &x, &z);
            bool shadowed;
            if (!certain_ground(shot->to_sun, shot->box_x, x, z, margin, &shadowed)) {
                continue;
            }
            const float b = brightness(px, py);
            const bool dark = b < 0.5F * st.lit_reference;
            if (shadowed) {
                st.certain_shadow++;
                st.wrong_shadow += dark ? 0U : 1U;
            } else {
                st.certain_lit++;
                st.wrong_lit += dark ? 1U : 0U;
            }
        }
    }
    return st;
}

#define REQUIRE_SCENE(s)                                                                           \
    if (!open_scene(&(s))) {                                                                       \
        if (render_gpu_required()) {                                                               \
            ASSERT_TRUE(false);                                                                    \
        }                                                                                          \
        return;                                                                                    \
    }                                                                                              \
    ASSERT_TRUE((s).ok)

static Shot default_shot(float elevation_deg, float azimuth_deg)
{
    Shot shot = {.elevation_deg = elevation_deg,
                 .azimuth_deg = azimuth_deg,
                 .shadows = true,
                 .view_half = VIEW_HALF,
                 .view_x = 0.0F};
    eng_shadow_params_default(&shot.params);
    return shot;
}

UTEST(shadow_gpu, the_sun_shadow_of_a_box_lands_where_the_geometry_says)
{
    Scene s;
    REQUIRE_SCENE(s);
    const float elevations[] = {25.0F, 40.0F, 55.0F, 70.0F};
    const float azimuths[] = {0.0F, 37.0F, 130.0F, 250.0F};
    for (uint32_t e = 0; e < 4; e++) {
        for (uint32_t a = 0; a < 4; a++) {
            Shot shot = default_shot(elevations[e], azimuths[a]);
            ASSERT_TRUE(render_shot(&s, &shot));
            const Stats st = compare_with_geometry(&shot);
            EXPECT_GT(st.lit_reference, 0.2F);
            EXPECT_GT(st.certain_lit, 3000U);
            EXPECT_GT(st.certain_shadow, 20U);
            EXPECT_EQ(st.wrong_lit, 0U);    /* pas d'acné, pas d'ombre fantôme */
            EXPECT_EQ(st.wrong_shadow, 0U); /* l'ombre est là */
            if (st.wrong_lit != 0U || st.wrong_shadow != 0U) {
                printf("elevation %.0f azimut %.0f : éclairés noircis %u, ombres éclairées %u\n",
                       (double)elevations[e], (double)azimuths[a], st.wrong_lit, st.wrong_shadow);
            }
        }
    }
    EXPECT_EQ(close_scene(&s), 0U);
}

UTEST(shadow_gpu, without_a_shadow_everything_is_lit)
{
    Scene s;
    REQUIRE_SCENE(s);
    Shot shot = default_shot(40.0F, 20.0F);
    shot.shadows = false;
    ASSERT_TRUE(render_shot(&s, &shot));
    const float reference = brightness(SIZE - 2U, 1U);
    EXPECT_GT(reference, 0.2F);
    uint32_t dark = 0;
    for (uint32_t py = 0; py < SIZE; py++) {
        for (uint32_t px = 0; px < SIZE; px++) {
            float x;
            float z;
            pixel_world(&shot, px, py, &x, &z);
            if (fabsf(x) > 1.2F || fabsf(z) > 1.2F) { /* le sol, pas le toit de la boîte */
                dark += brightness(px, py) < 0.5F * reference ? 1U : 0U;
            }
        }
    }
    EXPECT_EQ(dark, 0U);
    EXPECT_EQ(close_scene(&s), 0U);
}

/* Le sol est éclairé à la même luminosité avec et sans carte d'ombre, loin des ombres : le facteur
 * d'ombre ne doit changer que les pixels ombrés. */
UTEST(shadow_gpu, lit_pixels_keep_exactly_the_unshadowed_brightness)
{
    Scene s;
    REQUIRE_SCENE(s);
    Shot with = default_shot(55.0F, 200.0F);
    ASSERT_TRUE(render_shot(&s, &with));
    uint8_t shadowed_pixels[SIZE * SIZE * 4];
    memcpy(shadowed_pixels, g_pixels, sizeof shadowed_pixels);
    Shot without = with;
    without.shadows = false;
    ASSERT_TRUE(render_shot(&s, &without));
    const float margin = 3.0F * with.map.texel_world;
    uint32_t compared = 0;
    int worst = 0;
    for (uint32_t py = 0; py < SIZE; py++) {
        for (uint32_t px = 0; px < SIZE; px++) {
            float x;
            float z;
            pixel_world(&with, px, py, &x, &z);
            bool shadowed;
            if (certain_ground(with.to_sun, 0.0F, x, z, margin, &shadowed) && !shadowed) {
                const int a = shadowed_pixels[(((size_t)py * SIZE) + px) * 4U];
                const int b = g_pixels[(((size_t)py * SIZE) + px) * 4U];
                worst = abs(a - b) > worst ? abs(a - b) : worst;
                compared++;
            }
        }
    }
    EXPECT_GT(compared, 3000U);
    EXPECT_EQ(worst, 0);
    EXPECT_EQ(close_scene(&s), 0U);
}

/* L'ombre ne se décolle pas de la boîte (peter-panning) : le soleil du côté +x, l'ombre part du
 * pied de la face -x et est noire dès le premier pixel après la boîte. */
UTEST(shadow_gpu, the_shadow_starts_at_the_foot_of_the_box)
{
    Scene s;
    REQUIRE_SCENE(s);
    Shot shot = default_shot(50.0F, 0.0F);
    shot.view_half = 3.0F; /* 3 m de demi-vue : un pixel = 4,7 cm, soit 0,75 texel */
    ASSERT_TRUE(render_shot(&s, &shot));
    const float reference = brightness(SIZE - 2U, 1U);
    /* la ligne du milieu (z = 0), du pied de la boîte (x = -1) vers l'extrémité de l'ombre
     * (x = -1 - 2 / tan(50°) = -2,678) */
    const float foot = -1.0F;
    const float tip = -1.0F - (2.0F / tanf(glm_rad(50.0F)));
    const uint32_t row = SIZE / 2U;
    uint32_t first_dark = SIZE;
    uint32_t last_dark = 0;
    for (uint32_t px = 0; px < SIZE; px++) {
        float x;
        float z;
        pixel_world(&shot, px, row, &x, &z);
        if (x < foot && brightness(px, row) < 0.5F * reference) {
            first_dark = first_dark == SIZE ? px : first_dark;
            last_dark = px;
        }
    }
    ASSERT_LT(first_dark, SIZE);
    float x_first;
    float x_last;
    float z;
    pixel_world(&shot, first_dark, row, &x_first, &z);
    pixel_world(&shot, last_dark, row, &x_last, &z);
    const float texel = shot.map.texel_world;
    /* l'ombre s'étend de l'extrémité au pied, à moins de 2 texels de chaque bout */
    EXPECT_LT(fabsf(x_first - tip), 2.0F * texel);
    EXPECT_LT(fabsf(foot - x_last), 2.0F * texel);
    EXPECT_EQ(close_scene(&s), 0U);
}

/* Longueur de la pénombre au bord de l'ombre (extrémité, côté -x) en texels, sur la ligne z = 0 :
 * de 10 % à 90 % de la luminosité éclairée. */
static float penumbra_width_texels(Scene *s, uint32_t radius)
{
    Shot shot = default_shot(50.0F, 0.0F);
    shot.params.pcf_radius = radius;
    shot.view_half = 0.8F;
    shot.view_x = -2.678F; /* centrée sur l'extrémité de l'ombre */
    if (!render_shot(s, &shot)) {
        return -1.0F;
    }
    const float reference = brightness(1U, 1U); /* x = -3,4 : bien au soleil */
    const uint32_t row = SIZE / 2U;
    /* luminosité en fonction de x (croissante vers -x) : positions des franchissements */
    float x10 = 0.0F;
    float x90 = 0.0F;
    bool have10 = false;
    bool have90 = false;
    for (uint32_t px = 0; px + 1U < SIZE; px++) {
        const float b0 = brightness(px, row) / reference;
        const float b1 = brightness(px + 1U, row) / reference;
        float x0;
        float x1;
        float z;
        pixel_world(&shot, px, row, &x0, &z);
        pixel_world(&shot, px + 1U, row, &x1, &z);
        /* le long de +x on passe de la lumière (b ~ 1) à l'ombre (b ~ 0), vers le pied de la boîte
         */
        if (!have90 && b0 >= 0.9F && b1 < 0.9F) {
            x90 = x0 + ((x1 - x0) * ((b0 - 0.9F) / (b0 - b1)));
            have90 = true;
        }
        if (!have10 && b0 >= 0.1F && b1 < 0.1F) {
            x10 = x0 + ((x1 - x0) * ((b0 - 0.1F) / (b0 - b1)));
            have10 = true;
        }
    }
    if (!have10 || !have90) {
        return -1.0F;
    }
    return (x10 - x90) / shot.map.texel_world;
}

UTEST(shadow_gpu, the_penumbra_widens_with_the_pcf_radius)
{
    Scene s;
    REQUIRE_SCENE(s);
    float widths[4];
    for (uint32_t r = 0; r <= 3; r++) {
        widths[r] = penumbra_width_texels(&s, r);
        ASSERT_GE(widths[r], 0.0F);
        printf("rayon %u : pénombre 10-90 %% = %.2f texels\n", r, (double)widths[r]);
    }
    /* rayon 0 : une seule prise bilinéaire, une rampe d'un texel au plus (10-90 % : 0,8) */
    EXPECT_LT(widths[0], 1.3F);
    for (uint32_t r = 1; r <= 3; r++) {
        EXPECT_GT(widths[r], widths[r - 1U] + 1.2F);
    }
    /* rayon 3 : 7 prises, chacune une rampe bilinéaire d'un texel : 8 texels entre 0 et 100 %,
     * entre 10 et 90 % 7,3 mesurés (rayons 0 à 3 : 1,05 ; 3,14 ; 5,24 ; 7,29, soit 2,1 par rayon)
     */
    EXPECT_GT(widths[3], 4.0F);
    EXPECT_LT(widths[3], 8.5F);
    EXPECT_EQ(close_scene(&s), 0U);
}

UTEST(shadow_gpu, strength_scales_the_light_removed_in_the_shadow)
{
    Scene s;
    REQUIRE_SCENE(s);
    const float strengths[] = {1.0F, 0.5F, 0.0F};
    for (uint32_t i = 0; i < 3; i++) {
        Shot shot = default_shot(50.0F, 0.0F);
        shot.params.strength = strengths[i];
        ASSERT_TRUE(render_shot(&s, &shot));
        const float reference = brightness(SIZE - 2U, 1U);
        /* un pixel bien dans l'ombre : x = -1,6, z = 0 */
        const float pixel = 2.0F * shot.view_half / (float)SIZE;
        const uint32_t px = (uint32_t)((-1.6F + shot.view_half) / pixel);
        const float shade = brightness(px, SIZE / 2U) / reference;
        EXPECT_NEAR(shade, 1.0F - strengths[i], 0.02F);
    }
    EXPECT_EQ(close_scene(&s), 0U);
}

/* La carte ne couvre qu'une sphère de 8 m : au-delà, la lumière n'est pas arrêtée (documenté dans
 * docs/shadows.md), jamais noircie par des bords prolongés. Soleil à 7° du côté -x : l'ombre de la
 * boîte, longue de 16 m, traverse le bord de la carte vers x = 8,1 m. Le biais de pente, plafonné à
 * 8 en tangente, vaut ici 17 texels de profondeur (1,07 m) : il ramène dans la carte les points qui
 * la dépassent d'autant, l'ombre se prolonge donc jusqu'à x = 9,15 m (mesuré) ; au-delà, lumière.
 */
UTEST(shadow_gpu, ground_beyond_the_map_stays_lit)
{
    Scene s;
    REQUIRE_SCENE(s);
    Shot shot = default_shot(7.0F, 180.0F);
    shot.view_half = 3.0F;
    shot.view_x = 8.0F; /* x dans [5 ; 11] ; le sol s'arrête à x = 10 */
    ASSERT_TRUE(render_shot(&s, &shot));
    const uint32_t row = SIZE / 2U;
    float x;
    float z;
    uint32_t dark_inside = 0;
    uint32_t total_inside = 0;
    uint32_t dark_beyond = 0;
    uint32_t total_beyond = 0;
    float reference = 0.0F;
    for (uint32_t px = 0; px < SIZE; px++) {
        pixel_world(&shot, px, row, &x, &z);
        if (x > 9.3F && x < 9.8F) {
            reference = brightness(px, row);
        }
    }
    EXPECT_GT(reference, 0.05F);
    for (uint32_t px = 0; px < SIZE; px++) {
        pixel_world(&shot, px, row, &x, &z);
        if (x > 5.2F && x < 7.6F) {
            total_inside++;
            dark_inside += brightness(px, row) < 0.5F * reference ? 1U : 0U;
        } else if (x > 9.3F && x < 9.8F) {
            total_beyond++;
            dark_beyond += brightness(px, row) < 0.5F * reference ? 1U : 0U;
        }
    }
    EXPECT_GT(total_inside, 20U);
    EXPECT_GT(total_beyond, 8U);
    EXPECT_EQ(dark_inside, total_inside); /* dans la carte : l'ombre de la boîte est là */
    EXPECT_EQ(dark_beyond, 0U);           /* au-delà : lumière */
    EXPECT_EQ(close_scene(&s), 0U);
}

/* Le toit de la boîte (au soleil) et son flanc éclairé ne portent pas d'ombre sur eux-mêmes : mêmes
 * critères que le sol, avec une vue du dessus sur le toit. */
UTEST(shadow_gpu, the_roof_of_the_box_has_no_acne)
{
    Scene s;
    REQUIRE_SCENE(s);
    const float elevations[] = {20.0F, 45.0F, 80.0F};
    for (uint32_t i = 0; i < 3; i++) {
        Shot shot = default_shot(elevations[i], 60.0F);
        ASSERT_TRUE(render_shot(&s, &shot));
        const float reference = brightness(SIZE - 2U, 1U);
        uint32_t dark = 0;
        uint32_t total = 0;
        for (uint32_t py = 0; py < SIZE; py++) {
            for (uint32_t px = 0; px < SIZE; px++) {
                float x;
                float z;
                pixel_world(&shot, px, py, &x, &z);
                /* le toit : l'intérieur du carré [-1, 1], à 2 texels du bord (le toit est vu de
                 * dessus et son éclairement diffère du sol : on compare à lui-même) */
                if (fabsf(x) < 0.9F && fabsf(z) < 0.9F) {
                    total++;
                    dark += brightness(px, py) < 0.5F * reference * 0.5F ? 1U : 0U;
                }
            }
        }
        EXPECT_GE(total, 400U);
        EXPECT_EQ(dark, 0U);
    }
    EXPECT_EQ(close_scene(&s), 0U);
}

UTEST(shadow_gpu, the_shadow_pass_is_refused_for_a_texture_that_is_not_a_shadow_map)
{
    Scene s;
    REQUIRE_SCENE(s);
    EXPECT_EQ(eng_renderer_run_shadow_pass(&s.r, NULL, s.map, &s.r.shadow_queue), 0U);
    EXPECT_EQ(eng_renderer_run_shadow_pass(&s.r, (SDL_GPUCommandBuffer *)1, s.ground.white,
                                           &s.r.shadow_queue),
              0U);
    EXPECT_EQ(eng_renderer_create_shadow_map(&s.r, 4U).handle.generation, 0U);
    EXPECT_EQ(eng_renderer_create_shadow_map(&s.r, 16384U).handle.generation, 0U);
    EXPECT_EQ(close_scene(&s), 0U);
}

/* La boîte projette son ombre depuis où sa MATRICE DE MODÈLE la met, dans la passe d'ombre comme
 * dans l'image : le test géométrique est le même, la boîte est seulement décalée de 2,5 m. */
UTEST(shadow_gpu, a_caster_follows_its_model_matrix)
{
    Scene s;
    REQUIRE_SCENE(s);
    Shot shot = default_shot(40.0F, 37.0F);
    shot.box_x = 2.5F;
    ASSERT_TRUE(render_shot(&s, &shot));
    const Stats st = compare_with_geometry(&shot);
    EXPECT_GT(st.certain_lit, 3000U);
    EXPECT_GT(st.certain_shadow, 100U);
    EXPECT_EQ(st.wrong_lit, 0U);
    EXPECT_EQ(st.wrong_shadow, 0U);
    EXPECT_EQ(close_scene(&s), 0U);
}

/* Les dessins qui portent une carte d'ombre sont rejetés (et comptés) s'ils sont incohérents : un
 * sampler manquant, une carte manquante, ou moins de huit textures avant elle. Un dessin cohérent
 * passe. */
UTEST(shadow_gpu, inconsistent_shadow_bindings_are_rejected)
{
    Scene s;
    REQUIRE_SCENE(s);
    Shot shot = default_shot(40.0F, 37.0F);
    ASSERT_TRUE(render_shot(&s, &shot)); /* une image valide : carte rendue, file remplie */
    const uint64_t rejected_before = s.r.stats.rejected_commands;
    ShotContext context = {&s, &shot};
    EXPECT_EQ(rejected_before, 0U);

    for (int variant = 0; variant < 4; variant++) {
        EngRenderQueue *q = eng_renderer_begin_frame(&s.r);
        mat4 identity;
        glm_mat4_identity(identity);
        EngLighting light = {0};
        light.sun_direction[1] = 1.0F;
        const EngModelShadow shadow = {.map = s.map, .shadow = shot.map, .params = shot.params};
        ASSERT_EQ(eng_model_gpu_draw_shadowed(&s.ground, q, s.pipeline, identity, identity, &light,
                                              &shadow),
                  1U);
        EngDrawCmd *cmd = &q->commands[0];
        if (variant == 0) {
            cmd->shadow_sampler = ENG_SAMPLER_NULL; /* carte sans sampler */
        } else if (variant == 1) {
            cmd->shadow_map = ENG_TEXTURE_NULL; /* sampler sans carte */
        } else if (variant == 2) {
            cmd->extra_textures[6] = ENG_TEXTURE_NULL; /* sept textures seulement avant la carte */
        }
        const uint64_t before = s.r.stats.rejected_commands;
        const uint64_t drawn_before = s.r.stats.draws;
        ASSERT_TRUE(render_to_pixels_prepared(&s.f, eng_renderer_depth_format(&s.r), NULL,
                                              shot_draw, &context, SIZE, g_pixels));
        if (variant < 3) {
            EXPECT_EQ(s.r.stats.rejected_commands - before, 1U);
            EXPECT_EQ(s.r.stats.draws - drawn_before, 0U);
        } else { /* le dessin intact passe */
            EXPECT_EQ(s.r.stats.rejected_commands - before, 0U);
            EXPECT_EQ(s.r.stats.draws - drawn_before, 1U);
        }
    }
    EXPECT_EQ(close_scene(&s), 0U);
}

UTEST(shadow_gpu, the_shadow_queue_is_emptied_by_every_begin_and_depth_only_is_validated)
{
    Scene s;
    REQUIRE_SCENE(s);
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_shadow_queue(&s.r);
    ASSERT_TRUE(q != NULL);
    EXPECT_EQ(eng_model_gpu_draw_depth(&s.box, q, s.shadow_pipeline, identity, identity), 1U);
    EXPECT_EQ(q->count, 1U);
    EXPECT_GT(q->uniform_used, 0U);
    q->dropped = 5;
    q = eng_renderer_begin_shadow_queue(&s.r);
    ASSERT_TRUE(q != NULL);
    EXPECT_EQ(q->count, 0U);
    EXPECT_EQ(q->dropped, 0U);
    EXPECT_EQ(q->uniform_used, 0U);

    /* un pipeline de passe d'ombre écrit ET teste la profondeur, et n'a pas de cible supprimée */
    EngPipelineDesc desc;
    eng_model_shadow_pipeline_desc(&desc);
    /* les modèles ouverts projettent des deux côtés : pas d'élimination des faces */
    EXPECT_EQ((int)desc.cull_mode, (int)ENG_CULL_NONE);
    EXPECT_TRUE(desc.depth_only);
    desc.depth_write = false;
    EXPECT_EQ(eng_renderer_create_pipeline(&s.r, &desc).handle.generation, 0U);
    eng_model_shadow_pipeline_desc(&desc);
    desc.depth_test = false;
    desc.depth_write = false;
    EXPECT_EQ(eng_renderer_create_pipeline(&s.r, &desc).handle.generation, 0U);
    eng_model_shadow_pipeline_desc(&desc);
    desc.no_depth_target = true;
    EXPECT_EQ(eng_renderer_create_pipeline(&s.r, &desc).handle.generation, 0U);
    EXPECT_EQ(eng_renderer_pipeline_depth_format(&s.r, s.shadow_pipeline),
              (uint32_t)SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
    EXPECT_EQ(close_scene(&s), 0U);
}

/* ── Parité exacte shader / référence CPU, sur une carte de profondeur SYNTHÉTIQUE ──
 *
 * Les tests précédents jugent le résultat (l'ombre est là où la géométrie le dit, à quelques texels
 * près) ; ils ne voient pas une erreur d'un demi-texel, un biais oublié ou un terme du décalage
 * normal faux. Celui-ci compare le shader, pixel par pixel, à eng_shadow_receiver +
 * eng_shadow_visibility + eng_shadow_factor, sur une carte dont on connaît chaque texel.
 *
 * La carte est rendue par la vraie passe d'ombre avec une matrice choisie à la main : vue du
 * dessus, x et z dans [-6, 6] m ↔ (u, v) dans [0, 1] ; la profondeur ne dépend que de la hauteur, d
 * = 0,5 - k y (k = 0,0033 / m). Un sol à y = 0 (d = 0,5) et trois boîtes dont le dessus est plus
 * proche : des marches de profondeur de 2k, 5k et 10k = 0,0066 ; 0,0165 ; 0,033 (1,7 ; 4,2 ; 8,4
 * texels de profondeur : de l'ordre des biais, donc sensibles à chacun d'eux). Seul le SOL est
 * dessiné dans l'image, vu du dessus : chaque pixel est un point (x, 0, z) de normale +Y, dont
 * l'ombre dépend de la marche sous lui. Les boîtes 2 et 3 dépassent le bord de la carte : les
 * prises qui sortent de la carte liraient une ombre si le shader ne les ignorait pas (hors carte :
 * lumière).
 *
 * Attendu : la carte CPU est construite ici texel par texel (le centre du texel est-il dans
 * l'empreinte de la boîte ?, aucun centre n'est sur un bord) ; chaque pixel vaut
 * lumière × eng_shadow_factor(...), à 3/255 près. */

#define SYNTH_K 0.0033F
#define SYNTH_HALF 6.0F /* la carte couvre [-6, 6] m */
#define SYNTH_VIEW 6.5F /* l'image montre [-6,5 ; 6,5] m : 0,5 m au-delà de la carte */

typedef struct SynthBox {
    float x0;
    float x1;
    float z0;
    float z1;
    float top; /* hauteur du dessus (m) */
} SynthBox;

static const SynthBox SYNTH_BOXES[3] = {
    {-1.0F, 1.0F, -1.0F, 1.0F, 2.0F}, /* marche de 2k, au centre */
    {4.0F, 8.0F, -2.0F, 0.0F, 10.0F}, /* marche de 10k, dépasse le bord +x */
    {-4.0F, -2.0F, 4.0F, 8.0F, 5.0F}, /* marche de 5k, dépasse le bord +z */
};

static void synth_matrices(EngShadowMap *map, mat4 render)
{
    memset(map, 0, sizeof *map);
    glm_mat4_zero(render);
    /* clip = (x / 6, -z / 6, 0,5 - k y, 1) */
    render[0][0] = 1.0F / SYNTH_HALF;
    render[2][1] = -1.0F / SYNTH_HALF;
    render[1][2] = -SYNTH_K;
    render[3][2] = 0.5F;
    render[3][3] = 1.0F;
    glm_mat4_copy(render, map->view_proj);
    /* (u, v, d) = (0,5 x / 6 + 0,5 ; 0,5 + 0,5 z / 6 ; 0,5 - k y) */
    glm_mat4_zero(map->shadow_matrix);
    map->shadow_matrix[0][0] = 0.5F / SYNTH_HALF;
    map->shadow_matrix[3][0] = 0.5F;
    map->shadow_matrix[2][1] = 0.5F / SYNTH_HALF;
    map->shadow_matrix[3][1] = 0.5F;
    map->shadow_matrix[1][2] = -SYNTH_K;
    map->shadow_matrix[3][2] = 0.5F;
    map->shadow_matrix[3][3] = 1.0F;
    map->texel_world = 1.0F; /* volontairement grand : le décalage normal doit compter */
    map->depth_range = 1.0F / SYNTH_K;
    map->size = MAP_SIZE;
}

/* Profondeur attendue de chaque texel : celle du sol, ou celle du dessus de la boîte dont
 * l'empreinte contient le centre du texel. */
static void synth_depth(float *depth)
{
    for (uint32_t j = 0; j < MAP_SIZE; j++) {
        for (uint32_t i = 0; i < MAP_SIZE; i++) {
            const float x = (2.0F * SYNTH_HALF * ((float)i + 0.5F) / (float)MAP_SIZE) - SYNTH_HALF;
            const float z = (2.0F * SYNTH_HALF * ((float)j + 0.5F) / (float)MAP_SIZE) - SYNTH_HALF;
            float d = 0.5F;
            for (int b = 0; b < 3; b++) {
                if (x > SYNTH_BOXES[b].x0 && x < SYNTH_BOXES[b].x1 && z > SYNTH_BOXES[b].z0 &&
                    z < SYNTH_BOXES[b].z1) {
                    d = 0.5F - (SYNTH_K * SYNTH_BOXES[b].top);
                }
            }
            depth[((size_t)j * MAP_SIZE) + i] = d;
        }
    }
}

typedef struct ParityCase {
    const char *name;
    float elevation_deg;
    float azimuth_deg;
    float intensity;
    EngShadowParams params;
} ParityCase;

typedef struct ParityContext {
    Scene *scene;
    EngRenderQueue *shadow_queue;
} ParityContext;

static void parity_prepare(SDL_GPUCommandBuffer *commands, void *context)
{
    ParityContext *c = (ParityContext *)context;
    (void)eng_renderer_run_shadow_pass(&c->scene->r, commands, c->scene->map, c->shadow_queue);
}

static void parity_draw(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    ParityContext *c = (ParityContext *)context;
    (void)eng_renderer_execute(&c->scene->r, &c->scene->r.queue, commands, pass);
}

/* Compare le shader à la référence CPU pour un jeu de réglages. Retourne l'écart maximal (en
 * niveaux sur 255) et le nombre de pixels de pénombre / d'ombre / de lumière. */
typedef struct ParityResult {
    float worst;
    uint32_t penumbra;
    uint32_t dark;
    uint32_t lit;
    bool ok;
} ParityResult;

static ParityResult run_parity_case(Scene *s, const EngModelGpu boxes[3], const float *cpu_depth,
                                    const ParityCase *pc)
{
    ParityResult result = {0};
    EngShadowMap map;
    mat4 render;
    synth_matrices(&map, render);
    sun_vector(pc->elevation_deg, pc->azimuth_deg, map.to_sun);

    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *sq = eng_renderer_begin_shadow_queue(&s->r);
    if (sq == NULL ||
        eng_model_gpu_draw_depth(&s->ground, sq, s->shadow_pipeline, identity, render) != 1U) {
        return result;
    }
    for (int b = 0; b < 3; b++) {
        if (eng_model_gpu_draw_depth(&boxes[b], sq, s->shadow_pipeline, identity, render) != 1U) {
            return result;
        }
    }

    EngLighting light = {0};
    for (int c = 0; c < 3; c++) {
        light.sun_direction[c] = map.to_sun[c];
        light.sun_color[c] = 1.0F;
        light.ambient_color[c] = 1.0F;
    }
    light.sun_intensity = pc->intensity;
    light.ambient_intensity = 0.0F;
    light.camera_position[1] = 20.0F;
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    const vec3 eye = {0.0F, 20.0F, 0.0F};
    const vec3 target = {0.0F, 0.0F, 0.0F};
    const vec3 up = {0.0F, 0.0F, -1.0F};
    glm_lookat((float *)eye, (float *)target, (float *)up, view);
    glm_ortho(-SYNTH_VIEW, SYNTH_VIEW, -SYNTH_VIEW, SYNTH_VIEW, 0.1F, 50.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    const EngModelShadow shadow = {.map = s->map, .shadow = map, .params = pc->params};
    EngRenderQueue *q = eng_renderer_begin_frame(&s->r);
    if (eng_model_gpu_draw_shadowed(&s->ground, q, s->pipeline, identity, view_proj, &light,
                                    &shadow) != 1U) {
        return result;
    }
    ParityContext context = {s, sq};
    if (!render_to_pixels_prepared(&s->f, eng_renderer_depth_format(&s->r), parity_prepare,
                                   parity_draw, &context, SIZE, g_pixels)) {
        return result;
    }

    /* la lumière d'un pixel éclairé : le coin (hors de la carte, donc éclairé par construction) */
    const float lit = (float)g_pixels[(((size_t)1 * SIZE) + (SIZE - 2U)) * 4U] / 255.0F;
    EngShadowParams params = pc->params;
    (void)eng_shadow_params_sanitize(&params);
    const float normal[3] = {0.0F, 1.0F, 0.0F};
    for (uint32_t py = 0; py < SIZE; py++) {
        for (uint32_t px = 0; px < SIZE; px++) {
            const float pixel = 2.0F * SYNTH_VIEW / (float)SIZE;
            const float world[3] = {-SYNTH_VIEW + (((float)px + 0.5F) * pixel), 0.0F,
                                    -SYNTH_VIEW + (((float)py + 0.5F) * pixel)};
            float uvd[3];
            eng_shadow_receiver(&map, &params, world, normal, uvd);
            const float visibility =
                eng_shadow_visibility(cpu_depth, MAP_SIZE, uvd, params.pcf_radius);
            const float factor = eng_shadow_factor(&params, visibility);
            const float seen = (float)g_pixels[(((size_t)py * SIZE) + px) * 4U] / 255.0F;
            const float diff = fabsf(seen - (lit * factor)) * 255.0F;
            result.worst = diff > result.worst ? diff : result.worst;
            if (factor < 0.02F) {
                result.dark++;
            } else if (factor > 0.98F) {
                result.lit++;
            } else {
                result.penumbra++;
            }
        }
    }
    result.ok = true;
    return result;
}

UTEST(shadow_gpu, the_shader_equals_the_cpu_reference_on_a_known_depth_map)
{
    Scene s;
    REQUIRE_SCENE(s);
    EngModelGpu boxes[3];
    bool boxes_ok = true;
    for (int b = 0; b < 3; b++) {
        const float lo[3] = {SYNTH_BOXES[b].x0, 0.0F, SYNTH_BOXES[b].z0};
        const float hi[3] = {SYNTH_BOXES[b].x1, SYNTH_BOXES[b].top, SYNTH_BOXES[b].z1};
        boxes_ok = boxes_ok && make_gpu_box(&s, lo, hi, &boxes[b]);
    }
    ASSERT_TRUE(boxes_ok);
    float *cpu_depth = (float *)malloc(sizeof(float) * MAP_SIZE * MAP_SIZE);
    ASSERT_TRUE(cpu_depth != NULL);
    synth_depth(cpu_depth);

    /* chaque cas isole un terme : (constant, pente, normal, rayon, force) ; hauteur et azimut du
     * soleil pour que sin θ, cos θ et tan θ soient différents */
    const ParityCase cases[] = {
        {"bilinéaire seul", 54.0F, 31.0F, 1.0F, {0.0F, 0.0F, 0.0F, 0U, 1.0F}},
        {"constant", 54.0F, 31.0F, 1.0F, {1.5F, 0.0F, 0.0F, 0U, 1.0F}},
        {"pente, rayon 1", 54.0F, 31.0F, 1.0F, {0.0F, 1.0F, 0.0F, 1U, 1.0F}},
        {"pente, rayon 2", 54.0F, 31.0F, 1.0F, {0.0F, 2.0F, 0.0F, 2U, 1.0F}},
        {"normal 2,9", 54.0F, 31.0F, 1.0F, {0.0F, 0.0F, 2.9F, 0U, 1.0F}},
        {"normal 4", 54.0F, 31.0F, 1.0F, {0.0F, 0.0F, 4.0F, 0U, 1.0F}},
        {"défauts", 54.0F, 31.0F, 1.0F, {1.0F, 1.0F, 1.0F, 1U, 1.0F}},
        {"rayon 3, force 0,6", 54.0F, 31.0F, 1.0F, {1.0F, 1.0F, 1.0F, 3U, 0.6F}},
        {"mélange, rayon 2", 54.0F, 31.0F, 1.0F, {2.0F, 0.5F, 2.0F, 2U, 1.0F}},
        {"soleil rasant (plafond de pente)", 6.0F, 20.0F, 4.0F, {0.0F, 1.0F, 0.0F, 0U, 1.0F}},
    };
    for (uint32_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const ParityResult r = run_parity_case(&s, boxes, cpu_depth, &cases[i]);
        ASSERT_TRUE(r.ok);
        printf("  %-34s : écart max %.2f / 255 ; ombre %u, pénombre %u, lumière %u\n",
               cases[i].name, (double)r.worst, r.dark, r.penumbra, r.lit);
        EXPECT_LT(r.worst, 3.0F);
        EXPECT_GT(r.lit, 1000U);
    }
    free(cpu_depth);
    for (int b = 0; b < 3; b++) {
        eng_model_gpu_destroy(&boxes[b], &s.r);
    }
    EXPECT_EQ(close_scene(&s), 0U);
}

#else /* OW_SHADER_DIR */

UTEST(shadow_gpu, needs_compiled_shaders)
{
    EXPECT_TRUE(true);
}

#endif
