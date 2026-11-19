#include "engine/asset_types.h"

#include <string.h>

#include "engine/files.h"
#include "engine/log.h"
#include "engine/mesh_file.h"
#include "engine/model.h"

typedef struct DependencySink {
    EngAssetLoad *load;
    bool ok;
} DependencySink;

static void note_dependency(void *user, const char *path)
{
    DependencySink *sink = user;
    if (!eng_asset_load_dependency(sink->load, path)) {
        sink->ok = false;
    }
}

/* Octets que les tableaux du modèle GPU prennent dans son arène (parties, matériaux, deux jeux de
 * handles de textures), avec de la marge pour l'alignement. */
static size_t gpu_arena_bytes(const EngModel *model)
{
    return 4096U + ((size_t)model->part_count * sizeof(EngModelPart)) +
           ((size_t)model->material_count * sizeof(EngModelGpuMaterial)) +
           ((size_t)model->image_count * 2U * sizeof(EngTextureHandle));
}

static bool model_load(EngAssetLoad *load, void *payload, const char *path, void *user)
{
    EngModelAsset *asset = payload;
    const EngModelAssetContext *context = user;
    const size_t cpu_bytes =
        context->cpu_bytes != 0 ? context->cpu_bytes : ENG_MODEL_ASSET_DEFAULT_CPU_BYTES;

    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        ENG_LOG_ERROR("asset modèle : chemin « %s » trop long", path);
        return false;
    }
    DependencySink sink = {.load = load, .ok = eng_asset_load_dependency(load, resolved)};

    EngArena cpu;
    EngArena scratch;
    if (!eng_arena_init(&cpu, "asset-model-cpu", cpu_bytes)) {
        return false;
    }
    if (!eng_arena_init(&scratch, "asset-model-scratch", cpu_bytes)) {
        eng_arena_shutdown(&cpu);
        return false;
    }
    bool ok = false;
    EngModel model;
    if (eng_mesh_load_visit(&cpu, &scratch, path, &model, note_dependency, &sink) && sink.ok) {
        if (eng_arena_init(&asset->arena, "asset-model-gpu", gpu_arena_bytes(&model))) {
            ok = eng_model_gpu_create(&asset->gpu, context->renderer, &asset->arena, &model,
                                      context->anisotropy);
            if (!ok) {
                eng_arena_shutdown(&asset->arena);
            }
        }
    }
    if (ok && context->environment != NULL &&
        !eng_model_gpu_set_environment(&asset->gpu, context->renderer, context->environment)) {
        ENG_LOG_WARN("asset modèle %s : environnement refusé, éclairage par défaut", path);
    }
    eng_model_free(&model);
    eng_arena_shutdown(&scratch);
    eng_arena_shutdown(&cpu);
    return ok;
}

static void model_unload(void *payload, void *user)
{
    EngModelAsset *asset = payload;
    const EngModelAssetContext *context = user;
    eng_model_gpu_destroy(&asset->gpu, context->renderer);
    eng_arena_shutdown(&asset->arena);
}

uint32_t eng_model_assets_register(EngAssets *assets, EngModelAssetContext *context)
{
    if (context == NULL || context->renderer == NULL) {
        ENG_LOG_ERROR("assets : le type modèle demande un renderer");
        return 0;
    }
    return eng_assets_register(assets, &(EngAssetType){.name = "modèle",
                                                       .payload_size = sizeof(EngModelAsset),
                                                       .load = model_load,
                                                       .unload = model_unload,
                                                       .user = context});
}

const EngModelGpu *eng_model_asset_gpu(const EngAssets *assets, uint32_t model_type,
                                       EngAssetHandle handle)
{
    if (eng_assets_type_of(assets, handle) != model_type || model_type == 0) {
        return NULL;
    }
    const EngModelAsset *asset = eng_assets_get(assets, handle);
    return asset != NULL ? &asset->gpu : NULL;
}
