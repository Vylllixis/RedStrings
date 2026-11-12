#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "engine/lighting.h"
#include "engine/model.h"
#include "engine/model_gpu.h"
#include "engine/pbr.h"
#include "engine/renderer.h"
#include "engine/tonemap.h"
#include "test_gltf.h"
#include "test_render_util.h"
#include "test_ibl_ref.h"
#include "utest.h"

/* HDR, tonemapping et gamma pour de vrai (M1-14) : une texture RGBA16F aux valeurs EXACTES (2x2
 * texels, lus au plus proche : quatre quadrants), résolue par le vrai pipeline tonemap vers une
 * cible 8 bits, dont on lit les pixels. Les valeurs attendues viennent de la référence CPU
 * (eng_tonemap_apply puis eng_srgb_encode), elle-même vérifiée à la main dans test_tonemap.c. Sans
 * GPU ou sans shaders compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 64U
#define TOL 3 /* arrondi 8 bits, log2/pow/exp du GPU */

static const uint32_t FORMAT_UNORM = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
static const uint32_t FORMAT_SRGB = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
static const uint32_t FORMAT_F32 = (uint32_t)SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
static const uint32_t FORMAT_BGRA = (uint32_t)SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
static const uint32_t FORMAT_BGRA_SRGB = (uint32_t)SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;

/* Centres des quatre quadrants de la cible. */
static const uint32_t QX[4] = {16, 48, 16, 48};
static const uint32_t QY[4] = {16, 16, 48, 48};

/* ── flottants 16 bits (ni sous-normaux ni arrondi exotique : les valeurs de test sont simples) ──
 */

static uint16_t to_half(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    const uint16_t sign = (uint16_t)((bits >> 16) & 0x8000U);
    const int exponent = (int)((bits >> 23) & 0xFFU);
    const uint32_t mantissa = bits & 0x7FFFFFU;
    if (exponent == 255) {
        return (uint16_t)(sign | 0x7C00U | (mantissa != 0 ? 0x200U : 0U));
    }
    const int half_exponent = exponent - 127 + 15;
    if (half_exponent >= 31) {
        return (uint16_t)(sign | 0x7C00U);
    }
    if (half_exponent <= 0) {
        return sign; /* trop petit : 0 */
    }
    uint32_t half_mantissa = mantissa >> 13;
    const uint32_t rest = mantissa & 0x1FFFU;
    if (rest > 0x1000U || (rest == 0x1000U && (half_mantissa & 1U) != 0)) {
        half_mantissa++; /* arrondi au plus proche, pair en cas d'égalité ; la retenue passe dans
                            l'exposant */
    }
    return (uint16_t)(sign | (uint16_t)(((uint32_t)half_exponent << 10) + half_mantissa));
}

static float from_half(uint16_t h)
{
    const float sign = (h & 0x8000U) != 0 ? -1.0F : 1.0F;
    const int exponent = (h >> 10) & 0x1F;
    const int mantissa = h & 0x3FF;
    if (exponent == 31) {
        return mantissa != 0 ? NAN : sign * INFINITY;
    }
    if (exponent == 0) {
        return sign * ldexpf((float)mantissa, -24);
    }
    return sign * ldexpf((float)(1024 + mantissa), exponent - 25);
}

UTEST(half_float, the_test_helpers_round_trip_exactly_representable_values)
{
    const float values[] = {0.0F, 0.0625F, 0.125F,  0.25F,    0.5F,  1.0F, 2.0F,
                            3.5F, 8.0F,    1000.0F, 65504.0F, -1.0F, -4.0F};
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        EXPECT_NEAR(from_half(to_half(values[i])), values[i], 0.0F);
    }
    EXPECT_EQ((int)to_half(1.0F), 0x3C00); /* valeurs connues du format IEEE 754 binary16 */
    EXPECT_EQ((int)to_half(-1.0F), 0xBC00);
    EXPECT_EQ((int)to_half(INFINITY), 0x7C00);
    EXPECT_EQ((int)to_half(65504.0F), 0x7BFF);
    EXPECT_EQ((int)to_half(0.5F), 0x3800);
    EXPECT_TRUE(isinf(from_half(0x7C00)));
    /* arrondi du binary16 : 0,18 / 2^-13 = 1474,56 -> 1475 x 2^-13 = 0,180054 */
    EXPECT_NEAR(from_half(to_half(0.18F)), 0.180054F, 1e-6F);
}

/* ── contexte ── */

typedef struct TonemapTest {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
} TonemapTest;

static bool open_tonemap_with(TonemapTest *t, const char *offline_dir, bool hdr,
                              uint32_t resolve_format, bool depth)
{
    /* validation de SDL_GPU : refuse une passe dont les formats ne vont pas avec le pipeline */
    if (!open_render_fixture_with(&t->f, true)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "tonemap-test", (size_t)8 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = offline_dir,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 16384,
                                  .max_pipelines = 8,
                                  .depth_buffer = depth,
                                  .hdr = hdr,
                                  .resolve_format = resolve_format};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    return true;
}

static bool open_tonemap(TonemapTest *t, uint32_t resolve_format)
{
    return open_tonemap_with(t, OW_SHADER_DIR, true, resolve_format, false);
}

static uint32_t close_tonemap(TonemapTest *t)
{
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

#define CLOSE_TONEMAP(t) EXPECT_EQ(close_tonemap(&(t)), (uint32_t)0)

/* ── ressources GPU des tests ── */

/* Texture RGBA16F de w x h texels (rgba : w*h*4 flottants, ligne du haut en premier), lisible comme
 * texture et utilisable comme cible. NULL si une étape échoue. */
static SDL_GPUTexture *upload_hdr(SDL_GPUDevice *device, uint32_t w, uint32_t h, const float *rgba)
{
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = w;
    info.height = h;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &info);
    const uint32_t bytes = w * h * 4U * (uint32_t)sizeof(uint16_t);
    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = bytes};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);
    bool ok = false;
    if (texture != NULL && buffer != NULL) {
        uint16_t *mapped = (uint16_t *)SDL_MapGPUTransferBuffer(device, buffer, false);
        if (mapped != NULL) {
            for (uint32_t i = 0; i < w * h * 4U; i++) {
                mapped[i] = to_half(rgba[i]);
            }
            SDL_UnmapGPUTransferBuffer(device, buffer);
            SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
            const SDL_GPUTextureTransferInfo source = {
                .transfer_buffer = buffer, .pixels_per_row = w, .rows_per_layer = h};
            const SDL_GPUTextureRegion region = {.texture = texture, .w = w, .h = h, .d = 1};
            SDL_UploadToGPUTexture(copy, &source, &region, false);
            SDL_EndGPUCopyPass(copy);
            SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
            if (fence != NULL) {
                ok = SDL_WaitForGPUFences(device, true, &fence, 1);
                SDL_ReleaseGPUFence(device, fence);
            }
        }
    }
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    if (!ok && texture != NULL) {
        SDL_ReleaseGPUTexture(device, texture);
        texture = NULL;
    }
    return texture;
}

/* Fait lire `hdr` par la vraie résolution du renderer (les vrais réglages de passe :
 * eng_gpu_fill_resolve_target) dans une cible 8 bits de `format`, et copie les pixels dans `rgba`.
 * `scene` (peut être NULL) est appelée avant dans le même tampon de commandes, pour dessiner la
 * scène dans `hdr`. */
typedef bool (*SceneFn)(SDL_GPUCommandBuffer *commands, void *context);

static bool resolve_to_pixels(TonemapTest *t, SDL_GPUTexture *hdr, uint32_t format, SceneFn scene,
                              void *context, uint8_t *rgba)
{
    SDL_GPUDevice *device = t->f.gpu.device;
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = (SDL_GPUTextureFormat)format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = SIZE;
    info.height = SIZE;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture *target = SDL_CreateGPUTexture(device, &info);
    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = SIZE * SIZE * 4U};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    bool ok = false;
    if (target != NULL && buffer != NULL && commands != NULL &&
        (scene == NULL || scene(commands, context))) {
        SDL_GPUColorTargetInfo color;
        eng_gpu_fill_resolve_target(target, &color);
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &color, 1, NULL);
        const bool drawn = eng_renderer_resolve_hdr(&t->r, commands, pass, hdr);
        SDL_EndGPURenderPass(pass);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
        const SDL_GPUTextureRegion region = {.texture = target, .w = SIZE, .h = SIZE, .d = 1};
        const SDL_GPUTextureTransferInfo destination = {
            .transfer_buffer = buffer, .pixels_per_row = SIZE, .rows_per_layer = SIZE};
        SDL_DownloadFromGPUTexture(copy, &region, &destination);
        SDL_EndGPUCopyPass(copy);
        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        commands = NULL;
        if (fence != NULL) {
            ok = SDL_WaitForGPUFences(device, true, &fence, 1) && drawn;
            SDL_ReleaseGPUFence(device, fence);
        }
        if (ok) {
            const uint8_t *mapped =
                (const uint8_t *)SDL_MapGPUTransferBuffer(device, buffer, false);
            ok = mapped != NULL;
            if (ok) {
                memcpy(rgba, mapped, (size_t)SIZE * SIZE * 4U);
                SDL_UnmapGPUTransferBuffer(device, buffer);
                if (format == FORMAT_BGRA || format == FORMAT_BGRA_SRGB) {
                    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) { /* ramené en RGBA */
                        const uint8_t blue = rgba[i * 4];
                        rgba[i * 4] = rgba[i * 4 + 2];
                        rgba[i * 4 + 2] = blue;
                    }
                }
            }
        }
    }
    if (commands != NULL) {
        (void)SDL_CancelGPUCommandBuffer(commands);
    }
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    SDL_ReleaseGPUTexture(device, target);
    return ok;
}

/* Comme resolve_to_pixels, mais dans une cible RGBA 32 bits FLOTTANTS (le renderer doit avoir été
 * ouvert avec FORMAT_F32) : la valeur écrite par le shader, sans arrondi 8 bits ni écrêtage à 1,0.
 * Le shader encode en sRGB (la cible n'est pas _SRGB). `rgba` : SIZE * SIZE * 4 flottants. */
static bool resolve_to_floats(TonemapTest *t, SDL_GPUTexture *hdr, float *rgba)
{
    SDL_GPUDevice *device = t->f.gpu.device;
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = SIZE;
    info.height = SIZE;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture *target = SDL_CreateGPUTexture(device, &info);
    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
        .size = SIZE * SIZE * 4U * (uint32_t)sizeof(float)};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    bool ok = false;
    if (target != NULL && buffer != NULL && commands != NULL) {
        SDL_GPUColorTargetInfo color;
        eng_gpu_fill_resolve_target(target, &color);
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &color, 1, NULL);
        const bool drawn = eng_renderer_resolve_hdr(&t->r, commands, pass, hdr);
        SDL_EndGPURenderPass(pass);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
        const SDL_GPUTextureRegion region = {.texture = target, .w = SIZE, .h = SIZE, .d = 1};
        const SDL_GPUTextureTransferInfo destination = {
            .transfer_buffer = buffer, .pixels_per_row = SIZE, .rows_per_layer = SIZE};
        SDL_DownloadFromGPUTexture(copy, &region, &destination);
        SDL_EndGPUCopyPass(copy);
        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        commands = NULL;
        if (fence != NULL) {
            ok = SDL_WaitForGPUFences(device, true, &fence, 1) && drawn;
            SDL_ReleaseGPUFence(device, fence);
        }
        if (ok) {
            const float *mapped = (const float *)SDL_MapGPUTransferBuffer(device, buffer, false);
            ok = mapped != NULL;
            if (ok) {
                memcpy(rgba, mapped, (size_t)SIZE * SIZE * 4U * sizeof(float));
                SDL_UnmapGPUTransferBuffer(device, buffer);
            }
        }
    }
    if (commands != NULL) {
        (void)SDL_CancelGPUCommandBuffer(commands);
    }
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    SDL_ReleaseGPUTexture(device, target);
    return ok;
}

static uint8_t g_pixels[SIZE * SIZE * 4];

/* Octet attendu d'un canal : exposition + opérateur + sRGB, en partant de la valeur 16 bits. */
static int expected_byte(const EngTonemap *t, const float rgb[3], int channel)
{
    float out[3];
    eng_tonemap_apply(t, rgb, out);
    const float v = eng_srgb_encode(out[channel]);
    return (int)(v * 255.0F + 0.5F);
}

/* Compare les quatre quadrants de g_pixels à la référence CPU ; `texels` : 4 * RGBA. Retourne le
 * masque des quadrants qui ne correspondent pas (0 : tout est bon ; bit q : quadrant q). */
static int quadrant_mismatches(const EngTonemap *t, const float *texels, int tolerance)
{
    int mask = 0;
    for (int q = 0; q < 4; q++) {
        float rgb[3];
        for (int c = 0; c < 3; c++) {
            rgb[c] = from_half(to_half(texels[q * 4 + c])); /* ce que le GPU lit vraiment */
        }
        if (!pixel_is(g_pixels, SIZE, QX[q], QY[q], expected_byte(t, rgb, 0),
                      expected_byte(t, rgb, 1), expected_byte(t, rgb, 2), tolerance)) {
            mask |= 1 << q;
        }
    }
    return mask;
}

/* Quatre texels très différents (haut-gauche, haut-droite, bas-gauche, bas-droite) : toutes les
 * valeurs sont exactes en binary16, des deux côtés de 1,0 ; les trois canaux diffèrent pour que
 * l'ordre des canaux et l'orientation de l'image se voient. */
static const float QUADRANTS[16] = {
    0.25F, 0.5F,    1.0F,   1.0F, /* haut-gauche */
    1.0F,  2.0F,    4.0F,   1.0F, /* haut-droite */
    2.0F,  0.5F,    0.125F, 1.0F, /* bas-gauche */
    8.0F,  0.0625F, 1.0F,   1.0F, /* bas-droite */
};

UTEST(eng_tonemap_gpu, every_operator_and_exposure_matches_the_cpu_reference)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(eng_renderer_hdr_enabled(&t.r));
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, QUADRANTS);
    ASSERT_TRUE(hdr != NULL);
    const float evs[] = {0.0F, 1.0F, -1.5F};
    for (int op = 0; op < ENG_TONEMAP_COUNT; op++) {
        for (size_t e = 0; e < sizeof evs / sizeof evs[0]; e++) {
            const EngTonemap setting = {.op = (EngTonemapOperator)op, .exposure_ev = evs[e]};
            eng_renderer_set_tonemap(&t.r, &setting);
            ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, NULL, NULL, g_pixels));
            EXPECT_EQ(quadrant_mismatches(&setting, QUADRANTS, TOL), 0);
        }
    }
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, the_operators_really_differ_on_a_bright_pixel)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, QUADRANTS);
    ASSERT_TRUE(hdr != NULL);
    /* haut-droite = (1, 2, 4). Aucun : (255,255,255). ACES : (0,8038 ; 0,9149 ; 1 saturé) ->
     * encodé (232, 244, 255) : 1,055 x 0,8038^(1/2,4) - 0,055 = 0,9082 -> 231,6. Les trois sorties
     * sont distinctes à plus de 3 niveaux. */
    int red[3];
    for (int op = 0; op < ENG_TONEMAP_COUNT; op++) {
        const EngTonemap setting = {.op = (EngTonemapOperator)op, .exposure_ev = 0.0F};
        eng_renderer_set_tonemap(&t.r, &setting);
        ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, NULL, NULL, g_pixels));
        red[op] = g_pixels[((size_t)QY[1] * SIZE + QX[1]) * 4];
    }
    EXPECT_EQ(red[ENG_TONEMAP_NONE], 255);
    EXPECT_NEAR((float)red[ENG_TONEMAP_ACES], 232.0F, 1.0F);
    EXPECT_NEAR((float)red[ENG_TONEMAP_AGX],
                0.0F + (float)expected_byte(&(EngTonemap){.op = ENG_TONEMAP_AGX},
                                            (float[3]){1.0F, 2.0F, 4.0F}, 0),
                3.0F);
    EXPECT_TRUE(abs(red[ENG_TONEMAP_NONE] - red[ENG_TONEMAP_ACES]) > 10);
    EXPECT_TRUE(abs(red[ENG_TONEMAP_ACES] - red[ENG_TONEMAP_AGX]) > 10);
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, an_srgb_target_gets_the_same_picture_without_a_double_encoding)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_SRGB)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, QUADRANTS);
    ASSERT_TRUE(hdr != NULL);
    /* La cible sRGB encode elle-même à l'écriture : le shader sort du linéaire (z = 0). Les octets
     * lus doivent être ceux de la cible UNORM où le shader encode, pas une courbe doublée (qui
     * donnerait ~ 0,91 au lieu de 0,80 pour un linéaire de 0,6 : bien plus que la tolérance). */
    const EngTonemap setting = {.op = ENG_TONEMAP_ACES, .exposure_ev = 0.0F};
    eng_renderer_set_tonemap(&t.r, &setting);
    ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_SRGB, NULL, NULL, g_pixels));
    EXPECT_EQ(quadrant_mismatches(&setting, QUADRANTS, TOL), 0);
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

static float g_floats[SIZE * SIZE * 4];

/* Écart maximal (sur les 3 canaux des 4 quadrants) entre g_floats et eng_srgb_encode(tonemap CPU).
 */
static float max_float_error(const EngTonemap *t, const float *texels)
{
    float worst = 0.0F;
    for (int q = 0; q < 4; q++) {
        float rgb[3];
        float out[3];
        for (int c = 0; c < 3; c++) {
            rgb[c] = from_half(to_half(texels[q * 4 + c]));
        }
        eng_tonemap_apply(t, rgb, out);
        const float *pixel = g_floats + ((size_t)QY[q] * SIZE + QX[q]) * 4U;
        for (int c = 0; c < 3; c++) {
            const float error = fabsf(pixel[c] - eng_srgb_encode(out[c]));
            worst = error > worst ? error : worst;
        }
    }
    return worst;
}

/* Valeurs très faibles : le segment LINÉAIRE de l'encodage sRGB (12,92 x sous 0,0031308), invisible
 * en 8 bits tellement il est court, et le bas des courbes. */
static const float DARK[16] = {
    0.002F,  0.0009765625F, 0.0005F,    1.0F, /* haut-gauche */
    0.003F,  0.0001F,       0.0031F,    1.0F, /* haut-droite */
    0.0625F, 0.03125F,      0.015625F,  1.0F, /* bas-gauche */
    12.0F,   0.4F,          0.0078125F, 1.0F, /* bas-droite */
};

UTEST(eng_tonemap_gpu, the_shader_matches_the_cpu_reference_to_a_thousandth_in_float)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_F32)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const float *images[2] = {QUADRANTS, DARK};
    const float evs[] = {0.0F, 2.0F, -1.5F};
    for (int image = 0; image < 2; image++) {
        SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, images[image]);
        ASSERT_TRUE(hdr != NULL);
        for (int op = 0; op < ENG_TONEMAP_COUNT; op++) {
            for (size_t e = 0; e < sizeof evs / sizeof evs[0]; e++) {
                const EngTonemap setting = {.op = (EngTonemapOperator)op, .exposure_ev = evs[e]};
                eng_renderer_set_tonemap(&t.r, &setting);
                ASSERT_TRUE(resolve_to_floats(&t, hdr, g_floats));
                /* 5e-4 : sous un demi-niveau 8 bits (2e-3), au-dessus de l'erreur de pow/log2/exp
                 */
                EXPECT_TRUE(max_float_error(&setting, images[image]) < 5e-4F);
                /* la sortie est une couleur d'affichage : jamais au-delà de 1,0 (AgX et ACES la
                 * bornent explicitement) */
                for (size_t i = 0; i < (size_t)SIZE * SIZE * 4U; i++) {
                    ASSERT_TRUE(g_floats[i] <= 1.0F + 1e-6F && g_floats[i] >= 0.0F);
                }
            }
        }
        SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    }
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, the_hdr_texture_is_read_texel_by_texel_without_blending)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, QUADRANTS);
    ASSERT_TRUE(hdr != NULL);
    const EngTonemap setting = {.op = ENG_TONEMAP_NONE, .exposure_ev = -3.0F};
    eng_renderer_set_tonemap(&t.r, &setting);
    ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, NULL, NULL, g_pixels));
    /* La frontière entre deux texels est NETTE : un filtrage linéaire donnerait, de part et
     * d'autre, environ la moitié de l'écart (haut-gauche 0,03125 ; haut-droite 0,125 ; bas-droite
     * 1,0 après -3 EV). Colonne 31 = texel de gauche, colonne 32 = texel de droite ; ligne 31 =
     * haut, ligne 32 = bas. */
    const struct {
        uint32_t x, y;
        int quadrant;
    } probes[] = {{31, 16, 0}, {32, 16, 1}, {31, 48, 2}, {32, 48, 3}, {16, 31, 0},
                  {16, 32, 2}, {48, 31, 1}, {48, 32, 3}, {0, 0, 0},   {63, 63, 3}};
    for (size_t i = 0; i < sizeof probes / sizeof probes[0]; i++) {
        float rgb[3];
        for (int c = 0; c < 3; c++) {
            rgb[c] = QUADRANTS[probes[i].quadrant * 4 + c];
        }
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, probes[i].x, probes[i].y,
                             expected_byte(&setting, rgb, 0), expected_byte(&setting, rgb, 1),
                             expected_byte(&setting, rgb, 2), 1));
    }
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, blue_first_targets_get_the_same_picture_with_or_without_srgb)
{
    const uint32_t formats[] = {FORMAT_BGRA, FORMAT_BGRA_SRGB};
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; i++) {
        TonemapTest t;
        if (!open_tonemap(&t, formats[i])) {
            EXPECT_FALSE(render_gpu_required());
            return;
        }
        SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, QUADRANTS);
        ASSERT_TRUE(hdr != NULL);
        /* B8G8R8A8 est le format de swapchain le plus courant : sa variante _SRGB encode comme le
         * RGBA, le shader ne doit pas encoder une seconde fois. */
        const EngTonemap setting = {.op = ENG_TONEMAP_AGX, .exposure_ev = 0.5F};
        eng_renderer_set_tonemap(&t.r, &setting);
        ASSERT_TRUE(resolve_to_pixels(&t, hdr, formats[i], NULL, NULL, g_pixels));
        EXPECT_EQ(quadrant_mismatches(&setting, QUADRANTS, TOL), 0);
        SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
        CLOSE_TONEMAP(t);
    }
}

UTEST(eng_tonemap_gpu, overflow_negative_and_zero_texels_are_safe)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const float texels[16] = {
        INFINITY, 65504.0F, 1000.0F, 1.0F, /* débordement : du blanc, jamais du noir */
        -1.0F,    -4.0F,    0.0F,    1.0F, /* négatif et nul : noir */
        0.0F,     0.0F,     0.0F,    1.0F, /* noir */
        65504.0F, 0.0F,     16.0F,   1.0F, /* très lumineux, canaux inégaux */
    };
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, texels);
    ASSERT_TRUE(hdr != NULL);
    for (int op = 0; op < ENG_TONEMAP_COUNT; op++) {
        const EngTonemap setting = {.op = (EngTonemapOperator)op, .exposure_ev = 0.0F};
        eng_renderer_set_tonemap(&t.r, &setting);
        ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, NULL, NULL, g_pixels));
        EXPECT_EQ(quadrant_mismatches(&setting, texels, TOL), 0);
        EXPECT_TRUE(g_pixels[((size_t)QY[0] * SIZE + QX[0]) * 4] > 250); /* inf : blanc */
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, QX[1], QY[1], 0, 0, 0, 0));
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, QX[2], QY[2], 0, 0, 0, 0));
    }
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, a_flat_hdr_image_fills_the_whole_target_and_alpha_is_opaque)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    float texels[SIZE * SIZE * 4];
    for (uint32_t i = 0; i < SIZE * SIZE; i++) {
        texels[i * 4 + 0] = 0.5F;
        texels[i * 4 + 1] = 0.5F;
        texels[i * 4 + 2] = 0.5F;
        texels[i * 4 + 3] = 0.0F; /* alpha HDR nul : sans effet sur la sortie */
    }
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, SIZE, SIZE, texels);
    ASSERT_TRUE(hdr != NULL);
    const EngTonemap setting = {.op = ENG_TONEMAP_NONE, .exposure_ev = 0.0F};
    eng_renderer_set_tonemap(&t.r, &setting);
    ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, NULL, NULL, g_pixels));
    /* 0,5 linéaire -> sRGB 0,7354 -> 188 : partout, y compris les quatre coins et les bords (le
     * triangle plein écran ne laisse aucun trou) */
    const uint32_t xs[] = {0, SIZE - 1U, 0, SIZE - 1U, 31, 32};
    const uint32_t ys[] = {0, 0, SIZE - 1U, SIZE - 1U, 31, 32};
    for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, xs[i], ys[i], 188, 188, 188, 1));
    }
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

/* ── renderer ── */

UTEST(eng_tonemap_gpu, the_renderer_exposes_the_hdr_state_and_sanitizes_the_settings)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_TRUE(eng_renderer_hdr_enabled(&t.r));
    EXPECT_EQ(eng_renderer_scene_format(&t.r), (uint32_t)SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
    EXPECT_TRUE(t.f.gpu.hdr_enabled);
    EngTonemap now = eng_renderer_tonemap(&t.r);
    EXPECT_EQ((int)now.op, (int)ENG_TONEMAP_ACES); /* défaut */
    EXPECT_NEAR(now.exposure_ev, 0.0F, 0.0F);

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EngTonemap bad = {.op = (EngTonemapOperator)77, .exposure_ev = NAN};
    eng_renderer_set_tonemap(&t.r, &bad);
    now = eng_renderer_tonemap(&t.r);
    EXPECT_EQ((int)now.op, (int)ENG_TONEMAP_ACES);
    EXPECT_NEAR(now.exposure_ev, 0.0F, 0.0F);
    EXPECT_EQ((int)bad.op, 77); /* l'appelant n'est pas modifié */

    EngTonemap loud = {.op = ENG_TONEMAP_AGX, .exposure_ev = 99.0F};
    eng_renderer_set_tonemap(&t.r, &loud);
    now = eng_renderer_tonemap(&t.r);
    EXPECT_EQ((int)now.op, (int)ENG_TONEMAP_AGX);
    EXPECT_NEAR(now.exposure_ev, ENG_EXPOSURE_EV_MAX, 0.0F);
    eng_renderer_set_tonemap(&t.r, NULL); /* sans effet */
    EXPECT_EQ((int)eng_renderer_tonemap(&t.r).op, (int)ENG_TONEMAP_AGX);

    /* les pipelines sans color_format dessinent dans la cible HDR */
    EngPipelineDesc desc;
    eng_model_pipeline_desc(&desc);
    desc.depth_test = false;
    desc.depth_write = false;
    const EngPipelineHandle pipeline = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipeline));
    EXPECT_EQ(eng_renderer_pipeline_color_format(&t.r, pipeline),
              (uint32_t)SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
    desc.color_format = FORMAT_UNORM; /* un format explicite reste respecté */
    const EngPipelineHandle explicit_format = eng_renderer_create_pipeline(&t.r, &desc);
    EXPECT_EQ(eng_renderer_pipeline_color_format(&t.r, explicit_format), FORMAT_UNORM);
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, pipeline));
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, explicit_format));
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, resolve_refuses_null_parameters_and_counts_its_draws)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, QUADRANTS);
    ASSERT_TRUE(hdr != NULL);
    EXPECT_FALSE(eng_renderer_resolve_hdr(&t.r, NULL, NULL, NULL));
    EXPECT_FALSE(eng_renderer_resolve_hdr(&t.r, NULL, NULL, hdr));
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(t.f.gpu.device);
    ASSERT_TRUE(commands != NULL);
    EXPECT_FALSE(eng_renderer_resolve_hdr(&t.r, commands, NULL, hdr)); /* pas de passe */
    (void)SDL_CancelGPUCommandBuffer(commands);
    EXPECT_EQ(t.r.stats.resolve_draws, (uint64_t)0);
    ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, NULL, NULL, g_pixels));
    ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, NULL, NULL, g_pixels));
    EXPECT_EQ(t.r.stats.resolve_draws, (uint64_t)2);
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, no_depth_pipelines_are_refused_with_depth_tests)
{
    TonemapTest t;
    if (!open_tonemap_with(&t, OW_SHADER_DIR, true, FORMAT_UNORM, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngPipelineDesc desc;
    eng_model_pipeline_desc(&desc); /* depth_test + depth_write */
    desc.no_depth_target = true;
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &desc)));
    desc.depth_test = false;
    desc.depth_write = false;
    const EngPipelineHandle ok = eng_renderer_create_pipeline(&t.r, &desc);
    EXPECT_TRUE(eng_renderer_pipeline_is_valid(&t.r, ok));
    /* aucune profondeur déclarée, alors que le renderer en a une : celle de sa passe sans
     * profondeur */
    EXPECT_TRUE(eng_renderer_depth_format(&t.r) != 0);
    EXPECT_EQ(eng_renderer_pipeline_depth_format(&t.r, ok), (uint32_t)0);
    desc.no_depth_target = false; /* le même pipeline, dans la passe de scène : la profondeur */
    const EngPipelineHandle scene = eng_renderer_create_pipeline(&t.r, &desc);
    EXPECT_EQ(eng_renderer_pipeline_depth_format(&t.r, scene), eng_renderer_depth_format(&t.r));
    /* le pipeline de résolution du renderer lui-même n'a pas de profondeur */
    EXPECT_TRUE(eng_renderer_hdr_enabled(&t.r));
    EXPECT_EQ(eng_renderer_pipeline_depth_format(&t.r, t.r.tonemap_pipeline), (uint32_t)0);
    EXPECT_EQ(eng_renderer_pipeline_depth_format(&t.r, ENG_PIPELINE_NULL), (uint32_t)0);
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, scene));
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, ok));
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, shutdown_gives_hdr_back_and_counts_no_leak)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_TRUE(t.f.gpu.hdr_enabled);
    EXPECT_TRUE(t.f.gpu.resolve != NULL);
    EXPECT_EQ(eng_renderer_shutdown(&t.r),
              (uint32_t)0); /* nos pipeline et sampler : pas des fuites */
    EXPECT_FALSE(t.f.gpu.hdr_enabled);
    EXPECT_TRUE(t.f.gpu.resolve == NULL);
    EXPECT_FALSE(eng_renderer_hdr_enabled(&t.r)); /* arrêté */
    eng_arena_shutdown(&t.permanent);
    close_render_fixture(&t.f);
}

UTEST(eng_tonemap_gpu, without_the_hdr_option_nothing_changes)
{
    TonemapTest t;
    if (!open_tonemap_with(&t, OW_SHADER_DIR, false, 0, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_FALSE(eng_renderer_hdr_enabled(&t.r));
    EXPECT_FALSE(t.f.gpu.hdr_enabled);
    EXPECT_EQ(eng_renderer_scene_format(&t.r), eng_gpu_swapchain_format(&t.f.gpu));
    EXPECT_TRUE(t.f.gpu.resolve == NULL);
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, 2, 2, QUADRANTS);
    ASSERT_TRUE(hdr != NULL);
    EXPECT_FALSE(eng_renderer_resolve_hdr(&t.r, (SDL_GPUCommandBuffer *)&t, (SDL_GPURenderPass *)&t,
                                          hdr)); /* refusé avant tout usage des pointeurs */
    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, missing_tonemap_shaders_fall_back_to_a_working_renderer_without_hdr)
{
    TonemapTest t;
    if (!open_tonemap_with(&t, "/nonexistent-shader-dir", true, FORMAT_UNORM, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_FALSE(eng_renderer_hdr_enabled(&t.r));
    EXPECT_FALSE(t.f.gpu.hdr_enabled); /* le GPU n'a pas été basculé en HDR */
    EXPECT_TRUE(t.f.gpu.resolve == NULL);
    EXPECT_EQ(eng_renderer_scene_format(&t.r), eng_gpu_swapchain_format(&t.f.gpu));
    EXPECT_EQ(t.r.pipelines.count, (uint32_t)0); /* pas de pipeline de résolution orphelin */
    /* une image complète passe quand même */
    EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(queue != NULL);
    eng_renderer_end_frame(&t.r);
    EXPECT_EQ(t.f.gpu.frames_submitted, (uint64_t)1);
    CLOSE_TONEMAP(t);
}

/* ── image complète : scène -> HDR -> résolution -> overlay ── */

typedef struct OverlayCheck {
    EngGpu *gpu;
    uint64_t resolves_before;
    int draws;
} OverlayCheck;

static void count_overlay(void *user, SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass)
{
    OverlayCheck *check = (OverlayCheck *)user;
    (void)commands;
    (void)pass;
    check->resolves_before = check->gpu->resolve_passes;
    check->draws++;
}

UTEST(eng_tonemap_gpu, a_full_frame_resolves_once_then_draws_the_overlay)
{
    TonemapTest t;
    if (!open_tonemap_with(&t, OW_SHADER_DIR, true, 0, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(eng_renderer_hdr_enabled(&t.r));
    EXPECT_EQ(eng_renderer_scene_format(&t.r), (uint32_t)SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
    OverlayCheck check = {.gpu = &t.f.gpu};
    for (int i = 1; i <= 3; i++) {
        EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
        ASSERT_TRUE(queue != NULL);
        eng_renderer_end_frame(&t.r);
        EXPECT_EQ(t.r.stats.resolve_draws, (uint64_t)i); /* sans overlay : une résolution */
        EXPECT_EQ(t.f.gpu.resolve_passes, (uint64_t)i);
    }
    eng_renderer_set_overlay(&t.r, NULL, count_overlay, &check);
    EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(queue != NULL);
    eng_renderer_end_frame(&t.r);
    EXPECT_EQ(check.draws, 1);
    EXPECT_EQ(check.resolves_before, (uint64_t)4); /* la résolution a eu lieu AVANT l'overlay */
    EXPECT_EQ(t.r.stats.resolve_draws, (uint64_t)4);
    EXPECT_EQ(t.f.gpu.frames_submitted, (uint64_t)4);
    EXPECT_EQ(t.f.gpu.frames_skipped, (uint64_t)0);
    eng_renderer_set_overlay(&t.r, NULL, NULL, NULL);
    CLOSE_TONEMAP(t);
}

typedef struct FlatVertex {
    float position[3];
    float color[3];
} FlatVertex;

UTEST(eng_tonemap_gpu, a_full_frame_with_a_draw_in_the_hdr_pass_raises_no_gpu_error)
{
    TonemapTest t;
    if (!open_tonemap_with(&t, OW_SHADER_DIR, true, 0, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(eng_renderer_hdr_enabled(&t.r));
    /* Un triangle plein écran dessiné par le renderer dans la passe de scène (profondeur
     * comprise), puis la résolution : la validation de SDL_GPU (mode debug) refuse un pipeline dont
     * les formats ne correspondent pas à la passe, et note l'erreur. La passe de scène doit donc
     * viser la texture HDR avec la profondeur, la résolution la swapchain sans profondeur. */
    static const FlatVertex triangle[3] = {{{-1.0F, -1.0F, 0.5F}, {4.0F, 0.0F, 0.0F}},
                                           {{3.0F, -1.0F, 0.5F}, {4.0F, 0.0F, 0.0F}},
                                           {{-1.0F, 3.0F, 0.5F}, {4.0F, 0.0F, 0.0F}}};
    const EngBufferDesc buffer = {
        .usage = ENG_BUFFER_VERTEX, .size = sizeof triangle, .data = triangle};
    const EngBufferHandle vb = eng_renderer_create_buffer(&t.r, &buffer);
    EngPipelineDesc desc = {.name = "flat",
                            .vertex_shader = "triangle.vert",
                            .fragment_shader = "triangle.frag",
                            .vertex_stride = sizeof(FlatVertex),
                            .attribute_count = 2,
                            .cull_mode = ENG_CULL_NONE,
                            .depth_test = true,
                            .depth_write = true};
    desc.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(FlatVertex, position)};
    desc.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(FlatVertex, color)};
    const EngPipelineHandle pipeline = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipeline));
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, vb));
    for (int i = 0; i < 3; i++) {
        SDL_ClearError();
        EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
        ASSERT_TRUE(queue != NULL);
        EXPECT_TRUE(eng_render_draw(
            queue, &(EngDrawCmd){.pipeline = pipeline, .vertex_buffer = vb, .count = 3}));
        eng_renderer_end_frame(&t.r);
        EXPECT_STREQ(SDL_GetError(), "");
    }
    EXPECT_EQ(t.r.stats.draws, (uint64_t)3);
    EXPECT_EQ(t.r.stats.resolve_draws, (uint64_t)3);
    EXPECT_EQ(t.f.gpu.frames_submitted, (uint64_t)3);
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, pipeline));
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, vb));
    CLOSE_TONEMAP(t);
}

UTEST(eng_tonemap_gpu, the_main_pass_writes_unclamped_values_into_the_gpu_hdr_texture)
{
    TonemapTest t;
    if (!open_tonemap_with(&t, OW_SHADER_DIR, true, 0, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    static const FlatVertex big[3] = {{{-1.0F, -1.0F, 0.5F}, {4.0F, 0.5F, 0.0F}},
                                      {{3.0F, -1.0F, 0.5F}, {4.0F, 0.5F, 0.0F}},
                                      {{-1.0F, 3.0F, 0.5F}, {4.0F, 0.5F, 0.0F}}};
    const EngBufferDesc buffer = {.usage = ENG_BUFFER_VERTEX, .size = sizeof big, .data = big};
    const EngBufferHandle vb = eng_renderer_create_buffer(&t.r, &buffer);
    EngPipelineDesc desc = {.name = "flat",
                            .vertex_shader = "triangle.vert",
                            .fragment_shader = "triangle.frag",
                            .vertex_stride = sizeof(FlatVertex),
                            .attribute_count = 2,
                            .cull_mode = ENG_CULL_NONE,
                            .depth_test = true,
                            .depth_write = true};
    desc.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(FlatVertex, position)};
    desc.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(FlatVertex, color)};
    const EngPipelineHandle pipeline = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipeline));
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, vb));

    EngGpu *gpu = &t.f.gpu;
    ASSERT_TRUE(eng_gpu_begin_frame(gpu));
    ASSERT_TRUE(gpu->hdr_texture != NULL);
    ASSERT_EQ(gpu->hdr_width, SIZE);
    EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(queue != NULL);
    ASSERT_TRUE(eng_render_draw(
        queue, &(EngDrawCmd){.pipeline = pipeline, .vertex_buffer = vb, .count = 3}));
    /* la VRAIE passe principale du moteur ; on la termine nous-mêmes (sans résolution : on veut
     * lire la texture HDR) puis on la télécharge avant de soumettre l'image */
    SDL_GPURenderPass *pass = eng_gpu_begin_render_pass(gpu);
    ASSERT_TRUE(pass != NULL);
    ASSERT_EQ(eng_renderer_execute(&t.r, &t.r.queue, gpu->command_buffer, pass), (uint32_t)1);
    SDL_EndGPURenderPass(pass);
    gpu->render_pass = NULL;
    gpu->main_pass_ended = true;

    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
        .size = SIZE * SIZE * 4U * (uint32_t)sizeof(uint16_t)};
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(gpu->device, &buffer_info);
    ASSERT_TRUE(download != NULL);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(gpu->command_buffer);
    const SDL_GPUTextureRegion region = {.texture = gpu->hdr_texture, .w = SIZE, .h = SIZE, .d = 1};
    const SDL_GPUTextureTransferInfo destination = {
        .transfer_buffer = download, .pixels_per_row = SIZE, .rows_per_layer = SIZE};
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(gpu->command_buffer);
    gpu->command_buffer = NULL; /* consommé : end_frame n'a plus rien à soumettre */
    gpu->swapchain_texture = NULL;
    ASSERT_TRUE(fence != NULL);
    ASSERT_TRUE(SDL_WaitForGPUFences(gpu->device, true, &fence, 1));
    SDL_ReleaseGPUFence(gpu->device, fence);
    const uint16_t *halves =
        (const uint16_t *)SDL_MapGPUTransferBuffer(gpu->device, download, false);
    ASSERT_TRUE(halves != NULL);
    const uint32_t probes[][2] = {{0, 0}, {63, 0}, {32, 32}, {0, 63}, {63, 63}};
    for (size_t i = 0; i < sizeof probes / sizeof probes[0]; i++) {
        const uint16_t *pixel = halves + ((size_t)probes[i][1] * SIZE + probes[i][0]) * 4U;
        EXPECT_NEAR(from_half(pixel[0]), 4.0F, 0.0F); /* bien au-dessus de 1,0 : ni écrêté... */
        EXPECT_NEAR(from_half(pixel[1]), 0.5F, 0.0F); /* ... ni arrondi sur 8 bits */
        EXPECT_NEAR(from_half(pixel[2]), 0.0F, 0.0F);
    }
    SDL_UnmapGPUTransferBuffer(gpu->device, download);
    SDL_ReleaseGPUTransferBuffer(gpu->device, download);
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, pipeline));
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, vb));
    CLOSE_TONEMAP(t);
}

/* ── la chaîne complète : éclairage PBR HDR -> tonemapping -> pixels ── */

typedef struct SceneContext {
    TonemapTest *t;
    EngPipelineHandle pipeline;
    SDL_GPUTexture *hdr;
} SceneContext;

static bool draw_scene_into_hdr(SDL_GPUCommandBuffer *commands, void *context)
{
    SceneContext *c = (SceneContext *)context;
    const SDL_GPUColorTargetInfo color = {.texture = c->hdr,
                                          .clear_color = {0.0F, 0.0F, 0.0F, 1.0F},
                                          .load_op = SDL_GPU_LOADOP_CLEAR,
                                          .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &color, 1, NULL);
    if (pass == NULL) {
        return false;
    }
    (void)eng_renderer_execute(&c->t->r, &c->t->r.queue, commands, pass);
    SDL_EndGPURenderPass(pass);
    return true;
}

UTEST(eng_tonemap_gpu, a_lit_model_keeps_values_above_one_until_the_tonemapping)
{
    TonemapTest t;
    if (!open_tonemap(&t, FORMAT_UNORM)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngArena level;
    ASSERT_TRUE(eng_arena_init(&level, "tonemap-level", (size_t)1 << 20));
    /* Triangle rouge, soleil blanc d'intensité 3 en +Z, ambiante 0,5 : R ≈ 3 x 0,96 + 0,5 x
     * 0,96 ≈ 3,4 en PBR (bien au-delà de 1,0), G et B petites (reflet diélectrique et ambiante).
     * Une cible 8 bits aurait écrêté à 1,0 avant l'exposition. */
    static uint8_t file[BUF_SIZE];
    const Spec spec = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,0,0,1],"
                                   "\"metallicFactor\":0,\"roughnessFactor\":1}}"};
    const size_t size = build(&spec, file, sizeof file, NULL, NULL);
    EngModel model;
    ASSERT_TRUE(eng_model_parse(&level, &t.f.scratch, file, size, NULL, &model));
    EngModelGpu gpu;
    const bool created = eng_model_gpu_create(&gpu, &t.r, &t.permanent, &model, 0.0F);
    eng_model_free(&model);
    ASSERT_TRUE(created);

    EngPipelineDesc desc;
    eng_model_pipeline_desc(&desc);
    desc.depth_test = false; /* pas de depth buffer dans ce renderer */
    desc.depth_write = false;
    const EngPipelineHandle pipeline = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipeline));

    EngLighting light = {0};
    light.sun_direction[2] = 1.0F;
    for (int c = 0; c < 3; c++) {
        light.sun_color[c] = 1.0F;
        light.ambient_color[c] = 1.0F;
    }
    light.sun_intensity = 3.0F;
    light.ambient_intensity = 0.5F;
    light.camera_position[2] = 5.0F;

    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(queue != NULL);
    ASSERT_EQ(eng_model_gpu_draw(&gpu, queue, pipeline, identity, identity, &light),
              gpu.part_count);

    const float zeros[SIZE * SIZE * 4] = {0};
    SDL_GPUTexture *hdr = upload_hdr(t.f.gpu.device, SIZE, SIZE, zeros);
    ASSERT_TRUE(hdr != NULL);
    SceneContext context = {&t, pipeline, hdr};
    EngPbrSurface surface;
    eng_pbr_surface_default(&surface);
    surface.base_color[0] = 1.0F;
    surface.base_color[1] = 0.0F;
    surface.base_color[2] = 0.0F;
    surface.metallic = 0.0F;
    surface.roughness = 1.0F;
    const float normal[3] = {0.0F, 0.0F, 1.0F};
    const float position[3] = {0.390625F, 0.234375F, 0.0F};
    float lit[3];
    test_shade(&light, &surface, normal, position, lit);
    EXPECT_GT(lit[0], 2.0F); /* la référence CPU elle-même : bien au-dessus de 1 */

    for (int op = 0; op < ENG_TONEMAP_COUNT; op++) {
        const EngTonemap setting = {.op = (EngTonemapOperator)op, .exposure_ev = -0.5F};
        eng_renderer_set_tonemap(&t.r, &setting);
        ASSERT_TRUE(
            resolve_to_pixels(&t, hdr, FORMAT_UNORM, draw_scene_into_hdr, &context, g_pixels));
        const float stored[3] = {from_half(to_half(lit[0])), from_half(to_half(lit[1])),
                                 from_half(to_half(lit[2]))};
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, 44, 24, expected_byte(&setting, stored, 0),
                             expected_byte(&setting, stored, 1), expected_byte(&setting, stored, 2),
                             TOL));
        EXPECT_TRUE(
            pixel_is(g_pixels, SIZE, 16, 48, 0, 0, 0, 0)); /* hors du triangle : fond noir */
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, 16, 16, 0, 0, 0, 0)); /* l'orientation est conservée */
    }
    /* contre-épreuve : avec NONE, 3,5 x 2^-0,5 = 2,47 reste au-dessus de 1 -> 255. Si la scène
     * avait été clampée à 1,0 avant l'exposition on aurait 0,707 -> 219. */
    const EngTonemap none = {.op = ENG_TONEMAP_NONE, .exposure_ev = -0.5F};
    eng_renderer_set_tonemap(&t.r, &none);
    ASSERT_TRUE(resolve_to_pixels(&t, hdr, FORMAT_UNORM, draw_scene_into_hdr, &context, g_pixels));
    /* R sature ; G et B gardent la valeur du reflet diélectrique (F0 = 0,04) et de l'ambiante, pas
     * zéro : elles se lisent après l'exposition seule */
    const float stored_none[3] = {from_half(to_half(lit[0])), from_half(to_half(lit[1])),
                                  from_half(to_half(lit[2]))};
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 44, 24, 255, expected_byte(&none, stored_none, 1),
                         expected_byte(&none, stored_none, 2), TOL));

    SDL_ReleaseGPUTexture(t.f.gpu.device, hdr);
    eng_model_gpu_destroy(&gpu, &t.r);
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, pipeline));
    eng_arena_shutdown(&level);
    CLOSE_TONEMAP(t);
}

#else /* sans shaders compilés */

UTEST(eng_tonemap_gpu, needs_compiled_shaders)
{
    EXPECT_TRUE(true);
}

#endif
