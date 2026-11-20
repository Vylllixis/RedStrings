#include "engine/instancing.h"

#include <math.h>
#include <stddef.h>

#include "engine/log.h"
#include "engine/model.h"
#include "engine/model_gpu.h"

_Static_assert(sizeof(EngInstance) == ENG_INSTANCE_STRIDE, "EngInstance : 36 octets");

void eng_instanced_pipeline_desc(EngPipelineDesc *desc)
{
    eng_model_pipeline_desc(desc);
    desc->name = "model-instanced";
    desc->vertex_shader = "instanced.vert";
    desc->instance_stride = ENG_INSTANCE_STRIDE;
    desc->instance_attribute_count = 3;
    desc->instance_attributes[0] = (EngVertexAttribute){
        .location = 4, .format = ENG_VERTEX_FLOAT4, .offset = offsetof(EngInstance, position)};
    desc->instance_attributes[1] = (EngVertexAttribute){
        .location = 5, .format = ENG_VERTEX_FLOAT4, .offset = offsetof(EngInstance, rotation)};
    desc->instance_attributes[2] = (EngVertexAttribute){
        .location = 6, .format = ENG_VERTEX_UBYTE4_NORM, .offset = offsetof(EngInstance, tint)};
}

void eng_instanced_shadow_pipeline_desc(EngPipelineDesc *desc)
{
    eng_model_shadow_pipeline_desc(desc);
    desc->name = "model-instanced-shadow";
    desc->vertex_shader = "shadow_instanced.vert";
    desc->instance_stride = ENG_INSTANCE_STRIDE;
    desc->instance_attribute_count = 2;
    desc->instance_attributes[0] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT4, .offset = offsetof(EngInstance, position)};
    desc->instance_attributes[1] = (EngVertexAttribute){
        .location = 2, .format = ENG_VERTEX_FLOAT4, .offset = offsetof(EngInstance, rotation)};
}

uint32_t eng_model_gpu_draw_depth_instanced(const EngModelGpu *gpu, EngRenderQueue *queue,
                                            EngPipelineHandle pipeline, mat4 light_view_proj,
                                            EngBufferHandle instances, uint32_t count)
{
    if (gpu->skinned) {
        ENG_LOG_ERROR("modèle GPU skinné : pas d'ombre instanciée");
        return 0;
    }
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
            .instance_count = count,
            .instance_buffer = instances,
        };
        if (eng_render_draw_uniforms(queue, &cmd, light_view_proj, sizeof(mat4))) {
            accepted++;
        }
    }
    return accepted;
}

void eng_instance_make_yaw(EngInstance *instance, const float position[3], float scale, float yaw,
                           const uint8_t tint[4])
{
    for (int c = 0; c < 3; c++) {
        instance->position[c] = position[c];
    }
    instance->scale = scale;
    instance->rotation[0] = 0.0F;
    instance->rotation[1] = sinf(0.5F * yaw);
    instance->rotation[2] = 0.0F;
    instance->rotation[3] = cosf(0.5F * yaw);
    for (int c = 0; c < 4; c++) {
        instance->tint[c] = tint != NULL ? tint[c] : 255;
    }
}

void eng_instance_matrix(const EngInstance *instance, mat4 out)
{
    versor q = {instance->rotation[0], instance->rotation[1], instance->rotation[2],
                instance->rotation[3]};
    glm_quat_mat4(q, out);
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 3; r++) {
            out[c][r] *= instance->scale;
        }
        out[3][c] = instance->position[c];
    }
}

uint32_t eng_instances_clear_disc(EngInstance *list, uint32_t count, float x, float z, float radius)
{
    if (list == NULL || !isfinite(radius) || radius <= 0.0F) {
        return count;
    }
    const float r2 = radius * radius;
    uint32_t kept = 0;
    for (uint32_t i = 0; i < count; i++) {
        const float dx = list[i].position[0] - x;
        const float dz = list[i].position[2] - z;
        if ((dx * dx) + (dz * dz) >= r2) {
            list[kept++] = list[i];
        }
    }
    return kept;
}

bool eng_instance_valid(const EngInstance *instance)
{
    for (int c = 0; c < 3; c++) {
        if (!isfinite(instance->position[c])) {
            return false;
        }
    }
    float norm2 = 0.0F;
    for (int c = 0; c < 4; c++) {
        if (!isfinite(instance->rotation[c])) {
            return false;
        }
        norm2 += instance->rotation[c] * instance->rotation[c];
    }
    return isfinite(instance->scale) && instance->scale > 0.0F &&
           fabsf(sqrtf(norm2) - 1.0F) <= 1e-3F;
}

EngBufferHandle eng_instances_upload(EngRenderer *renderer, const EngInstance *instances,
                                     uint32_t count)
{
    if (instances == NULL || count == 0 || count > (UINT32_MAX / ENG_INSTANCE_STRIDE)) {
        ENG_LOG_ERROR("instances : %u instance(s) refusée(s)", count);
        return ENG_BUFFER_NULL;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (!eng_instance_valid(&instances[i])) {
            ENG_LOG_ERROR("instances : l'instance %u n'est pas valide (non finie, échelle <= 0 ou "
                          "quaternion non unitaire)",
                          i);
            return ENG_BUFFER_NULL;
        }
    }
    const EngBufferDesc desc = {.name = "instances",
                                .usage = ENG_BUFFER_VERTEX,
                                .size = count * ENG_INSTANCE_STRIDE,
                                .data = instances};
    return eng_renderer_create_buffer(renderer, &desc);
}
