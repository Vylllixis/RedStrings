/* Rendu des arbres réalistes : pipelines, uniforms du vent et du feuillage, mise en file des
 * dessins, références CPU des shaders. Voir engine/tree_render.h. */
#include "engine/tree_render.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "engine/log.h"
#include "engine/model.h"

#define REFERENCE_HEIGHT_M 10.0F /* l'arbre dont la cime bouge de `strength` mètres */

static float clamp_unit(float x)
{
    return isfinite(x) ? (x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x)) : 0.0F;
}

static float positive(float x, float fallback)
{
    return isfinite(x) && x > 0.0F ? x : fallback;
}

EngTreeLook eng_tree_look_default(void)
{
    return (EngTreeLook){.translucency = 0.6F,
                         .crown_normal = 0.45F,
                         .wrap = 0.35F,
                         .sky_occlusion = 0.75F,
                         .sun_occlusion = 0.4F,
                         .dither_near = 0.15F,
                         .dither_far = 0.9F,
                         .dither_distance = 60.0F,
                         .shadow_offset = 0.3F,
                         .edge_fade = 0.25F,
                         .leaf_gain = 1.45F,
                         .leaf_reflection = 0.35F,
                         .shadow_density = 0.72F};
}

EngTreeWind eng_tree_wind_default(void)
{
    return (EngTreeWind){.direction = {1.0F, 0.0F}, .strength = 0.22F, .flutter = 0.035F};
}

EngTreeShape eng_tree_shape_from_bounds(const float bounds_min[3], const float bounds_max[3],
                                        float crown_height)
{
    EngTreeShape shape = {.height = 1.0F, .crown_y = 0.5F, .radius = 1.0F};
    if (bounds_min == NULL || bounds_max == NULL) {
        return shape;
    }
    shape.height = positive(bounds_max[1], 1.0F);
    shape.crown_y = clamp_unit(crown_height) * shape.height;
    float radius = 0.0F;
    for (int a = 0; a < 3; a += 2) {
        radius = fmaxf(radius, fmaxf(fabsf(bounds_min[a]), fabsf(bounds_max[a])));
    }
    shape.radius = positive(radius, 1.0F);
    return shape;
}

void eng_tree_pack_vertex(const EngTreeShape *shape, const EngTreeWind *wind,
                          float out[ENG_TREE_VERTEX_FLOATS])
{
    memset(out, 0, ENG_TREE_VERTEX_FLOATS * sizeof(float));
    out[0] = 1.0F; /* sans vent : une direction quelconque, une force nulle */
    out[4] = 1.0F;
    out[6] = 1.0F;
    if (wind != NULL) {
        const float length = hypotf(wind->direction[0], wind->direction[1]);
        if (isfinite(length) && length > 1e-6F) {
            out[0] = wind->direction[0] / length;
            out[1] = wind->direction[1] / length;
            out[2] = positive(wind->strength, 0.0F);
            out[7] = positive(wind->flutter, 0.0F);
        }
        out[3] = isfinite(wind->time_s) ? wind->time_s : 0.0F;
    }
    if (shape != NULL) {
        out[4] = positive(shape->height, 1.0F);
        out[5] = isfinite(shape->crown_y) ? shape->crown_y : 0.5F * out[4];
        out[6] = positive(shape->radius, 1.0F);
    }
}

void eng_tree_pack_fragment(const EngTreeLook *look, const float *fog,
                            float out[ENG_TREE_FRAGMENT_FLOATS])
{
    const EngTreeLook fallback = eng_tree_look_default();
    const EngTreeLook *l = look != NULL ? look : &fallback;
    out[0] = clamp_unit(l->translucency);
    out[1] = clamp_unit(l->crown_normal);
    out[2] = clamp_unit(l->sky_occlusion);
    out[3] = clamp_unit(l->sun_occlusion);
    out[4] = clamp_unit(l->dither_near);
    out[5] = clamp_unit(l->dither_far);
    out[6] = positive(l->dither_distance, 1.0F);
    out[7] = positive(l->shadow_offset, 0.0F);
    out[8] = clamp_unit(l->edge_fade);
    out[9] = clamp_unit(l->wrap);
    out[10] = positive(l->leaf_gain, 1.0F);
    out[11] = clamp_unit(l->leaf_reflection);
    if (fog != NULL) {
        memcpy(&out[ENG_TREE_LOOK_FLOATS], fog, ENG_FOG_UNIFORM_FLOATS * sizeof(float));
    } else {
        memset(&out[ENG_TREE_LOOK_FLOATS], 0, ENG_FOG_UNIFORM_FLOATS * sizeof(float));
    }
}

static void rotate(const float q[4], const float v[3], float out[3])
{
    /* v + 2 q.xyz x (q.xyz x v + q.w v) : comme les shaders */
    const float inner[3] = {(q[1] * v[2]) - (q[2] * v[1]) + (q[3] * v[0]),
                            (q[2] * v[0]) - (q[0] * v[2]) + (q[3] * v[1]),
                            (q[0] * v[1]) - (q[1] * v[0]) + (q[3] * v[2])};
    out[0] = v[0] + (2.0F * ((q[1] * inner[2]) - (q[2] * inner[1])));
    out[1] = v[1] + (2.0F * ((q[2] * inner[0]) - (q[0] * inner[2])));
    out[2] = v[2] + (2.0F * ((q[0] * inner[1]) - (q[1] * inner[0])));
}

void eng_tree_vertex_world(const float packed[ENG_TREE_VERTEX_FLOATS], const EngInstance *instance,
                           const float local[3], float out[3])
{
    const float scale = instance->scale;
    const float time = packed[3];
    const float height = packed[4];
    const float h = clamp_unit(local[1] / height);
    const float phase = (0.37F * instance->position[0]) + (0.23F * instance->position[2]);
    const float gust = 0.6F + (0.4F * sinf((0.7F * time) + phase));
    const float bend = packed[2] * (height * scale / REFERENCE_HEIGHT_M) * h * h * gust;
    const float reach = clamp_unit(hypotf(local[0], local[2]) / packed[6]);
    const float wave =
        sinf((2.3F * time) + phase + (1.7F * local[0]) + (2.9F * local[1]) + (1.3F * local[2]));
    const float flap = packed[7] * reach * h * wave * scale;
    const float scaled[3] = {local[0] * scale, local[1] * scale, local[2] * scale};
    float rotated[3];
    rotate(instance->rotation, scaled, rotated);
    const float along = bend + (0.4F * flap);
    out[0] = rotated[0] + instance->position[0] + (packed[0] * along);
    out[1] = rotated[1] + instance->position[1] + (0.6F * flap);
    out[2] = rotated[2] + instance->position[2] + (packed[1] * along);
}

float eng_tree_dither(float pixel_x, float pixel_y)
{
    const float inner = (pixel_x * 0.06711056F) + (pixel_y * 0.00583715F);
    const float outer = 52.9829189F * (inner - floorf(inner));
    return outer - floorf(outer);
}

void eng_tree_pipeline_desc(EngPipelineDesc *desc)
{
    eng_instanced_pipeline_desc(desc);
    desc->name = "tree";
    desc->vertex_shader = "tree.vert";
    desc->fragment_shader = "tree.frag";
    desc->cull_mode = ENG_CULL_NONE;
}

void eng_tree_shadow_pipeline_desc(EngPipelineDesc *desc)
{
    eng_model_shadow_pipeline_desc(desc);
    desc->name = "tree-shadow";
    desc->vertex_shader = "tree_shadow.vert";
    desc->fragment_shader = "tree_shadow.frag";
    desc->attribute_count = 2;
    desc->attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT2, .offset = offsetof(EngModelVertex, uv)};
    desc->instance_stride = ENG_INSTANCE_STRIDE;
    desc->instance_attribute_count = 2;
    desc->instance_attributes[0] = (EngVertexAttribute){
        .location = 2, .format = ENG_VERTEX_FLOAT4, .offset = offsetof(EngInstance, position)};
    desc->instance_attributes[1] = (EngVertexAttribute){
        .location = 3, .format = ENG_VERTEX_FLOAT4, .offset = offsetof(EngInstance, rotation)};
}

static bool drawable(const EngModelGpu *gpu, const EngRenderQueue *queue, const EngTreeDraw *draw)
{
    return gpu != NULL && queue != NULL && draw != NULL && draw->count > 0U &&
           draw->instances.handle.generation != 0;
}

uint32_t eng_tree_draw(const EngModelGpu *gpu, EngRenderQueue *queue, EngPipelineHandle pipeline,
                       mat4 view_proj, const EngLighting *lighting, const EngModelShadow *shadow,
                       const EngTreeDraw *draw)
{
    if (!drawable(gpu, queue, draw)) {
        return 0;
    }
    float vertex[ENG_TREE_VERTEX_FLOATS];
    float fragment[ENG_TREE_FRAGMENT_FLOATS];
    eng_tree_pack_vertex(&draw->shape, &draw->wind, vertex);
    eng_tree_pack_fragment(draw->look, draw->fog, fragment);
    const EngModelDrawOptions options = {.instance_buffer = draw->instances,
                                         .instance_count = draw->count,
                                         .vertex_tail = vertex,
                                         .vertex_tail_floats = ENG_TREE_VERTEX_FLOATS,
                                         .fragment_tail = fragment,
                                         .fragment_tail_floats = ENG_TREE_FRAGMENT_FLOATS,
                                         .parts = draw->parts,
                                         .part_count = draw->part_count};
    mat4 identity;
    glm_mat4_identity(identity);
    return eng_model_gpu_draw_ex(gpu, queue, pipeline, identity, view_proj, lighting, shadow,
                                 &options);
}

uint32_t eng_tree_draw_depth(const EngModelGpu *gpu, EngRenderQueue *queue,
                             EngPipelineHandle pipeline, mat4 light_view_proj,
                             const EngTreeDraw *draw)
{
    if (!drawable(gpu, queue, draw)) {
        return 0;
    }
    if (gpu->skinned) {
        ENG_LOG_ERROR("arbres : pas d'ombre pour un modèle skinné");
        return 0;
    }
    struct {
        mat4 light_view_proj;
        float tree[ENG_TREE_VERTEX_FLOATS];
    } vertex;
    glm_mat4_copy(light_view_proj, vertex.light_view_proj);
    eng_tree_pack_vertex(&draw->shape, &draw->wind, vertex.tree);
    const EngTreeLook fallback = eng_tree_look_default();
    const float wanted = (draw->look != NULL ? draw->look : &fallback)->shadow_density;
    const float density =
        isfinite(wanted) ? (wanted < 0.0F ? 0.0F : (wanted > 1.0F ? 1.0F : wanted)) : 1.0F;
    uint32_t accepted = 0;
    const uint32_t count = draw->parts != NULL ? draw->part_count : gpu->part_count;
    for (uint32_t k = 0; k < count; k++) {
        const uint32_t i = draw->parts != NULL ? draw->parts[k] : k;
        if (i >= gpu->part_count) {
            continue;
        }
        const EngModelPart *part = &gpu->parts[i];
        const EngModelGpuMaterial *material = &gpu->materials[part->material];
        /* x : seuil de découpe de la partie (0 : opaque, la texture n'est pas lue) ; y : densité
         * de l'ombre du feuillage */
        const float cut[4] = {material->alpha_cutoff, density, 0.0F, 0.0F};
        const EngDrawCmd cmd = {
            .pipeline = pipeline,
            .vertex_buffer = gpu->vertices,
            .index_buffer = gpu->indices,
            .index_type = ENG_INDEX_U32,
            .count = part->index_count,
            .first = part->first_index,
            .instance_count = draw->count,
            .instance_buffer = draw->instances,
            .texture = material->texture,
            .sampler = gpu->sampler,
        };
        if (eng_render_draw_uniforms2(queue, &cmd, &vertex, (uint32_t)sizeof vertex, cut,
                                      (uint32_t)sizeof cut)) {
            accepted++;
        }
    }
    return accepted;
}
