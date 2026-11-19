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
