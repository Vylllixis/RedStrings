#include "engine/renderer.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "engine/gpu_debug.h"
#include "engine/image.h"
#include "engine/log.h"
#include "engine/shader_hot.h"

#define DEFAULT_MAX_BUFFERS 1024U
#define DEFAULT_MAX_PIPELINES 64U
#define DEFAULT_MAX_TEXTURES 256U
#define DEFAULT_MAX_SAMPLERS 32U
#define DEFAULT_MAX_COMMANDS 4096U
#define DEFAULT_MAX_UNIFORM_BYTES (256U * 1024U)
#define DEFAULT_MAX_SHADOW_UNIFORM_BYTES (64U * 1024U)
#define MAX_SHADOW_MAP_SIZE 8192U
#define PIPELINE_NAME_SIZE 64

typedef struct Buffer {
    SDL_GPUBuffer *gpu;
    uint32_t size;
    EngBufferUsage usage;
    /* Buffer de streaming (eng_renderer_create_stream_buffer) : tampon de transfert permanent et
     * zone écrite depuis le dernier envoi, [pending_begin, pending_end). */
    SDL_GPUTransferBuffer *staging;
    uint32_t pending_begin;
    uint32_t pending_end; /* 0 : rien en attente */
} Buffer;

typedef struct Texture {
    SDL_GPUTexture *gpu;
    uint32_t width;
    uint32_t height;
    uint32_t mip_levels;
    uint32_t layers;
    EngTextureFormat format;
} Texture;

typedef struct Sampler {
    SDL_GPUSampler *gpu;
    float anisotropy; /* appliquée ; 0 : désactivée */
} Sampler;

typedef struct Pipeline {
    SDL_GPUGraphicsPipeline *gpu;
    EngShaderHot vertex;
    EngShaderHot fragment;
    uint32_t built_vertex_gen; /* versions des shaders utilisées par `gpu` */
    uint32_t built_fragment_gen;
    char name[PIPELINE_NAME_SIZE];
    uint32_t vertex_stride;
    EngVertexAttribute attributes[ENG_MAX_VERTEX_ATTRIBUTES];
    uint32_t attribute_count;
    EngTopology topology;
    EngCullMode cull_mode;
    EngFrontFace front_face;
    uint32_t color_format;
    bool depth_test;
    bool depth_write;
    bool no_depth_target;
    bool depth_only;
} Pipeline;

/* ── Conversions vers SDL_GPU ── */

static bool to_sdl_vertex_format(EngVertexFormat format, SDL_GPUVertexElementFormat *out)
{
    switch (format) {
    case ENG_VERTEX_FLOAT1:
        *out = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
        return true;
    case ENG_VERTEX_FLOAT2:
        *out = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        return true;
    case ENG_VERTEX_FLOAT3:
        *out = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        return true;
    case ENG_VERTEX_FLOAT4:
        *out = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        return true;
    case ENG_VERTEX_UBYTE4_NORM:
        *out = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        return true;
    default:
        return false;
    }
}

/* ── Initialisation ── */

static bool copy_dir(char *out, size_t size, const char *dir, bool *has)
{
    *has = dir != NULL;
    if (dir == NULL) {
        out[0] = '\0';
        return true;
    }
    return SDL_strlcpy(out, dir, size) < size;
}

static void resolve_callback(void *user, SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass,
                             SDL_GPUTexture *hdr)
{
    (void)eng_renderer_resolve_hdr((EngRenderer *)user, commands, pass, hdr);
}

/* Cible sRGB : le GPU encode lui-même à l'écriture, le shader ne doit pas le refaire. */
static bool format_is_srgb(uint32_t format)
{
    return format == (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB ||
           format == (uint32_t)SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
}

/* Met en place le HDR : sampler et pipeline de résolution d'abord (ce qui peut échouer : shaders),
 * puis seulement le HDR du GPU. En cas d'échec, tout est défait et le renderer continue sans HDR.
 */
static void setup_hdr(EngRenderer *r, uint32_t resolve_format)
{
    r->resolve_format = resolve_format != 0 ? resolve_format : eng_gpu_swapchain_format(r->gpu);
    const SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    SDL_GPUSamplerCreateInfo named_sampler = sampler_info;
    const uint32_t sampler_props = eng_gpu_debug_props(
        "sampler", SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, "HDR scene sampler");
    named_sampler.props = sampler_props;
    r->hdr_sampler = SDL_CreateGPUSampler(r->gpu->device, &named_sampler);
    eng_gpu_debug_props_free(sampler_props);
    const EngPipelineDesc pipeline = {.name = "tonemap",
                                      .vertex_shader = "tonemap.vert",
                                      .fragment_shader = "tonemap.frag",
                                      .cull_mode = ENG_CULL_NONE,
                                      .color_format = r->resolve_format,
                                      .no_depth_target = true};
    if (r->hdr_sampler != NULL) {
        r->tonemap_pipeline = eng_renderer_create_pipeline(r, &pipeline);
    }
    if (r->hdr_sampler == NULL || !eng_renderer_pipeline_is_valid(r, r->tonemap_pipeline) ||
        !eng_gpu_enable_hdr(r->gpu)) {
        ENG_LOG_WARN("renderer : HDR indisponible (sampler, shaders tonemap.* ou GPU) : la scène "
                     "est dessinée directement dans l'écran");
        if (eng_renderer_pipeline_is_valid(r, r->tonemap_pipeline)) {
            (void)eng_renderer_destroy_pipeline(r, r->tonemap_pipeline);
        }
        r->tonemap_pipeline = ENG_PIPELINE_NULL;
        if (r->hdr_sampler != NULL) {
            SDL_ReleaseGPUSampler(r->gpu->device, r->hdr_sampler);
            r->hdr_sampler = NULL;
        }
        return;
    }
    r->hdr = true;
    r->scene_format = eng_gpu_hdr_format();
    eng_gpu_set_resolve(r->gpu, resolve_callback, r);
}

bool eng_renderer_init(EngRenderer *r, EngGpu *gpu, EngArena *permanent, EngArena *scratch,
                       const EngRendererDesc *desc)
{
    *r = (EngRenderer){0};
    if (gpu == NULL || gpu->device == NULL) {
        ENG_LOG_ERROR("renderer : GPU non initialisé");
        return false;
    }
    r->gpu = gpu;
    r->scratch = scratch;
    if (!copy_dir(r->source_dir, sizeof r->source_dir, desc->source_dir, &r->has_source_dir) ||
        !copy_dir(r->offline_dir, sizeof r->offline_dir, desc->offline_dir, &r->has_offline_dir)) {
        ENG_LOG_ERROR("renderer : chemin des shaders trop long");
        return false;
    }
    const uint32_t max_buffers = desc->max_buffers != 0 ? desc->max_buffers : DEFAULT_MAX_BUFFERS;
    const uint32_t max_pipelines =
        desc->max_pipelines != 0 ? desc->max_pipelines : DEFAULT_MAX_PIPELINES;
    const uint32_t max_textures =
        desc->max_textures != 0 ? desc->max_textures : DEFAULT_MAX_TEXTURES;
    const uint32_t max_samplers =
        desc->max_samplers != 0 ? desc->max_samplers : DEFAULT_MAX_SAMPLERS;
    const uint32_t max_commands =
        desc->max_commands != 0 ? desc->max_commands : DEFAULT_MAX_COMMANDS;
    const uint32_t max_uniform_bytes =
        desc->max_uniform_bytes != 0 ? desc->max_uniform_bytes : DEFAULT_MAX_UNIFORM_BYTES;
    if (desc->depth_buffer) {
        if (!eng_gpu_enable_depth(gpu)) {
            ENG_LOG_ERROR("renderer : profondeur indisponible");
            *r = (EngRenderer){0};
            return false;
        }
        r->depth_format = eng_gpu_depth_format(gpu);
    }

    const EngArenaMark mark = eng_arena_mark(permanent);
    r->queue.commands = ENG_ARENA_NEW_ARRAY(permanent, EngDrawCmd, max_commands);
    r->queue.capacity = max_commands;
    r->queue.uniforms = ENG_ARENA_NEW_ARRAY(permanent, uint8_t, max_uniform_bytes);
    r->queue.uniform_capacity = max_uniform_bytes;
    if (desc->max_shadow_commands != 0) {
        const uint32_t shadow_bytes = desc->max_shadow_uniform_bytes != 0
                                          ? desc->max_shadow_uniform_bytes
                                          : DEFAULT_MAX_SHADOW_UNIFORM_BYTES;
        r->shadow_queue.commands =
            ENG_ARENA_NEW_ARRAY(permanent, EngDrawCmd, desc->max_shadow_commands);
        r->shadow_queue.capacity = desc->max_shadow_commands;
        r->shadow_queue.uniforms = ENG_ARENA_NEW_ARRAY(permanent, uint8_t, shadow_bytes);
        r->shadow_queue.uniform_capacity = shadow_bytes;
    }
    if (r->queue.commands == NULL || r->queue.uniforms == NULL ||
        (desc->max_shadow_commands != 0 &&
         (r->shadow_queue.commands == NULL || r->shadow_queue.uniforms == NULL)) ||
        !eng_pool_init(&r->buffers, permanent, sizeof(Buffer), max_buffers) ||
        !eng_pool_init(&r->pipelines, permanent, sizeof(Pipeline), max_pipelines) ||
        !eng_pool_init(&r->textures, permanent, sizeof(Texture), max_textures) ||
        !eng_pool_init(&r->samplers, permanent, sizeof(Sampler), max_samplers)) {
        eng_arena_reset_to(permanent, mark);
        *r = (EngRenderer){0};
        return false;
    }
    r->ready = true;
    r->scene_format = eng_gpu_swapchain_format(gpu);
    eng_tonemap_default(&r->tonemap);
    if (desc->hdr) {
        setup_hdr(r, desc->resolve_format);
    }
    return true;
}

uint32_t eng_renderer_shutdown(EngRenderer *r)
{
    if (!r->ready) {
        return 0;
    }
    if (r->hdr) { /* nos propres ressources HDR ne sont pas des fuites */
        eng_gpu_disable_hdr(r->gpu);
        (void)eng_renderer_destroy_pipeline(r, r->tonemap_pipeline);
        SDL_ReleaseGPUSampler(r->gpu->device, r->hdr_sampler);
    }
    uint32_t leaked = 0;
    EngHandle handle;
    uint32_t cursor = 0;
    while (eng_pool_next(&r->pipelines, &cursor, &handle)) {
        leaked++;
        (void)eng_renderer_destroy_pipeline(r, (EngPipelineHandle){handle});
    }
    cursor = 0;
    while (eng_pool_next(&r->buffers, &cursor, &handle)) {
        leaked++;
        (void)eng_renderer_destroy_buffer(r, (EngBufferHandle){handle});
    }
    cursor = 0;
    while (eng_pool_next(&r->textures, &cursor, &handle)) {
        leaked++;
        (void)eng_renderer_destroy_texture(r, (EngTextureHandle){handle});
    }
    cursor = 0;
    while (eng_pool_next(&r->samplers, &cursor, &handle)) {
        leaked++;
        (void)eng_renderer_destroy_sampler(r, (EngSamplerHandle){handle});
    }
    if (leaked != 0) {
        ENG_LOG_WARN("renderer : %u ressource(s) non détruite(s) libérée(s) à l'arrêt", leaked);
    }
    /* Les pools vivent dans l'arène permanente : elle les libère avec le reste. */
    *r = (EngRenderer){0};
    return leaked;
}

/* ── Buffers ── */

EngBufferHandle eng_renderer_create_buffer(EngRenderer *r, const EngBufferDesc *desc)
{
    if (!r->ready || desc == NULL || desc->size == 0 || desc->data == NULL ||
        (desc->usage != ENG_BUFFER_VERTEX && desc->usage != ENG_BUFFER_INDEX)) {
        ENG_LOG_ERROR("renderer : description de buffer invalide");
        return ENG_BUFFER_NULL;
    }
    SDL_GPUDevice *device = r->gpu->device;
    const SDL_GPUBufferCreateInfo buffer_info = {
        .usage = desc->usage == ENG_BUFFER_VERTEX ? SDL_GPU_BUFFERUSAGE_VERTEX
                                                  : SDL_GPU_BUFFERUSAGE_INDEX,
        .size = desc->size,
    };
    SDL_GPUBuffer *gpu_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    if (gpu_buffer == NULL) {
        ENG_LOG_ERROR("renderer : buffer GPU impossible : %s", SDL_GetError());
        return ENG_BUFFER_NULL;
    }
    eng_gpu_debug_name_buffer(device, gpu_buffer,
                              desc->name != NULL                 ? desc->name
                              : desc->usage == ENG_BUFFER_VERTEX ? "Vertex buffer"
                                                                 : "Index buffer");

    /* Envoi : tampon de transfert (visible du CPU) -> copie GPU. SDL attend la fin de la copie
     * avant de libérer le tampon de transfert. */
    bool ok = false;
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = desc->size};
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    void *mapped = transfer != NULL ? SDL_MapGPUTransferBuffer(device, transfer, false) : NULL;
    if (mapped != NULL) {
        memcpy(mapped, desc->data, desc->size);
        SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
        if (commands != NULL) {
            eng_gpu_debug_push_fmt(commands, "Upload buffer %s",
                                   desc->name != NULL ? desc->name : "(unnamed)");
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
            const SDL_GPUTransferBufferLocation source = {.transfer_buffer = transfer, .offset = 0};
            const SDL_GPUBufferRegion destination = {
                .buffer = gpu_buffer, .offset = 0, .size = desc->size};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
            SDL_EndGPUCopyPass(copy);
            eng_gpu_debug_pop(commands);
            ok = SDL_SubmitGPUCommandBuffer(commands);
        }
    }
    if (transfer != NULL) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
    }
    if (!ok) {
        ENG_LOG_ERROR("renderer : envoi du buffer impossible : %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, gpu_buffer);
        return ENG_BUFFER_NULL;
    }

    EngHandle handle;
    Buffer *buffer = (Buffer *)eng_pool_alloc(&r->buffers, &handle);
    if (buffer == NULL) {
        ENG_LOG_ERROR("renderer : trop de buffers (limite %u)", (unsigned)r->buffers.capacity);
        SDL_ReleaseGPUBuffer(device, gpu_buffer);
        return ENG_BUFFER_NULL;
    }
    *buffer = (Buffer){.gpu = gpu_buffer, .size = desc->size, .usage = desc->usage};
    return (EngBufferHandle){handle};
}

EngBufferHandle eng_renderer_create_stream_buffer(EngRenderer *r, EngBufferUsage usage,
                                                  uint32_t capacity)
{
    if (!r->ready || capacity == 0 || (usage != ENG_BUFFER_VERTEX && usage != ENG_BUFFER_INDEX)) {
        ENG_LOG_ERROR("renderer : description de buffer de streaming invalide");
        return ENG_BUFFER_NULL;
    }
    SDL_GPUDevice *device = r->gpu->device;
    const SDL_GPUBufferCreateInfo buffer_info = {
        .usage =
            usage == ENG_BUFFER_VERTEX ? SDL_GPU_BUFFERUSAGE_VERTEX : SDL_GPU_BUFFERUSAGE_INDEX,
        .size = capacity,
    };
    SDL_GPUBuffer *gpu_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    eng_gpu_debug_name_buffer(device, gpu_buffer,
                              usage == ENG_BUFFER_VERTEX ? "Stream vertex buffer"
                                                         : "Stream index buffer");
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = capacity};
    SDL_GPUTransferBuffer *staging =
        gpu_buffer != NULL ? SDL_CreateGPUTransferBuffer(device, &transfer_info) : NULL;
    EngHandle handle;
    Buffer *buffer = staging != NULL ? (Buffer *)eng_pool_alloc(&r->buffers, &handle) : NULL;
    if (buffer == NULL) {
        ENG_LOG_ERROR("renderer : buffer de streaming impossible (%s)",
                      staging == NULL ? SDL_GetError() : "trop de buffers");
        if (staging != NULL) {
            SDL_ReleaseGPUTransferBuffer(device, staging);
        }
        if (gpu_buffer != NULL) {
            SDL_ReleaseGPUBuffer(device, gpu_buffer);
        }
        return ENG_BUFFER_NULL;
    }
    *buffer = (Buffer){.gpu = gpu_buffer, .size = capacity, .usage = usage, .staging = staging};
    return (EngBufferHandle){handle};
}

bool eng_renderer_stream_write(EngRenderer *r, EngBufferHandle handle, uint32_t offset,
                               const void *data, uint32_t size)
{
    Buffer *buffer = r->ready ? (Buffer *)eng_pool_get(&r->buffers, handle.handle) : NULL;
    if (buffer == NULL || buffer->staging == NULL || data == NULL || offset > buffer->size ||
        size > buffer->size - offset) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    /* Première écriture depuis le dernier envoi : on prend un tampon de transfert libre (cycle),
     * le GPU peut encore lire l'ancien. Les suivantes complètent le même tampon. */
    const bool first = buffer->pending_end == 0;
    uint8_t *mapped = (uint8_t *)SDL_MapGPUTransferBuffer(r->gpu->device, buffer->staging, first);
    if (mapped == NULL) {
        return false;
    }
    memcpy(mapped + offset, data, size);
    SDL_UnmapGPUTransferBuffer(r->gpu->device, buffer->staging);
    if (first) {
        buffer->pending_begin = offset;
        buffer->pending_end = offset + size;
        r->streams_pending++;
    } else {
        buffer->pending_begin = buffer->pending_begin < offset ? buffer->pending_begin : offset;
        buffer->pending_end =
            buffer->pending_end > offset + size ? buffer->pending_end : offset + size;
    }
    return true;
}

uint32_t eng_renderer_flush_streams(EngRenderer *r, SDL_GPUCommandBuffer *commands)
{
    if (!r->ready || commands == NULL || r->streams_pending == 0) {
        return 0;
    }
    uint32_t flushed = 0;
    eng_gpu_debug_push(commands, ENG_GPU_PASS_UPLOAD_STREAMS);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == NULL) {
        eng_gpu_debug_pop(commands);
        return 0;
    }
    EngHandle handle;
    uint32_t cursor = 0;
    while (eng_pool_next(&r->buffers, &cursor, &handle)) {
        Buffer *buffer = (Buffer *)eng_pool_get(&r->buffers, handle);
        if (buffer == NULL || buffer->pending_end == 0) {
            continue;
        }
        const SDL_GPUTransferBufferLocation source = {.transfer_buffer = buffer->staging,
                                                      .offset = buffer->pending_begin};
        const SDL_GPUBufferRegion destination = {.buffer = buffer->gpu,
                                                 .offset = buffer->pending_begin,
                                                 .size =
                                                     buffer->pending_end - buffer->pending_begin};
        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        buffer->pending_begin = 0;
        buffer->pending_end = 0;
        flushed++;
    }
    SDL_EndGPUCopyPass(copy);
    eng_gpu_debug_pop(commands);
    r->streams_pending = 0;
    return flushed;
}

bool eng_renderer_destroy_buffer(EngRenderer *r, EngBufferHandle handle)
{
    if (!r->ready) {
        return false;
    }
    Buffer *buffer = (Buffer *)eng_pool_get(&r->buffers, handle.handle);
    if (buffer == NULL) {
        ENG_LOG_WARN("renderer : destruction d'un buffer invalide ou déjà détruit");
        return false;
    }
    if (buffer->staging != NULL) {
        SDL_ReleaseGPUTransferBuffer(r->gpu->device, buffer->staging);
        if (buffer->pending_end != 0 && r->streams_pending > 0) {
            r->streams_pending--;
        }
    }
    SDL_ReleaseGPUBuffer(r->gpu->device, buffer->gpu);
    return eng_pool_free(&r->buffers, handle.handle);
}

bool eng_renderer_buffer_is_valid(const EngRenderer *r, EngBufferHandle handle)
{
    return r->ready && eng_pool_is_valid(&r->buffers, handle.handle);
}

/* ── Textures ── */

static SDL_GPUTextureFormat to_sdl_texture_format(EngTextureFormat format)
{
    switch (format) {
    case ENG_TEXTURE_RGBA8_SRGB:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case ENG_TEXTURE_RGBA16F:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case ENG_TEXTURE_RG16F:
        return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
    case ENG_TEXTURE_NONE:
    case ENG_TEXTURE_RGBA8:
    default:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    }
}

static uint32_t texel_bytes(EngTextureFormat format)
{
    return format == ENG_TEXTURE_RGBA16F ? 8U : 4U;
}

/* Envoie les niveaux 0 à levels-1 de `transfer` (niveaux consécutifs) vers `texture`. */
static bool upload_texture(SDL_GPUDevice *device, SDL_GPUTexture *texture,
                           SDL_GPUTransferBuffer *transfer, uint32_t width, uint32_t height,
                           uint32_t levels, const char *name)
{
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == NULL) {
        return false;
    }
    eng_gpu_debug_push_fmt(commands, "Upload texture %s", name);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    uint32_t offset = 0;
    for (uint32_t level = 0; level < levels; level++) {
        uint32_t w;
        uint32_t h;
        eng_image_mip_size(width, height, level, &w, &h);
        const SDL_GPUTextureTransferInfo source = {.transfer_buffer = transfer,
                                                   .offset = offset,
                                                   .pixels_per_row = w,
                                                   .rows_per_layer = h};
        const SDL_GPUTextureRegion destination = {
            .texture = texture, .mip_level = level, .layer = 0, .w = w, .h = h, .d = 1};
        SDL_UploadToGPUTexture(copy, &source, &destination, false);
        offset += w * h * 4U;
    }
    SDL_EndGPUCopyPass(copy);
    eng_gpu_debug_pop(commands);
    return SDL_SubmitGPUCommandBuffer(commands);
}

EngTextureHandle eng_renderer_create_texture(EngRenderer *r, const EngTextureDesc *desc)
{
    const uint32_t full_chain = desc != NULL ? eng_image_mip_count(desc->width, desc->height) : 0;
    if (!r->ready || desc == NULL || desc->pixels == NULL || desc->width == 0 ||
        desc->height == 0 || desc->width > ENG_IMAGE_MAX_SIZE ||
        desc->height > ENG_IMAGE_MAX_SIZE ||
        (desc->format != ENG_TEXTURE_RGBA8 && desc->format != ENG_TEXTURE_RGBA8_SRGB) ||
        desc->mip_levels > full_chain) {
        ENG_LOG_ERROR("renderer : description de texture invalide (« %s »)",
                      desc != NULL && desc->name != NULL ? desc->name : "?");
        return ENG_TEXTURE_NULL;
    }
    const uint32_t levels = desc->mip_levels != 0 ? desc->mip_levels : full_chain;
    const size_t total = eng_image_mip_chain_size(desc->width, desc->height, levels);
    const size_t base_size = (size_t)desc->width * desc->height * 4U;
    if (total > UINT32_MAX) {
        ENG_LOG_ERROR("renderer : texture « %s » trop grande",
                      desc->name != NULL ? desc->name : "?");
        return ENG_TEXTURE_NULL;
    }
    EngHandle handle;
    Texture *texture = (Texture *)eng_pool_alloc(&r->textures, &handle);
    if (texture == NULL) {
        ENG_LOG_ERROR("renderer : trop de textures (limite %u)", (unsigned)r->textures.capacity);
        return ENG_TEXTURE_NULL;
    }

    SDL_GPUDevice *device = r->gpu->device;
    const SDL_GPUTextureCreateInfo texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = to_sdl_texture_format(desc->format),
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = desc->width,
        .height = desc->height,
        .layer_count_or_depth = 1,
        .num_levels = levels,
    };
    SDL_GPUTexture *gpu_texture = SDL_CreateGPUTexture(device, &texture_info);
    if (gpu_texture == NULL) {
        ENG_LOG_ERROR("renderer : texture GPU impossible : %s", SDL_GetError());
        (void)eng_pool_free(&r->textures, handle);
        return ENG_TEXTURE_NULL;
    }
    eng_gpu_debug_name_texture(device, gpu_texture, desc->name);
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = (Uint32)total};
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    uint8_t *mapped =
        transfer != NULL ? (uint8_t *)SDL_MapGPUTransferBuffer(device, transfer, false) : NULL;
    bool ok = false;
    if (mapped != NULL) {
        memcpy(mapped, desc->pixels, base_size);
        const bool mips_ok = eng_image_generate_mips(
            (const uint8_t *)desc->pixels, desc->width, desc->height, levels,
            desc->format == ENG_TEXTURE_RGBA8_SRGB, mapped + base_size);
        SDL_UnmapGPUTransferBuffer(device, transfer);
        ok = mips_ok && upload_texture(device, gpu_texture, transfer, desc->width, desc->height,
                                       levels, desc->name != NULL ? desc->name : "?");
    }
    if (transfer != NULL) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
    }
    if (!ok) {
        ENG_LOG_ERROR("renderer : envoi de la texture « %s » impossible : %s",
                      desc->name != NULL ? desc->name : "?", SDL_GetError());
        SDL_ReleaseGPUTexture(device, gpu_texture);
        (void)eng_pool_free(&r->textures, handle);
        return ENG_TEXTURE_NULL;
    }
    texture->gpu = gpu_texture;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->mip_levels = levels;
    texture->layers = 1;
    texture->format = desc->format;
    return (EngTextureHandle){handle};
}

/* Envoie `layers` x `levels` régions (couche par couche, niveaux consécutifs) de `transfer`. */
static bool upload_texture_data(SDL_GPUDevice *device, SDL_GPUTexture *texture,
                                SDL_GPUTransferBuffer *transfer, uint32_t width, uint32_t height,
                                uint32_t layers, uint32_t levels, uint32_t texel, const char *name)
{
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == NULL) {
        return false;
    }
    eng_gpu_debug_push_fmt(commands, "Upload texture %s", name);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    uint32_t offset = 0;
    for (uint32_t layer = 0; layer < layers; layer++) {
        for (uint32_t level = 0; level < levels; level++) {
            uint32_t w;
            uint32_t h;
            eng_image_mip_size(width, height, level, &w, &h);
            const SDL_GPUTextureTransferInfo source = {.transfer_buffer = transfer,
                                                       .offset = offset,
                                                       .pixels_per_row = w,
                                                       .rows_per_layer = h};
            const SDL_GPUTextureRegion destination = {
                .texture = texture, .mip_level = level, .layer = layer, .w = w, .h = h, .d = 1};
            SDL_UploadToGPUTexture(copy, &source, &destination, false);
            offset += w * h * texel;
        }
    }
    SDL_EndGPUCopyPass(copy);
    eng_gpu_debug_pop(commands);
    return SDL_SubmitGPUCommandBuffer(commands);
}

EngTextureHandle eng_renderer_create_texture_data(EngRenderer *r, const EngTextureDataDesc *desc)
{
    const char *name = desc != NULL && desc->name != NULL ? desc->name : "?";
    const uint32_t full_chain = desc != NULL ? eng_image_mip_count(desc->width, desc->height) : 0;
    const bool format_ok =
        desc != NULL &&
        (desc->format == ENG_TEXTURE_RGBA8 || desc->format == ENG_TEXTURE_RGBA8_SRGB ||
         desc->format == ENG_TEXTURE_RGBA16F || desc->format == ENG_TEXTURE_RG16F);
    if (!r->ready || desc == NULL || desc->data == NULL || desc->width == 0 || desc->height == 0 ||
        desc->width > ENG_IMAGE_MAX_SIZE || desc->height > ENG_IMAGE_MAX_SIZE || !format_ok ||
        (desc->layers != 1 && desc->layers != 6) ||
        (desc->layers == 6 && desc->width != desc->height) || desc->mip_levels == 0 ||
        desc->mip_levels > full_chain) {
        ENG_LOG_ERROR("renderer : description de texture invalide (« %s »)", name);
        return ENG_TEXTURE_NULL;
    }
    const uint32_t texel = texel_bytes(desc->format);
    const size_t per_layer =
        eng_image_mip_chain_size(desc->width, desc->height, desc->mip_levels) / 4U * texel;
    const size_t total = per_layer * desc->layers;
    if (total > UINT32_MAX) {
        ENG_LOG_ERROR("renderer : texture « %s » trop grande", name);
        return ENG_TEXTURE_NULL;
    }
    EngHandle handle;
    Texture *texture = (Texture *)eng_pool_alloc(&r->textures, &handle);
    if (texture == NULL) {
        ENG_LOG_ERROR("renderer : trop de textures (limite %u)", (unsigned)r->textures.capacity);
        return ENG_TEXTURE_NULL;
    }
    SDL_GPUDevice *device = r->gpu->device;
    const SDL_GPUTextureCreateInfo texture_info = {
        .type = desc->layers == 6 ? SDL_GPU_TEXTURETYPE_CUBE : SDL_GPU_TEXTURETYPE_2D,
        .format = to_sdl_texture_format(desc->format),
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = desc->width,
        .height = desc->height,
        .layer_count_or_depth = desc->layers,
        .num_levels = desc->mip_levels,
    };
    SDL_GPUTexture *gpu_texture = SDL_CreateGPUTexture(device, &texture_info);
    if (gpu_texture == NULL) {
        ENG_LOG_ERROR("renderer : texture GPU impossible : %s", SDL_GetError());
        (void)eng_pool_free(&r->textures, handle);
        return ENG_TEXTURE_NULL;
    }
    eng_gpu_debug_name_texture(device, gpu_texture, name);
    const SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = (Uint32)total};
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    uint8_t *mapped =
        transfer != NULL ? (uint8_t *)SDL_MapGPUTransferBuffer(device, transfer, false) : NULL;
    bool ok = false;
    if (mapped != NULL) {
        memcpy(mapped, desc->data, total);
        SDL_UnmapGPUTransferBuffer(device, transfer);
        ok = upload_texture_data(device, gpu_texture, transfer, desc->width, desc->height,
                                 desc->layers, desc->mip_levels, texel, name);
    }
    if (transfer != NULL) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
    }
    if (!ok) {
        ENG_LOG_ERROR("renderer : envoi de la texture « %s » impossible : %s", name,
                      SDL_GetError());
        SDL_ReleaseGPUTexture(device, gpu_texture);
        (void)eng_pool_free(&r->textures, handle);
        return ENG_TEXTURE_NULL;
    }
    texture->gpu = gpu_texture;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->mip_levels = desc->mip_levels;
    texture->layers = desc->layers;
    texture->format = desc->format;
    return (EngTextureHandle){handle};
}

uint32_t eng_renderer_texture_layers(const EngRenderer *r, EngTextureHandle handle)
{
    const Texture *texture =
        r->ready ? (const Texture *)eng_pool_get(&r->textures, handle.handle) : NULL;
    return texture != NULL ? texture->layers : 0;
}

bool eng_renderer_destroy_texture(EngRenderer *r, EngTextureHandle handle)
{
    if (!r->ready) {
        return false;
    }
    Texture *texture = (Texture *)eng_pool_get(&r->textures, handle.handle);
    if (texture == NULL) {
        ENG_LOG_WARN("renderer : destruction d'une texture invalide ou déjà détruite");
        return false;
    }
    SDL_ReleaseGPUTexture(r->gpu->device, texture->gpu);
    return eng_pool_free(&r->textures, handle.handle);
}

bool eng_renderer_texture_is_valid(const EngRenderer *r, EngTextureHandle handle)
{
    return r->ready && eng_pool_is_valid(&r->textures, handle.handle);
}

EngTextureHandle eng_renderer_create_shadow_map(EngRenderer *r, uint32_t size)
{
    if (!r->ready || size < 8U || size > MAX_SHADOW_MAP_SIZE) {
        ENG_LOG_ERROR("renderer : carte d'ombre de %u texels refusée (8 à %u)", (unsigned)size,
                      (unsigned)MAX_SHADOW_MAP_SIZE);
        return ENG_TEXTURE_NULL;
    }
    const SDL_GPUTextureUsageFlags usage =
        SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if (!SDL_GPUTextureSupportsFormat(r->gpu->device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                      SDL_GPU_TEXTURETYPE_2D, usage)) {
        ENG_LOG_ERROR("renderer : le GPU ne gère pas D32F comme cible et comme texture lue");
        return ENG_TEXTURE_NULL;
    }
    EngHandle handle;
    Texture *texture = (Texture *)eng_pool_alloc(&r->textures, &handle);
    if (texture == NULL) {
        ENG_LOG_ERROR("renderer : trop de textures (limite %u)", (unsigned)r->textures.capacity);
        return ENG_TEXTURE_NULL;
    }
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.usage = usage;
    info.width = size;
    info.height = size;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    texture->gpu = SDL_CreateGPUTexture(r->gpu->device, &info);
    if (texture->gpu == NULL) {
        ENG_LOG_ERROR("renderer : carte d'ombre impossible : %s", SDL_GetError());
        (void)eng_pool_free(&r->textures, handle);
        return ENG_TEXTURE_NULL;
    }
    eng_gpu_debug_name_texture(r->gpu->device, texture->gpu, "Shadow map depth");
    texture->width = size;
    texture->height = size;
    texture->mip_levels = 1;
    texture->layers = 1;
    texture->format = ENG_TEXTURE_DEPTH32F;
    return (EngTextureHandle){handle};
}

uint32_t eng_renderer_texture_mip_levels(const EngRenderer *r, EngTextureHandle handle)
{
    const Texture *texture =
        r->ready ? (const Texture *)eng_pool_get(&r->textures, handle.handle) : NULL;
    return texture != NULL ? texture->mip_levels : 0;
}

/* ── Samplers ── */

static SDL_GPUFilter to_sdl_filter(EngFilter filter)
{
    return filter == ENG_FILTER_NEAREST ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
}

static SDL_GPUSamplerAddressMode to_sdl_address(EngAddressMode mode)
{
    return mode == ENG_ADDRESS_CLAMP    ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
           : mode == ENG_ADDRESS_MIRROR ? SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT
                                        : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
}

EngSamplerHandle eng_renderer_create_sampler(EngRenderer *r, const EngSamplerDesc *desc)
{
    if (!r->ready || desc == NULL || !(desc->max_anisotropy >= 0.0F) ||
        (unsigned)desc->min_filter > ENG_FILTER_NEAREST ||
        (unsigned)desc->mag_filter > ENG_FILTER_NEAREST ||
        (unsigned)desc->mip_filter > ENG_MIP_OFF ||
        (unsigned)desc->address_u > ENG_ADDRESS_MIRROR ||
        (unsigned)desc->address_v > ENG_ADDRESS_MIRROR) {
        ENG_LOG_ERROR("renderer : description de sampler invalide");
        return ENG_SAMPLER_NULL;
    }
    const float anisotropy = eng_clamp_anisotropy(desc->max_anisotropy);
    SDL_GPUSamplerCreateInfo info;
    SDL_zero(info);
    info.min_filter = to_sdl_filter(desc->min_filter);
    info.mag_filter = to_sdl_filter(desc->mag_filter);
    info.mipmap_mode = desc->mip_filter == ENG_MIP_NEAREST ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                                                           : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    info.address_mode_u = to_sdl_address(desc->address_u);
    info.address_mode_v = to_sdl_address(desc->address_v);
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.enable_anisotropy = anisotropy > 1.0F;
    info.max_anisotropy = anisotropy > 1.0F ? anisotropy : 1.0F;
    info.min_lod = 0.0F;
    /* LOD maximal : sans cela (0) seul le niveau de base serait lu. Une valeur très grande laisse
     * le GPU choisir jusqu'au dernier niveau. */
    info.max_lod = desc->mip_filter == ENG_MIP_OFF ? 0.0F : 1000.0F;

    EngHandle handle;
    Sampler *sampler = (Sampler *)eng_pool_alloc(&r->samplers, &handle);
    if (sampler == NULL) {
        ENG_LOG_ERROR("renderer : trop de samplers (limite %u)", (unsigned)r->samplers.capacity);
        return ENG_SAMPLER_NULL;
    }
    const uint32_t sampler_props =
        eng_gpu_debug_props("sampler", SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, desc->name);
    info.props = sampler_props;
    sampler->gpu = SDL_CreateGPUSampler(r->gpu->device, &info);
    eng_gpu_debug_props_free(sampler_props);
    if (sampler->gpu == NULL) {
        ENG_LOG_ERROR("renderer : sampler GPU impossible : %s", SDL_GetError());
        (void)eng_pool_free(&r->samplers, handle);
        return ENG_SAMPLER_NULL;
    }
    sampler->anisotropy = anisotropy > 1.0F ? anisotropy : 0.0F;
    return (EngSamplerHandle){handle};
}

bool eng_renderer_destroy_sampler(EngRenderer *r, EngSamplerHandle handle)
{
    if (!r->ready) {
        return false;
    }
    Sampler *sampler = (Sampler *)eng_pool_get(&r->samplers, handle.handle);
    if (sampler == NULL) {
        ENG_LOG_WARN("renderer : destruction d'un sampler invalide ou déjà détruit");
        return false;
    }
    SDL_ReleaseGPUSampler(r->gpu->device, sampler->gpu);
    return eng_pool_free(&r->samplers, handle.handle);
}

bool eng_renderer_sampler_is_valid(const EngRenderer *r, EngSamplerHandle handle)
{
    return r->ready && eng_pool_is_valid(&r->samplers, handle.handle);
}

float eng_renderer_sampler_anisotropy(const EngRenderer *r, EngSamplerHandle handle)
{
    const Sampler *sampler =
        r->ready ? (const Sampler *)eng_pool_get(&r->samplers, handle.handle) : NULL;
    return sampler != NULL ? sampler->anisotropy : 0.0F;
}

/* ── Pipelines ── */

/* Profondeur que le pipeline déclare : celle du renderer, sauf pour une passe sans profondeur. */
static uint32_t pipeline_depth_format(const EngRenderer *r, const Pipeline *p)
{
    if (p->depth_only) {
        return (uint32_t)SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    return p->no_depth_target ? 0 : r->depth_format;
}

static SDL_GPUGraphicsPipeline *build_pipeline(EngRenderer *r, const Pipeline *p,
                                               SDL_GPUShader *vertex, SDL_GPUShader *fragment)
{
    SDL_GPUVertexAttribute attributes[ENG_MAX_VERTEX_ATTRIBUTES];
    for (uint32_t i = 0; i < p->attribute_count; i++) {
        SDL_GPUVertexElementFormat format;
        if (!to_sdl_vertex_format(p->attributes[i].format, &format)) {
            ENG_LOG_ERROR("renderer : pipeline « %s » : format de sommet %d inconnu", p->name,
                          (int)p->attributes[i].format);
            return NULL;
        }
        attributes[i] = (SDL_GPUVertexAttribute){.location = p->attributes[i].location,
                                                 .buffer_slot = 0,
                                                 .format = format,
                                                 .offset = p->attributes[i].offset};
    }
    const SDL_GPUVertexBufferDescription buffer_description = {
        .slot = 0, .pitch = p->vertex_stride, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX};
    const SDL_GPUColorTargetDescription target = {.format = (SDL_GPUTextureFormat)p->color_format};

    SDL_GPUGraphicsPipelineCreateInfo info;
    SDL_zero(info);
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    if (p->attribute_count != 0) {
        info.vertex_input_state.vertex_buffer_descriptions = &buffer_description;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes;
        info.vertex_input_state.num_vertex_attributes = p->attribute_count;
    }
    info.primitive_type = p->topology == ENG_TOPOLOGY_LINES ? SDL_GPU_PRIMITIVETYPE_LINELIST
                                                            : SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    /* true = découpage (clip) aux plans proche et lointain ; false = écrêtage (clamp), qui
     * dessinerait ce qui est derrière la caméra ou au-delà de la distance maximale. */
    info.rasterizer_state.enable_depth_clip = true;
    info.rasterizer_state.cull_mode = p->cull_mode == ENG_CULL_NONE    ? SDL_GPU_CULLMODE_NONE
                                      : p->cull_mode == ENG_CULL_FRONT ? SDL_GPU_CULLMODE_FRONT
                                                                       : SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = p->front_face == ENG_FRONT_CLOCKWISE
                                           ? SDL_GPU_FRONTFACE_CLOCKWISE
                                           : SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.target_info.color_target_descriptions = p->depth_only ? NULL : &target;
    info.target_info.num_color_targets = p->depth_only ? 0 : 1;
    if (pipeline_depth_format(r, p) != 0) {
        info.target_info.has_depth_stencil_target = true;
        info.target_info.depth_stencil_format = (SDL_GPUTextureFormat)pipeline_depth_format(r, p);
        info.depth_stencil_state.enable_depth_test = p->depth_test;
        info.depth_stencil_state.enable_depth_write = p->depth_write;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    }

    const uint32_t pipeline_props =
        eng_gpu_debug_props("pipeline", SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING, p->name);
    info.props = pipeline_props;
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(r->gpu->device, &info);
    eng_gpu_debug_props_free(pipeline_props);
    if (pipeline == NULL) {
        ENG_LOG_ERROR("renderer : pipeline « %s » impossible : %s", p->name, SDL_GetError());
    }
    return pipeline;
}

/* Chemin « dir/<shader>.hlsl » ; false s'il ne tient pas. */
static bool source_path(const EngRenderer *r, const char *shader, char *out, size_t size)
{
    return SDL_snprintf(out, size, "%s/%s.hlsl", r->source_dir, shader) < (int)size;
}

static bool init_shader(EngRenderer *r, EngShaderHot *hot, const char *shader, EngShaderStage stage)
{
    char path[512];
    const char *source = NULL;
    if (r->has_source_dir) {
        if (!source_path(r, shader, path, sizeof path)) {
            ENG_LOG_ERROR("renderer : chemin du shader « %s » trop long", shader);
            return false;
        }
        source = path;
    }
    return eng_shader_hot_init(hot, r->gpu, r->scratch, source,
                               r->has_offline_dir ? r->offline_dir : NULL, shader, stage);
}

EngPipelineHandle eng_renderer_create_pipeline(EngRenderer *r, const EngPipelineDesc *desc)
{
    if (!r->ready || desc == NULL || desc->vertex_shader == NULL || desc->fragment_shader == NULL ||
        desc->attribute_count > ENG_MAX_VERTEX_ATTRIBUTES ||
        (desc->attribute_count != 0 && desc->vertex_stride == 0) ||
        (desc->depth_write && !desc->depth_test) ||
        (desc->depth_only && (!desc->depth_test || !desc->depth_write || desc->no_depth_target)) ||
        ((desc->depth_test || desc->depth_write) && !desc->depth_only &&
         (r->depth_format == 0 || desc->no_depth_target))) {
        ENG_LOG_ERROR("renderer : description de pipeline invalide (profondeur demandée sans depth "
                      "buffer ou avec no_depth_target, ou écriture sans test ?)");
        return ENG_PIPELINE_NULL;
    }
    EngHandle handle;
    Pipeline *p = (Pipeline *)eng_pool_alloc(&r->pipelines, &handle);
    if (p == NULL) {
        ENG_LOG_ERROR("renderer : trop de pipelines (limite %u)", (unsigned)r->pipelines.capacity);
        return ENG_PIPELINE_NULL;
    }
    SDL_strlcpy(p->name, desc->name != NULL ? desc->name : desc->vertex_shader, sizeof p->name);
    p->vertex_stride = desc->vertex_stride;
    memcpy(p->attributes, desc->attributes, sizeof p->attributes);
    p->attribute_count = desc->attribute_count;
    p->topology = desc->topology;
    p->cull_mode = desc->cull_mode;
    p->front_face = desc->front_face;
    p->depth_test = desc->depth_test;
    p->depth_write = desc->depth_write;
    p->no_depth_target = desc->no_depth_target;
    p->depth_only = desc->depth_only;
    p->color_format = desc->color_format != 0 ? desc->color_format : r->scene_format;

    bool vertex_ok = false;
    bool fragment_ok = false;
    if (init_shader(r, &p->vertex, desc->vertex_shader, ENG_SHADER_VERTEX)) {
        vertex_ok = true;
        fragment_ok = init_shader(r, &p->fragment, desc->fragment_shader, ENG_SHADER_FRAGMENT);
    }
    if (fragment_ok) {
        p->gpu = build_pipeline(r, p, p->vertex.shader, p->fragment.shader);
    }
    if (p->gpu == NULL) {
        if (vertex_ok) {
            eng_shader_hot_release(&p->vertex, r->gpu);
        }
        if (fragment_ok) {
            eng_shader_hot_release(&p->fragment, r->gpu);
        }
        (void)eng_pool_free(&r->pipelines, handle);
        return ENG_PIPELINE_NULL;
    }
    p->built_vertex_gen = p->vertex.generation;
    p->built_fragment_gen = p->fragment.generation;
    return (EngPipelineHandle){handle};
}

bool eng_renderer_destroy_pipeline(EngRenderer *r, EngPipelineHandle handle)
{
    if (!r->ready) {
        return false;
    }
    Pipeline *p = (Pipeline *)eng_pool_get(&r->pipelines, handle.handle);
    if (p == NULL) {
        ENG_LOG_WARN("renderer : destruction d'un pipeline invalide ou déjà détruit");
        return false;
    }
    SDL_ReleaseGPUGraphicsPipeline(r->gpu->device, p->gpu);
    eng_shader_hot_release(&p->vertex, r->gpu);
    eng_shader_hot_release(&p->fragment, r->gpu);
    return eng_pool_free(&r->pipelines, handle.handle);
}

bool eng_renderer_pipeline_is_valid(const EngRenderer *r, EngPipelineHandle handle)
{
    return r->ready && eng_pool_is_valid(&r->pipelines, handle.handle);
}

uint32_t eng_renderer_pipeline_color_format(const EngRenderer *r, EngPipelineHandle handle)
{
    const Pipeline *p =
        r->ready ? (const Pipeline *)eng_pool_get(&r->pipelines, handle.handle) : NULL;
    return p != NULL ? p->color_format : 0;
}

uint32_t eng_renderer_pipeline_depth_format(const EngRenderer *r, EngPipelineHandle handle)
{
    const Pipeline *p =
        r->ready ? (const Pipeline *)eng_pool_get(&r->pipelines, handle.handle) : NULL;
    return p != NULL ? pipeline_depth_format(r, p) : 0;
}

uint32_t eng_renderer_update(EngRenderer *r, double now_seconds)
{
    if (!r->ready) {
        return 0;
    }
    uint32_t rebuilt = 0;
    EngHandle handle;
    uint32_t cursor = 0;
    while (eng_pool_next(&r->pipelines, &cursor, &handle)) {
        Pipeline *p = (Pipeline *)eng_pool_get(&r->pipelines, handle);
        (void)eng_shader_hot_poll(&p->vertex, r->gpu, r->scratch, now_seconds);
        (void)eng_shader_hot_poll(&p->fragment, r->gpu, r->scratch, now_seconds);
        if (p->vertex.generation == p->built_vertex_gen &&
            p->fragment.generation == p->built_fragment_gen) {
            continue;
        }
        /* Un shader a été rechargé : nouveau pipeline AVANT de libérer l'ancien. Les versions
         * sont notées même en cas d'échec : pas de nouvel essai à chaque image pour le même
         * shader. */
        SDL_GPUGraphicsPipeline *fresh = build_pipeline(r, p, p->vertex.shader, p->fragment.shader);
        p->built_vertex_gen = p->vertex.generation;
        p->built_fragment_gen = p->fragment.generation;
        if (fresh == NULL) {
            ENG_LOG_WARN("renderer : pipeline « %s » : l'ancienne version reste en service",
                         p->name);
            continue;
        }
        SDL_ReleaseGPUGraphicsPipeline(r->gpu->device, p->gpu);
        p->gpu = fresh;
        r->stats.pipeline_rebuilds++;
        rebuilt++;
    }
    return rebuilt;
}

uint32_t eng_renderer_depth_format(const EngRenderer *r)
{
    return r->depth_format;
}

/* ── HDR (M1-14) ── */

bool eng_renderer_hdr_enabled(const EngRenderer *r)
{
    return r->ready && r->hdr;
}

uint32_t eng_renderer_scene_format(const EngRenderer *r)
{
    return r->ready ? r->scene_format : 0;
}

void eng_renderer_set_tonemap(EngRenderer *r, const EngTonemap *t)
{
    if (!r->ready || t == NULL) {
        return;
    }
    r->tonemap = *t;
    (void)eng_tonemap_sanitize(&r->tonemap);
}

EngTonemap eng_renderer_tonemap(const EngRenderer *r)
{
    EngTonemap t;
    eng_tonemap_default(&t);
    if (r->ready) {
        t = r->tonemap;
    }
    return t;
}

bool eng_renderer_resolve_hdr(EngRenderer *r, SDL_GPUCommandBuffer *commands,
                              SDL_GPURenderPass *pass, SDL_GPUTexture *hdr)
{
    if (!r->ready || !r->hdr || commands == NULL || pass == NULL || hdr == NULL) {
        return false;
    }
    const Pipeline *pipeline =
        (const Pipeline *)eng_pool_get(&r->pipelines, r->tonemap_pipeline.handle);
    if (pipeline == NULL) {
        return false;
    }
    SDL_BindGPUGraphicsPipeline(pass, pipeline->gpu);
    const SDL_GPUTextureSamplerBinding binding = {.texture = hdr, .sampler = r->hdr_sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    const float params[4] = {eng_tonemap_exposure_scale(&r->tonemap), (float)r->tonemap.op,
                             format_is_srgb(r->resolve_format) ? 0.0F : 1.0F, 0.0F};
    SDL_PushGPUFragmentUniformData(commands, 0, params, sizeof params);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0); /* un triangle plein écran, sans buffer */
    r->stats.resolve_draws++;
    return true;
}

/* ── Image ── */

void eng_renderer_set_clear_color(EngRenderer *r, float red, float green, float blue, float alpha)
{
    eng_gpu_set_clear_color(r->gpu, red, green, blue, alpha);
}

EngRenderQueue *eng_renderer_begin_frame(EngRenderer *r)
{
    if (!r->ready) {
        return NULL;
    }
    r->queue.count = 0;
    r->queue.dropped = 0;
    r->queue.uniform_used = 0;
    return &r->queue;
}

uint32_t eng_renderer_execute(EngRenderer *r, const EngRenderQueue *queue,
                              SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass)
{
    if (!r->ready || pass == NULL) {
        return 0;
    }
    uint32_t drawn = 0;
    const Pipeline *bound_pipeline = NULL;
    const Buffer *bound_vertex = NULL;
    const Buffer *bound_index = NULL;
    EngIndexType bound_index_type = ENG_INDEX_U16;
    const Texture *bound_textures[ENG_MAX_DRAW_TEXTURES] = {NULL};
    uint32_t bound_texture_count = 0;
    const Sampler *bound_sampler = NULL;
    const Texture *bound_shadow = NULL;
    const Sampler *bound_shadow_sampler = NULL;

    for (uint32_t i = 0; i < queue->count; i++) {
        const EngDrawCmd *cmd = &queue->commands[i];
        const Pipeline *pipeline =
            (const Pipeline *)eng_pool_get(&r->pipelines, cmd->pipeline.handle);
        const Buffer *vertex = (const Buffer *)eng_pool_get(&r->buffers, cmd->vertex_buffer.handle);
        const bool indexed = cmd->index_buffer.handle.generation != 0;
        const Buffer *index =
            indexed ? (const Buffer *)eng_pool_get(&r->buffers, cmd->index_buffer.handle) : NULL;
        const bool has_uniforms = cmd->uniform_size != 0;
        const bool uniforms_ok =
            !has_uniforms || (commands != NULL && cmd->uniform_size <= ENG_MAX_UNIFORM_BYTES &&
                              cmd->uniform_offset <= queue->uniform_used &&
                              cmd->uniform_size <= queue->uniform_used - cmd->uniform_offset);
        const bool has_fragment_uniforms = cmd->fragment_uniform_size != 0;
        const bool fragment_uniforms_ok =
            !has_fragment_uniforms ||
            (commands != NULL && cmd->fragment_uniform_size <= ENG_MAX_UNIFORM_BYTES &&
             cmd->fragment_uniform_offset <= queue->uniform_used &&
             cmd->fragment_uniform_size <= queue->uniform_used - cmd->fragment_uniform_offset);
        const bool has_texture = cmd->texture.handle.generation != 0;
        const bool has_sampler = cmd->sampler.handle.generation != 0;
        const Sampler *sampler =
            has_sampler ? (const Sampler *)eng_pool_get(&r->samplers, cmd->sampler.handle) : NULL;
        /* textures liées : l'emplacement 0, puis les supplémentaires, sans trou */
        const Texture *textures[ENG_MAX_DRAW_TEXTURES] = {NULL};
        uint32_t texture_count = 0;
        bool textures_ok = has_texture == has_sampler && (!has_sampler || sampler != NULL);
        if (has_texture) {
            textures[0] = (const Texture *)eng_pool_get(&r->textures, cmd->texture.handle);
            texture_count = 1;
            textures_ok = textures_ok && textures[0] != NULL;
        }
        bool gap = !has_texture; /* un emplacement vide : les suivants sont interdits */
        for (uint32_t t = 1; t < ENG_MAX_DRAW_TEXTURES; t++) {
            const EngTextureHandle extra = cmd->extra_textures[t - 1];
            if (extra.handle.generation == 0) {
                gap = true;
                continue;
            }
            textures[t] = (const Texture *)eng_pool_get(&r->textures, extra.handle);
            textures_ok = textures_ok && !gap && textures[t] != NULL;
            texture_count = t + 1;
        }
        /* carte d'ombre : les deux ou aucun, et les huit emplacements précédents occupés */
        const bool has_shadow = cmd->shadow_map.handle.generation != 0;
        const bool has_shadow_sampler = cmd->shadow_sampler.handle.generation != 0;
        const Texture *shadow =
            has_shadow ? (const Texture *)eng_pool_get(&r->textures, cmd->shadow_map.handle) : NULL;
        const Sampler *shadow_sampler =
            has_shadow_sampler
                ? (const Sampler *)eng_pool_get(&r->samplers, cmd->shadow_sampler.handle)
                : NULL;
        const bool shadow_ok = has_shadow == has_shadow_sampler &&
                               (!has_shadow || (shadow != NULL && shadow_sampler != NULL &&
                                                texture_count == ENG_SHADOW_SLOT));
        if (pipeline == NULL || vertex == NULL || (indexed && index == NULL) || cmd->count == 0 ||
            !uniforms_ok || !fragment_uniforms_ok || !textures_ok || !shadow_ok) {
            r->stats.rejected_commands++;
            if (!r->warned_rejected) {
                r->warned_rejected = true;
                ENG_LOG_WARN("renderer : dessin ignoré (handle invalide ou périmé, 0 sommet, ou "
                             "uniforms ou texture incohérents) ; les suivants ne sont plus "
                             "signalés, voir les "
                             "statistiques");
            }
            continue;
        }
        if (pipeline != bound_pipeline) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline->gpu);
            bound_pipeline = pipeline;
            bound_vertex = NULL; /* par prudence, on relie les buffers après un changement */
            bound_index = NULL;
            bound_texture_count = 0;
            bound_sampler = NULL;
            bound_shadow = NULL;
            bound_shadow_sampler = NULL;
            r->stats.pipeline_binds++;
        }
        if (vertex != bound_vertex) {
            const SDL_GPUBufferBinding binding = {.buffer = vertex->gpu, .offset = 0};
            SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
            bound_vertex = vertex;
        }
        if (texture_count != 0) {
            bool same = sampler == bound_sampler && texture_count == bound_texture_count;
            for (uint32_t t = 0; same && t < texture_count; t++) {
                same = textures[t] == bound_textures[t];
            }
            if (!same) {
                SDL_GPUTextureSamplerBinding bindings[ENG_MAX_DRAW_TEXTURES];
                for (uint32_t t = 0; t < texture_count; t++) {
                    bindings[t] = (SDL_GPUTextureSamplerBinding){.texture = textures[t]->gpu,
                                                                 .sampler = sampler->gpu};
                    bound_textures[t] = textures[t];
                }
                SDL_BindGPUFragmentSamplers(pass, 0, bindings, texture_count);
                bound_texture_count = texture_count;
                bound_sampler = sampler;
                r->stats.sampler_binds++;
            }
        }
        if (has_shadow && (shadow != bound_shadow || shadow_sampler != bound_shadow_sampler)) {
            const SDL_GPUTextureSamplerBinding binding = {.texture = shadow->gpu,
                                                          .sampler = shadow_sampler->gpu};
            SDL_BindGPUFragmentSamplers(pass, ENG_SHADOW_SLOT, &binding, 1);
            bound_shadow = shadow;
            bound_shadow_sampler = shadow_sampler;
            r->stats.sampler_binds++;
        }
        if (has_uniforms) {
            SDL_PushGPUVertexUniformData(commands, 0, queue->uniforms + cmd->uniform_offset,
                                         cmd->uniform_size);
            r->stats.uniform_pushes++;
        }
        if (has_fragment_uniforms) {
            SDL_PushGPUFragmentUniformData(commands, 0,
                                           queue->uniforms + cmd->fragment_uniform_offset,
                                           cmd->fragment_uniform_size);
            r->stats.uniform_pushes++;
        }
        const Uint32 instances = cmd->instance_count != 0 ? cmd->instance_count : 1;
        if (indexed) {
            if (index != bound_index || cmd->index_type != bound_index_type) {
                const SDL_GPUBufferBinding binding = {.buffer = index->gpu, .offset = 0};
                SDL_BindGPUIndexBuffer(pass, &binding,
                                       cmd->index_type == ENG_INDEX_U32
                                           ? SDL_GPU_INDEXELEMENTSIZE_32BIT
                                           : SDL_GPU_INDEXELEMENTSIZE_16BIT);
                bound_index = index;
                bound_index_type = cmd->index_type;
            }
            SDL_DrawGPUIndexedPrimitives(pass, cmd->count, instances, cmd->first,
                                         cmd->vertex_offset, 0);
        } else {
            SDL_DrawGPUPrimitives(pass, cmd->count, instances, cmd->first, 0);
        }
        drawn++;
    }
    r->stats.draws += drawn;
    r->stats.dropped_commands += queue->dropped;
    return drawn;
}

uint32_t eng_renderer_run_shadow_pass(EngRenderer *r, SDL_GPUCommandBuffer *commands,
                                      EngTextureHandle map, const EngRenderQueue *queue)
{
    const Texture *texture = r->ready && commands != NULL
                                 ? (const Texture *)eng_pool_get(&r->textures, map.handle)
                                 : NULL;
    if (texture == NULL || texture->format != ENG_TEXTURE_DEPTH32F || queue == NULL) {
        return 0;
    }
    SDL_GPUDepthStencilTargetInfo depth;
    SDL_zero(depth);
    depth.texture = texture->gpu;
    depth.clear_depth = 1.0F;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    eng_gpu_debug_push(commands, ENG_GPU_PASS_SHADOW);
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, NULL, 0, &depth);
    if (pass == NULL) {
        ENG_LOG_ERROR("renderer : passe d'ombre impossible : %s", SDL_GetError());
        eng_gpu_debug_pop(commands);
        return 0;
    }
    const uint32_t drawn = eng_renderer_execute(r, queue, commands, pass);
    SDL_EndGPURenderPass(pass);
    eng_gpu_debug_pop(commands);
    r->stats.shadow_passes++;
    return drawn;
}

EngRenderQueue *eng_renderer_begin_shadow_queue(EngRenderer *r)
{
    if (!r->ready || r->shadow_queue.capacity == 0) {
        return NULL;
    }
    r->shadow_queue.count = 0;
    r->shadow_queue.dropped = 0;
    r->shadow_queue.uniform_used = 0;
    return &r->shadow_queue;
}

void eng_renderer_set_shadow_map(EngRenderer *r, EngTextureHandle map)
{
    r->shadow_map = map;
}

void eng_renderer_set_overlay(EngRenderer *r, EngOverlayPrepareFn prepare, EngOverlayDrawFn draw,
                              void *user)
{
    r->overlay_prepare = prepare;
    r->overlay_draw = draw;
    r->overlay_user = user;
}

void eng_renderer_end_frame(EngRenderer *r)
{
    if (!r->ready) {
        return;
    }
    if (eng_gpu_begin_frame(r->gpu)) {
        SDL_GPUCommandBuffer *commands = r->gpu->command_buffer;
        (void)eng_renderer_flush_streams(r, commands); /* avant la passe de rendu */
        if (r->shadow_queue.capacity != 0) {
            (void)eng_renderer_run_shadow_pass(r, commands, r->shadow_map, &r->shadow_queue);
        }
        bool overlay_wanted = r->overlay_draw != NULL;
        if (overlay_wanted && r->overlay_prepare != NULL) {
            eng_gpu_debug_push(commands, ENG_GPU_PASS_OVERLAY_UPLOAD);
            overlay_wanted = r->overlay_prepare(r->overlay_user, commands); /* passe de copie */
            eng_gpu_debug_pop(commands);
        }
        (void)eng_renderer_execute(r, &r->queue, commands, eng_gpu_begin_render_pass(r->gpu));
        if (overlay_wanted) {
            SDL_GPURenderPass *overlay = eng_gpu_begin_overlay_pass(r->gpu);
            if (overlay != NULL) {
                r->overlay_draw(r->overlay_user, commands, overlay);
                r->stats.overlay_passes++;
            }
        }
    }
    eng_gpu_end_frame(r->gpu);
    r->stats.frames++;
}
