#include "engine/debug_lines_gpu.h"

#include <stddef.h>

#include "engine/log.h"

static void fill_desc(EngPipelineDesc *desc, const char *name, bool depth_test,
                      uint32_t color_format)
{
    *desc = (EngPipelineDesc){
        .name = name,
        .vertex_shader = "lines.vert",
        .fragment_shader = "lines.frag",
        .vertex_stride = sizeof(EngDebugVertex),
        .attribute_count = 2,
        .topology = ENG_TOPOLOGY_LINES,
        .cull_mode = ENG_CULL_NONE,
        .color_format = color_format,
        .depth_test = depth_test,
        .depth_write = false,
    };
    desc->attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(EngDebugVertex, position)};
    desc->attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_UBYTE4_NORM, .offset = offsetof(EngDebugVertex, color)};
}

void eng_debug_lines_gpu_destroy(EngDebugLinesGpu *gpu, EngRenderer *renderer)
{
    (void)eng_renderer_destroy_pipeline(renderer, gpu->overlay_pipeline);
    (void)eng_renderer_destroy_pipeline(renderer, gpu->depth_pipeline);
    (void)eng_renderer_destroy_buffer(renderer, gpu->buffer);
    *gpu = (EngDebugLinesGpu){0};
}

bool eng_debug_lines_gpu_create(EngDebugLinesGpu *gpu, EngRenderer *renderer, uint32_t max_lines,
                                uint32_t color_format)
{
    *gpu = (EngDebugLinesGpu){0};
    if (max_lines == 0 || max_lines > ENG_DEBUG_MAX_LINES) {
        ENG_LOG_ERROR("debug draw : capacité invalide (%u lignes)", (unsigned)max_lines);
        return false;
    }
    const uint32_t bytes = max_lines * 4U * (uint32_t)sizeof(EngDebugVertex); /* 2 listes */
    gpu->buffer = eng_renderer_create_stream_buffer(renderer, ENG_BUFFER_VERTEX, bytes);
    EngPipelineDesc desc;
    fill_desc(&desc, "debug-lines", eng_renderer_depth_format(renderer) != 0, color_format);
    gpu->depth_pipeline = eng_renderer_create_pipeline(renderer, &desc);
    fill_desc(&desc, "debug-lines-overlay", false, color_format);
    gpu->overlay_pipeline = eng_renderer_create_pipeline(renderer, &desc);
    if (!eng_renderer_buffer_is_valid(renderer, gpu->buffer) ||
        !eng_renderer_pipeline_is_valid(renderer, gpu->depth_pipeline) ||
        !eng_renderer_pipeline_is_valid(renderer, gpu->overlay_pipeline)) {
        ENG_LOG_ERROR("debug draw : ressources GPU impossibles");
        eng_debug_lines_gpu_destroy(gpu, renderer);
        return false;
    }
    gpu->max_lines = max_lines;
    return true;
}

uint32_t eng_debug_lines_gpu_draw(const EngDebugLinesGpu *gpu, EngRenderer *renderer,
                                  EngRenderQueue *queue, const EngDebugLines *lines, mat4 view_proj)
{
    if (gpu->max_lines == 0 || lines->capacity > gpu->max_lines) {
        return 0;
    }
    /* Disposition : [liste testée : max_lines × 2 sommets][liste superposée : idem]. */
    const uint32_t overlay_first = gpu->max_lines * 2U;
    const uint32_t stride = (uint32_t)sizeof(EngDebugVertex);
    if (lines->depth_count != 0 &&
        !eng_renderer_stream_write(renderer, gpu->buffer, 0, lines->depth,
                                   lines->depth_count * stride)) {
        return 0;
    }
    if (lines->overlay_count != 0 &&
        !eng_renderer_stream_write(renderer, gpu->buffer, overlay_first * stride, lines->overlay,
                                   lines->overlay_count * stride)) {
        return 0;
    }
    uint32_t accepted = 0;
    if (lines->depth_count != 0) {
        const EngDrawCmd cmd = {.pipeline = gpu->depth_pipeline,
                                .vertex_buffer = gpu->buffer,
                                .count = lines->depth_count,
                                .first = 0};
        accepted += eng_render_draw_uniforms(queue, &cmd, view_proj, sizeof(mat4)) ? 1U : 0U;
    }
    if (lines->overlay_count != 0) {
        const EngDrawCmd cmd = {.pipeline = gpu->overlay_pipeline,
                                .vertex_buffer = gpu->buffer,
                                .count = lines->overlay_count,
                                .first = overlay_first};
        accepted += eng_render_draw_uniforms(queue, &cmd, view_proj, sizeof(mat4)) ? 1U : 0U;
    }
    return accepted;
}
