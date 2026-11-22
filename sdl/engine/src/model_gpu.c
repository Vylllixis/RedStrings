#include "engine/model_gpu.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>

#include "engine/log.h"
#include "engine/skeleton.h"

/* Uniforms du vertex shader model.vert : mêmes types et ordre que son cbuffer. */
typedef struct ModelUniforms {
    mat4 mvp;
    mat4 model;         /* modèle -> monde : position monde du fragment */
    mat4 normal_matrix; /* inverse transposée du 3x3 de la matrice modèle, dans un 4x4 */
    float color[4];
} ModelUniforms;

/* Sommet d'un modèle skinné dans le buffer GPU (flux unique, entrelacé). */
typedef struct SkinnedGpuVertex {
    EngModelVertex vertex;
    EngSkinVertex skin;
} SkinnedGpuVertex;

/* La palette suit ModelUniforms dans le bloc de vertex : 3 float4 (lignes 0 à 2) par articulation.
 */
#define SKIN_ROW_FLOATS 12U

/* Uniforms du vertex shader instanced.vert (M3-07) : matrice vue-projection et couleur du matériau
 * ; la position de chaque copie vient du flux d'instances. */
typedef struct InstancedUniforms {
    mat4 view_proj;
    float color[4];
} InstancedUniforms;

/* Uniforms du fragment shader model.frag : trente-deux float4, mêmes types et ordre que son
 * cbuffer. Les couleurs sont déjà multipliées par leur intensité. */
typedef struct FragmentUniforms {
    float sun_direction[4]; /* xyz : vers le soleil, unitaire ; w : seuil de découpe du matériau */
    float sun_radiance[4];  /* rgb : couleur * intensité ; w : 1 si le matériau a deux faces */
    float ambient[4];       /* rgb : couleur * intensité */
    float camera_position[4]; /* xyz */
    float material[4];        /* métal, rugosité, échelle de la normale, force de l'occlusion */
    float emissive[4];        /* rgb : facteur ; w : 1 si le matériau a une normal map */
    float environment[4];     /* x : dernier niveau de mip du cube préfiltré */
    mat4 shadow_matrix;       /* monde -> (u, v, profondeur) */
    float shadow_a[4];        /* xyz : vers le soleil ; w : côté d'un texel (m) */
    float shadow_b[4];        /* taille de la carte, biais constant, biais de pente, biais normal */
    float shadow_c[4];        /* rayon du PCF, force, 1 si active */
    float cascade_info[4];    /* x : nombre de cascades (0 ou 1 : carte unique) */
    float cascade_texel[4];   /* côté d'un texel (m) de chaque cascade */
    mat4 cascade_matrix[ENG_MODEL_SHADOW_CASCADES]; /* monde -> (u, v, prof.) de chaque cascade */
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

void eng_model_skinned_pipeline_desc(EngPipelineDesc *desc)
{
    eng_model_pipeline_desc(desc);
    desc->name = "model-skinned";
    desc->vertex_shader = "model_skinned.vert";
    desc->vertex_stride = sizeof(SkinnedGpuVertex);
    desc->attribute_count = 6;
    desc->attributes[4] = (EngVertexAttribute){.location = 4,
                                               .format = ENG_VERTEX_UBYTE4_NORM,
                                               .offset = offsetof(SkinnedGpuVertex, skin.joints)};
    desc->attributes[5] = (EngVertexAttribute){.location = 5,
                                               .format = ENG_VERTEX_UBYTE4_NORM,
                                               .offset = offsetof(SkinnedGpuVertex, skin.weights)};
}

void eng_model_skinned_shadow_pipeline_desc(EngPipelineDesc *desc)
{
    eng_model_shadow_pipeline_desc(desc);
    desc->name = "model-skinned-shadow";
    desc->vertex_shader = "shadow_skinned.vert";
    desc->vertex_stride = sizeof(SkinnedGpuVertex);
    desc->attribute_count = 3;
    /* locations 0, 1, 2 : voir shadow_skinned.vert (contiguës dans l'ordre de déclaration) */
    desc->attributes[1] = (EngVertexAttribute){.location = 1,
                                               .format = ENG_VERTEX_UBYTE4_NORM,
                                               .offset = offsetof(SkinnedGpuVertex, skin.joints)};
    desc->attributes[2] = (EngVertexAttribute){.location = 2,
                                               .format = ENG_VERTEX_UBYTE4_NORM,
                                               .offset = offsetof(SkinnedGpuVertex, skin.weights)};
}

/* Écrit la palette (3 lignes par articulation) dans `out` ; false si elle est inutilisable :
 * pointeur NULL, 0 ou trop d'articulations, valeur non finie. */
static bool pack_palette(const float *matrices, uint32_t count, float *out)
{
    if (matrices == NULL || count == 0 || count > ENG_SKELETON_MAX_JOINTS) {
        return false;
    }
    for (uint32_t j = 0; j < count; j++) {
        const float *m = &matrices[(size_t)j * 16U];
        float *row = &out[(size_t)j * SKIN_ROW_FLOATS];
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                const float v = m[(c * 4) + r];
                if (!isfinite(v)) {
                    return false;
                }
                row[(r * 4) + c] = v;
            }
        }
    }
    return true;
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

static bool create_impl(EngModelGpu *gpu, EngRenderer *renderer, EngArena *arena,
                        const EngModel *model, const EngSkinVertex *skin, float anisotropy)
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

    SkinnedGpuVertex *interleaved = NULL;
    if (skin != NULL) {
        /* flux unique : EngModelVertex puis EngSkinVertex, sommet par sommet */
        interleaved = SDL_malloc((size_t)model->vertex_count * sizeof(SkinnedGpuVertex));
        if (interleaved == NULL) {
            ENG_LOG_ERROR("modèle GPU : mémoire insuffisante (%u sommets skinnés)",
                          model->vertex_count);
            *gpu = (EngModelGpu){0};
            return false;
        }
        for (uint32_t v = 0; v < model->vertex_count; v++) {
            interleaved[v].vertex = model->vertices[v];
            interleaved[v].skin = skin[v];
        }
        gpu->skinned = true;
    }
    const EngBufferDesc vertex_buffer = {
        .name = skin != NULL ? "Skinned model vertices" : "Model vertices",
        .usage = ENG_BUFFER_VERTEX,
        .size = model->vertex_count *
                (uint32_t)(skin != NULL ? sizeof(SkinnedGpuVertex) : sizeof(EngModelVertex)),
        .data = skin != NULL ? (const void *)interleaved : (const void *)model->vertices};
    const EngBufferDesc index_buffer = {.name = "Model indices",
                                        .usage = ENG_BUFFER_INDEX,
                                        .size = model->index_count * (uint32_t)sizeof(uint32_t),
                                        .data = model->indices};
    gpu->vertices = eng_renderer_create_buffer(renderer, &vertex_buffer);
    SDL_free(interleaved); /* le buffer GPU a sa propre copie */
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
        if (image->encoding != ENG_IMAGE_RGBA8) {
            /* Texture compressée (BC7) : chaîne de mips déjà calculée, espace de couleur fixé par
             * le fichier. Elle ne peut servir que dans le rôle qui lui correspond. */
            const bool srgb = image->encoding == ENG_IMAGE_BC7_SRGB;
            if ((srgb && as_data) || (!srgb && as_color)) {
                ENG_LOG_ERROR("modèle GPU : l'image %u est %s mais un matériau l'emploie en %s", i,
                              srgb ? "BC7 sRGB" : "BC7 linéaire",
                              srgb ? "données linéaires" : "couleur");
                ok = false;
                break;
            }
            const EngTextureDataDesc compressed = {.name = "model-texture-bc7",
                                                   .width = image->width,
                                                   .height = image->height,
                                                   .layers = 1,
                                                   .mip_levels = image->mip_count,
                                                   .format = srgb ? ENG_TEXTURE_BC7_SRGB
                                                                  : ENG_TEXTURE_BC7,
                                                   .data = image->pixels};
            EngTextureHandle *slot = srgb ? &gpu->textures[i] : &gpu->linear_textures[i];
            *slot = eng_renderer_create_texture_data(renderer, &compressed);
            ok = eng_renderer_texture_is_valid(renderer, *slot);
            continue;
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
        dst->alpha_cutoff = src->alpha_cutoff;
        dst->double_sided = src->double_sided;
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

bool eng_model_gpu_create(EngModelGpu *gpu, EngRenderer *renderer, EngArena *arena,
                          const EngModel *model, float anisotropy)
{
    return create_impl(gpu, renderer, arena, model, NULL, anisotropy);
}

bool eng_model_gpu_create_skinned(EngModelGpu *gpu, EngRenderer *renderer, EngArena *arena,
                                  const EngModel *model, const EngSkinVertex *skin,
                                  float anisotropy)
{
    if (skin == NULL) {
        ENG_LOG_ERROR("modèle GPU skinné : poids de skinning absents");
        *gpu = (EngModelGpu){0};
        return false;
    }
    return create_impl(gpu, renderer, arena, model, skin, anisotropy);
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
    if (gpu->skinned) {
        ENG_LOG_ERROR("modèle GPU skinné : utiliser eng_model_gpu_draw_depth_skinned");
        return 0;
    }
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

uint32_t eng_model_gpu_draw_depth_skinned(const EngModelGpu *gpu, EngRenderQueue *queue,
                                          EngPipelineHandle pipeline, mat4 model_matrix,
                                          mat4 light_view_proj, const float *skin_matrices,
                                          uint32_t skin_joint_count)
{
    if (!gpu->skinned) {
        ENG_LOG_ERROR("modèle GPU : ombre skinnée demandée pour un modèle ordinaire");
        return 0;
    }
    float blob[16U + ((size_t)ENG_SKELETON_MAX_JOINTS * SKIN_ROW_FLOATS)]; /* mat4, puis palette */
    if (!pack_palette(skin_matrices, skin_joint_count, &blob[16])) {
        ENG_LOG_ERROR("modèle GPU skinné : palette d'ombre absente ou invalide");
        return 0;
    }
    mat4 mvp;
    glm_mat4_mul(light_view_proj, model_matrix, mvp);
    memcpy(blob, mvp, sizeof mvp);
    const uint32_t size =
        (uint32_t)(sizeof(mat4) + ((size_t)skin_joint_count * SKIN_ROW_FLOATS * sizeof(float)));
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
        if (eng_render_draw_uniforms(queue, &cmd, blob, size)) {
            accepted++;
        }
    }
    return accepted;
}

uint32_t eng_model_gpu_draw_shadowed(const EngModelGpu *gpu, EngRenderQueue *queue,
                                     EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                                     const EngLighting *lighting, const EngModelShadow *shadow)
{
    return eng_model_gpu_draw_ex(gpu, queue, pipeline, model_matrix, view_proj, lighting, shadow,
                                 NULL);
}

uint32_t eng_model_gpu_draw_ex(const EngModelGpu *gpu, EngRenderQueue *queue,
                               EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                               const EngLighting *lighting, const EngModelShadow *shadow,
                               const EngModelDrawOptions *options)
{
    const EngModelDrawOptions none_options = {0};
    const EngModelDrawOptions *skin_opt = options != NULL ? options : &none_options;
    /* palette de skinning : obligatoire pour un modèle skinné, interdite pour un autre */
    float skin_blob[(sizeof(ModelUniforms) / sizeof(float)) +
                    ((size_t)ENG_SKELETON_MAX_JOINTS * SKIN_ROW_FLOATS)];
    uint32_t skin_blob_size = 0;
    if (gpu->skinned) {
        if (!pack_palette(skin_opt->skin_matrices, skin_opt->skin_joint_count,
                          &skin_blob[sizeof(ModelUniforms) / sizeof(float)])) {
            ENG_LOG_ERROR("modèle GPU skinné : palette absente ou invalide (%u matrices)",
                          skin_opt->skin_joint_count);
            return 0;
        }
        skin_blob_size = (uint32_t)(sizeof(ModelUniforms) + ((size_t)skin_opt->skin_joint_count *
                                                             SKIN_ROW_FLOATS * sizeof(float)));
    } else if (skin_opt->skin_matrices != NULL || skin_opt->skin_joint_count != 0) {
        ENG_LOG_ERROR("modèle GPU : une palette de skinning n'est valable que pour un modèle "
                      "skinné");
        return 0;
    }
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
        const uint32_t cascades = shadow->cascade_count <= ENG_MODEL_SHADOW_CASCADES
                                      ? shadow->cascade_count
                                      : ENG_MODEL_SHADOW_CASCADES;
        if (cascades >= 2U) { /* M7-09 : atlas ; la cascade 0 donne soleil et taille */
            fragment.cascade_info[0] = (float)cascades;
            for (int c = 0; c < 3; c++) {
                fragment.shadow_a[c] = shadow->cascades[0].to_sun[c];
            }
            fragment.shadow_b[0] = (float)shadow->cascades[0].size;
            for (uint32_t k = 0; k < cascades; k++) {
                memcpy(fragment.cascade_matrix[k], shadow->cascades[k].shadow_matrix,
                       sizeof fragment.cascade_matrix[k]);
                fragment.cascade_texel[k] = shadow->cascades[k].texel_world;
            }
        }
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
    const EngModelDrawOptions none = {0};
    const EngModelDrawOptions *opt = options != NULL ? options : &none;
    const bool instanced = opt->instance_buffer.handle.generation != 0;
    InstancedUniforms instanced_uniforms;
    glm_mat4_copy(view_proj, instanced_uniforms.view_proj);
    const EngSamplerHandle sampler =
        opt->sampler.handle.generation != 0 ? opt->sampler : gpu->sampler;
    const uint32_t count = opt->parts != NULL ? opt->part_count : gpu->part_count;
    uint32_t accepted = 0;
    /* queue du bloc de fragment (M3-06) : copiée après les uniforms d'éclairage, qui changent à
     * chaque partie (matériau) : on réécrit la partie commune dans `blob` à chaque dessin */
    uint32_t tail_floats = opt->fragment_tail != NULL ? opt->fragment_tail_floats : 0U;
    if (tail_floats > ENG_MODEL_MAX_TAIL_FLOATS) {
        tail_floats = ENG_MODEL_MAX_TAIL_FLOATS;
    }
    tail_floats &= ~3U; /* le bloc de fragment est aligné sur 16 octets : des float4 entiers */
    uint8_t blob[sizeof(FragmentUniforms) + (ENG_MODEL_MAX_TAIL_FLOATS * sizeof(float))];
    if (tail_floats > 0) {
        memcpy(blob + sizeof(FragmentUniforms), opt->fragment_tail,
               (size_t)tail_floats * sizeof(float));
    }
    /* queue du bloc de vertex d'un dessin instancié (M3-09) */
    uint32_t vertex_tail_floats =
        instanced && opt->vertex_tail != NULL ? opt->vertex_tail_floats : 0U;
    if (vertex_tail_floats > ENG_MODEL_MAX_VERTEX_TAIL_FLOATS) {
        vertex_tail_floats = ENG_MODEL_MAX_VERTEX_TAIL_FLOATS;
    }
    vertex_tail_floats &= ~3U;
    for (uint32_t k = 0; k < count; k++) {
        const uint32_t i = opt->parts != NULL ? opt->parts[k] : k;
        if (i >= gpu->part_count) {
            continue;
        }
        const EngModelPart *part = &gpu->parts[i];
        const EngModelGpuMaterial *material = &gpu->materials[part->material];
        const EngTextureHandle *tex = opt->textures;
        for (int c = 0; c < 4; c++) {
            uniforms.color[c] = material->base_color[c];
        }
        fragment.material[0] = material->metallic;
        fragment.material[1] = material->roughness;
        fragment.material[2] = material->normal_scale;
        fragment.material[3] = material->occlusion_strength;
        for (int c = 0; c < 3; c++) {
            fragment.emissive[c] = material->emissive[c] + opt->emissive_add[c];
        }
        fragment.emissive[3] = material->has_normal_map ? 1.0F : 0.0F;
        /* matière découpée, deux faces : lus par tree.frag, ignorés par model.frag */
        fragment.sun_direction[3] = material->alpha_cutoff;
        fragment.sun_radiance[3] = material->double_sided ? 1.0F : 0.0F;
        fragment.environment[0] = gpu->environment.prefiltered_levels > 1U
                                      ? (float)(gpu->environment.prefiltered_levels - 1U)
                                      : 0.0F;
        for (int c = 0; c < 3; c++) {
            fragment.environment[1 + c] = opt->user[c];
        }
        for (int c = 0; c < 4; c++) {
            instanced_uniforms.color[c] = material->base_color[c];
        }
        const EngDrawCmd cmd = {
            .pipeline = pipeline,
            .vertex_buffer = gpu->vertices,
            .index_buffer = gpu->indices,
            .index_type = ENG_INDEX_U32,
            .count = part->index_count,
            .first = part->first_index,
            .instance_count = instanced ? opt->instance_count : 0U,
            .instance_buffer = opt->instance_buffer,
            .texture = tex != NULL ? tex[0] : material->texture,
            .sampler = sampler,
            .extra_textures = {tex != NULL ? tex[1] : material->metallic_roughness_texture,
                               tex != NULL ? tex[2] : material->normal_texture,
                               tex != NULL ? tex[3] : material->occlusion_texture,
                               tex != NULL ? tex[4] : material->emissive_texture,
                               gpu->environment.irradiance, gpu->environment.prefiltered,
                               gpu->environment.brdf_lut},
            .shadow_map = shadow_map,
            .shadow_sampler = gpu->shadow_sampler,
            .tail_textures = {opt->tail_textures != NULL ? opt->tail_textures[0] : ENG_TEXTURE_NULL,
                              opt->tail_textures != NULL ? opt->tail_textures[1] : ENG_TEXTURE_NULL,
                              opt->tail_textures != NULL ? opt->tail_textures[2]
                                                         : ENG_TEXTURE_NULL},
        };
        const void *fragment_data = &fragment;
        uint32_t fragment_size = (uint32_t)sizeof fragment;
        if (tail_floats > 0) {
            memcpy(blob, &fragment, sizeof fragment);
            fragment_data = blob;
            fragment_size = (uint32_t)(sizeof fragment + ((size_t)tail_floats * sizeof(float)));
        }
        const void *vertex_data = instanced ? (const void *)&instanced_uniforms : &uniforms;
        uint32_t vertex_size =
            instanced ? (uint32_t)sizeof instanced_uniforms : (uint32_t)sizeof uniforms;
        if (gpu->skinned) {
            memcpy(skin_blob, &uniforms, sizeof uniforms);
            vertex_data = skin_blob;
            vertex_size = skin_blob_size;
        }
        uint8_t vertex_blob[sizeof(InstancedUniforms) +
                            (ENG_MODEL_MAX_VERTEX_TAIL_FLOATS * sizeof(float))];
        if (vertex_tail_floats > 0) {
            memcpy(vertex_blob, &instanced_uniforms, sizeof instanced_uniforms);
            memcpy(vertex_blob + sizeof instanced_uniforms, opt->vertex_tail,
                   (size_t)vertex_tail_floats * sizeof(float));
            vertex_data = vertex_blob;
            vertex_size = (uint32_t)(sizeof instanced_uniforms +
                                     ((size_t)vertex_tail_floats * sizeof(float)));
        }
        if (eng_render_draw_uniforms2(queue, &cmd, vertex_data, vertex_size, fragment_data,
                                      fragment_size)) {
            accepted++;
        }
    }
    return accepted;
}
