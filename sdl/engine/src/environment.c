#include "engine/environment.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <string.h>

#include <stb_image.h>

#include "engine/log.h"

/* LUT BRDF : la même pour tous les environnements, calculée une fois. */
static float g_lut[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
static bool g_lut_ready;

static const float *default_lut(void)
{
    if (!g_lut_ready) {
        eng_ibl_brdf_lut(g_lut, ENG_IBL_BRDF_LUT_SIZE, 512U);
        g_lut_ready = true;
    }
    return g_lut;
}

static bool is_pow2(uint32_t v)
{
    return v != 0U && (v & (v - 1U)) == 0U;
}

static EngTextureHandle upload_cube(EngRenderer *r, const char *name, const float *rgb,
                                    uint32_t size, uint32_t levels)
{
    const size_t texels = eng_ibl_cube_floats(size, levels) / 3U;
    uint16_t *half = (uint16_t *)SDL_malloc(texels * 4U * sizeof(uint16_t));
    if (half == NULL) {
        ENG_LOG_ERROR("environnement : mémoire insuffisante (%s)", name);
        return ENG_TEXTURE_NULL;
    }
    eng_ibl_pack_rgb_to_rgba16f(rgb, texels, half);
    const EngTextureDataDesc desc = {.name = name,
                                     .width = size,
                                     .height = size,
                                     .layers = 6,
                                     .mip_levels = levels,
                                     .format = ENG_TEXTURE_RGBA16F,
                                     .data = half};
    const EngTextureHandle handle = eng_renderer_create_texture_data(r, &desc);
    SDL_free(half);
    return handle;
}

static bool create_common(EngEnvironment *env, EngRenderer *r)
{
    static const uint32_t dummy[1] = {0};
    const EngSamplerDesc sampler = {
        .name = "environment", .address_u = ENG_ADDRESS_CLAMP, .address_v = ENG_ADDRESS_CLAMP};
    env->sampler = eng_renderer_create_sampler(r, &sampler);
    const EngBufferDesc buffer = {.name = "Sky dummy vertices",
                                  .usage = ENG_BUFFER_VERTEX,
                                  .size = sizeof dummy,
                                  .data = dummy};
    env->dummy_vertex = eng_renderer_create_buffer(r, &buffer);
    uint16_t lut16[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
    eng_ibl_pack_rg_to_rg16f(default_lut(), (size_t)ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE,
                             lut16);
    const EngTextureDataDesc lut = {.name = "environment-brdf-lut",
                                    .width = ENG_IBL_BRDF_LUT_SIZE,
                                    .height = ENG_IBL_BRDF_LUT_SIZE,
                                    .layers = 1,
                                    .mip_levels = 1,
                                    .format = ENG_TEXTURE_RG16F,
                                    .data = lut16};
    env->brdf_lut = eng_renderer_create_texture_data(r, &lut);
    return eng_renderer_sampler_is_valid(r, env->sampler) &&
           eng_renderer_buffer_is_valid(r, env->dummy_vertex) &&
           eng_renderer_texture_is_valid(r, env->brdf_lut);
}

bool eng_environment_load_hdr(const char *path, float **rgb, uint32_t *width, uint32_t *height)
{
    int w = 0;
    int h = 0;
    int channels = 0;
    float *data = stbi_loadf(path, &w, &h, &channels, 3);
    if (data == NULL || w < 2 || h < 2 || w > 16384 || h > 16384) {
        ENG_LOG_ERROR("environnement : image HDR illisible (%s) : %s", path,
                      stbi_failure_reason() != NULL ? stbi_failure_reason() : "dimensions");
        if (data != NULL) {
            stbi_image_free(data);
        }
        return false;
    }
    *rgb = data;
    *width = (uint32_t)w;
    *height = (uint32_t)h;
    return true;
}

void eng_environment_free_hdr(float *rgb)
{
    if (rgb != NULL) {
        stbi_image_free(rgb);
    }
}

bool eng_environment_is_valid(const EngEnvironment *env, const EngRenderer *r)
{
    return eng_renderer_texture_is_valid(r, env->sky) &&
           eng_renderer_texture_is_valid(r, env->irradiance) &&
           eng_renderer_texture_is_valid(r, env->prefiltered) &&
           eng_renderer_texture_is_valid(r, env->brdf_lut) && env->prefiltered_levels != 0;
}

void eng_environment_destroy(EngEnvironment *env, EngRenderer *r)
{
    if (eng_renderer_texture_is_valid(r, env->sky)) {
        (void)eng_renderer_destroy_texture(r, env->sky);
    }
    if (eng_renderer_texture_is_valid(r, env->irradiance)) {
        (void)eng_renderer_destroy_texture(r, env->irradiance);
    }
    if (eng_renderer_texture_is_valid(r, env->prefiltered)) {
        (void)eng_renderer_destroy_texture(r, env->prefiltered);
    }
    if (eng_renderer_texture_is_valid(r, env->brdf_lut)) {
        (void)eng_renderer_destroy_texture(r, env->brdf_lut);
    }
    if (eng_renderer_sampler_is_valid(r, env->sampler)) {
        (void)eng_renderer_destroy_sampler(r, env->sampler);
    }
    if (eng_renderer_buffer_is_valid(r, env->dummy_vertex)) {
        (void)eng_renderer_destroy_buffer(r, env->dummy_vertex);
    }
    *env = (EngEnvironment){0};
}

bool eng_environment_create_uniform(EngEnvironment *env, EngRenderer *r, const float rgb[3])
{
    *env = (EngEnvironment){0};
    const float cube[6 * 3] = {rgb[0], rgb[1], rgb[2], rgb[0], rgb[1], rgb[2],
                               rgb[0], rgb[1], rgb[2], rgb[0], rgb[1], rgb[2],
                               rgb[0], rgb[1], rgb[2], rgb[0], rgb[1], rgb[2]};
    env->sky = upload_cube(r, "environment-sky", cube, 1U, 1U);
    env->irradiance = upload_cube(r, "environment-irradiance", cube, 1U, 1U);
    env->prefiltered = upload_cube(r, "environment-prefiltered", cube, 1U, 1U);
    env->prefiltered_levels = 1U;
    if (!create_common(env, r) || !eng_environment_is_valid(env, r)) {
        ENG_LOG_ERROR("environnement : création impossible");
        eng_environment_destroy(env, r);
        return false;
    }
    return true;
}

bool eng_environment_create(EngEnvironment *env, EngRenderer *r, const EngEnvironmentDesc *desc)
{
    *env = (EngEnvironment){0};
    const uint32_t sky_size =
        desc->sky_size != 0 ? desc->sky_size : ENG_ENVIRONMENT_DEFAULT_SKY_SIZE;
    const uint32_t irr_size = desc->irradiance_size != 0 ? desc->irradiance_size
                                                         : ENG_ENVIRONMENT_DEFAULT_IRRADIANCE_SIZE;
    const uint32_t pre_size = desc->prefiltered_size != 0
                                  ? desc->prefiltered_size
                                  : ENG_ENVIRONMENT_DEFAULT_PREFILTERED_SIZE;
    const uint32_t samples = desc->samples != 0 ? desc->samples : ENG_ENVIRONMENT_DEFAULT_SAMPLES;
    const bool equirect = desc->equirect_rgb != NULL;
    if (!is_pow2(sky_size) || !is_pow2(irr_size) || !is_pow2(pre_size) || sky_size < 8U ||
        irr_size < 2U || pre_size < 8U || sky_size > 1024U || pre_size > sky_size ||
        (equirect && (desc->equirect_width < 2U || desc->equirect_height < 2U))) {
        ENG_LOG_ERROR("environnement : tailles invalides (puissances de 2, ciel >= préfiltré)");
        return false;
    }
    const uint32_t sky_levels = eng_ibl_full_levels(sky_size);
    /* niveaux du pré-filtré : on s'arrête à 16 x 16 (le dernier niveau est à r = 1 ; à 8 x 8 par
     * face, l'erreur d'échantillonnage dépassait 12 % près d'un soleil très intense) */
    uint32_t pre_levels = 1;
    while ((pre_size >> pre_levels) >= 16U && pre_levels < 7U) {
        pre_levels++;
    }
    float *sky = (float *)SDL_calloc(eng_ibl_cube_floats(sky_size, sky_levels), sizeof(float));
    float *irr = (float *)SDL_calloc(eng_ibl_cube_floats(irr_size, 1U), sizeof(float));
    float *pre = (float *)SDL_calloc(eng_ibl_cube_floats(pre_size, pre_levels), sizeof(float));
    bool ok = sky != NULL && irr != NULL && pre != NULL;
    const uint64_t start = SDL_GetPerformanceCounter();
    if (ok) {
        if (equirect) {
            eng_ibl_from_equirect(sky, sky_size, sky_levels, desc->equirect_rgb,
                                  desc->equirect_width, desc->equirect_height);
        } else {
            EngSkyDesc fallback;
            eng_ibl_sky_default(&fallback);
            eng_ibl_make_sky(sky, sky_size, sky_levels, desc->sky != NULL ? desc->sky : &fallback);
        }
        eng_ibl_build_mips(sky, sky_size, sky_levels);
        eng_ibl_irradiance(sky, sky_size, sky_levels, irr, irr_size);
        eng_ibl_prefilter(sky, sky_size, sky_levels, pre, pre_size, pre_levels, samples);
        env->sky = upload_cube(r, "environment-sky", sky, sky_size, sky_levels);
        env->irradiance = upload_cube(r, "environment-irradiance", irr, irr_size, 1U);
        env->prefiltered = upload_cube(r, "environment-prefiltered", pre, pre_size, pre_levels);
        env->prefiltered_levels = pre_levels;
        ok = create_common(env, r) && eng_environment_is_valid(env, r);
        ENG_LOG_INFO("environnement : ciel %u, irradiance %u, préfiltré %u (%u niveaux), %u "
                     "échantillons, %.0f ms",
                     sky_size, irr_size, pre_size, pre_levels, samples,
                     (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                         (double)SDL_GetPerformanceFrequency());
    } else {
        ENG_LOG_ERROR("environnement : mémoire insuffisante");
    }
    SDL_free(sky);
    SDL_free(irr);
    SDL_free(pre);
    if (!ok) {
        eng_environment_destroy(env, r);
    }
    return ok;
}

void eng_sky_pipeline_desc(EngPipelineDesc *desc)
{
    *desc = (EngPipelineDesc){
        .name = "sky",
        .vertex_shader = "sky.vert",
        .fragment_shader = "sky.frag",
        .cull_mode = ENG_CULL_NONE,
        .depth_test = false,
        .depth_write = false,
    };
}

typedef struct SkyVertexUniforms {
    float inv_view_proj[16];
    float camera[4];
} SkyVertexUniforms;

bool eng_environment_draw_sky(const EngEnvironment *env, EngRenderQueue *queue,
                              EngPipelineHandle pipeline, mat4 view_proj, const float camera[3],
                              float intensity)
{
    SkyVertexUniforms vertex;
    mat4 inverse;
    glm_mat4_inv(view_proj, inverse);
    memcpy(vertex.inv_view_proj, inverse, sizeof vertex.inv_view_proj);
    vertex.camera[0] = camera[0];
    vertex.camera[1] = camera[1];
    vertex.camera[2] = camera[2];
    vertex.camera[3] = 0.0F;
    const float fragment[4] = {intensity >= 0.0F ? intensity : 0.0F, 0.0F, 0.0F, 0.0F};
    const EngDrawCmd cmd = {
        .pipeline = pipeline,
        .vertex_buffer = env->dummy_vertex,
        .count = 3,
        .texture = env->sky,
        .sampler = env->sampler,
    };
    return eng_render_draw_uniforms2(queue, &cmd, &vertex, sizeof vertex, fragment,
                                     sizeof fragment);
}
