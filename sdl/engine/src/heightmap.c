#include "engine/heightmap.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <math.h>
#include <stb_image.h>
#include <string.h>

#include "engine/asset_format.h"
#include "engine/files.h"
#include "engine/log.h"

#define SAMPLE_MAX 65535U
#define MAX_OCTAVES 12U
#define MAX_GAIN 8.0F
#define INFO_BYTES 16U

static bool finite_range(float spacing, float low, float high)
{
    return isfinite(spacing) && spacing > 0.0F && isfinite(low) && isfinite(high) && high > low;
}

static bool size_ok(uint32_t size)
{
    return size >= ENG_HEIGHTMAP_MIN_SIZE && size <= ENG_HEIGHTMAP_MAX_SIZE;
}

EngHeightmapParams eng_heightmap_default_params(void)
{
    return (EngHeightmapParams){.size = 1025,
                                .spacing = 1.0F,
                                .altitude_min = 0.0F,
                                .altitude_max = 120.0F,
                                .seed = 1337,
                                .wavelength = 512.0F,
                                .octaves = 6,
                                .lacunarity = 2.0F,
                                .gain = 0.5F,
                                .fractal = ENG_NOISE_FBM};
}

bool eng_heightmap_params_valid(const EngHeightmapParams *p)
{
    if (p == NULL) {
        return false;
    }
    const char *why = NULL;
    if (!size_ok(p->size)) {
        why = "taille hors de [2, 8193]";
    } else if (!finite_range(p->spacing, p->altitude_min, p->altitude_max)) {
        why = "espacement > 0 et altitude_max > altitude_min (valeurs finies) attendus";
    } else if (!isfinite(p->wavelength) || p->wavelength <= 0.0F) {
        why = "longueur d'onde > 0 attendue";
    } else if (p->octaves < 1U || p->octaves > MAX_OCTAVES) {
        why = "octaves hors de [1, 12]";
    } else if (!isfinite(p->lacunarity) || p->lacunarity <= 0.0F || p->lacunarity > MAX_GAIN) {
        why = "lacunarité hors de ]0, 8]";
    } else if (!isfinite(p->gain) || p->gain <= 0.0F || p->gain > MAX_GAIN) {
        why = "gain hors de ]0, 8]";
    } else if (p->fractal != ENG_NOISE_FBM && p->fractal != ENG_NOISE_RIDGED) {
        why = "fractale inconnue";
    }
    if (why != NULL) {
        ENG_LOG_ERROR("champ de hauteurs : paramètres invalides : %s", why);
        return false;
    }
    return true;
}

static EngNoiseParams noise_of(const EngHeightmapParams *p)
{
    return (EngNoiseParams){.seed = p->seed,
                            .frequency = 1.0F / p->wavelength,
                            .octaves = p->octaves,
                            .lacunarity = p->lacunarity,
                            .gain = p->gain,
                            .fractal = p->fractal};
}

bool eng_heightmap_generate(EngArena *arena, const EngHeightmapParams *p, EngHeightmap *out)
{
    *out = (EngHeightmap){0};
    if (!eng_heightmap_params_valid(p)) {
        return false;
    }
    const uint32_t size = p->size;
    const size_t count = (size_t)size * size;
    const EngArenaMark mark = eng_arena_mark(arena);
    /* La ligne de travail est prise AVANT les échantillons : ils terminent l'allocation, si bien
     * qu'une écriture au-delà du dernier échantillon tombe hors de l'arène (détectée sous ASan). */
    float *row = eng_arena_alloc_array(arena, sizeof(float), size, _Alignof(float));
    uint16_t *samples = eng_arena_alloc(arena, count * sizeof(uint16_t), ENG_ASSET_ALIGN);
    if (samples == NULL || row == NULL) {
        eng_arena_reset_to(arena, mark);
        return false;
    }
    const EngNoiseParams noise = noise_of(p);

    /* Première passe : étendue du bruit. Seconde : la même ligne recalculée (déterministe),
     * étalée sur [0, 65535]. Recalculer évite un tableau de flottants de la taille du champ. */
    float low = INFINITY;
    float high = -INFINITY;
    for (uint32_t z = 0; z < size; z++) {
        eng_noise_row_2d(&noise, 0.0F, p->spacing, (float)z * p->spacing, size, row);
        for (uint32_t x = 0; x < size; x++) {
            if (!isfinite(row[x])) {
                ENG_LOG_ERROR("champ de hauteurs : le bruit a produit une valeur non finie");
                eng_arena_reset_to(arena, mark);
                return false;
            }
            low = fminf(low, row[x]);
            high = fmaxf(high, row[x]);
        }
    }
    const float range = high - low;
    for (uint32_t z = 0; z < size; z++) {
        eng_noise_row_2d(&noise, 0.0F, p->spacing, (float)z * p->spacing, size, row);
        for (uint32_t x = 0; x < size; x++) {
            float v = range > 0.0F ? (row[x] - low) / range : 0.0F;
            v = fminf(fmaxf(v, 0.0F), 1.0F);
            samples[((size_t)z * size) + x] = (uint16_t)((v * (float)SAMPLE_MAX) + 0.5F);
        }
    }
    *out = (EngHeightmap){.size = size,
                          .spacing = p->spacing,
                          .altitude_min = p->altitude_min,
                          .altitude_max = p->altitude_max,
                          .samples = samples};
    return true;
}

static float altitude_of(const EngHeightmap *map, float raw)
{
    return map->altitude_min +
           ((raw / (float)SAMPLE_MAX) * (map->altitude_max - map->altitude_min));
}

float eng_heightmap_altitude(const EngHeightmap *map, uint32_t x, uint32_t z)
{
    if (x >= map->size || z >= map->size) {
        return map->altitude_min;
    }
    return altitude_of(map, (float)map->samples[((size_t)z * map->size) + x]);
}

float eng_heightmap_extent(const EngHeightmap *map)
{
    return (float)(map->size - 1U) * map->spacing;
}

/* Position en échantillons, ramenée dans [0, size - 1] (NaN : 0). */
static float grid_position(const EngHeightmap *map, float meters)
{
    float u = meters / map->spacing;
    if (!(u > 0.0F)) {
        u = 0.0F;
    }
    const float last = (float)(map->size - 1U);
    return u > last ? last : u;
}

float eng_heightmap_sample(const EngHeightmap *map, float x_m, float z_m)
{
    const float u = grid_position(map, x_m);
    const float v = grid_position(map, z_m);
    uint32_t x0 = (uint32_t)u;
    uint32_t z0 = (uint32_t)v;
    if (x0 > map->size - 2U) {
        x0 = map->size - 2U;
    }
    if (z0 > map->size - 2U) {
        z0 = map->size - 2U;
    }
    const float fx = u - (float)x0;
    const float fz = v - (float)z0;
    const size_t row0 = (size_t)z0 * map->size;
    const size_t row1 = row0 + map->size;
    const float h00 = (float)map->samples[row0 + x0];
    const float h10 = (float)map->samples[row0 + x0 + 1U];
    const float h01 = (float)map->samples[row1 + x0];
    const float h11 = (float)map->samples[row1 + x0 + 1U];
    const float top = h00 + ((h10 - h00) * fx);
    const float bottom = h01 + ((h11 - h01) * fx);
    return altitude_of(map, top + ((bottom - top) * fz));
}

/* ── .hmap ── */

static bool map_valid(const EngHeightmap *map)
{
    return map != NULL && map->samples != NULL && size_ok(map->size) &&
           finite_range(map->spacing, map->altitude_min, map->altitude_max);
}

static void make_info(const EngHeightmap *map, uint8_t info[INFO_BYTES])
{
    memcpy(info, &map->size, 4);
    memcpy(info + 4, &map->spacing, 4);
    memcpy(info + 8, &map->altitude_min, 4);
    memcpy(info + 12, &map->altitude_max, 4);
}

static void make_sections(const EngHeightmap *map, const uint8_t info[INFO_BYTES],
                          EngAssetSectionDesc sections[2])
{
    const uint32_t count = map->size * map->size;
    sections[0] = (EngAssetSectionDesc){
        .tag = ENG_ASSET_TAG_INFO, .data = info, .size = INFO_BYTES, .count = 1};
    sections[1] = (EngAssetSectionDesc){.tag = ENG_ASSET_TAG_DATA,
                                        .data = map->samples,
                                        .size = count * (uint32_t)sizeof(uint16_t),
                                        .count = count};
}

size_t eng_heightmap_file_size(const EngHeightmap *map)
{
    if (!map_valid(map)) {
        return 0;
    }
    uint8_t info[INFO_BYTES];
    EngAssetSectionDesc sections[2];
    make_info(map, info);
    make_sections(map, info, sections);
    return eng_asset_file_size(sections, 2);
}

bool eng_heightmap_encode(const EngHeightmap *map, void *out, size_t capacity, size_t *written)
{
    if (!map_valid(map)) {
        ENG_LOG_ERROR("champ de hauteurs : encodage d'un champ invalide");
        return false;
    }
    uint8_t info[INFO_BYTES];
    EngAssetSectionDesc sections[2];
    make_info(map, info);
    make_sections(map, info, sections);
    return eng_asset_build(ENG_ASSET_MAGIC_HMAP, ENG_ASSET_VERSION_HMAP, sections, 2, out, capacity,
                           written);
}

static bool refuse(const char *name, const char *why)
{
    ENG_LOG_ERROR("%s : champ de hauteurs refusé : %s", name != NULL ? name : "(mémoire)", why);
    return false;
}

bool eng_heightmap_open(const void *data, size_t size, const char *name, EngHeightmap *out)
{
    *out = (EngHeightmap){0};
    EngAssetView view;
    if (eng_asset_open(data, size, ENG_ASSET_MAGIC_HMAP, ENG_ASSET_VERSION_HMAP, name, &view) !=
        ENG_ASSET_OK) {
        return false;
    }
    EngAssetSection info;
    EngAssetSection samples;
    if (!eng_asset_find_section(&view, ENG_ASSET_TAG_INFO, &info) ||
        !eng_asset_find_section(&view, ENG_ASSET_TAG_DATA, &samples)) {
        return refuse(name, "section INFO ou DATA absente");
    }
    if (info.size != INFO_BYTES) {
        return refuse(name, "section INFO de taille inattendue");
    }
    EngHeightmap map = {0};
    memcpy(&map.size, info.data, 4);
    memcpy(&map.spacing, info.data + 4, 4);
    memcpy(&map.altitude_min, info.data + 8, 4);
    memcpy(&map.altitude_max, info.data + 12, 4);
    if (!size_ok(map.size)) {
        return refuse(name, "taille hors de [2, 8193]");
    }
    if (!finite_range(map.spacing, map.altitude_min, map.altitude_max)) {
        return refuse(name, "espacement ou altitudes invalides");
    }
    const size_t count = (size_t)map.size * map.size;
    if (samples.size != count * sizeof(uint16_t) || samples.count != count) {
        return refuse(name, "la section DATA ne contient pas size x size échantillons");
    }
    if (((uintptr_t)samples.data % _Alignof(uint16_t)) != 0U) {
        return refuse(name, "échantillons mal alignés");
    }
    /* alignement vérifié ci-dessus ; lecture en place des échantillons 16 bits */
    // NOLINTNEXTLINE(bugprone-casting-through-void)
    map.samples = (const uint16_t *)(const void *)samples.data;
    *out = map;
    return true;
}

bool eng_heightmap_load(EngArena *arena, const char *path, EngHeightmap *out)
{
    *out = (EngHeightmap){0};
    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        ENG_LOG_ERROR("champ de hauteurs : chemin « %s » trop long", path);
        return false;
    }
    EngFileData file;
    if (!eng_file_read_all(arena, resolved, &file)) {
        return false;
    }
    return eng_heightmap_open(file.data, file.size, path, out);
}

bool eng_heightmap_save(EngArena *scratch, const EngHeightmap *map, const char *path)
{
    const size_t bytes = eng_heightmap_file_size(map);
    if (bytes == 0U) {
        ENG_LOG_ERROR("champ de hauteurs : enregistrement d'un champ invalide");
        return false;
    }
    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        ENG_LOG_ERROR("champ de hauteurs : chemin « %s » trop long", path);
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(scratch);
    void *file = eng_arena_alloc(scratch, bytes, ENG_ASSET_ALIGN);
    bool ok = file != NULL && eng_heightmap_encode(map, file, bytes, NULL);
    if (ok && !SDL_SaveFile(resolved, file, bytes)) {
        ENG_LOG_ERROR("champ de hauteurs : écriture de %s impossible : %s", resolved,
                      SDL_GetError());
        ok = false;
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}

/* ── PNG 16 bits ── */

bool eng_heightmap_import_image(EngArena *arena, const void *data, size_t size, float spacing,
                                float altitude_min, float altitude_max, EngHeightmap *out)
{
    *out = (EngHeightmap){0};
    if (data == NULL || size == 0 || size > (size_t)INT32_MAX) {
        ENG_LOG_ERROR("champ de hauteurs : image vide ou trop grande");
        return false;
    }
    if (!finite_range(spacing, altitude_min, altitude_max)) {
        ENG_LOG_ERROR("champ de hauteurs : espacement > 0 et altitude_max > altitude_min attendus");
        return false;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    uint16_t *pixels =
        stbi_load_16_from_memory((const stbi_uc *)data, (int)size, &width, &height, &channels, 1);
    if (pixels == NULL) {
        ENG_LOG_ERROR("champ de hauteurs : décodage impossible : %s", stbi_failure_reason());
        return false;
    }
    bool ok = false;
    if (width != height) {
        ENG_LOG_ERROR("champ de hauteurs : image non carrée (%d x %d)", width, height);
    } else if (width < 0 || !size_ok((uint32_t)width)) {
        ENG_LOG_ERROR("champ de hauteurs : côté %d hors de [2, 8193]", width);
    } else {
        const size_t count = (size_t)width * (size_t)width;
        uint16_t *samples = eng_arena_alloc(arena, count * sizeof(uint16_t), ENG_ASSET_ALIGN);
        if (samples != NULL) {
            memcpy(samples, pixels, count * sizeof(uint16_t));
            *out = (EngHeightmap){.size = (uint32_t)width,
                                  .spacing = spacing,
                                  .altitude_min = altitude_min,
                                  .altitude_max = altitude_max,
                                  .samples = samples};
            ok = true;
        }
    }
    stbi_image_free(pixels);
    return ok;
}

typedef struct PngWriter {
    uint8_t *p;
} PngWriter;

static void put_u32_be(PngWriter *w, uint32_t v)
{
    *w->p++ = (uint8_t)(v >> 24);
    *w->p++ = (uint8_t)(v >> 16);
    *w->p++ = (uint8_t)(v >> 8);
    *w->p++ = (uint8_t)v;
}

static void put_bytes(PngWriter *w, const void *data, size_t n)
{
    memcpy(w->p, data, n);
    w->p += n;
}

static uint32_t crc32_of(const uint8_t *data, size_t n)
{
    uint32_t table[256];
    for (uint32_t i = 0; i < 256U; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1U) != 0U ? 0xEDB88320U ^ (c >> 1) : c >> 1;
        }
        table[i] = c;
    }
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < n; i++) {
        crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

/* Octet d'indice i du flux brut : une ligne = 1 octet de filtre (0) puis size échantillons en
 * grand-boutiste. */
static uint8_t raw_byte(const EngHeightmap *map, size_t i)
{
    const size_t row_bytes = 1U + ((size_t)map->size * 2U);
    const size_t z = i / row_bytes;
    const size_t col = i % row_bytes;
    if (col == 0U) {
        return 0U;
    }
    const uint16_t v = map->samples[(z * map->size) + ((col - 1U) / 2U)];
    return ((col - 1U) % 2U) == 0U ? (uint8_t)(v >> 8) : (uint8_t)(v & 0xFFU);
}

#define STORED_BLOCK_MAX 65535U

bool eng_heightmap_encode_png(EngArena *arena, const EngHeightmap *map, const void **out,
                              size_t *out_size)
{
    *out = NULL;
    *out_size = 0;
    if (!map_valid(map)) {
        ENG_LOG_ERROR("champ de hauteurs : export PNG d'un champ invalide");
        return false;
    }
    const size_t raw = (size_t)map->size * (1U + ((size_t)map->size * 2U));
    const size_t blocks = (raw + STORED_BLOCK_MAX - 1U) / STORED_BLOCK_MAX;
    const size_t zlib_bytes = 2U + (blocks * 5U) + raw + 4U;
    if (zlib_bytes > (size_t)INT32_MAX) {
        ENG_LOG_ERROR("champ de hauteurs : image trop grande pour un seul bloc IDAT");
        return false;
    }
    const size_t total = 8U + 25U + (12U + zlib_bytes) + 12U;
    uint8_t *file = eng_arena_alloc(arena, total, ENG_ASSET_ALIGN);
    if (file == NULL) {
        return false;
    }
    PngWriter w = {.p = file};
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    put_bytes(&w, signature, sizeof signature);

    uint8_t *chunk = w.p;
    put_u32_be(&w, 13U);
    put_bytes(&w, "IHDR", 4);
    put_u32_be(&w, map->size);
    put_u32_be(&w, map->size);
    const uint8_t depth_and_type[5] = {
        16, 0, 0, 0, 0}; /* 16 bits, gris, deflate, filtre 0, sans entrelacement */
    put_bytes(&w, depth_and_type, sizeof depth_and_type);
    put_u32_be(&w, crc32_of(chunk + 4, 4U + 13U));

    chunk = w.p;
    put_u32_be(&w, (uint32_t)zlib_bytes);
    put_bytes(&w, "IDAT", 4);
    *w.p++ = 0x78U; /* zlib : deflate, fenêtre 32 Kio */
    *w.p++ = 0x01U; /* sans dictionnaire, niveau 0 ; (0x78 * 256 + 0x01) est multiple de 31 */
    uint32_t adler_a = 1U;
    uint32_t adler_b = 0U;
    size_t done = 0;
    for (size_t b = 0; b < blocks; b++) {
        const size_t n = raw - done < STORED_BLOCK_MAX ? raw - done : STORED_BLOCK_MAX;
        *w.p++ = (b + 1U == blocks) ? 1U : 0U; /* BFINAL, type 00 : non compressé */
        *w.p++ = (uint8_t)(n & 0xFFU);
        *w.p++ = (uint8_t)(n >> 8);
        *w.p++ = (uint8_t)(~n & 0xFFU);
        *w.p++ = (uint8_t)((~n >> 8) & 0xFFU);
        for (size_t i = 0; i < n; i++) {
            const uint8_t byte = raw_byte(map, done + i);
            *w.p++ = byte;
            adler_a = (adler_a + byte) % 65521U;
            adler_b = (adler_b + adler_a) % 65521U;
        }
        done += n;
    }
    put_u32_be(&w, (adler_b << 16) | adler_a);
    put_u32_be(&w, crc32_of(chunk + 4, 4U + zlib_bytes));

    put_u32_be(&w, 0U);
    put_bytes(&w, "IEND", 4);
    put_u32_be(&w, crc32_of(w.p - 4, 4U));
    *out = file;
    *out_size = (size_t)(w.p - file);
    return true;
}
