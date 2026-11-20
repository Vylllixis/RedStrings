#include "demo_scene.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/image.h"
#include "engine/instancing.h"
#include "engine/log.h"
#include "engine/model.h"
#include "engine/grass.h"
#include "engine/placement.h"
#include "engine/rock.h"
#include "engine/math.h"

#define CHECKER_PATH "assets/textures/checker.png"
#define DEBUG_MAX_LINES 4096U /* lignes par liste : 2 × 4096 × 32 octets côté CPU */

/* Position (x, y, z) en mètres, teinte (r, g, b) et coordonnées de texture (u, v). Une teinte par
 * face : 4 sommets par face, donc 24 sommets, et 2 triangles par face, soit 36 index. */
/* Uniforms de textured_lit.vert (cubes de la démonstration, M8-07). */
typedef struct CubeUniforms {
    float mvp[16];
    float light[4];
} CubeUniforms;

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

#define MODEL_PATH "assets/cooked/models/demo.mesh"
#define ASSET_CAPACITY 64U

/* Le modèle cuit (tools/assetc, M2-04) est chargé par le gestionnaire d'assets (M2-05) : aucun
 * parsing glTF à l'exécution. Il est facultatif : sans lui (fichier absent, shader absent), le cube
 * central reste. */
static void load_model(DemoScene *scene, EngRenderer *renderer, float anisotropy)
{
    if (!eng_renderer_pipeline_is_valid(renderer, scene->model_pipeline) ||
        !eng_assets_init(&scene->assets, ASSET_CAPACITY)) {
        return;
    }
    scene->has_assets = true;
    scene->model_context = (EngModelAssetContext){
        .renderer = renderer,
        .anisotropy = anisotropy,
        .environment = scene->has_environment ? &scene->environment : NULL,
    };
    scene->model_type = eng_model_assets_register(&scene->assets, &scene->model_context);
    if (scene->model_type == 0) {
        return;
    }
    scene->model_handle = eng_assets_acquire(&scene->assets, scene->model_type, MODEL_PATH);
    scene->model = eng_model_asset_gpu(&scene->assets, scene->model_type, scene->model_handle);
    if (scene->model == NULL) {
        ENG_LOG_ERROR("modèle de démonstration introuvable ou invalide (%s) : cube seul",
                      MODEL_PATH);
        return;
    }
    scene->has_model = true;
    ENG_LOG_INFO("modèle chargé : %s (%u partie(s))", MODEL_PATH, scene->model->part_count);
}

/* --watch-assets : le modèle de démonstration vient d'un glTF (+ .bin + image) cuit par `assetc` ;
 * quand l'un de ces trois fichiers change dans assets/, on recuit le .mesh, ce que le gestionnaire
 * remarque ensuite pour recharger le modèle. Seulement si le lanceur sait où est assetc. */
#ifdef OW_ASSETC_PATH
static const char *const COOK_SOURCES[3] = {"models/demo.gltf", "models/demo.bin",
                                            "textures/checker.png"};

static void start_cooking(DemoScene *scene)
{
    EngCookRule rules[3];
    if (!eng_path_resolve(scene->cook_output, sizeof scene->cook_output, MODEL_PATH)) {
        return;
    }
    for (uint32_t i = 0; i < 3U; i++) {
        SDL_snprintf(scene->cook_sources[i], sizeof scene->cook_sources[i], "%s/%s",
                     OW_ASSET_SOURCE_DIR, COOK_SOURCES[i]);
    }
    scene->cook_argv[0] = OW_ASSETC_PATH;
    scene->cook_argv[1] = "mesh";
    scene->cook_argv[2] = scene->cook_sources[0]; /* le glTF : l'entrée de la commande */
    scene->cook_argv[3] = scene->cook_output;
    scene->cook_argv[4] = NULL;
    for (uint32_t i = 0; i < 3U; i++) {
        rules[i] = (EngCookRule){
            .name = COOK_SOURCES[i], .source = scene->cook_sources[i], .argv = scene->cook_argv};
    }
    scene->has_cook = eng_asset_cook_init(&scene->cook, rules, 3U, ENG_FILE_WATCH_DEFAULT_INTERVAL);
    if (scene->has_cook) {
        eng_assets_set_hot_reload(&scene->assets, true, ENG_FILE_WATCH_DEFAULT_INTERVAL);
        ENG_LOG_INFO("rechargement à chaud : sources surveillées dans %s", OW_ASSET_SOURCE_DIR);
    }
}
#endif

/* ── Monde édité ── */

static const EngModelGpu *world_model(const DemoScene *scene, const char *path)
{
    for (uint32_t i = 0; i < scene->world_model_count; i++) {
        if (strcmp(scene->world_models[i].path, path) == 0) {
            return eng_model_asset_gpu(&scene->assets, scene->model_type,
                                       scene->world_models[i].handle);
        }
    }
    return NULL;
}

/* Demande au gestionnaire les modèles que les entités citent et qu'on n'a pas encore vus. */
static void resolve_world_models(DemoScene *scene)
{
    if (scene->world == NULL || !scene->has_assets || scene->model_type == 0) {
        return;
    }
    const uint32_t count = eng_world_table_count(scene->world, ENG_COMPONENT_MESH_RENDERER);
    for (uint32_t i = 0; i < count; i++) {
        const EngMeshRenderer *renderer =
            eng_world_table_data(scene->world, ENG_COMPONENT_MESH_RENDERER, i);
        if (renderer->model[0] == '\0') {
            continue;
        }
        bool known = false;
        for (uint32_t k = 0; k < scene->world_model_count && !known; k++) {
            known = strcmp(scene->world_models[k].path, renderer->model) == 0;
        }
        if (known) {
            continue;
        }
        if (scene->world_model_count >= DEMO_WORLD_MODELS) {
            if (!scene->world_models_full_warned) {
                ENG_LOG_WARN("monde : plus de %u modèles distincts, les suivants ne sont pas "
                             "dessinés",
                             (unsigned)DEMO_WORLD_MODELS);
                scene->world_models_full_warned = true;
            }
            continue;
        }
        DemoWorldModel *entry = &scene->world_models[scene->world_model_count++];
        (void)snprintf(entry->path, sizeof entry->path, "%s", renderer->model);
        entry->handle = eng_assets_acquire(&scene->assets, scene->model_type, entry->path);
    }
}

void demo_scene_set_world(DemoScene *scene, EngWorld *world)
{
    scene->world = world;
}

bool demo_scene_entity_bounds(void *user, EngEntity entity, float min[3], float max[3])
{
    const DemoScene *scene = user;
    if (scene->world == NULL) {
        return false;
    }
    const EngMeshRenderer *renderer =
        eng_world_get(scene->world, entity, ENG_COMPONENT_MESH_RENDERER);
    const EngModelGpu *model = renderer != NULL ? world_model(scene, renderer->model) : NULL;
    if (model == NULL) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        min[i] = model->bounds_min[i];
        max[i] = model->bounds_max[i];
    }
    return true;
}

/* Les entités visibles du monde : `casters` seulement celles qui projettent une ombre (file
 * d'ombre), sinon dessin éclairé. */
static void draw_world(const DemoScene *scene, EngRenderQueue *queue, mat4 view_proj,
                       const EngLighting *light, const EngModelShadow *receiving, bool casters)
{
    if (scene->world == NULL) {
        return;
    }
    const uint32_t count = eng_world_table_count(scene->world, ENG_COMPONENT_MESH_RENDERER);
    for (uint32_t i = 0; i < count; i++) {
        const EngMeshRenderer *renderer =
            eng_world_table_data(scene->world, ENG_COMPONENT_MESH_RENDERER, i);
        const EngModelGpu *model = world_model(scene, renderer->model);
        const EngEntity entity =
            eng_world_table_entity(scene->world, ENG_COMPONENT_MESH_RENDERER, i);
        mat4 matrix;
        if (!renderer->visible || model == NULL ||
            !eng_world_world_matrix(scene->world, entity, matrix)) {
            continue;
        }
        if (casters) {
            if (renderer->cast_shadow) {
                (void)eng_model_gpu_draw_depth(model, queue, scene->model_shadow_pipeline, matrix,
                                               view_proj);
            }
        } else {
            (void)eng_model_gpu_draw_shadowed(model, queue, scene->model_pipeline, matrix,
                                              view_proj, light, receiving);
        }
    }
}

void demo_scene_update(DemoScene *scene, double now_seconds)
{
    resolve_world_models(scene);
    if (scene->has_cook) {
        (void)eng_asset_cook_poll(&scene->cook, now_seconds);
    }
    if (scene->has_assets) {
        (void)eng_assets_poll(&scene->assets, now_seconds);
    }
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
    /* Ciel analytique (M3-05) sauf si une image HDR est demandée : le shader atmosphere.frag
     * redessine le ciel à chaque image avec la direction courante du soleil. L'éclairage par
     * image (reflets, ambiante), lui, est CUIT une fois au chargement pour le soleil initial
     * (docs/atmosphere.md). Sans le shader : la skybox cubemap, dont le disque ne suit pas le
     * soleil (docs/ibl.md). */
    EngLighting light;
    eng_lighting_default(&light);
    (void)eng_lighting_set_sun_angles(&light, options->sun_azimuth, options->sun_elevation);
    EngSkyDesc sky;
    eng_ibl_sky_default(&sky);
    if (options->environment_path == NULL) {
        EngPipelineDesc atmosphere_desc;
        eng_atmosphere_pipeline_desc(&atmosphere_desc);
        scene->atmosphere_pipeline = eng_renderer_create_pipeline(renderer, &atmosphere_desc);
        eng_atmosphere_default(&scene->atmosphere);
        eng_fog_default(&scene->fog);
        if (options->fog_density >= 0.0F) {
            scene->fog.density = options->fog_density;
        }
        scene->has_atmosphere =
            eng_renderer_pipeline_is_valid(renderer, scene->atmosphere_pipeline) &&
            eng_atmosphere_to_sky_desc(&scene->atmosphere, light.sun_direction, &sky);
        if (!scene->has_atmosphere) {
            (void)eng_renderer_destroy_pipeline(renderer, scene->atmosphere_pipeline);
            scene->atmosphere_pipeline = ENG_PIPELINE_NULL;
            eng_ibl_sky_default(&sky);
            ENG_LOG_WARN("ciel : shader atmosphere.frag indisponible, skybox cubemap");
        }
    }
    if (!scene->has_atmosphere) {
        for (int c = 0; c < 3; c++) {
            sky.sun_direction[c] = light.sun_direction[c];
        }
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
        if (eng_renderer_pipeline_is_valid(renderer, scene->atmosphere_pipeline)) {
            (void)eng_renderer_destroy_pipeline(renderer, scene->atmosphere_pipeline);
        }
        scene->atmosphere_pipeline = ENG_PIPELINE_NULL;
        scene->has_atmosphere = false;
        (void)eng_renderer_destroy_pipeline(renderer, scene->sky_pipeline);
        scene->sky_pipeline = ENG_PIPELINE_NULL;
    }
}

/* mètres au-dessus du sol du centre : point de vue conseillé */
#define TERRAIN_VIEW_HEIGHT 40.0F

/* Le terrain est facultatif : un échec est journalisé et la démonstration reste affichée.
 * `level` reçoit le champ puis le maillage CPU (restitués ensuite), le modèle GPU va dans
 * `permanent`. */
/* Rochers (M3-07) : un maillage procédural et `count` instances au hasard sur le terrain (graine
 * fixe : mêmes rochers à chaque lancement), posées à la hauteur du champ. Facultatif : un échec est
 * journalisé et le terrain reste affiché. `map` doit être vivant ; `level` reçoit les temporaires.
 */
static void load_rocks(DemoScene *scene, EngRenderer *renderer, EngArena *permanent,
                       EngArena *level, const EngHeightmap *map, uint32_t count, float anisotropy)
{
    EngPipelineDesc pipeline;
    eng_instanced_pipeline_desc(&pipeline);
    scene->rock_pipeline = eng_renderer_create_pipeline(renderer, &pipeline);
    EngRockParams rock_params = eng_rock_default_params();
    rock_params.subdivisions = 1; /* 80 triangles : 50 000 rochers = 4 millions de triangles */
    EngModel model;
    EngInstance *instances = ENG_ARENA_NEW_ARRAY(level, EngInstance, count);
    bool ok = instances != NULL && eng_renderer_pipeline_is_valid(renderer, scene->rock_pipeline) &&
              eng_rock_build(level, &rock_params, &model) &&
              eng_model_gpu_create(&scene->rocks, renderer, permanent, &model, anisotropy);
    if (ok) {
        const float extent = eng_heightmap_extent(map);
        const float half = extent * 0.5F;
        uint32_t seed = 20260601U;
        for (uint32_t i = 0; i < count; i++) {
            float r[4];
            for (int k = 0; k < 4; k++) {
                seed = (seed * 1664525U) + 1013904223U;
                r[k] = (float)(seed >> 8) * (1.0F / 16777216.0F);
            }
            const float x = (0.02F + (0.96F * r[0])) * extent;
            const float z = (0.02F + (0.96F * r[1])) * extent;
            const float scale = 0.5F + (1.5F * r[2] * r[2]);
            const float grey = 0.75F + (0.25F * r[3]);
            const uint8_t shade = (uint8_t)(grey * 255.0F);
            const uint8_t tint[4] = {shade, shade, shade, 255};
            /* le terrain est en coordonnées monde centrées (origin = -half) */
            const float position[3] = {x - half, eng_heightmap_sample(map, x, z) - (0.15F * scale),
                                       z - half};
            eng_instance_make_yaw(&instances[i], position, scale, r[3] * 6.2831853F, tint);
        }
        scene->rock_instances = eng_instances_upload(renderer, instances, count);
        ok = eng_renderer_buffer_is_valid(renderer, scene->rock_instances);
        if (!ok) {
            eng_model_gpu_destroy(&scene->rocks, renderer);
        }
    }
    if (!ok) {
        (void)eng_renderer_destroy_pipeline(renderer, scene->rock_pipeline);
        scene->rock_pipeline = ENG_PIPELINE_NULL;
        ENG_LOG_ERROR("rochers : création impossible (%u demandés), le terrain reste affiché",
                      count);
        return;
    }
    scene->rock_count = count;
    scene->has_rocks = true;
}

/* Placement procédural (M3-08) : les rochers suivent la roche de la splatmap, la végétation (le
 * même petit maillage, teinte verte) l'herbe plate. Facultatif comme load_rocks. */
static bool upload_group(EngRenderer *renderer, EngInstance *list, uint32_t count,
                         EngBufferHandle *buffer)
{
    if (count == 0) {
        return true;
    }
    *buffer = eng_instances_upload(renderer, list, count);
    return eng_renderer_buffer_is_valid(renderer, *buffer);
}

static void load_placement(DemoScene *scene, EngRenderer *renderer, EngArena *permanent,
                           EngArena *level, const EngHeightmap *map, const EngSplatmap *splat,
                           uint32_t seed, float anisotropy)
{
    const float half = eng_heightmap_extent(map) * 0.5F;
    EngPipelineDesc pipeline;
    eng_instanced_pipeline_desc(&pipeline);
    scene->rock_pipeline = eng_renderer_create_pipeline(renderer, &pipeline);
    EngRockParams rock_params = eng_rock_default_params();
    rock_params.subdivisions = 1;
    EngModel model;
    EngPlacementRule rock_rule = eng_placement_rock_rule();
    EngPlacementRule veg_rule = eng_placement_vegetation_rule();
    rock_rule.seed = seed;
    veg_rule.seed = seed + 1U;
    EngInstance *rocks = NULL;
    EngInstance *vegetation = NULL;
    uint32_t rock_count = 0;
    uint32_t veg_count = 0;
    bool ok =
        eng_renderer_pipeline_is_valid(renderer, scene->rock_pipeline) &&
        eng_placement_generate(level, map, splat, &rock_rule, -half, -half, &rocks, &rock_count) &&
        eng_placement_generate(level, map, splat, &veg_rule, -half, -half, &vegetation,
                               &veg_count) &&
        eng_rock_build(level, &rock_params, &model) &&
        eng_model_gpu_create(&scene->rocks, renderer, permanent, &model, anisotropy);
    if (ok) {
        for (uint32_t i = 0; i < veg_count; i++) {
            const uint8_t shade = vegetation[i].tint[0];
            const uint8_t green[4] = {(uint8_t)(shade / 4U), shade, (uint8_t)(shade / 4U), 255};
            memcpy(vegetation[i].tint, green, sizeof green);
        }
        ok = upload_group(renderer, rocks, rock_count, &scene->rock_instances) &&
             upload_group(renderer, vegetation, veg_count, &scene->veg_instances);
        if (!ok) {
            (void)eng_renderer_destroy_buffer(renderer, scene->rock_instances);
            (void)eng_renderer_destroy_buffer(renderer, scene->veg_instances);
            scene->rock_instances = ENG_BUFFER_NULL;
            scene->veg_instances = ENG_BUFFER_NULL;
            eng_model_gpu_destroy(&scene->rocks, renderer);
        }
    }
    if (!ok) {
        (void)eng_renderer_destroy_pipeline(renderer, scene->rock_pipeline);
        scene->rock_pipeline = ENG_PIPELINE_NULL;
        ENG_LOG_ERROR("placement : création impossible, le terrain reste affiché");
        return;
    }
    scene->rock_count = rock_count;
    scene->veg_count = veg_count;
    scene->has_rocks = rock_count > 0;
    scene->has_veg = veg_count > 0;
    ENG_LOG_INFO("placement : %u rochers sur la roche, %u touffes sur l'herbe (graine %u)",
                 rock_count, veg_count, seed);
    if (!scene->has_rocks && !scene->has_veg) {
        eng_model_gpu_destroy(&scene->rocks, renderer);
        (void)eng_renderer_destroy_pipeline(renderer, scene->rock_pipeline);
        scene->rock_pipeline = ENG_PIPELINE_NULL;
    }
}

/* Herbe (M3-09) : un brin de 3 segments, le champ placé par la règle d'herbe sur la splatmap,
 * UN dessin par image avec le vent. Facultatif comme les rochers. */
static void load_grass(DemoScene *scene, EngRenderer *renderer, EngArena *permanent,
                       EngArena *level, const EngHeightmap *map, const EngSplatmap *splat,
                       uint32_t seed, float anisotropy)
{
    const float half = eng_heightmap_extent(map) * 0.5F;
    EngPipelineDesc pipeline;
    eng_grass_pipeline_desc(&pipeline);
    scene->grass_pipeline = eng_renderer_create_pipeline(renderer, &pipeline);
    EngPlacementRule rule = eng_placement_grass_rule();
    rule.seed = seed + 2U;
    EngModel blade;
    EngInstance *field = NULL;
    uint32_t count = 0;
    bool ok = eng_renderer_pipeline_is_valid(renderer, scene->grass_pipeline) &&
              eng_placement_generate(level, map, splat, &rule, -half, -half, &field, &count) &&
              count > 0 && eng_grass_blade_build(level, 3, 0.12F, NULL, &blade) &&
              eng_model_gpu_create(&scene->grass_blade, renderer, permanent, &blade, anisotropy);
    if (ok) {
        scene->grass_instances = eng_instances_upload(renderer, field, count);
        ok = eng_renderer_buffer_is_valid(renderer, scene->grass_instances);
        if (!ok) {
            eng_model_gpu_destroy(&scene->grass_blade, renderer);
        }
    }
    if (!ok) {
        (void)eng_renderer_destroy_pipeline(renderer, scene->grass_pipeline);
        scene->grass_pipeline = ENG_PIPELINE_NULL;
        ENG_LOG_ERROR("herbe : création impossible, le terrain reste affiché");
        return;
    }
    scene->grass_style = eng_grass_style_default();
    scene->grass_count = count;
    scene->has_grass = true;
    ENG_LOG_INFO("herbe : %u brins (%u triangles chacun), vent %.2f m, fondu %.0f à %.0f m", count,
                 eng_grass_blade_triangle_count(3), (double)scene->grass_style.wind_strength,
                 (double)scene->grass_style.fade_start, (double)scene->grass_style.fade_end);
}

/* Eau (M3-10) : un plan au niveau demandé, par défaut 20 % de la plage d'altitude du champ. Le
 * plan est opaque (pas de mélange alpha dans le renderer). Facultatif : sans ciel physique ou sans
 * cellule mouillée, le terrain reste affiché seul. */
static void load_water(DemoScene *scene, EngRenderer *renderer, EngArena *permanent,
                       EngArena *level, const EngHeightmap *map, const DemoOptions *options,
                       float anisotropy)
{
    const float half = eng_heightmap_extent(map) * 0.5F;
    const float default_level =
        map->altitude_min + (0.2F * (map->altitude_max - map->altitude_min));
    scene->water_params =
        eng_water_default(options->water_level_set ? options->water_level : default_level);
    EngPipelineDesc pipeline;
    eng_water_pipeline_desc(&pipeline);
    scene->water_pipeline = eng_renderer_create_pipeline(renderer, &pipeline);
    const EngArenaMark mark = eng_arena_mark(level);
    EngModel model;
    bool ok = eng_renderer_pipeline_is_valid(renderer, scene->water_pipeline) &&
              scene->has_atmosphere &&
              eng_water_build(level, map, &scene->water_params, -half, -half, &model) &&
              model.vertex_count > 0 &&
              eng_model_gpu_create(&scene->water_model, renderer, permanent, &model, anisotropy);
    eng_arena_reset_to(level, mark);
    if (!ok) {
        (void)eng_renderer_destroy_pipeline(renderer, scene->water_pipeline);
        scene->water_pipeline = ENG_PIPELINE_NULL;
        ENG_LOG_WARN(
            "eau : création impossible (ciel physique, shader ou aucune cellule sous %.1f m)",
            (double)scene->water_params.level);
        return;
    }
    scene->water_cells = eng_water_wet_cells(map, &scene->water_params);
    scene->has_water = true;
    ENG_LOG_INFO("eau : niveau %.1f m, %u cellules mouillées", (double)scene->water_params.level,
                 scene->water_cells);
}

static void load_terrain(DemoScene *scene, EngRenderer *renderer, EngArena *permanent,
                         EngArena *level, const char *path, float anisotropy,
                         const DemoOptions *options)
{
    const EngArenaMark level_mark = eng_arena_mark(level);
    const EngArenaMark permanent_mark = eng_arena_mark(permanent);
    EngHeightmap map;
    EngTerrain terrain;
    EngSplatmap splat;
    bool ok = eng_heightmap_load(level, path, &map);
    if (ok) {
        const float half = eng_heightmap_extent(&map) * 0.5F;
        const EngSplatParams splat_params = eng_splat_default_params();
        const EngModelMaterial material = eng_terrain_splat_material();
        const EngTerrainParams params = {
            .origin_x = -half, .origin_z = -half, .material = &material};
        EngPipelineDesc pipeline;
        eng_terrain_pipeline_desc(&pipeline);
        scene->terrain_pipeline = eng_renderer_create_pipeline(renderer, &pipeline);
        const EngTerrainGpuParams gpu_params = {.splat = &splat, .anisotropy = anisotropy};
        ok = eng_renderer_pipeline_is_valid(renderer, scene->terrain_pipeline) &&
             eng_splatmap_generate(level, &map, &splat_params, &splat) &&
             eng_terrain_build(level, &map, &params, &terrain) &&
             eng_terrain_gpu_create(&scene->terrain, renderer, permanent, level, &terrain,
                                    &gpu_params);
        if (ok) {
            scene->terrain_view[0] = 0.0F;
            scene->terrain_view[1] = eng_heightmap_sample(&map, half, half) + TERRAIN_VIEW_HEIGHT;
            scene->terrain_view[2] = 0.0F;
            ENG_LOG_INFO("terrain %s : %u x %u échantillons, %.0f m de côté, %u chunks", path,
                         map.size, map.size, (double)eng_heightmap_extent(&map),
                         terrain.chunks_per_side * terrain.chunks_per_side);
            if (options->water) {
                load_water(scene, renderer, permanent, level, &map, options, anisotropy);
            }
            if (options->grass) {
                load_grass(scene, renderer, permanent, level, &map, &splat, options->placement_seed,
                           anisotropy);
            }
            if (options->placement) {
                load_placement(scene, renderer, permanent, level, &map, &splat,
                               options->placement_seed, anisotropy);
            } else if (options->rock_count > 0) {
                load_rocks(scene, renderer, permanent, level, &map, options->rock_count,
                           anisotropy);
            }
            const uint32_t chunk_total = terrain.chunks_per_side * terrain.chunks_per_side;
            if (chunk_total <= DEMO_MAX_TERRAIN_CHUNKS) {
                memcpy(scene->terrain_bounds, terrain.chunks,
                       (size_t)chunk_total * sizeof(EngTerrainChunk));
                scene->terrain_chunk_total = chunk_total;
            }
        }
    }
    eng_arena_reset_to(level, level_mark);
    if (!ok) {
        eng_arena_reset_to(permanent, permanent_mark);
        (void)eng_renderer_destroy_pipeline(renderer, scene->terrain_pipeline);
        scene->terrain_pipeline = ENG_PIPELINE_NULL;
        ENG_LOG_ERROR("terrain %s : chargement impossible, la démonstration reste affichée", path);
        return;
    }
    scene->has_terrain = true;
    if (scene->has_environment) {
        (void)eng_terrain_gpu_set_environment(&scene->terrain, renderer, &scene->environment);
        if (scene->has_rocks) {
            (void)eng_model_gpu_set_environment(&scene->rocks, renderer, &scene->environment);
        }
    }
}

bool demo_scene_terrain_view(const DemoScene *scene, float position[3])
{
    if (!scene->has_terrain) {
        return false;
    }
    for (int c = 0; c < 3; c++) {
        position[c] = scene->terrain_view[c];
    }
    return true;
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
        .vertex_shader = "textured_lit.vert", /* M8-07 : assombris la nuit */
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
    load_model(scene, renderer, options->anisotropy);
#ifdef OW_ASSETC_PATH
    if (options->watch_assets && scene->has_model) {
        start_cooking(scene);
    } else if (options->watch_assets) {
        ENG_LOG_WARN("--watch-assets : aucun modèle chargé, rien à surveiller");
    }
#else
    if (options->watch_assets) {
        ENG_LOG_WARN("--watch-assets : ce lanceur ne connaît pas l'outil assetc");
    }
#endif
    load_shadow(scene, renderer, permanent, level);
    if (options->terrain_path != NULL) {
        load_terrain(scene, renderer, permanent, level, options->terrain_path, options->anisotropy,
                     options);
    }
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
    if (scene->world_mode) { /* M7 : seulement le monde streamé et les personnages */
        if (scene->model_hook != NULL) {
            scene->model_hook(scene->model_user, shadow_queue, light_view_proj, NULL, NULL, true);
        }
        return;
    }
    (void)eng_model_gpu_draw_depth(&scene->ground, shadow_queue, scene->model_shadow_pipeline,
                                   identity, light_view_proj);
    const EngDrawCmd cmd = {
        .pipeline = scene->cube_shadow_pipeline,
        .vertex_buffer = scene->vertices,
        .index_buffer = scene->indices,
        .index_type = ENG_INDEX_U16,
        .count = CUBE_INDICES,
    };
    const int casters_half = scene->hide_props ? -1 : GRID_HALF; /* -1 : boucle vide */
    for (int gz = -casters_half; gz <= casters_half; gz++) {
        for (int gx = -casters_half; gx <= casters_half; gx++) {
            mat4 model;
            mat4 mvp;
            glm_translate_make(model,
                               (vec3){(float)gx * GRID_SPACING, 0.0F, (float)gz * GRID_SPACING});
            if (gx == 0 && gz == 0) {
                if (scene->world != NULL) {
                    continue; /* le monde remplace le modèle central */
                }
                if (scene->has_model) {
                    model_matrix(time_seconds, model);
                    (void)eng_model_gpu_draw_depth(scene->model, shadow_queue,
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
    draw_world(scene, shadow_queue, light_view_proj, NULL, NULL, true);
    if (scene->model_hook != NULL) {
        scene->model_hook(scene->model_user, shadow_queue, light_view_proj, NULL, NULL, true);
    }
}

/* Clair de lune (M8-07) : quand le soleil est couché (poids `night` > 0), une lumière faible et
 * bleutée opposée au soleil (au moins 20° au-dessus de l'horizon) remplace le soleil s'il est
 * plus faible qu'elle, et l'ambiante reçoit un plancher bleuté : la nuit reste lisible. */
static void apply_moonlight(EngLighting *light, float night)
{
    if (!(night > 0.0F)) {
        return;
    }
    static const float tint[3] = {0.55F, 0.65F, 1.0F};
    const float w = night > 1.0F ? 1.0F : night;
    const float moon_intensity = 0.05F * w;
    if (light->sun_intensity < moon_intensity) {
        float m[3] = {-light->sun_direction[0], -light->sun_direction[1], -light->sun_direction[2]};
        if (m[1] < 0.34F) {
            m[1] = 0.34F;
        }
        const float len = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
        for (int c = 0; c < 3; c++) {
            light->sun_direction[c] = m[c] / len;
            light->sun_color[c] = tint[c];
        }
        light->sun_intensity = moon_intensity;
    }
    for (int c = 0; c < 3; c++) {
        const float floor_c = 0.03F * w * tint[c];
        if (light->ambient_color[c] < floor_c) {
            light->ambient_color[c] = floor_c;
        }
    }
}

bool demo_scene_use_cascades(DemoScene *scene, EngRenderer *renderer)
{
    if (scene == NULL || renderer == NULL || !scene->has_shadow) {
        return false;
    }
    const EngTextureHandle atlas =
        eng_renderer_create_shadow_map(renderer, 2U * scene->shadow_size);
    if (!eng_renderer_texture_is_valid(renderer, atlas)) {
        ENG_LOG_WARN("ombres en cascade indisponibles : atlas de %u texels refusé",
                     2U * scene->shadow_size);
        return false;
    }
    (void)eng_renderer_destroy_texture(renderer, scene->shadow_map);
    scene->shadow_map = atlas;
    scene->cascades = true;
    ENG_LOG_INFO("ombres : %u cascades jusqu'à %.0f m, atlas de %u x %u (cascades de %u texels)",
                 DEMO_CSM_COUNT, (double)DEMO_CSM_FAR, 2U * scene->shadow_size,
                 2U * scene->shadow_size, scene->shadow_size);
    return true;
}

void demo_scene_draw(DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
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
    /* ciel atmosphérique : la couleur du soleil, son intensité (0 sous l'horizon) et l'ambiante
     * suivent sa hauteur (engine/atmosphere.h) */
    float sky_intensity = light.ambient_intensity;
    if (scene->has_atmosphere) {
        float color[3];
        float visibility;
        float ambient[3];
        if (eng_atmosphere_sun_light(&scene->atmosphere, light.sun_direction, color, &visibility) &&
            eng_atmosphere_ambient(&scene->atmosphere, light.sun_direction, ambient)) {
            for (int c = 0; c < 3; c++) {
                light.sun_color[c] *= color[c];
                light.ambient_color[c] *= ambient[c];
                scene->last_sun_color[c] = color[c];
                scene->last_ambient[c] = ambient[c];
            }
            light.sun_intensity *= visibility;
            scene->last_sun_visibility = visibility;
            scene->last_sun_elevation_deg = asinf(light.sun_direction[1]) * (180.0F / 3.14159265F);
        }
    }
    /* le ciel garde la direction du vrai soleil ; la nuit, la lumière directe vient de la lune */
    float sky_sun[3] = {light.sun_direction[0], light.sun_direction[1], light.sun_direction[2]};
    apply_moonlight(&light, scene->night);
    mat4 view_proj;
    view_projection(camera, aspect, view_proj);

    /* ombre : la caméra du soleil, la file d'ombre, et ce que reçoivent le sol et le modèle ; sans
     * ombre, la carte est retirée du renderer et aucune passe d'ombre n'est ouverte */
    EngModelShadow receiver = {0};
    const EngModelShadow *receiving = NULL;
    EngRenderQueue *shadow_queue = NULL;
    if (shadow != NULL && scene->has_shadow && scene->world_mode && scene->cascades) {
        /* M7-09 : 4 cascades autour de la caméra, chacune dans son quart de l'atlas */
        EngCsmView cv = {.fov_y = camera->fov_y,
                         .aspect = aspect,
                         .near_plane = camera->near_z,
                         .far_plane = DEMO_CSM_FAR};
        for (int c = 0; c < 3; c++) {
            cv.position[c] = camera->position[c];
        }
        eng_camera_forward(camera, cv.forward);
        shadow_queue = eng_renderer_begin_shadow_queue(renderer);
        if (shadow_queue != NULL &&
            eng_csm_update(&scene->csm, &cv, light.sun_direction, DEMO_CSM_COUNT, DEMO_CSM_LAMBDA,
                           scene->shadow_size)) {
            receiver.map = scene->shadow_map;
            receiver.params = *shadow;
            receiver.shadow = scene->csm.maps[0];
            receiver.cascade_count = scene->csm.count;
            for (uint32_t k = 0; k < scene->csm.count && k < ENG_MODEL_SHADOW_CASCADES; k++) {
                receiver.cascades[k] = scene->csm.maps[k];
                const uint32_t first = shadow_queue->count;
                if (scene->model_hook != NULL) {
                    scene->model_hook(scene->model_user, shadow_queue, scene->csm.maps[k].view_proj,
                                      NULL, NULL, true);
                }
                for (uint32_t i = first; i < shadow_queue->count; i++) {
                    shadow_queue->commands[i].shadow_tile = (uint8_t)(k + 1U);
                }
            }
            receiving = &receiver;
        }
    } else if (shadow != NULL && scene->has_shadow && !scene->has_terrain) {
        static const float origin[3] = {0.0F, 0.0F, 0.0F};
        const float *center = scene->world_mode ? scene->shadow_focus : origin;
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
    if (scene->has_atmosphere && scene->has_environment) {
        /* M8-07 : étoiles et lune la nuit ; la lune est là où apply_moonlight met sa lumière */
        EngNightSky night_sky = {.stars = scene->night, .moon = scene->night};
        night_sky.moon_dir[0] = -sky_sun[0];
        night_sky.moon_dir[1] = fmaxf(-sky_sun[1], 0.34F);
        night_sky.moon_dir[2] = -sky_sun[2];
        (void)eng_atmosphere_draw_ex(&scene->atmosphere, queue, scene->atmosphere_pipeline,
                                     scene->environment.dummy_vertex, view_proj, camera->position,
                                     sky_sun, sky_intensity, &night_sky);
    } else if (scene->has_environment) {
        (void)eng_environment_draw_sky(&scene->environment, queue, scene->sky_pipeline, view_proj,
                                       camera->position, sky_intensity);
    }
    if (scene->has_terrain) {
        /* le terrain remplace la démonstration (sol, cubes, modèle, monde) */
        /* culling CPU (M3-04) : seuls les chunks dont la boîte touche la pyramide de vision */
        uint32_t visible[DEMO_MAX_TERRAIN_CHUNKS];
        const uint32_t total = scene->terrain_chunk_total;
        EngFrustum frustum;
        uint32_t drawn = 0;
        /* brouillard : sa couleur est celle du ciel à l'horizon, à l'exposition du ciel dessiné */
        if (scene->has_atmosphere) {
            (void)eng_terrain_gpu_set_fog(&scene->terrain, &scene->fog, &scene->atmosphere,
                                          sky_intensity);
            eng_fog_set_sun(scene->terrain.fog, sky_sun); /* M8-07 : pas la lune */
        }
        if (total > 0 && eng_frustum_from_view_proj(&frustum, view_proj)) {
            drawn = eng_chunks_visible(scene->terrain_bounds, total, &frustum, visible,
                                       DEMO_MAX_TERRAIN_CHUNKS);
            if (drawn > 0) {
                (void)eng_terrain_gpu_draw(&scene->terrain, queue, scene->terrain_pipeline,
                                           view_proj, &light, NULL, visible, drawn);
            }
        } else {
            (void)eng_terrain_gpu_draw(&scene->terrain, queue, scene->terrain_pipeline, view_proj,
                                       &light, NULL, NULL, 0);
            drawn = total;
        }
        scene->terrain_chunks_drawn = drawn;
        if (scene->has_water && scene->has_atmosphere) {
            EngSkyDesc sky;
            EngWaterSky water_sky;
            float tail[ENG_FOG_UNIFORM_FLOATS + ENG_WATER_UNIFORM_FLOATS];
            if (eng_atmosphere_to_sky_desc(&scene->atmosphere, light.sun_direction, &sky)) {
                for (int c = 0; c < 3; c++) {
                    water_sky.horizon[c] = sky.horizon[c] * sky_intensity;
                    water_sky.zenith[c] = sky.zenith[c] * sky_intensity;
                }
                memcpy(tail, scene->terrain.fog, ENG_FOG_UNIFORM_FLOATS * sizeof(float));
                if (eng_water_pack(&scene->water_params, &water_sky, time_seconds,
                                   tail + ENG_FOG_UNIFORM_FLOATS)) {
                    mat4 identity;
                    glm_mat4_identity(identity);
                    const EngModelDrawOptions water_options = {
                        .fragment_tail = tail,
                        .fragment_tail_floats = ENG_FOG_UNIFORM_FLOATS + ENG_WATER_UNIFORM_FLOATS};
                    (void)eng_model_gpu_draw_ex(&scene->water_model, queue, scene->water_pipeline,
                                                identity, view_proj, &light, NULL, &water_options);
                }
            }
        }
        if (scene->has_grass) {
            float tail[ENG_GRASS_UNIFORM_FLOATS];
            if (eng_grass_pack(&scene->grass_style, camera->position, time_seconds, tail)) {
                mat4 identity;
                glm_mat4_identity(identity);
                const EngModelDrawOptions grass_options = {
                    .instance_buffer = scene->grass_instances,
                    .instance_count = scene->grass_count,
                    .vertex_tail = tail,
                    .vertex_tail_floats = ENG_GRASS_UNIFORM_FLOATS};
                (void)eng_model_gpu_draw_ex(&scene->grass_blade, queue, scene->grass_pipeline,
                                            identity, view_proj, &light, NULL, &grass_options);
            }
        }
        if (scene->has_veg) {
            mat4 identity;
            glm_mat4_identity(identity);
            const EngModelDrawOptions veg_options = {.instance_buffer = scene->veg_instances,
                                                     .instance_count = scene->veg_count};
            (void)eng_model_gpu_draw_ex(&scene->rocks, queue, scene->rock_pipeline, identity,
                                        view_proj, &light, NULL, &veg_options);
        }
        if (scene->has_rocks) {
            /* tous les rochers en UN dessin ; le GPU élimine ce qui est hors champ */
            mat4 identity;
            glm_mat4_identity(identity);
            const EngModelDrawOptions rock_options = {.instance_buffer = scene->rock_instances,
                                                      .instance_count = scene->rock_count};
            (void)eng_model_gpu_draw_ex(&scene->rocks, queue, scene->rock_pipeline, identity,
                                        view_proj, &light, NULL, &rock_options);
        }
        if (scene->model_hook != NULL) {
            scene->model_hook(scene->model_user, queue, view_proj, &light, NULL, false);
        }
        return;
    }
    if (scene->world_mode) { /* M7 : le monde streamé remplace sol, cubes et modèle */
        scene->last_sky_intensity = sky_intensity;
        for (int c = 0; c < 3; c++) {
            scene->last_true_sun[c] = sky_sun[c];
        }
        if (scene->model_hook != NULL) {
            scene->model_hook(scene->model_user, queue, view_proj, &light, receiving, false);
        }
        return;
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
    /* M8-07 : les cubes (non éclairés par pixel) suivent la lumière : soleil ou lune visible
     * (intensité, couleur), plus l'ambiante ; 1 en plein jour (comme avant), ~0,1 la nuit */
    float cube_light[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    for (int c = 0; c < 3; c++) {
        const float sun = light.sun_color[c] * light.sun_intensity;
        const float sky = light.ambient_color[c] * light.ambient_intensity;
        const float f = (0.75F * sun) + (0.5F * sky);
        cube_light[c] = f < 0.0F ? 0.0F : (f > 1.0F ? 1.0F : f);
    }
    const int props_half = scene->hide_props ? -1 : GRID_HALF; /* -1 : boucle vide */
    for (int gz = -props_half; gz <= props_half; gz++) {
        for (int gx = -props_half; gx <= props_half; gx++) {
            mat4 model;
            mat4 mvp;
            glm_translate_make(model,
                               (vec3){(float)gx * GRID_SPACING, 0.0F, (float)gz * GRID_SPACING});
            if (gx == 0 && gz == 0) {
                if (scene->world != NULL) {
                    continue;
                }
                if (scene->has_model) {
                    model_matrix(time_seconds, model);
                    (void)eng_model_gpu_draw_shadowed(scene->model, queue, scene->model_pipeline,
                                                      model, view_proj, &light, receiving);
                    continue;
                }
                glm_rotate(model, angle * 0.8F, ENG_AXIS_UP);
                glm_rotate(model, angle * 0.5F, ENG_AXIS_RIGHT);
            }
            glm_mat4_mul(view_proj, model, mvp);
            CubeUniforms u;
            memcpy(u.mvp, mvp, sizeof u.mvp);
            memcpy(u.light, cube_light, sizeof u.light);
            (void)eng_render_draw_uniforms(queue, &cmd, &u, sizeof u);
        }
    }
    draw_world(scene, queue, view_proj, &light, receiving, false);
    if (scene->model_hook != NULL) {
        scene->model_hook(scene->model_user, queue, view_proj, &light, receiving, false);
    }
}

void demo_scene_draw_debug(DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
                           double time_seconds, const EngCamera *camera, float aspect,
                           bool scene_debug, const EngEditor *editor)
{
    if (!scene->has_lines) {
        return;
    }
    EngDebugLines *lines = &scene->lines;
    eng_debug_lines_clear(lines);
    if (scene_debug) {
        /* testées en profondeur : le sol (sous les cubes, dont la base est à y = -0,5) et les
         * sphères */
        (void)eng_debug_grid(lines, (vec3){0.0F, -0.5F, 0.0F}, 30, 2.0F, ENG_DEBUG_GRAY,
                             ENG_DEBUG_WHITE);
        for (int gz = -GRID_HALF; gz <= GRID_HALF; gz += 2 * GRID_HALF) {
            for (int gx = -GRID_HALF; gx <= GRID_HALF; gx += 2 * GRID_HALF) {
                (void)eng_debug_sphere(
                    lines, (vec3){(float)gx * GRID_SPACING, 0.0F, (float)gz * GRID_SPACING}, 1.5F,
                    ENG_DEBUG_MAGENTA, 24);
            }
        }
        if (scene->has_model && scene->world == NULL) {
            mat4 model;
            model_matrix(time_seconds, model);
            vec3 center;
            vec3 half;
            for (int axis = 0; axis < 3; axis++) {
                center[axis] =
                    0.5F * (scene->model->bounds_min[axis] + scene->model->bounds_max[axis]);
                half[axis] =
                    0.5F * (scene->model->bounds_max[axis] - scene->model->bounds_min[axis]);
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
        mat4 world_axes;
        glm_translate_make(world_axes, (vec3){0.0F, -0.5F, 0.0F});
        (void)eng_debug_axes(lines, world_axes, 4.0F);
        eng_debug_lines_set_overlay(lines, false);
    }
    if (editor != NULL && scene->world != NULL) {
        eng_editor_draw(editor, scene->world, demo_scene_entity_bounds, scene, lines,
                        camera->position);
    }

    if (scene->debug_hook != NULL) {
        mat4 model;
        model_matrix(time_seconds, model);
        scene->debug_hook(scene->debug_user, lines, model,
                          scene->has_model && scene->world == NULL);
    }

    mat4 view_proj;
    view_projection(camera, aspect, view_proj);
    (void)eng_debug_lines_gpu_draw(&scene->lines_gpu, renderer, queue, lines, view_proj);
}

void demo_scene_set_debug_hook(DemoScene *scene, DemoDebugHook hook, void *user)
{
    scene->debug_hook = hook;
    scene->debug_user = user;
}

void demo_scene_set_model_hook(DemoScene *scene, DemoModelHook hook, void *user)
{
    scene->model_hook = hook;
    scene->model_user = user;
}

void demo_scene_shutdown(DemoScene *scene, EngRenderer *renderer)
{
    if (scene->has_lines) {
        eng_debug_lines_gpu_destroy(&scene->lines_gpu, renderer);
    }
    if (scene->has_cook) {
        eng_asset_cook_shutdown(&scene->cook);
    }
    if (scene->has_assets) {
        for (uint32_t i = 0; i < scene->world_model_count; i++) {
            if (scene->world_models[i].handle.generation != 0) {
                (void)eng_assets_release(&scene->assets, scene->world_models[i].handle);
            }
        }
        if (scene->model_handle.generation != 0) {
            (void)eng_assets_release(&scene->assets, scene->model_handle);
        }
        (void)eng_assets_shutdown(&scene->assets);
    }
    if (scene->has_terrain) {
        if (scene->terrain_chunk_total > 0) {
            ENG_LOG_INFO("terrain : %u/%u chunks dessinés à la dernière image",
                         scene->terrain_chunks_drawn, scene->terrain_chunk_total);
        }
        if (scene->has_atmosphere) {
            ENG_LOG_INFO("brouillard : densité %.4f /m, décroissance %.4f /m, opacité max %.2f",
                         (double)scene->fog.density, (double)scene->fog.height_falloff,
                         (double)scene->fog.max_opacity);
        }
        if (scene->has_water) {
            ENG_LOG_INFO("eau : niveau %.1f m, %u cellules mouillées, 1 dessin par image",
                         (double)scene->water_params.level, scene->water_cells);
            eng_model_gpu_destroy(&scene->water_model, renderer);
            (void)eng_renderer_destroy_pipeline(renderer, scene->water_pipeline);
        }
        if (scene->has_grass) {
            ENG_LOG_INFO("herbe : %u brins, 1 dessin par image", scene->grass_count);
            (void)eng_renderer_destroy_buffer(renderer, scene->grass_instances);
            eng_model_gpu_destroy(&scene->grass_blade, renderer);
            (void)eng_renderer_destroy_pipeline(renderer, scene->grass_pipeline);
        }
        if (scene->has_rocks || scene->has_veg) {
            if (scene->has_veg) {
                ENG_LOG_INFO("végétation : %u instances, 1 dessin par image", scene->veg_count);
                (void)eng_renderer_destroy_buffer(renderer, scene->veg_instances);
            }
            ENG_LOG_INFO("rochers : %u instances, 1 dessin par image", scene->rock_count);
            if (scene->has_rocks) {
                (void)eng_renderer_destroy_buffer(renderer, scene->rock_instances);
            }
            eng_model_gpu_destroy(&scene->rocks, renderer);
            (void)eng_renderer_destroy_pipeline(renderer, scene->rock_pipeline);
        }
        eng_terrain_gpu_destroy(&scene->terrain, renderer);
    }
    if (eng_renderer_pipeline_is_valid(renderer, scene->terrain_pipeline)) {
        (void)eng_renderer_destroy_pipeline(renderer, scene->terrain_pipeline);
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
    if (scene->has_atmosphere) {
        ENG_LOG_INFO("ciel : analytique, soleil à %.1f°, lumière directe (%.2f %.2f %.2f) x %.2f, "
                     "ambiante (%.2f %.2f %.2f)",
                     (double)scene->last_sun_elevation_deg, (double)scene->last_sun_color[0],
                     (double)scene->last_sun_color[1], (double)scene->last_sun_color[2],
                     (double)scene->last_sun_visibility, (double)scene->last_ambient[0],
                     (double)scene->last_ambient[1], (double)scene->last_ambient[2]);
    }
    if (scene->has_atmosphere) {
        (void)eng_renderer_destroy_pipeline(renderer, scene->atmosphere_pipeline);
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
