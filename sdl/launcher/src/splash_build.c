/* Écran de chargement : ressources GPU (pipeline, image du titre). Voir splash.h. */
#include <string.h>

#include "engine/files.h"
#include "engine/shader.h"
#include "engine/text_sdf.h"
#include "splash_internal.h"

#define TITLE_TEXT "RED STRINGS"
#define LABEL_TEXT "CHARGEMENT"

static bool read_font(EngArena *scratch, const char *relative, EngFileData *out)
{
    char path[1024];
    return eng_path_resolve(path, sizeof path, relative) && eng_file_read_all(scratch, path, out);
}

/* le titre (gras, lettres espacées) au-dessus de la légende, dans une image à un canal */
bool splash_build_text(Splash *s, EngArena *scratch)
{
    EngFileData bold;
    EngFileData regular;
    EngTextSdf title;
    EngTextSdf label;
    if (!read_font(scratch, "assets/fonts/NotoSans-Bold.ttf", &bold) ||
        !read_font(scratch, "assets/fonts/NotoSans-Regular.ttf", &regular) ||
        !eng_text_sdf_build(scratch, bold.data, bold.size, TITLE_TEXT, 150.0F, 30.0F, 22.0F,
                            &title) ||
        !eng_text_sdf_build(scratch, regular.data, regular.size, LABEL_TEXT, 46.0F, 16.0F, 8.0F,
                            &label)) {
        return false;
    }
    const uint32_t width = title.width > label.width ? title.width : label.width;
    const uint32_t height = title.height + label.height;
    uint8_t *pixels = ENG_ARENA_NEW_ARRAY(scratch, uint8_t, (size_t)width * height);
    if (pixels == NULL) {
        return false;
    }
    memset(pixels, 0, (size_t)width * height);
    for (uint32_t y = 0; y < title.height; y++) {
        memcpy(&pixels[(size_t)y * width], &title.pixels[(size_t)y * title.width], title.width);
    }
    for (uint32_t y = 0; y < label.height; y++) {
        memcpy(&pixels[(size_t)(title.height + y) * width], &label.pixels[(size_t)y * label.width],
               label.width);
    }
    s->title_rect[0] = 0.0F;
    s->title_rect[1] = 0.0F;
    s->title_rect[2] = (float)title.width / (float)width;
    s->title_rect[3] = (float)title.height / (float)height;
    s->label_rect[0] = 0.0F;
    s->label_rect[1] = (float)title.height / (float)height;
    s->label_rect[2] = (float)label.width / (float)width;
    s->label_rect[3] = 1.0F;
    s->text_info[0] = (float)title.width / (float)title.height;
    s->text_info[1] = (float)label.width / (float)label.height;

    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    s->text = SDL_CreateGPUTexture(s->device, &info);
    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = width * height};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(s->device, &buffer_info);
    uint8_t *mapped = buffer != NULL ? SDL_MapGPUTransferBuffer(s->device, buffer, false) : NULL;
    SDL_GPUCommandBuffer *cmd =
        s->text != NULL && mapped != NULL ? SDL_AcquireGPUCommandBuffer(s->device) : NULL;
    bool ok = false;
    if (cmd != NULL) {
        memcpy(mapped, pixels, (size_t)width * height);
        SDL_UnmapGPUTransferBuffer(s->device, buffer);
        mapped = NULL;
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        const SDL_GPUTextureTransferInfo source = {
            .transfer_buffer = buffer, .pixels_per_row = width, .rows_per_layer = height};
        const SDL_GPUTextureRegion region = {.texture = s->text, .w = width, .h = height, .d = 1};
        SDL_UploadToGPUTexture(copy, &source, &region, false);
        SDL_EndGPUCopyPass(copy);
        ok = SDL_SubmitGPUCommandBuffer(cmd);
    }
    if (mapped != NULL) {
        SDL_UnmapGPUTransferBuffer(s->device, buffer);
    }
    if (buffer != NULL) {
        SDL_ReleaseGPUTransferBuffer(s->device, buffer);
    }
    return ok;
}

bool splash_build_pipeline(Splash *s, EngGpu *gpu, EngArena *scratch)
{
    SDL_GPUShader *vertex = eng_shader_load(gpu, scratch, NULL, "tonemap.vert", ENG_SHADER_VERTEX);
    SDL_GPUShader *fragment =
        eng_shader_load(gpu, scratch, NULL, "splash.frag", ENG_SHADER_FRAGMENT);
    if (vertex != NULL && fragment != NULL) {
        SDL_GPUColorTargetDescription target;
        SDL_zero(target);
        target.format = SDL_GetGPUSwapchainTextureFormat(s->device, s->window);
        SDL_GPUGraphicsPipelineCreateInfo info;
        SDL_zero(info);
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.target_info.num_color_targets = 1;
        info.target_info.color_target_descriptions = &target;
        s->pipeline = SDL_CreateGPUGraphicsPipeline(s->device, &info);
    }
    if (vertex != NULL) {
        eng_shader_release(gpu, vertex);
    }
    if (fragment != NULL) {
        eng_shader_release(gpu, fragment);
    }
    SDL_GPUSamplerCreateInfo sampler;
    SDL_zero(sampler);
    sampler.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    s->sampler = SDL_CreateGPUSampler(s->device, &sampler);
    return s->pipeline != NULL && s->sampler != NULL;
}
