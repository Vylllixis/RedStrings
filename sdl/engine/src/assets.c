#include "engine/assets.h"

#include <SDL3/SDL_stdinc.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"

typedef struct AssetEntry {
    uint32_t type;
    uint32_t refs;
    bool loading; /* en cours de chargement : une demande du même asset est une boucle */
    char path[ENG_ASSET_PATH_MAX];
    void *payload;
    char **deps;
    uint32_t dep_count;
    uint32_t dep_capacity;
    EngFileWatch *watches; /* une par dépendance, créées par eng_assets_poll */
} AssetEntry;

struct EngAssetLoad {
    AssetEntry *entry;
};

static uint64_t index_key(uint32_t type, const char *path)
{
    char text[ENG_ASSET_PATH_MAX + 16U];
    SDL_snprintf(text, sizeof text, "%u:%s", (unsigned)type, path);
    return eng_hash_string(text);
}

bool eng_asset_load_dependency(EngAssetLoad *load, const char *path)
{
    AssetEntry *entry = load->entry;
    if (path == NULL || path[0] == '\0') {
        ENG_LOG_ERROR("asset %s : dépendance sans chemin", entry->path);
        return false;
    }
    for (uint32_t i = 0; i < entry->dep_count; i++) {
        if (strcmp(entry->deps[i], path) == 0) {
            return true; /* déjà déclarée */
        }
    }
    if (entry->dep_count >= ENG_ASSET_MAX_DEPENDENCIES) {
        ENG_LOG_ERROR("asset %s : plus de %u dépendances", entry->path,
                      (unsigned)ENG_ASSET_MAX_DEPENDENCIES);
        return false;
    }
    if (entry->dep_count == entry->dep_capacity) {
        const uint32_t capacity = entry->dep_capacity == 0 ? 8U : entry->dep_capacity * 2U;
        char **grown = (char **)SDL_realloc((void *)entry->deps, (size_t)capacity * sizeof *grown);
        if (grown == NULL) {
            ENG_LOG_ERROR("asset %s : mémoire insuffisante", entry->path);
            return false;
        }
        entry->deps = grown;
        entry->dep_capacity = capacity;
    }
    char *copy = SDL_strdup(path);
    if (copy == NULL) {
        ENG_LOG_ERROR("asset %s : mémoire insuffisante", entry->path);
        return false;
    }
    entry->deps[entry->dep_count++] = copy;
    return true;
}

static void free_watches(AssetEntry *entry)
{
    SDL_free(entry->watches);
    entry->watches = NULL;
}

static void free_dependencies(AssetEntry *entry)
{
    for (uint32_t i = 0; i < entry->dep_count; i++) {
        SDL_free(entry->deps[i]);
    }
    SDL_free((void *)entry->deps);
    entry->deps = NULL;
    entry->dep_count = 0;
    entry->dep_capacity = 0;
}

bool eng_assets_init(EngAssets *assets, uint32_t capacity)
{
    *assets = (EngAssets){0};
    if (capacity == 0) {
        ENG_LOG_ERROR("assets : capacité nulle");
        return false;
    }
    if (!eng_pool_init(&assets->pool, NULL, sizeof(AssetEntry), capacity)) {
        return false;
    }
    if (!eng_map_init(&assets->index, NULL, sizeof(EngAssetHandle), capacity)) {
        eng_pool_shutdown(&assets->pool);
        *assets = (EngAssets){0};
        return false;
    }
    return true;
}

/* Décharge l'entrée (payload, dépendances, case du pool) ; l'index doit déjà ne plus la citer. */
static void destroy_entry(EngAssets *assets, EngAssetHandle handle, AssetEntry *entry)
{
    const EngAssetType *type = &assets->types[entry->type - 1U];
    if (type->unload != NULL) {
        type->unload(entry->payload, type->user);
    }
    SDL_free(entry->payload);
    free_dependencies(entry);
    free_watches(entry);
    (void)eng_pool_free(&assets->pool, handle);
}

uint32_t eng_assets_shutdown(EngAssets *assets)
{
    uint32_t leaks = 0;
    if (assets->pool.items == NULL) {
        return 0;
    }
    EngAssetHandle handle;
    uint32_t cursor = 0;
    while (eng_pool_next(&assets->pool, &cursor, &handle)) {
        AssetEntry *entry = eng_pool_get(&assets->pool, handle);
        if (entry->refs > 0) {
            ENG_LOG_WARN("assets : « %s » (%s) encore référencé %u fois à l'arrêt", entry->path,
                         assets->types[entry->type - 1U].name, (unsigned)entry->refs);
            leaks++;
        }
        destroy_entry(assets, handle, entry);
    }
    eng_map_shutdown(&assets->index);
    eng_pool_shutdown(&assets->pool);
    *assets = (EngAssets){0};
    return leaks;
}

uint32_t eng_assets_register(EngAssets *assets, const EngAssetType *type)
{
    if (type == NULL || type->name == NULL || type->name[0] == '\0' || type->payload_size == 0 ||
        type->load == NULL) {
        ENG_LOG_ERROR("assets : type invalide (nom, taille du payload et chargeur obligatoires)");
        return 0;
    }
    if (assets->type_count >= ENG_ASSET_MAX_TYPES) {
        ENG_LOG_ERROR("assets : trop de types (%u)", (unsigned)ENG_ASSET_MAX_TYPES);
        return 0;
    }
    assets->types[assets->type_count] = *type;
    return ++assets->type_count;
}

static AssetEntry *entry_of(const EngAssets *assets, EngAssetHandle handle)
{
    return assets->pool.items != NULL ? eng_pool_get(&assets->pool, handle) : NULL;
}

EngAssetHandle eng_assets_acquire(EngAssets *assets, uint32_t type, const char *path)
{
    if (type == 0 || type > assets->type_count) {
        ENG_LOG_ERROR("assets : type %u inconnu", (unsigned)type);
        return ENG_HANDLE_NULL;
    }
    const size_t length = path != NULL ? strlen(path) : 0;
    if (length == 0 || length >= ENG_ASSET_PATH_MAX) {
        ENG_LOG_ERROR("assets : chemin vide ou de plus de %u octets",
                      (unsigned)ENG_ASSET_PATH_MAX - 1U);
        return ENG_HANDLE_NULL;
    }
    const EngAssetType *desc = &assets->types[type - 1U];
    const uint64_t key = index_key(type, path);

    const EngAssetHandle *known = eng_map_get(&assets->index, key);
    if (known != NULL) {
        const EngAssetHandle handle = *known;
        AssetEntry *entry = entry_of(assets, handle);
        if (entry == NULL || entry->type != type || strcmp(entry->path, path) != 0) {
            ENG_LOG_ERROR("assets : collision de hachage sur « %s »", path);
            return ENG_HANDLE_NULL;
        }
        if (entry->loading) {
            ENG_LOG_ERROR("assets : « %s » se demande lui-même pendant son chargement", path);
            return ENG_HANDLE_NULL;
        }
        entry->refs++;
        assets->stats.hits++;
        return handle;
    }

    EngAssetHandle handle;
    AssetEntry *entry = eng_pool_alloc(&assets->pool, &handle);
    if (entry == NULL) {
        ENG_LOG_ERROR("assets : plus de place (%u assets) pour « %s »",
                      (unsigned)assets->pool.capacity, path);
        return ENG_HANDLE_NULL;
    }
    entry->type = type;
    entry->refs = 1;
    entry->loading = true;
    memcpy(entry->path, path, length + 1U);
    entry->payload = SDL_calloc(1, desc->payload_size);
    bool ok = entry->payload != NULL && eng_map_put(&assets->index, key, &handle);
    if (!ok) {
        ENG_LOG_ERROR("assets : mémoire insuffisante pour « %s »", path);
    } else {
        EngAssetLoad load = {.entry = entry};
        ok = desc->load(&load, entry->payload, path, desc->user);
        if (!ok) {
            (void)eng_map_remove(&assets->index, key);
        }
    }
    if (!ok) {
        SDL_free(entry->payload);
        free_dependencies(entry);
        (void)eng_pool_free(&assets->pool, handle);
        assets->stats.failures++;
        return ENG_HANDLE_NULL;
    }
    entry->loading = false;
    assets->stats.loads++;
    return handle;
}

EngAssetHandle eng_assets_retain(EngAssets *assets, EngAssetHandle handle)
{
    AssetEntry *entry = entry_of(assets, handle);
    if (entry == NULL || entry->loading) {
        ENG_LOG_WARN("assets : retain d'un handle invalide");
        return ENG_HANDLE_NULL;
    }
    entry->refs++;
    return handle;
}

bool eng_assets_release(EngAssets *assets, EngAssetHandle handle)
{
    AssetEntry *entry = entry_of(assets, handle);
    if (entry == NULL || entry->loading || entry->refs == 0) {
        ENG_LOG_WARN("assets : release d'un handle invalide ou déjà rendu");
        return false;
    }
    if (--entry->refs > 0) {
        return true;
    }
    (void)eng_map_remove(&assets->index, index_key(entry->type, entry->path));
    destroy_entry(assets, handle, entry);
    assets->stats.unloads++;
    return true;
}

void *eng_assets_get(const EngAssets *assets, EngAssetHandle handle)
{
    const AssetEntry *entry = entry_of(assets, handle);
    return entry != NULL && !entry->loading ? entry->payload : NULL;
}

uint32_t eng_assets_refs(const EngAssets *assets, EngAssetHandle handle)
{
    const AssetEntry *entry = entry_of(assets, handle);
    return entry != NULL ? entry->refs : 0;
}

const char *eng_assets_path(const EngAssets *assets, EngAssetHandle handle)
{
    const AssetEntry *entry = entry_of(assets, handle);
    return entry != NULL ? entry->path : NULL;
}

uint32_t eng_assets_type_of(const EngAssets *assets, EngAssetHandle handle)
{
    const AssetEntry *entry = entry_of(assets, handle);
    return entry != NULL ? entry->type : 0;
}

uint32_t eng_assets_dependency_count(const EngAssets *assets, EngAssetHandle handle)
{
    const AssetEntry *entry = entry_of(assets, handle);
    return entry != NULL ? entry->dep_count : 0;
}

const char *eng_assets_dependency(const EngAssets *assets, EngAssetHandle handle, uint32_t index)
{
    const AssetEntry *entry = entry_of(assets, handle);
    return entry != NULL && index < entry->dep_count ? entry->deps[index] : NULL;
}

EngAssetStats eng_assets_stats(const EngAssets *assets)
{
    EngAssetStats stats = assets->stats;
    stats.loaded = eng_pool_count(&assets->pool);
    return stats;
}

/* ── Rechargement (M2-06) ── */

bool eng_assets_reload(EngAssets *assets, EngAssetHandle handle)
{
    AssetEntry *entry = entry_of(assets, handle);
    if (entry == NULL || entry->loading || entry->refs == 0) {
        ENG_LOG_WARN("assets : rechargement d'un handle invalide");
        return false;
    }
    const EngAssetType *type = &assets->types[entry->type - 1U];
    AssetEntry fresh = {.type = entry->type, .loading = true};
    memcpy(fresh.path, entry->path, sizeof fresh.path);
    void *payload = SDL_calloc(1, type->payload_size);
    if (payload == NULL) {
        ENG_LOG_ERROR("assets : mémoire insuffisante pour recharger « %s »", entry->path);
        assets->stats.reload_failures++;
        return false;
    }
    EngAssetLoad load = {.entry = &fresh};
    entry->loading = true; /* le chargeur ne peut pas redemander cet asset */
    const bool ok = type->load(&load, payload, entry->path, type->user);
    entry->loading = false;
    if (!ok) {
        ENG_LOG_WARN("assets : rechargement de « %s » échoué, l'ancienne version reste en service",
                     entry->path);
        SDL_free(payload);
        free_dependencies(&fresh);
        assets->stats.reload_failures++;
        return false;
    }
    if (type->unload != NULL) {
        type->unload(entry->payload, type->user);
    }
    memcpy(entry->payload, payload, type->payload_size);
    SDL_free(payload);
    free_dependencies(entry);
    entry->deps = fresh.deps;
    entry->dep_count = fresh.dep_count;
    entry->dep_capacity = fresh.dep_capacity;
    free_watches(entry);
    assets->stats.reloads++;
    ENG_LOG_INFO("assets : « %s » rechargé", entry->path);
    return true;
}

void eng_assets_set_hot_reload(EngAssets *assets, bool enabled, double interval_seconds)
{
    assets->hot = enabled;
    assets->hot_interval = interval_seconds;
}

/* Crée les surveillances d'une entrée (la version présente à ce moment est la référence). */
static bool make_watches(const EngAssets *assets, AssetEntry *entry)
{
    entry->watches =
        SDL_calloc(entry->dep_count > 0 ? entry->dep_count : 1U, sizeof *entry->watches);
    if (entry->watches == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < entry->dep_count; i++) {
        if (!eng_file_watch_init(&entry->watches[i], entry->deps[i], assets->hot_interval)) {
            free_watches(entry);
            return false;
        }
    }
    return true;
}

uint32_t eng_assets_poll(EngAssets *assets, double now_seconds)
{
    if (!assets->hot || assets->pool.items == NULL) {
        return 0;
    }
    uint32_t reloaded = 0;
    EngAssetHandle handle;
    uint32_t cursor = 0;
    while (eng_pool_next(&assets->pool, &cursor, &handle)) {
        AssetEntry *entry = eng_pool_get(&assets->pool, handle);
        if (entry->loading || entry->dep_count == 0) {
            continue;
        }
        if (entry->watches == NULL && !make_watches(assets, entry)) {
            continue; /* mémoire ou chemin : pas de surveillance pour cet asset */
        }
        bool changed = false;
        for (uint32_t i = 0; i < entry->dep_count; i++) {
            changed = eng_file_watch_poll(&entry->watches[i], now_seconds) || changed;
        }
        if (changed && eng_assets_reload(assets, handle)) {
            reloaded++;
        }
    }
    return reloaded;
}
