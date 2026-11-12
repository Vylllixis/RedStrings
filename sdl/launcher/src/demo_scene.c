#include "demo_scene.h"

#include <math.h>
#include <stddef.h>

#include "engine/image.h"
#include "engine/log.h"
#include "engine/model.h"
#include "engine/math.h"

#define CHECKER_PATH "assets/textures/checker.png"
#define DEBUG_MAX_LINES 4096U /* lignes par liste : 2 × 4096 × 32 octets côté CPU */

/* Position (x, y, z) en mètres, teinte (r, g, b) et coordonnées de texture (u, v). Une teinte par
 * face : 4 sommets par face, donc 24 sommets, et 2 triangles par face, soit 36 index. */
typedef struct Vertex {
    float position[3];
    float tint[3];
    float uv[2];
} Vertex;

#define CUBE_VERTICES 24
#define CUBE_INDICES 36

/* Une face : normale n, axes u et v avec u × v = n (main droite), couleur. Les sommets (-u-v),
 * (+u-v), (+u+v), (-u+v) sont dans le sens antihoraire vu de l'extérieur : face avant. */
typedef struct Face {
    float n[3];
    float u[3];
    float v[3];
    float color[3];
} Face;

static const Face FACES[6] = {
    {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {0.90F, 0.25F, 0.25F}},  /* +X rouge */
    {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0.25F, 0.70F, 0.85F}},  /* -X cyan */
    {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0.35F, 0.85F, 0.35F}},  /* +Y vert */
    {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, {0.85F, 0.35F, 0.80F}},  /* -Y magenta */
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0.25F, 0.40F, 0.90F}},   /* +Z bleu */
    {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0.95F, 0.80F, 0.25F}}, /* -Z jaune */
};

static void build_cube(Vertex *vertices, uint16_t *indices)
{
    static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    const float half = 0.5F;
    for (int f = 0; f < 6; f++) {
        for (int c = 0; c < 4; c++) {
            Vertex *vertex = &vertices[(f * 4) + c];
            for (int axis = 0; axis < 3; axis++) {
                vertex->position[axis] =
                    half * (FACES[f].n[axis] + (corner[c][0] * FACES[f].u[axis]) +
                            (corner[c][1] * FACES[f].v[axis]));
                /* teinte douce : la texture domine, les faces restent reconnaissables */
                vertex->tint[axis] = 0.55F + (0.45F * FACES[f].color[axis]);
            }
            /* (0, 0) = haut-gauche de l'image : le haut de la face (+v) a v = 0 */
            vertex->uv[0] = (corner[c][0] + 1.0F) * 0.5F;
            vertex->uv[1] = (1.0F - corner[c][1]) * 0.5F;
        }
        const uint16_t base = (uint16_t)(f * 4);
        uint16_t *quad = &indices[(size_t)f * 6U];
        quad[0] = base;
        quad[1] = (uint16_t)(base + 1);
        quad[2] = (uint16_t)(base + 2);
        quad[3] = base;
        quad[4] = (uint16_t)(base + 2);
        quad[5] = (uint16_t)(base + 3);
    }
}

/* Texture de remplacement si l'image est introuvable : damier magenta et noir, impossible à
 * rater, pour que le programme continue. */
static EngTextureHandle make_missing_texture(EngRenderer *renderer)
{
    uint8_t pixels[8 * 8 * 4];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            uint8_t *p = &pixels[((size_t)((y * 8) + x)) * 4U];
            const bool on = ((x ^ y) & 1) != 0;
            p[0] = on ? 255 : 0;
            p[1] = 0;
            p[2] = on ? 255 : 0;
            p[3] = 255;
        }
    }
    const EngTextureDesc desc = {.name = "manquante",
                                 .width = 8,
                                 .height = 8,
                                 .format = ENG_TEXTURE_RGBA8_SRGB,
                                 .pixels = pixels,
                                 .mip_levels = 1};
    return eng_renderer_create_texture(renderer, &desc);
}

#define MODEL_PATH "assets/models/demo.gltf"

/* Le modèle glTF est facultatif : sans lui (fichier absent, shader absent), le cube central reste.
 */
static void load_model(DemoScene *scene, EngRenderer *renderer, EngArena *permanent,
                       EngArena *level, EngArena *scratch, float anisotropy)
{
    const EngArenaMark level_mark = eng_arena_mark(level);
    const EngArenaMark permanent_mark = eng_arena_mark(permanent);
    EngModel model;
    if (!eng_model_load(level, scratch, MODEL_PATH, &model)) {
        ENG_LOG_ERROR("modèle de démonstration introuvable ou invalide (%s) : cube seul",
                      MODEL_PATH);
        return;
    }
    if (eng_renderer_pipeline_is_valid(renderer, scene->model_pipeline) &&
        eng_model_gpu_create(&scene->model, renderer, permanent, &model, anisotropy)) {
        scene->has_model = true;
        if (scene->has_environment) {
            (void)eng_model_gpu_set_environment(&scene->model, renderer, &scene->environment);
        }
        ENG_LOG_INFO("modèle chargé : %s (%u sommets, %u triangles, %u partie(s))", MODEL_PATH,
                     model.vertex_count, model.index_count / 3U, model.part_count);
    } else {
        eng_arena_reset_to(permanent, permanent_mark);
    }
    eng_model_free(&model);
    eng_arena_reset_to(level, level_mark);
}

/* Le sol : [-28, 28] x [-28, 28] m, dessus à y = -0,5 (base des cubes). */
#define GROUND_HALF 28.0F
#define GROUND_TOP (-0.5F)
#define SHADOW_RADIUS 42.0F /* contient le sol (coin à 39,6 m) */

/* Le sol et l'ombre sont facultatifs : sans eux la scène est dessinée comme avant. */
static void load_shadow(DemoScene *scene, EngRenderer *renderer, EngArena *permanent,
                        EngArena *level)
{
    if (!eng_renderer_pipeline_is_valid(renderer, scene->model_pipeline) ||
        eng_renderer_begin_shadow_queue(renderer) == NULL) {
        ENG_LOG_WARN("ombres : pipeline du modèle ou file d'ombre indisponible, pas d'ombre");
        return;
    }
    const EngArenaMark level_mark = eng_arena_mark(level);
    const EngArenaMark permanent_mark = eng_arena_mark(permanent);
    const float lo[3] = {-GROUND_HALF, GROUND_TOP - 1.0F, -GROUND_HALF};
    const float hi[3] = {GROUND_HALF, GROUND_TOP, GROUND_HALF};
    const EngModelMaterial material = {.base_color = {0.55F, 0.55F, 0.55F, 1.0F},
                                       .image = -1,
                                       .metallic = 0.0F,
                                       .roughness = 0.9F,
                                       .metallic_roughness_image = -1,
                                       .normal_scale = 1.0F,
                                       .normal_image = -1,
                                       .occlusion_strength = 1.0F,
                                       .occlusion_image = -1,
                                       .emissive_image = -1};
    EngModel ground;
    if (eng_model_make_box(level, lo, hi, &material, &ground)) {
        scene->has_ground =
            eng_model_gpu_create(&scene->ground, renderer, permanent, &ground, 0.0F);
        if (scene->has_ground && scene->has_environment) {
            (void)eng_model_gpu_set_environment(&scene->ground, renderer, &scene->environment);
        }
    }
    eng_arena_reset_to(level, level_mark);
    if (!scene->has_ground) {
        eng_arena_reset_to(permanent, permanent_mark);
        ENG_LOG_WARN("ombres : sol impossible, pas d'ombre");
        return;
    }
    EngPipelineDesc desc;
    eng_model_shadow_pipeline_desc(&desc);
    scene->model_shadow_pipeline = eng_renderer_create_pipeline(renderer, &desc);
    desc = (EngPipelineDesc){
        .name = "cube-shadow",
        .vertex_shader = "shadow.vert",
        .fragment_shader = "shadow.frag",
        .vertex_stride = sizeof(Vertex),
        .attribute_count = 1,
        .cull_mode = ENG_CULL_NONE,
        .depth_test = true,
        .depth_write = true,
        .depth_only = true,
    };
    desc.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, position)};
    scene->cube_shadow_pipeline = eng_renderer_create_pipeline(renderer, &desc);
    scene->shadow_size = DEMO_SHADOW_SIZE;
    scene->shadow_map = eng_renderer_create_shadow_map(renderer, scene->shadow_size);
    scene->has_shadow = eng_renderer_pipeline_is_valid(renderer, scene->model_shadow_pipeline) &&
                        eng_renderer_pipeline_is_valid(renderer, scene->cube_shadow_pipeline) &&
                        eng_renderer_texture_is_valid(renderer, scene->shadow_map);
    if (!scene->has_shadow) {
        ENG_LOG_WARN("ombres indisponibles : shaders shadow.* ou carte de profondeur D32F");
    } else {
        ENG_LOG_INFO("ombres : carte de %u x %u, sol de %.0f m de côté", scene->shadow_size,
                     scene->shadow_size, (double)(2.0F * GROUND_HALF));
    }
}

/* Le ciel est facultatif : sans lui, pas de skybox et le modèle garde son ciel blanc uniforme. */
static void load_environment(DemoScene *scene, EngRenderer *renderer, const DemoOptions *options)
{
    EngPipelineDesc desc;
    eng_sky_pipeline_desc(&desc);
    scene->sky_pipeline = eng_renderer_create_pipeline(renderer, &desc);
    if (!eng_renderer_pipeline_is_valid(renderer, scene->sky_pipeline)) {
        scene->sky_pipeline = ENG_PIPELINE_NULL;
        ENG_LOG_WARN("ciel : shaders sky.* indisponibles, pas de skybox");
        return;
    }
    /* Le disque du ciel est dans la direction du soleil de l'éclairage par défaut (azimut 30°,
     * hauteur 45°) : il ne suit pas les curseurs du soleil (docs/ibl.md). */
    EngLighting light;
    eng_lighting_default(&light);
    EngSkyDesc sky;
    eng_ibl_sky_default(&sky);
    for (int c = 0; c < 3; c++) {
        sky.sun_direction[c] = light.sun_direction[c];
    }
    EngEnvironmentDesc env = {.sky = &sky, .samples = options->environment_samples};
    float *hdr = NULL;
    if (options->environment_path != NULL &&
        eng_environment_load_hdr(options->environment_path, &hdr, &env.equirect_width,
                                 &env.equirect_height)) {
        env.equirect_rgb = hdr;
    }
    scene->has_environment = eng_environment_create(&scene->environment, renderer, &env);
    eng_environment_free_hdr(hdr);
    if (!scene->has_environment) {
        (void)eng_renderer_destroy_pipeline(renderer, scene->sky_pipeline);
        scene->sky_pipeline = ENG_PIPELINE_NULL;
    }
}

bool demo_scene_init(DemoScene *scene, EngRenderer *renderer, EngArena *permanent, EngArena *level,
                     EngArena *scratch, const DemoOptions *options)
{
    *scene = (DemoScene){0};
    Vertex vertices[CUBE_VERTICES];
    uint16_t indices[CUBE_INDICES];
    build_cube(vertices, indices);

    const EngBufferDesc vertex_buffer = {.name = "Demo cube vertices",
                                         .usage = ENG_BUFFER_VERTEX,
                                         .size = sizeof vertices,
                                         .data = vertices};
    const EngBufferDesc index_buffer = {.name = "Demo cube indices",
                                        .usage = ENG_BUFFER_INDEX,
                                        .size = sizeof indices,
                                        .data = indices};
    scene->vertices = eng_renderer_create_buffer(renderer, &vertex_buffer);
    scene->indices = eng_renderer_create_buffer(renderer, &index_buffer);

    /* Texture : image PNG décodée par stb_image, envoyée avec ses mipmaps (ou sans : --no-mipmaps)
     */
    EngImage image;
    if (eng_image_load(scratch, CHECKER_PATH, &image)) {
        const EngTextureDesc desc = {.name = CHECKER_PATH,
                                     .width = image.width,
                                     .height = image.height,
                                     .format = ENG_TEXTURE_RGBA8_SRGB,
                                     .pixels = image.pixels,
                                     .mip_levels = options->mipmaps ? 0 : 1};
        scene->texture = eng_renderer_create_texture(renderer, &desc);
        eng_image_free(&image);
    } else {
        ENG_LOG_ERROR("texture de démonstration introuvable (%s) : texture de remplacement",
                      CHECKER_PATH);
        scene->texture = make_missing_texture(renderer);
    }
    const EngSamplerDesc sampler = {.name = "demo",
                                    .mip_filter = options->mipmaps ? ENG_MIP_LINEAR : ENG_MIP_OFF,
                                    .max_anisotropy = options->anisotropy};
    scene->sampler = eng_renderer_create_sampler(renderer, &sampler);

    EngPipelineDesc pipeline = {
        .name = "cube",
        .vertex_shader = "textured.vert",
        .fragment_shader = "textured.frag",
        .vertex_stride = sizeof(Vertex),
        .attribute_count = 3,
        .depth_test = true,
        .depth_write = true,
    };
    pipeline.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, position)};
    pipeline.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, tint)};
    pipeline.attributes[2] = (EngVertexAttribute){
        .location = 2, .format = ENG_VERTEX_FLOAT2, .offset = offsetof(Vertex, uv)};
    scene->pipeline = eng_renderer_create_pipeline(renderer, &pipeline);

    if (!eng_renderer_buffer_is_valid(renderer, scene->vertices) ||
        !eng_renderer_buffer_is_valid(renderer, scene->indices) ||
        !eng_renderer_texture_is_valid(renderer, scene->texture) ||
        !eng_renderer_sampler_is_valid(renderer, scene->sampler) ||
        !eng_renderer_pipeline_is_valid(renderer, scene->pipeline)) {
        demo_scene_shutdown(scene, renderer);
        return false;
    }
    load_environment(scene, renderer, options);
    EngPipelineDesc model_desc;
    eng_model_pipeline_desc(&model_desc);
    scene->model_pipeline = eng_renderer_create_pipeline(renderer, &model_desc);
    load_model(scene, renderer, permanent, level, scratch, options->anisotropy);
    load_shadow(scene, renderer, permanent, level);
    /* Debug draw : facultatif (shaders de lignes absents ou arène pleine : on s'en passe). */
    const EngArenaMark lines_mark = eng_arena_mark(permanent);
    scene->has_lines = eng_debug_lines_init(&scene->lines, permanent, DEBUG_MAX_LINES) &&
                       eng_debug_lines_gpu_create(&scene->lines_gpu, renderer, DEBUG_MAX_LINES, 0);
    if (!scene->has_lines) {
        eng_arena_reset_to(permanent, lines_mark);
        scene->lines = (EngDebugLines){0};
        ENG_LOG_WARN("debug draw indisponible : l'image sera dessinée sans lui");
    }
    return true;
}

#define GRID_HALF 3       /* la grille va de -3 à +3 sur X et Z */
#define GRID_SPACING 8.0F /* mètres entre deux cubes */

#define MODEL_SPIN 0.5F /* rad/s : rotation du modèle central autour de Y */

/* Matrice du modèle central (rotation seule : il est à l'origine). */
static void model_matrix(double time_seconds, mat4 out)
{
    glm_mat4_identity(out);
    glm_rotate(out, (float)time_seconds * MODEL_SPIN, ENG_AXIS_UP);
}

static void view_projection(const EngCamera *camera, float aspect, mat4 out)
{
    mat4 view;
    mat4 proj;
    eng_camera_view(camera, view);
    eng_camera_projection(camera, aspect, proj);
    glm_mat4_mul(proj, view, out);
}

/* Ajoute à la file d'ombre les casters de la scène : le sol, la grille de cubes et le modèle (ou le
 * cube central). `light_view_proj` : monde -> découpage de la caméra du soleil. */
static void draw_casters(const DemoScene *scene, EngRenderQueue *shadow_queue, double time_seconds,
                         mat4 light_view_proj)
{
    const float angle = (float)time_seconds;
    mat4 identity;
    glm_mat4_identity(identity);
    (void)eng_model_gpu_draw_depth(&scene->ground, shadow_queue, scene->model_shadow_pipeline,
                                   identity, light_view_proj);
    const EngDrawCmd cmd = {
        .pipeline = scene->cube_shadow_pipeline,
        .vertex_buffer = scene->vertices,
        .index_buffer = scene->indices,
        .index_type = ENG_INDEX_U16,
        .count = CUBE_INDICES,
    };
    for (int gz = -GRID_HALF; gz <= GRID_HALF; gz++) {
        for (int gx = -GRID_HALF; gx <= GRID_HALF; gx++) {
            mat4 model;
            mat4 mvp;
            glm_translate_make(model,
                               (vec3){(float)gx * GRID_SPACING, 0.0F, (float)gz * GRID_SPACING});
            if (gx == 0 && gz == 0) {
                if (scene->has_model) {
                    model_matrix(time_seconds, model);
                    (void)eng_model_gpu_draw_depth(&scene->model, shadow_queue,
                                                   scene->model_shadow_pipeline, model,
                                                   light_view_proj);
                    continue;
                }
                glm_rotate(model, angle * 0.8F, ENG_AXIS_UP);
                glm_rotate(model, angle * 0.5F, ENG_AXIS_RIGHT);
            }
            glm_mat4_mul(light_view_proj, model, mvp);
            (void)eng_render_draw_uniforms(shadow_queue, &cmd, mvp, sizeof mvp);
        }
    }
}

void demo_scene_draw(const DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
                     double time_seconds, const EngCamera *camera, float aspect,
                     const EngLighting *lighting, const EngShadowParams *shadow)
{
    const float angle = (float)time_seconds;
    EngLighting light;
    if (lighting != NULL) {
        light = *lighting;
    } else {
        eng_lighting_default(&light);
    }
    (void)eng_lighting_sanitize(&light);
    for (int c = 0; c < 3; c++) {
        light.camera_position[c] = camera->position[c];
    }
    mat4 view_proj;
    view_projection(camera, aspect, view_proj);

    /* ombre : la caméra du soleil, la file d'ombre, et ce que reçoivent le sol et le modèle ; sans
     * ombre, la carte est retirée du renderer et aucune passe d'ombre n'est ouverte */
    EngModelShadow receiver = {0};
    const EngModelShadow *receiving = NULL;
    EngRenderQueue *shadow_queue = NULL;
    if (shadow != NULL && scene->has_shadow) {
        static const float center[3] = {0.0F, 0.0F, 0.0F};
        shadow_queue = eng_renderer_begin_shadow_queue(renderer);
        if (shadow_queue != NULL && eng_shadow_fit(&receiver.shadow, light.sun_direction, center,
                                                   SHADOW_RADIUS, scene->shadow_size)) {
            receiver.map = scene->shadow_map;
            receiver.params = *shadow;
            receiving = &receiver;
            draw_casters(scene, shadow_queue, time_seconds, receiver.shadow.view_proj);
        }
    }
    eng_renderer_set_shadow_map(renderer, receiving != NULL ? scene->shadow_map : ENG_TEXTURE_NULL);

    /* le ciel d'abord : sans profondeur, tout ce qui suit le recouvre */
    if (scene->has_environment) {
        (void)eng_environment_draw_sky(&scene->environment, queue, scene->sky_pipeline, view_proj,
                                       camera->position, light.ambient_intensity);
    }
    if (scene->has_ground) {
        mat4 identity;
        glm_mat4_identity(identity);
        (void)eng_model_gpu_draw_shadowed(&scene->ground, queue, scene->model_pipeline, identity,
                                          view_proj, &light, receiving);
    }

    EngDrawCmd cmd = {
        .pipeline = scene->pipeline,
        .vertex_buffer = scene->vertices,
        .index_buffer = scene->indices,
        .index_type = ENG_INDEX_U16,
        .count = CUBE_INDICES,
        .texture = scene->texture,
        .sampler = scene->sampler,
    };
    for (int gz = -GRID_HALF; gz <= GRID_HALF; gz++) {
        for (int gx = -GRID_HALF; gx <= GRID_HALF; gx++) {
            mat4 model;
            mat4 mvp;
            glm_translate_make(model,
                               (vec3){(float)gx * GRID_SPACING, 0.0F, (float)gz * GRID_SPACING});
            if (gx == 0 && gz == 0) {
                if (scene->has_model) {
                    model_matrix(time_seconds, model);
                    (void)eng_model_gpu_draw_shadowed(&scene->model, queue, scene->model_pipeline,
                                                      model, view_proj, &light, receiving);
                    continue;
                }
                glm_rotate(model, angle * 0.8F, ENG_AXIS_UP);
                glm_rotate(model, angle * 0.5F, ENG_AXIS_RIGHT);
            }
            glm_mat4_mul(view_proj, model, mvp);
            (void)eng_render_draw_uniforms(queue, &cmd, mvp, sizeof mvp);
        }
    }
}

void demo_scene_draw_debug(DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
                           double time_seconds, const EngCamera *camera, float aspect)
{
    if (!scene->has_lines) {
        return;
    }
    EngDebugLines *lines = &scene->lines;
    eng_debug_lines_clear(lines);
    /* testées en profondeur : le sol (sous les cubes, dont la base est à y = -0,5) et les sphères
     */
    (void)eng_debug_grid(lines, (vec3){0.0F, -0.5F, 0.0F}, 30, 2.0F, ENG_DEBUG_GRAY,
                         ENG_DEBUG_WHITE);
    for (int gz = -GRID_HALF; gz <= GRID_HALF; gz += 2 * GRID_HALF) {
        for (int gx = -GRID_HALF; gx <= GRID_HALF; gx += 2 * GRID_HALF) {
            (void)eng_debug_sphere(lines,
                                   (vec3){(float)gx * GRID_SPACING, 0.0F, (float)gz * GRID_SPACING},
                                   1.5F, ENG_DEBUG_MAGENTA, 24);
        }
    }
    if (scene->has_model) {
        mat4 model;
        model_matrix(time_seconds, model);
        vec3 center;
        vec3 half;
        for (int axis = 0; axis < 3; axis++) {
            center[axis] = 0.5F * (scene->model.bounds_min[axis] + scene->model.bounds_max[axis]);
            half[axis] = 0.5F * (scene->model.bounds_max[axis] - scene->model.bounds_min[axis]);
        }
        mat4 box;
        glm_translate_to(model, center, box);
        (void)eng_debug_box(lines, box, half, ENG_DEBUG_YELLOW);
        /* repère du modèle, superposé : on voit son orientation à travers la matière */
        eng_debug_lines_set_overlay(lines, true);
        (void)eng_debug_axes(lines, model, 2.0F);
        eng_debug_lines_set_overlay(lines, false);
    }
    /* repère du monde à l'origine du sol, superposé */
    eng_debug_lines_set_overlay(lines, true);
    mat4 world;
    glm_translate_make(world, (vec3){0.0F, -0.5F, 0.0F});
    (void)eng_debug_axes(lines, world, 4.0F);
    eng_debug_lines_set_overlay(lines, false);

    mat4 view_proj;
    view_projection(camera, aspect, view_proj);
    (void)eng_debug_lines_gpu_draw(&scene->lines_gpu, renderer, queue, lines, view_proj);
}

void demo_scene_shutdown(DemoScene *scene, EngRenderer *renderer)
{
    if (scene->has_lines) {
        eng_debug_lines_gpu_destroy(&scene->lines_gpu, renderer);
    }
    if (scene->has_model) {
        eng_model_gpu_destroy(&scene->model, renderer);
    }
    if (scene->has_ground) {
        eng_model_gpu_destroy(&scene->ground, renderer);
    }
    if (scene->has_shadow) {
        eng_renderer_set_shadow_map(renderer, ENG_TEXTURE_NULL);
    }
    (void)eng_renderer_destroy_texture(renderer, scene->shadow_map);
    (void)eng_renderer_destroy_pipeline(renderer, scene->cube_shadow_pipeline);
    (void)eng_renderer_destroy_pipeline(renderer, scene->model_shadow_pipeline);
    if (scene->has_environment) {
        eng_environment_destroy(&scene->environment, renderer);
    }
    (void)eng_renderer_destroy_pipeline(renderer, scene->sky_pipeline);
    (void)eng_renderer_destroy_pipeline(renderer, scene->model_pipeline);
    (void)eng_renderer_destroy_pipeline(renderer, scene->pipeline);
    (void)eng_renderer_destroy_sampler(renderer, scene->sampler);
    (void)eng_renderer_destroy_texture(renderer, scene->texture);
    (void)eng_renderer_destroy_buffer(renderer, scene->indices);
    (void)eng_renderer_destroy_buffer(renderer, scene->vertices);
    *scene = (DemoScene){0};
}
