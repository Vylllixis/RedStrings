#include "engine/terrain_gpu.h"

#include <math.h>

#include <string.h>

#include "engine/log.h"
#include "engine/terrain_texture.h"

#define DEFAULT_TILE_METERS 8.0F
#define DEFAULT_SHARPNESS 4.0F

void eng_terrain_pipeline_desc(EngPipelineDesc *desc)
{
    eng_model_pipeline_desc(desc);
    desc->name = "terrain";
    desc->fragment_shader = "terrain.frag";
}

void eng_terrain_gpu_destroy(EngTerrainGpu *gpu, EngRenderer *renderer)
{
    eng_model_gpu_destroy(&gpu->model, renderer);
    for (uint32_t i = 0; i < ENG_SPLAT_LAYERS; i++) {
        (void)eng_renderer_destroy_texture(renderer, gpu->layers[i]);
    }
    (void)eng_renderer_destroy_texture(renderer, gpu->splat);
    const EngTextureHandle extra[3] = {gpu->extra[0], gpu->extra[1], gpu->splat2};
    for (int i = 0; i < 3; i++) {
        if (extra[i].handle.generation != 0) {
            (void)eng_renderer_destroy_texture(renderer, extra[i]);
        }
    }
    (void)eng_renderer_destroy_sampler(renderer, gpu->sampler);
    *gpu = (EngTerrainGpu){0};
}

bool eng_terrain_gpu_create(EngTerrainGpu *gpu, EngRenderer *renderer, EngArena *arena,
                            EngArena *scratch, const EngTerrain *terrain,
                            const EngTerrainGpuParams *params)
{
    *gpu = (EngTerrainGpu){0};
    gpu->splat = ENG_TEXTURE_NULL;
    gpu->sampler = ENG_SAMPLER_NULL;
    for (uint32_t i = 0; i < ENG_SPLAT_LAYERS; i++) {
        gpu->layers[i] = ENG_TEXTURE_NULL;
    }
    if (terrain == NULL || params == NULL || params->splat == NULL ||
        params->splat->pixels == NULL) {
        ENG_LOG_ERROR("terrain GPU : terrain ou splatmap absents");
        return false;
    }
    /* le terrain a un sommet par échantillon : size x size ; sa splatmap, un texel par sommet */
    uint32_t size = 0;
    while ((size_t)size * size < terrain->model.vertex_count) {
        size++;
    }
    if ((size_t)size * size != terrain->model.vertex_count || params->splat->size != size) {
        ENG_LOG_ERROR("terrain GPU : la splatmap (%u x %u) n'a pas le côté du terrain (%u)",
                      params->splat->size, params->splat->size, size);
        return false;
    }
    const uint32_t layer_size =
        params->layer_size != 0 ? params->layer_size : ENG_TERRAIN_LAYER_TEXTURE_SIZE;
    if (layer_size < 4U) {
        ENG_LOG_ERROR("terrain GPU : texture de couche trop petite (%u)", layer_size);
        return false;
    }
    gpu->tile_meters = params->tile_meters > 0.0F ? params->tile_meters : DEFAULT_TILE_METERS;
    gpu->sharpness =
        params->triplanar_sharpness >= 1.0F ? params->triplanar_sharpness : DEFAULT_SHARPNESS;
    if (!eng_model_gpu_create(&gpu->model, renderer, arena, &terrain->model, params->anisotropy)) {
        *gpu = (EngTerrainGpu){0};
        return false;
    }
    gpu->part_count = gpu->model.part_count;

    const EngSamplerDesc sampler = {.name = "terrain", .max_anisotropy = params->anisotropy};
    gpu->sampler = eng_renderer_create_sampler(renderer, &sampler);
    const EngTextureDesc splat = {.name = "terrain-splat",
                                  .width = size,
                                  .height = size,
                                  .format = ENG_TEXTURE_RGBA8, /* poids : linéaires */
                                  .pixels = params->splat->pixels,
                                  .mip_levels = 1};
    gpu->splat = eng_renderer_create_texture(renderer, &splat);
    bool ok = eng_renderer_sampler_is_valid(renderer, gpu->sampler) &&
              eng_renderer_texture_is_valid(renderer, gpu->splat);
    const EngArenaMark mark = eng_arena_mark(scratch);
    for (uint32_t i = 0; ok && i < ENG_SPLAT_LAYERS; i++) {
        const uint8_t *pixels = params->layer_pixels[i];
        if (pixels == NULL) {
            uint8_t *generated =
                eng_arena_alloc_array(scratch, 4U, (size_t)layer_size * layer_size, 4U);
            ok = generated != NULL;
            if (ok) {
                eng_terrain_layer_pixels((EngSplatLayer)i, layer_size, generated);
                pixels = generated;
            }
        }
        if (ok) {
            const EngTextureDesc layer = {.name = "terrain-layer",
                                          .width = layer_size,
                                          .height = layer_size,
                                          .format = ENG_TEXTURE_RGBA8_SRGB,
                                          .pixels = pixels,
                                          .mip_levels = 0};
            gpu->layers[i] = eng_renderer_create_texture(renderer, &layer);
            ok = eng_renderer_texture_is_valid(renderer, gpu->layers[i]);
        }
    }
    eng_arena_reset_to(scratch, mark);
    ok = ok && eng_terrain_gpu_create_extra(gpu, renderer, scratch, layer_size);
    if (!ok) {
        ENG_LOG_ERROR("terrain GPU : création des textures impossible");
        eng_terrain_gpu_destroy(gpu, renderer);
        return false;
    }
    return true;
}

bool eng_terrain_gpu_set_environment(EngTerrainGpu *gpu, EngRenderer *renderer,
                                     const EngEnvironment *env)
{
    return eng_model_gpu_set_environment(&gpu->model, renderer, env);
}

bool eng_terrain_gpu_set_fog(EngTerrainGpu *gpu, const EngFog *fog, const EngAtmosphere *atmosphere,
                             float sky_scale)
{
    eng_fog_pack(fog, atmosphere, sky_scale, gpu->fog);
    return fog == NULL || atmosphere == NULL ||
           (eng_fog_valid(fog) && eng_atmosphere_valid(atmosphere) && sky_scale >= 0.0F &&
            isfinite(sky_scale));
}

uint32_t eng_terrain_gpu_draw(const EngTerrainGpu *gpu, EngRenderQueue *queue,
                              EngPipelineHandle pipeline, mat4 view_proj,
                              const EngLighting *lighting, const EngModelShadow *shadow,
                              const uint32_t *chunks, uint32_t count)
{
    mat4 identity;
    glm_mat4_identity(identity);
    const EngTextureHandle textures[5] = {gpu->splat, gpu->layers[0], gpu->layers[1],
                                          gpu->layers[2], gpu->layers[3]};
    EngTextureHandle tail[ENG_MAX_TAIL_TEXTURES];
    eng_terrain_gpu_tail(gpu, ENG_TEXTURE_NULL, tail);
    const EngModelDrawOptions options = {.textures = textures,
                                         .tail_textures = tail,
                                         .sampler = gpu->sampler,
                                         .user = {gpu->tile_meters, gpu->sharpness, 0.0F},
                                         .parts = chunks,
                                         .part_count = count,
                                         .fragment_tail = gpu->fog,
                                         .fragment_tail_floats = ENG_FOG_UNIFORM_FLOATS};
    return eng_model_gpu_draw_ex(&gpu->model, queue, pipeline, identity, view_proj, lighting,
                                 shadow, &options);
}

uint32_t eng_terrain_gpu_draw_depth(const EngTerrainGpu *gpu, EngRenderQueue *queue,
                                    EngPipelineHandle pipeline, mat4 light_view_proj)
{
    mat4 identity;
    glm_mat4_identity(identity);
    return eng_model_gpu_draw_depth(&gpu->model, queue, pipeline, identity, light_view_proj);
}
