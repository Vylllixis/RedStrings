#include "engine/model_gpu.h"

#include <stdalign.h>
#include <stddef.h>
#include <string.h>

#include "engine/log.h"

/* Uniforms du vertex shader model.vert : mêmes types et ordre que son cbuffer. */
typedef struct ModelUniforms {
    mat4 mvp;
    mat4 model;         /* modèle -> monde : position monde du fragment */
    mat4 normal_matrix; /* inverse transposée du 3x3 de la matrice modèle, dans un 4x4 */
    float color[4];
} ModelUniforms;

/* Uniforms du fragment shader model.frag : quatorze float4, mêmes types et ordre que son cbuffer.
 * Les couleurs sont déjà multipliées par leur intensité. */
typedef struct FragmentUniforms {
    float sun_direction[4];   /* xyz : vers le soleil, unitaire */
    float sun_radiance[4];    /* rgb : couleur * intensité */
    float ambient[4];         /* rgb : couleur * intensité */
    float camera_position[4]; /* xyz */
    float material[4];        /* métal, rugosité, échelle de la normale, force de l'occlusion */
    float emissive[4];        /* rgb : facteur ; w : 1 si le matériau a une normal map */
    float environment[4];     /* x : dernier niveau de mip du cube préfiltré */
    mat4 shadow_matrix;       /* monde -> (u, v, profondeur) */
    float shadow_a[4];        /* xyz : vers le soleil ; w : côté d'un texel (m) */
    float shadow_b[4];        /* taille de la carte, biais constant, biais de pente, biais normal */
    float shadow_c[4];        /* rayon du PCF, force, 1 si active */
} FragmentUniforms;

void eng_model_pipeline_desc(EngPipelineDesc *desc)
{
    *desc = (EngPipelineDesc){
        .name = "model",
        .vertex_shader = "model.vert",
        .fragment_shader = "model.frag",
        .vertex_stride = sizeof(EngModelVertex),
        .attribute_count = 4,
        .cull_mode = ENG_CULL_BACK,
        .depth_test = true,
        .depth_write = true,
    };
    desc->attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(EngModelVertex, position)};
    desc->attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(EngModelVertex, normal)};
    desc->attributes[2] = (EngVertexAttribute){
        .location = 2, .format = ENG_VERTEX_FLOAT2, .offset = offsetof(EngModelVertex, uv)};
    desc->attributes[3] = (EngVertexAttribute){
        .location = 3, .format = ENG_VERTEX_FLOAT4, .offset = offsetof(EngModelVertex, tangent)};
}

void eng_model_shadow_pipeline_desc(EngPipelineDesc *desc)
{
    *desc = (EngPipelineDesc){
        .name = "model-shadow",
        .vertex_shader = "shadow.vert",
        .fragment_shader = "shadow.frag",
        .vertex_stride = sizeof(EngModelVertex),
        .attribute_count = 1,
        .cull_mode = ENG_CULL_NONE,
        .depth_test = true,
        .depth_write = true,
        .depth_only = true,
    };
    desc->attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(EngModelVertex, position)};
}

void eng_model_gpu_destroy(EngModelGpu *gpu, EngRenderer *renderer)
{
    for (uint32_t i = 0; i < gpu->texture_count; i++) {
        /* une image n'a que les versions dont un matériau a besoin : on ne détruit que celles-là */
        if (eng_renderer_texture_is_valid(renderer, gpu->textures[i])) {
            (void)eng_renderer_destroy_texture(renderer, gpu->textures[i]);
        }
        if (eng_renderer_texture_is_valid(renderer, gpu->linear_textures[i])) {
            (void)eng_renderer_destroy_texture(renderer, gpu->linear_textures[i]);
        }
    }
    (void)eng_renderer_destroy_texture(renderer, gpu->white);
    if (gpu->owns_environment) {
        eng_environment_destroy(&gpu->environment, renderer);
    }
    (void)eng_renderer_destroy_sampler(renderer, gpu->shadow_sampler);
    (void)eng_renderer_destroy_sampler(renderer, gpu->sampler);
    (void)eng_renderer_destroy_buffer(renderer, gpu->indices);
    (void)eng_renderer_destroy_buffer(renderer, gpu->vertices);
    *gpu = (EngModelGpu){0};
}

bool eng_model_gpu_create(EngModelGpu *gpu, EngRenderer *renderer, EngArena *arena,
                          const EngModel *model, float anisotropy)
{
    *gpu = (EngModelGpu){0};
    gpu->vertices = ENG_BUFFER_NULL;
    gpu->indices = ENG_BUFFER_NULL;
    gpu->sampler = ENG_SAMPLER_NULL;
    gpu->shadow_sampler = ENG_SAMPLER_NULL;
    gpu->white = ENG_TEXTURE_NULL;
    if (model->vertex_count == 0 || model->index_count == 0 || model->part_count == 0) {
        ENG_LOG_ERROR("modèle GPU : modèle vide");
        return false;
    }
    gpu->texture_count = model->image_count;
    gpu->part_count = model->part_count;
    gpu->material_count = model->material_count;
    gpu->textures = model->image_count != 0
                        ? eng_arena_alloc_array(arena, sizeof(EngTextureHandle), model->image_count,
                                                alignof(EngTextureHandle))
                        : NULL;
    gpu->linear_textures =
        model->image_count != 0
            ? eng_arena_alloc_array(arena, sizeof(EngTextureHandle), model->image_count,
                                    alignof(EngTextureHandle))
            : NULL;
    gpu->parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, model->part_count);
    gpu->materials = ENG_ARENA_NEW_ARRAY(arena, EngModelGpuMaterial, model->material_count);
    if ((model->image_count != 0 && (gpu->textures == NULL || gpu->linear_textures == NULL)) ||
        gpu->parts == NULL || gpu->materials == NULL) {
        ENG_LOG_ERROR("modèle GPU : arène trop petite");
        *gpu = (EngModelGpu){0};
        return false;
    }
    for (uint32_t i = 0; i < gpu->texture_count; i++) {
        gpu->textures[i] = ENG_TEXTURE_NULL;
        gpu->linear_textures[i] = ENG_TEXTURE_NULL;
    }

    const EngBufferDesc vertex_buffer = {.name = "Model vertices",
                                         .usage = ENG_BUFFER_VERTEX,
                                         .size =
                                             model->vertex_count * (uint32_t)sizeof(EngModelVertex),
                                         .data = model->vertices};
    const EngBufferDesc index_buffer = {.name = "Model indices",
                                        .usage = ENG_BUFFER_INDEX,
                                        .size = model->index_count * (uint32_t)sizeof(uint32_t),
                                        .data = model->indices};
    gpu->vertices = eng_renderer_create_buffer(renderer, &vertex_buffer);
    gpu->indices = eng_renderer_create_buffer(renderer, &index_buffer);
    const EngSamplerDesc sampler = {.name = "model", .max_anisotropy = anisotropy};
    gpu->sampler = eng_renderer_create_sampler(renderer, &sampler);
    const EngSamplerDesc shadow_sampler = {.name = "model-shadow",
                                           .min_filter = ENG_FILTER_NEAREST,
                                           .mag_filter = ENG_FILTER_NEAREST,
                                           .mip_filter = ENG_MIP_OFF,
                                           .address_u = ENG_ADDRESS_CLAMP,
                                           .address_v = ENG_ADDRESS_CLAMP};
    gpu->shadow_sampler = eng_renderer_create_sampler(renderer, &shadow_sampler);
    static const uint8_t white_pixel[4] = {255, 255, 255, 255};
    const EngTextureDesc white = {.name = "model-white",
                                  .width = 1,
                                  .height = 1,
                                  .format = ENG_TEXTURE_RGBA8_SRGB,
                                  .pixels = white_pixel,
                                  .mip_levels = 1};
    gpu->white = eng_renderer_create_texture(renderer, &white);

    const float white_sky[3] = {1.0F, 1.0F, 1.0F};
    gpu->owns_environment = eng_environment_create_uniform(&gpu->environment, renderer, white_sky);
    bool ok = gpu->owns_environment && eng_renderer_buffer_is_valid(renderer, gpu->vertices) &&
              eng_renderer_buffer_is_valid(renderer, gpu->indices) &&
              eng_renderer_sampler_is_valid(renderer, gpu->sampler) &&
              eng_renderer_sampler_is_valid(renderer, gpu->shadow_sampler) &&
              eng_renderer_texture_is_valid(renderer, gpu->white);
    /* Une texture par image et par FAMILLE de rôles utilisée : couleurs (sRGB) ou données
     * (linéaire). Les images que personne n'utilise n'ont pas de pixels. */
    for (uint32_t i = 0; ok && i < model->image_count; i++) {
        const EngImage *image = &model->images[i];
        if (image->pixels == NULL) {
            continue;
        }
        bool as_color = false;
        bool as_data = false;
        for (uint32_t m = 0; m < model->material_count; m++) {
            const EngModelMaterial *mat = &model->materials[m];
            as_color = as_color || mat->image == (int32_t)i || mat->emissive_image == (int32_t)i;
            as_data = as_data || mat->metallic_roughness_image == (int32_t)i ||
                      mat->normal_image == (int32_t)i || mat->occlusion_image == (int32_t)i;
        }
        const EngTextureDesc desc = {.name = "model-texture",
                                     .width = image->width,
                                     .height = image->height,
                                     .format = ENG_TEXTURE_RGBA8_SRGB,
                                     .pixels = image->pixels,
                                     .mip_levels = 0};
        if (as_color) {
            gpu->textures[i] = eng_renderer_create_texture(renderer, &desc);
            ok = eng_renderer_texture_is_valid(renderer, gpu->textures[i]);
        }
        if (ok && as_data) {
            EngTextureDesc linear = desc;
            linear.name = "model-data";
            linear.format = ENG_TEXTURE_RGBA8;
            gpu->linear_textures[i] = eng_renderer_create_texture(renderer, &linear);
            ok = eng_renderer_texture_is_valid(renderer, gpu->linear_textures[i]);
        }
    }
    if (!ok) {
        ENG_LOG_ERROR("modèle GPU : création des ressources impossible");
        eng_model_gpu_destroy(gpu, renderer);
        return false;
    }

    for (uint32_t i = 0; i < model->part_count; i++) {
        gpu->parts[i] = model->parts[i];
    }
    for (uint32_t m = 0; m < model->material_count; m++) {
        const EngModelMaterial *src = &model->materials[m];
        EngModelGpuMaterial *dst = &gpu->materials[m];
        for (int c = 0; c < 4; c++) {
            dst->base_color[c] = src->base_color[c];
        }
        for (int c = 0; c < 3; c++) {
            dst->emissive[c] = src->emissive[c];
        }
        dst->metallic = src->metallic;
        dst->roughness = src->roughness;
        dst->normal_scale = src->normal_scale;
        dst->occlusion_strength = src->occlusion_strength;
        dst->has_normal_map = src->normal_image >= 0;
        dst->texture = src->image >= 0 ? gpu->textures[src->image] : gpu->white;
        dst->metallic_roughness_texture = src->metallic_roughness_image >= 0
                                              ? gpu->linear_textures[src->metallic_roughness_image]
                                              : gpu->white;
        dst->normal_texture =
            src->normal_image >= 0 ? gpu->linear_textures[src->normal_image] : gpu->white;
        dst->occlusion_texture =
            src->occlusion_image >= 0 ? gpu->linear_textures[src->occlusion_image] : gpu->white;
        dst->emissive_texture =
            src->emissive_image >= 0 ? gpu->textures[src->emissive_image] : gpu->white;
    }
    for (int a = 0; a < 3; a++) {
        gpu->bounds_min[a] = model->bounds_min[a];
        gpu->bounds_max[a] = model->bounds_max[a];
    }
    return true;
}

bool eng_model_gpu_set_environment(EngModelGpu *gpu, EngRenderer *renderer,
                                   const EngEnvironment *env)
{
    if (!eng_environment_is_valid(env, renderer)) {
        ENG_LOG_ERROR("modèle GPU : environnement invalide");
        return false;
    }
    if (gpu->owns_environment) {
        eng_environment_destroy(&gpu->environment, renderer);
    }
    gpu->environment = *env;
    gpu->owns_environment = false;
    return true;
}

uint32_t eng_model_gpu_draw(const EngModelGpu *gpu, EngRenderQueue *queue,
                            EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                            const EngLighting *lighting)
{
    return eng_model_gpu_draw_shadowed(gpu, queue, pipeline, model_matrix, view_proj, lighting,
                                       NULL);
}

uint32_t eng_model_gpu_draw_depth(const EngModelGpu *gpu, EngRenderQueue *queue,
                                  EngPipelineHandle pipeline, mat4 model_matrix,
                                  mat4 light_view_proj)
{
    mat4 mvp;
    glm_mat4_mul(light_view_proj, model_matrix, mvp);
    uint32_t accepted = 0;
    for (uint32_t i = 0; i < gpu->part_count; i++) {
        const EngModelPart *part = &gpu->parts[i];
        const EngDrawCmd cmd = {
            .pipeline = pipeline,
            .vertex_buffer = gpu->vertices,
            .index_buffer = gpu->indices,
            .index_type = ENG_INDEX_U32,
            .count = part->index_count,
            .first = part->first_index,
        };
        if (eng_render_draw_uniforms(queue, &cmd, mvp, sizeof(mat4))) {
            accepted++;
        }
    }
    return accepted;
}

uint32_t eng_model_gpu_draw_shadowed(const EngModelGpu *gpu, EngRenderQueue *queue,
                                     EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                                     const EngLighting *lighting, const EngModelShadow *shadow)
{
    EngLighting light;
    if (lighting != NULL) {
        light = *lighting;
        (void)eng_lighting_sanitize(&light);
    } else {
        eng_lighting_default(&light);
    }
    FragmentUniforms fragment = {0};
    for (int c = 0; c < 3; c++) {
        fragment.sun_direction[c] = light.sun_direction[c];
        fragment.sun_radiance[c] = light.sun_color[c] * light.sun_intensity;
        fragment.ambient[c] = light.ambient_color[c] * light.ambient_intensity;
        fragment.camera_position[c] = light.camera_position[c];
    }

    glm_mat4_identity(fragment.shadow_matrix);
    EngTextureHandle shadow_map = gpu->white; /* sans ombre : une texture quelconque, jamais lue */
    if (shadow != NULL && shadow->map.handle.generation != 0) {
        EngShadowParams params = shadow->params;
        (void)eng_shadow_params_sanitize(&params);
        shadow_map = shadow->map;
        memcpy(fragment.shadow_matrix, shadow->shadow.shadow_matrix, sizeof fragment.shadow_matrix);
        for (int c = 0; c < 3; c++) {
            fragment.shadow_a[c] = shadow->shadow.to_sun[c];
        }
        fragment.shadow_a[3] = shadow->shadow.texel_world;
        fragment.shadow_b[0] = (float)shadow->shadow.size;
        fragment.shadow_b[1] = params.constant_bias;
        fragment.shadow_b[2] = params.slope_bias;
        fragment.shadow_b[3] = params.normal_bias;
        fragment.shadow_c[0] = (float)params.pcf_radius;
        fragment.shadow_c[1] = params.strength;
        fragment.shadow_c[2] = 1.0F;
    }

    ModelUniforms uniforms;
    glm_mat4_mul(view_proj, model_matrix, uniforms.mvp);
    glm_mat4_copy(model_matrix, uniforms.model);
    mat3 linear;
    mat3 normal;
    glm_mat4_pick3(model_matrix, linear);
    glm_mat3_inv(linear, normal);
    glm_mat3_transpose(normal);
    glm_mat4_identity(uniforms.normal_matrix);
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 3; r++) {
            uniforms.normal_matrix[c][r] = normal[c][r];
        }
    }
    uint32_t accepted = 0;
    for (uint32_t i = 0; i < gpu->part_count; i++) {
        const EngModelPart *part = &gpu->parts[i];
        const EngModelGpuMaterial *material = &gpu->materials[part->material];
        for (int c = 0; c < 4; c++) {
            uniforms.color[c] = material->base_color[c];
        }
        fragment.material[0] = material->metallic;
        fragment.material[1] = material->roughness;
        fragment.material[2] = material->normal_scale;
        fragment.material[3] = material->occlusion_strength;
        for (int c = 0; c < 3; c++) {
            fragment.emissive[c] = material->emissive[c];
        }
        fragment.emissive[3] = material->has_normal_map ? 1.0F : 0.0F;
        fragment.environment[0] = gpu->environment.prefiltered_levels > 1U
                                      ? (float)(gpu->environment.prefiltered_levels - 1U)
                                      : 0.0F;
        const EngDrawCmd cmd = {
            .pipeline = pipeline,
            .vertex_buffer = gpu->vertices,
            .index_buffer = gpu->indices,
            .index_type = ENG_INDEX_U32,
            .count = part->index_count,
            .first = part->first_index,
            .texture = material->texture,
            .sampler = gpu->sampler,
            .extra_textures = {material->metallic_roughness_texture, material->normal_texture,
                               material->occlusion_texture, material->emissive_texture,
                               gpu->environment.irradiance, gpu->environment.prefiltered,
                               gpu->environment.brdf_lut},
            .shadow_map = shadow_map,
            .shadow_sampler = gpu->shadow_sampler,
        };
        if (eng_render_draw_uniforms2(queue, &cmd, &uniforms, sizeof uniforms, &fragment,
                                      sizeof fragment)) {
            accepted++;
        }
    }
    return accepted;
}
