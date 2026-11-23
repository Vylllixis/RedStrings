#include "engine/tex_file.h"

#include <SDL3/SDL_stdinc.h>
#include <string.h>

#include "engine/bc7.h"
#include "engine/log.h"

/* Disposition sur disque de la section INFO (8 x u32). */
typedef struct DiskTexInfo {
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t mip_count;
    uint32_t layer_count;
    uint32_t flags;
    uint32_t reserved[2];
} DiskTexInfo;

_Static_assert(sizeof(DiskTexInfo) == 32, "INFO d'un .tex : 32 octets");

#define MIP_ENTRY_SIZE 8U
#define TEX_BC7_EFFORT                                                                             \
    0U /* niveau d'effort de bc7enc : 0 = rapide (voir docs/asset-formats.md)                      \
        */

static uint32_t read_u32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v); /* petit-boutiste vérifié à la compilation (asset_format.h) */
    return v;
}

bool eng_tex_format_block(EngTexFormat format, uint32_t *block_bytes, uint32_t *block_size)
{
    uint32_t bytes = 0;
    uint32_t size = 1;
    switch (format) {
    case ENG_TEX_RGBA8_UNORM:
    case ENG_TEX_RGBA8_SRGB:
        bytes = 4;
        break;
    case ENG_TEX_BC7_UNORM:
    case ENG_TEX_BC7_SRGB:
    case ENG_TEX_BC5_UNORM:
        bytes = 16;
        size = 4;
        break;
    case ENG_TEX_BC4_UNORM:
        bytes = 8;
        size = 4;
        break;
    case ENG_TEX_RGBA16F:
        bytes = 8;
        break;
    case ENG_TEX_R8_UNORM:
        bytes = 1;
        break;
    default:
        return false;
    }
    if (block_bytes != NULL) {
        *block_bytes = bytes;
    }
    if (block_size != NULL) {
        *block_size = size;
    }
    return true;
}

size_t eng_tex_level_bytes(EngTexFormat format, uint32_t width, uint32_t height)
{
    uint32_t bytes = 0;
    uint32_t size = 0;
    if (!eng_tex_format_block(format, &bytes, &size)) {
        return 0;
    }
    const size_t blocks_x = ((size_t)width + size - 1U) / size;
    const size_t blocks_y = ((size_t)height + size - 1U) / size;
    return blocks_x * blocks_y * bytes;
}

static uint64_t align16(uint64_t v)
{
    return (v + 15U) & ~(uint64_t)15U;
}

/* ── Lecture ── */

bool eng_tex_open(const void *data, size_t size, const char *name, EngTexView *view)
{
    memset(view, 0, sizeof *view);
    const char *shown = name != NULL ? name : "(sans nom)";
    EngAssetView asset;
    if (eng_asset_open(data, size, ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, name, &asset) !=
        ENG_ASSET_OK) {
        return false;
    }
    EngAssetSection info_section;
    EngAssetSection mips_section;
    EngAssetSection data_section;
    if (!eng_asset_find_section(&asset, ENG_ASSET_TAG_INFO, &info_section) ||
        !eng_asset_find_section(&asset, ENG_ASSET_TAG_MIPS, &mips_section) ||
        !eng_asset_find_section(&asset, ENG_ASSET_TAG_DATA, &data_section)) {
        ENG_LOG_ERROR("tex %s : section INFO, MIPS ou DATA absente", shown);
        return false;
    }
    if (info_section.size != sizeof(DiskTexInfo) || info_section.count != 1U) {
        ENG_LOG_ERROR("tex %s : section INFO de taille inattendue (%u)", shown,
                      (unsigned)info_section.size);
        return false;
    }
    DiskTexInfo disk;
    memcpy(&disk, info_section.data, sizeof disk);
    EngTexInfo info = {.format = (EngTexFormat)disk.format,
                       .width = disk.width,
                       .height = disk.height,
                       .mip_count = disk.mip_count,
                       .layer_count = disk.layer_count,
                       .flags = disk.flags};
    if (!eng_tex_format_block(info.format, NULL, NULL)) {
        ENG_LOG_ERROR("tex %s : format %u inconnu", shown, (unsigned)disk.format);
        return false;
    }
    if (info.width == 0 || info.height == 0 || info.width > ENG_IMAGE_MAX_SIZE ||
        info.height > ENG_IMAGE_MAX_SIZE) {
        ENG_LOG_ERROR("tex %s : dimensions hors limites (%u x %u)", shown, (unsigned)info.width,
                      (unsigned)info.height);
        return false;
    }
    if (info.mip_count == 0 || info.mip_count > eng_image_mip_count(info.width, info.height)) {
        ENG_LOG_ERROR("tex %s : %u niveaux de mips invalides pour %u x %u", shown,
                      (unsigned)info.mip_count, (unsigned)info.width, (unsigned)info.height);
        return false;
    }
    if (info.layer_count == 0 || info.layer_count > ENG_TEX_MAX_LAYERS ||
        (info.flags & ~ENG_TEX_FLAG_CUBE) != 0U || disk.reserved[0] != 0U ||
        disk.reserved[1] != 0U) {
        ENG_LOG_ERROR("tex %s : couches, drapeaux ou champs réservés invalides", shown);
        return false;
    }
    if ((info.flags & ENG_TEX_FLAG_CUBE) != 0U &&
        (info.layer_count != 6U || info.width != info.height)) {
        ENG_LOG_ERROR("tex %s : une cube map a 6 couches carrées", shown);
        return false;
    }

    const uint64_t subresources = (uint64_t)info.layer_count * info.mip_count;
    if (mips_section.count != subresources || mips_section.size != subresources * MIP_ENTRY_SIZE ||
        data_section.size != data_section.count) {
        ENG_LOG_ERROR("tex %s : table MIPS incohérente avec INFO", shown);
        return false;
    }
    /* chaque sous-resource : taille exacte, alignée, dans DATA, sans chevauchement */
    uint64_t cursor = 0;
    for (uint32_t layer = 0; layer < info.layer_count; layer++) {
        for (uint32_t level = 0; level < info.mip_count; level++) {
            const uint8_t *entry =
                mips_section.data + ((size_t)layer * info.mip_count + level) * MIP_ENTRY_SIZE;
            const uint32_t offset = read_u32(entry);
            const uint32_t bytes = read_u32(entry + 4);
            uint32_t w = 0;
            uint32_t h = 0;
            eng_image_mip_size(info.width, info.height, level, &w, &h);
            if (bytes != eng_tex_level_bytes(info.format, w, h) || (offset % 16U) != 0U ||
                offset < cursor || (uint64_t)offset + bytes > data_section.size) {
                ENG_LOG_ERROR("tex %s : sous-ressource (couche %u, niveau %u) invalide", shown,
                              (unsigned)layer, (unsigned)level);
                return false;
            }
            cursor = (uint64_t)offset + bytes;
        }
    }
    view->info = info;
    view->data = data_section.data;
    view->data_size = data_section.size;
    view->mips = mips_section.data;
    return true;
}

bool eng_tex_subresource(const EngTexView *view, uint32_t layer, uint32_t level,
                         const uint8_t **data, uint32_t *size)
{
    if (view->data == NULL || layer >= view->info.layer_count || level >= view->info.mip_count) {
        return false;
    }
    const uint8_t *entry =
        view->mips + ((size_t)layer * view->info.mip_count + level) * MIP_ENTRY_SIZE;
    if (data != NULL) {
        *data = view->data + read_u32(entry);
    }
    if (size != NULL) {
        *size = read_u32(entry + 4);
    }
    return true;
}

/* BC7 : TOUS les niveaux de la couche 0, mis bout à bout dans l'image (le GPU les lit tels quels).
 * Un cube ou un tableau de couches ne se convertit pas en image. */
static bool bc7_to_image(const EngTexView *view, EngImage *out, bool *srgb)
{
    const EngTexInfo *info = &view->info;
    if (info->layer_count != 1U || (info->flags & ENG_TEX_FLAG_CUBE) != 0U) {
        ENG_LOG_ERROR("tex : une image BC7 n'a qu'une couche (ni cube ni tableau)");
        return false;
    }
    const size_t total = eng_image_encoded_chain_size(info->width, info->height, info->mip_count);
    uint8_t *chain = total > 0 ? SDL_malloc(total) : NULL;
    if (chain == NULL) {
        ENG_LOG_ERROR("tex : mémoire insuffisante ou chaîne invalide (%zu octets)", total);
        return false;
    }
    size_t at = 0;
    bool ok = true;
    for (uint32_t level = 0; level < info->mip_count && ok; level++) {
        const uint8_t *src = NULL;
        uint32_t bytes = 0;
        ok = eng_tex_subresource(view, 0, level, &src, &bytes) && at + bytes <= total;
        if (ok) {
            memcpy(chain + at, src, bytes);
            at += bytes;
        }
    }
    const EngImageEncoding encoding =
        info->format == ENG_TEX_BC7_SRGB ? ENG_IMAGE_BC7_SRGB : ENG_IMAGE_BC7;
    ok = ok && at == total &&
         eng_image_create_encoded(info->width, info->height, encoding, info->mip_count, chain,
                                  total, out);
    SDL_free(chain);
    if (ok && srgb != NULL) {
        *srgb = encoding == ENG_IMAGE_BC7_SRGB;
    }
    return ok;
}

bool eng_tex_to_image(const EngTexView *view, EngImage *out, bool *srgb)
{
    *out = (EngImage){0};
    if (view->data == NULL) {
        return false;
    }
    const EngTexFormat format = view->info.format;
    if (format == ENG_TEX_BC7_UNORM || format == ENG_TEX_BC7_SRGB) {
        return bc7_to_image(view, out, srgb);
    }
    if (format != ENG_TEX_RGBA8_UNORM && format != ENG_TEX_RGBA8_SRGB) {
        ENG_LOG_ERROR("tex : le format %u n'est pas convertible en image (RGBA8 et BC7 seulement)",
                      (unsigned)format);
        return false;
    }
    const uint8_t *pixels = NULL;
    uint32_t bytes = 0;
    if (!eng_tex_subresource(view, 0, 0, &pixels, &bytes)) {
        return false;
    }
    if (!eng_image_create(view->info.width, view->info.height, pixels, out)) {
        return false;
    }
    if (srgb != NULL) {
        *srgb = view->info.format == ENG_TEX_RGBA8_SRGB;
    }
    return true;
}

/* ── Écriture ── */

/* Compresse le niveau `w` x `h` (RGBA8, lignes serrées) en blocs BC7 dans `dst`. Les blocs de bord
 * répètent le dernier texel de l'image (aucun texel inventé : pas de couleur parasite). */
static void compress_level_bc7(uint8_t *dst, const uint8_t *src, uint32_t w, uint32_t h,
                               bool perceptual, uint32_t effort)
{
    const uint32_t blocks_x = (w + 3U) / 4U;
    const uint32_t blocks_y = (h + 3U) / 4U;
    for (uint32_t by = 0; by < blocks_y; by++) {
        for (uint32_t bx = 0; bx < blocks_x; bx++) {
            uint8_t texels[64];
            for (uint32_t y = 0; y < 4; y++) {
                const uint32_t sy = by * 4U + y < h ? by * 4U + y : h - 1U;
                for (uint32_t x = 0; x < 4; x++) {
                    const uint32_t sx = bx * 4U + x < w ? bx * 4U + x : w - 1U;
                    memcpy(texels + (size_t)(y * 4U + x) * 4U, src + ((size_t)sy * w + sx) * 4U, 4);
                }
            }
            eng_bc7_encode_block(dst + ((size_t)by * blocks_x + bx) * ENG_BC7_BLOCK_BYTES, texels,
                                 perceptual, effort);
        }
    }
}

static bool encode(EngArena *arena, EngArena *scratch, const EngImage *image, bool srgb,
                   bool compress, const void **out, size_t *out_size)
{
    *out = NULL;
    *out_size = 0;
    if (image->pixels == NULL || image->encoding != ENG_IMAGE_RGBA8 || image->width == 0 ||
        image->height == 0 || image->width > ENG_IMAGE_MAX_SIZE ||
        image->height > ENG_IMAGE_MAX_SIZE) {
        ENG_LOG_ERROR("tex : image invalide (%u x %u)", (unsigned)image->width,
                      (unsigned)image->height);
        return false;
    }
    const uint32_t levels = eng_image_mip_count(image->width, image->height);
    const EngArenaMark arena_mark = eng_arena_mark(arena);
    const EngArenaMark scratch_mark = eng_arena_mark(scratch);
    bool ok = false;

    const size_t base_bytes = (size_t)image->width * image->height * 4U;
    const size_t chain_bytes = eng_image_mip_chain_size(image->width, image->height, levels);
    uint8_t *tail = levels > 1U ? eng_arena_alloc(scratch, chain_bytes - base_bytes, 16) : NULL;
    /* positions des niveaux dans DATA : chacun aligné sur 16 octets */
    uint32_t offsets[32];
    uint32_t sizes[32];
    uint64_t cursor = 0;
    for (uint32_t level = 0; level < levels; level++) {
        uint32_t w = 0;
        uint32_t h = 0;
        eng_image_mip_size(image->width, image->height, level, &w, &h);
        sizes[level] =
            (uint32_t)eng_tex_level_bytes(compress ? ENG_TEX_BC7_UNORM : ENG_TEX_RGBA8_UNORM, w, h);
        cursor = align16(cursor);
        offsets[level] = (uint32_t)cursor;
        cursor += sizes[level];
    }
    if (cursor >
        UINT32_MAX / 2U) { /* ne se produit pas sous ENG_IMAGE_MAX_SIZE, mais ne parie pas */
        ENG_LOG_ERROR("tex : image trop grande");
        goto done;
    }
    uint8_t *data = eng_arena_alloc_zero(scratch, (size_t)cursor, 16);
    uint32_t mip_table[64];
    const EngTexFormat raw_format = srgb ? ENG_TEX_RGBA8_SRGB : ENG_TEX_RGBA8_UNORM;
    const EngTexFormat bc7_format = srgb ? ENG_TEX_BC7_SRGB : ENG_TEX_BC7_UNORM;
    const DiskTexInfo info = {.format = compress ? bc7_format : raw_format,
                              .width = image->width,
                              .height = image->height,
                              .mip_count = levels,
                              .layer_count = 1};
    if (data == NULL || (levels > 1U && tail == NULL)) {
        ENG_LOG_ERROR("tex : mémoire insuffisante");
        goto done;
    }
    if (levels > 1U &&
        !eng_image_generate_mips(image->pixels, image->width, image->height, levels, srgb, tail)) {
        ENG_LOG_ERROR("tex : génération des mips impossible");
        goto done;
    }
    if (compress) {
        eng_bc7_init();
        /* couleurs : erreur perceptuelle ; données (normales...) : erreur RGB */
        compress_level_bc7(data, image->pixels, image->width, image->height, srgb, TEX_BC7_EFFORT);
        size_t from = 0;
        for (uint32_t level = 1; level < levels; level++) {
            uint32_t w = 0;
            uint32_t h = 0;
            eng_image_mip_size(image->width, image->height, level, &w, &h);
            compress_level_bc7(data + offsets[level], tail + from, w, h, srgb, TEX_BC7_EFFORT);
            from += (size_t)w * h * 4U;
        }
    } else {
        memcpy(data, image->pixels, base_bytes);
        size_t from = 0; /* les niveaux 1.. se suivent sans remplissage dans `tail` */
        for (uint32_t level = 1; level < levels; level++) {
            memcpy(data + offsets[level], tail + from, sizes[level]);
            from += sizes[level];
        }
    }
    for (uint32_t level = 0; level < levels; level++) {
        const size_t k = (size_t)level * 2U;
        mip_table[k] = offsets[level];
        mip_table[k + 1U] = sizes[level];
    }
    const EngAssetSectionDesc sections[3] = {
        {ENG_ASSET_TAG_INFO, &info, sizeof info, 1},
        {ENG_ASSET_TAG_MIPS, mip_table, levels * MIP_ENTRY_SIZE, levels},
        {ENG_ASSET_TAG_DATA, data, (uint32_t)cursor, (uint32_t)cursor},
    };
    const size_t file_size = eng_asset_file_size(sections, 3);
    uint8_t *file = file_size > 0 ? eng_arena_alloc(arena, file_size, 16) : NULL;
    if (file == NULL || !eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, sections, 3,
                                         file, file_size, NULL)) {
        ENG_LOG_ERROR("tex : écriture impossible (mémoire insuffisante ?)");
        goto done;
    }
    *out = file;
    *out_size = file_size;
    ok = true;
done:
    eng_arena_reset_to(scratch, scratch_mark);
    if (!ok) {
        eng_arena_reset_to(arena, arena_mark);
    }
    return ok;
}

bool eng_tex_encode(EngArena *arena, EngArena *scratch, const EngImage *image, bool srgb,
                    const void **out, size_t *out_size)
{
    return encode(arena, scratch, image, srgb, false, out, out_size);
}

bool eng_tex_encode_bc7(EngArena *arena, EngArena *scratch, const EngImage *image, bool srgb,
                        const void **out, size_t *out_size)
{
    return encode(arena, scratch, image, srgb, true, out, out_size);
}
