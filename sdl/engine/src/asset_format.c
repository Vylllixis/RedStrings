#include "engine/asset_format.h"

#include <string.h>

#include "engine/assert.h"
#include "engine/files.h"
#include "engine/log.h"

/* Lectures et écritures octet par octet : aucun problème d'alignement ni de boutisme de l'hôte pour
 * l'en-tête et la table (seul le CONTENU des sections est lu en place). */
static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static uint64_t align_up(uint64_t value)
{
    return (value + (ENG_ASSET_ALIGN - 1U)) & ~(uint64_t)(ENG_ASSET_ALIGN - 1U);
}

const char *eng_asset_status_name(EngAssetStatus status)
{
    switch (status) {
    case ENG_ASSET_OK:
        return "ok";
    case ENG_ASSET_TOO_SMALL:
        return "too_small";
    case ENG_ASSET_BAD_MAGIC:
        return "bad_magic";
    case ENG_ASSET_BAD_ENDIAN:
        return "bad_endian";
    case ENG_ASSET_VERSION_OLDER:
        return "version_older";
    case ENG_ASSET_VERSION_NEWER:
        return "version_newer";
    case ENG_ASSET_BAD_HEADER:
        return "bad_header";
    case ENG_ASSET_SIZE_MISMATCH:
        return "size_mismatch";
    case ENG_ASSET_BAD_TABLE:
        return "bad_table";
    case ENG_ASSET_BAD_TAG:
        return "bad_tag";
    case ENG_ASSET_DUPLICATE_TAG:
        return "duplicate_tag";
    case ENG_ASSET_SECTION_ALIGN:
        return "section_align";
    case ENG_ASSET_SECTION_OVERLAP:
        return "section_overlap";
    case ENG_ASSET_SECTION_RANGE:
        return "section_range";
    }
    return "unknown";
}

/* ── Vérification ── */

EngAssetStatus eng_asset_check(const void *data, size_t size, uint32_t magic, uint16_t version,
                               EngAssetView *out)
{
    ENG_ASSERT_MSG(version >= 1U, "les versions commencent à 1");
    if (out != NULL) {
        *out = (EngAssetView){0};
    }
    if (data == NULL || size < ENG_ASSET_HEADER_SIZE) {
        return ENG_ASSET_TOO_SMALL;
    }
    const uint8_t *bytes = data;
    if (get_u32(bytes + 0) != magic) {
        return ENG_ASSET_BAD_MAGIC;
    }
    if (get_u32(bytes + 4) != ENG_ASSET_ENDIAN_TAG) {
        return ENG_ASSET_BAD_ENDIAN;
    }
    const uint16_t file_version = get_u16(bytes + 8);
    if (file_version != version) {
        return file_version < version ? ENG_ASSET_VERSION_OLDER : ENG_ASSET_VERSION_NEWER;
    }
    if (get_u16(bytes + 10) != ENG_ASSET_HEADER_SIZE || get_u32(bytes + 12) != 0U ||
        get_u32(bytes + 24) != 0U || get_u32(bytes + 28) != 0U) {
        return ENG_ASSET_BAD_HEADER;
    }
    if (get_u32(bytes + 16) !=
        size) { /* file_size est un u32 : un fichier de 4 Gio ou plus échoue */
        return ENG_ASSET_SIZE_MISMATCH;
    }
    const uint32_t count = get_u32(bytes + 20);
    if (count > ENG_ASSET_MAX_SECTIONS) {
        return ENG_ASSET_BAD_TABLE;
    }
    const uint64_t table_end =
        (uint64_t)ENG_ASSET_HEADER_SIZE + (uint64_t)count * ENG_ASSET_SECTION_ENTRY_SIZE;
    if (table_end > size) {
        return ENG_ASSET_BAD_TABLE;
    }

    uint64_t min_offset =
        align_up(table_end); /* première position permise pour la section suivante */
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *entry =
            bytes + ENG_ASSET_HEADER_SIZE + (size_t)i * ENG_ASSET_SECTION_ENTRY_SIZE;
        const uint32_t tag = get_u32(entry + 0);
        const uint32_t offset = get_u32(entry + 4);
        const uint32_t section_size = get_u32(entry + 8);
        if (tag == 0U) {
            return ENG_ASSET_BAD_TAG;
        }
        for (uint32_t j = 0; j < i; j++) {
            const uint8_t *other =
                bytes + ENG_ASSET_HEADER_SIZE + (size_t)j * ENG_ASSET_SECTION_ENTRY_SIZE;
            if (get_u32(other) == tag) {
                return ENG_ASSET_DUPLICATE_TAG;
            }
        }
        if ((offset % ENG_ASSET_ALIGN) != 0U) {
            return ENG_ASSET_SECTION_ALIGN;
        }
        if (offset < min_offset) {
            return ENG_ASSET_SECTION_OVERLAP;
        }
        if ((uint64_t)offset + section_size > size) { /* 64 bits : offset + taille ne déborde pas */
            return ENG_ASSET_SECTION_RANGE;
        }
        min_offset = align_up((uint64_t)offset + section_size);
    }

    if (out != NULL) {
        out->data = bytes;
        out->size = size;
        out->magic = magic;
        out->version = file_version;
        out->section_count = count;
    }
    return ENG_ASSET_OK;
}

EngAssetStatus eng_asset_open(const void *data, size_t size, uint32_t magic, uint16_t version,
                              const char *name, EngAssetView *out)
{
    EngAssetView local;
    EngAssetView *target = out != NULL ? out : &local;
    const EngAssetStatus status = eng_asset_check(data, size, magic, version, target);
    if (status == ENG_ASSET_OK) {
        return status;
    }
    const char *shown = name != NULL ? name : "(sans nom)";
    const uint8_t *bytes = data;
    switch (status) {
    case ENG_ASSET_VERSION_OLDER:
        ENG_LOG_ERROR("asset %s : version %u, le moteur lit la version %u : recuire l'asset", shown,
                      (unsigned)get_u16(bytes + 8), (unsigned)version);
        break;
    case ENG_ASSET_VERSION_NEWER:
        ENG_LOG_ERROR(
            "asset %s : version %u, le moteur ne lit que la version %u : moteur trop ancien", shown,
            (unsigned)get_u16(bytes + 8), (unsigned)version);
        break;
    case ENG_ASSET_BAD_MAGIC:
        ENG_LOG_ERROR("asset %s : type de fichier inattendu (magic 0x%08X, attendu 0x%08X)", shown,
                      (unsigned)get_u32(bytes), (unsigned)magic);
        break;
    default:
        ENG_LOG_ERROR("asset %s : fichier refusé (%s)", shown, eng_asset_status_name(status));
        break;
    }
    return status;
}

bool eng_asset_load(EngArena *arena, const char *path, uint32_t magic, uint16_t version,
                    EngAssetView *out)
{
    EngFileData file;
    if (!eng_file_read_all(arena, path, &file)) {
        if (out != NULL) {
            *out = (EngAssetView){0};
        }
        return false;
    }
    return eng_asset_open(file.data, file.size, magic, version, path, out) == ENG_ASSET_OK;
}

bool eng_asset_section_at(const EngAssetView *view, uint32_t index, EngAssetSection *out)
{
    if (view == NULL || view->data == NULL || index >= view->section_count) {
        return false;
    }
    const uint8_t *entry =
        view->data + ENG_ASSET_HEADER_SIZE + (size_t)index * ENG_ASSET_SECTION_ENTRY_SIZE;
    if (out != NULL) {
        out->tag = get_u32(entry + 0);
        out->data = view->data + get_u32(entry + 4);
        out->size = get_u32(entry + 8);
        out->count = get_u32(entry + 12);
    }
    return true;
}

bool eng_asset_find_section(const EngAssetView *view, uint32_t tag, EngAssetSection *out)
{
    if (view == NULL || view->data == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < view->section_count; i++) {
        EngAssetSection section;
        if (eng_asset_section_at(view, i, &section) && section.tag == tag) {
            if (out != NULL) {
                *out = section;
            }
            return true;
        }
    }
    return false;
}

/* ── Écriture ── */

/* Calcule les offsets (un par section) et la taille totale. false si les sections sont invalides.
 */
static bool layout(const EngAssetSectionDesc *sections, uint32_t count, uint32_t *offsets,
                   uint64_t *total)
{
    if (count > ENG_ASSET_MAX_SECTIONS || (count > 0U && sections == NULL)) {
        return false;
    }
    uint64_t cursor =
        (uint64_t)ENG_ASSET_HEADER_SIZE + (uint64_t)count * ENG_ASSET_SECTION_ENTRY_SIZE;
    uint64_t end = cursor;
    for (uint32_t i = 0; i < count; i++) {
        const EngAssetSectionDesc *s = &sections[i];
        if (s->tag == 0U || (s->data == NULL && s->size > 0U)) {
            return false;
        }
        for (uint32_t j = 0; j < i; j++) {
            if (sections[j].tag == s->tag) {
                return false;
            }
        }
        cursor = align_up(cursor);
        if (cursor > UINT32_MAX) {
            return false;
        }
        offsets[i] = (uint32_t)cursor;
        cursor += s->size;
        end = cursor;
        if (end > UINT32_MAX) { /* file_size et les offsets tiennent sur 32 bits */
            return false;
        }
    }
    *total = end;
    return true;
}

size_t eng_asset_file_size(const EngAssetSectionDesc *sections, uint32_t count)
{
    uint32_t offsets[ENG_ASSET_MAX_SECTIONS];
    uint64_t total = 0;
    if (!layout(sections, count, offsets, &total)) {
        return 0;
    }
    return (size_t)total;
}

bool eng_asset_build(uint32_t magic, uint16_t version, const EngAssetSectionDesc *sections,
                     uint32_t count, void *out, size_t capacity, size_t *written)
{
    uint32_t offsets[ENG_ASSET_MAX_SECTIONS];
    uint64_t total = 0;
    if (version == 0U || out == NULL || !layout(sections, count, offsets, &total) ||
        total > capacity) {
        return false;
    }
    uint8_t *bytes = out;
    memset(bytes, 0, (size_t)total); /* remplissage à zéro : sortie déterministe */
    put_u32(bytes + 0, magic);
    put_u32(bytes + 4, ENG_ASSET_ENDIAN_TAG);
    put_u16(bytes + 8, version);
    put_u16(bytes + 10, (uint16_t)ENG_ASSET_HEADER_SIZE);
    put_u32(bytes + 16, (uint32_t)total);
    put_u32(bytes + 20, count);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t *entry = bytes + ENG_ASSET_HEADER_SIZE + (size_t)i * ENG_ASSET_SECTION_ENTRY_SIZE;
        put_u32(entry + 0, sections[i].tag);
        put_u32(entry + 4, offsets[i]);
        put_u32(entry + 8, sections[i].size);
        put_u32(entry + 12, sections[i].count);
        if (sections[i].size > 0U) {
            memcpy(bytes + offsets[i], sections[i].data, sections[i].size);
        }
    }
    if (written != NULL) {
        *written = (size_t)total;
    }
    return true;
}
