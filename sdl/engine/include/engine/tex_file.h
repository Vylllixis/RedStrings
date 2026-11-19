#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/asset_format.h"
#include "engine/image.h"

/* Fichiers .tex (M2-02) : une image prête pour le GPU, mips précalculés. Disposition exacte :
 * docs/asset-formats.md §4. Ce module lit et écrit le contenu ; le conteneur (en-tête, table des
 * sections) est celui de engine/asset_format.h.
 *
 * Le lecteur VÉRIFIE tout ce qu'il lit : format connu, dimensions, nombre de niveaux, table MIPS
 * (chaque niveau a exactement la taille calculée, tient dans DATA, sans chevauchement). Il refuse
 * un fichier incohérent au lieu de le lire. Il convertit en EngImage les formats RGBA8
 * (niveau 0) et BC7 (toute la chaîne, compressée) ; les autres formats de l'énumération sont
 * VALIDÉS mais refusés à la conversion. */

typedef enum EngTexFormat {
    ENG_TEX_RGBA8_UNORM = 1,
    ENG_TEX_RGBA8_SRGB = 2,
    ENG_TEX_BC7_UNORM = 3,
    ENG_TEX_BC7_SRGB = 4,
    ENG_TEX_BC5_UNORM = 5,
    ENG_TEX_BC4_UNORM = 6,
    ENG_TEX_RGBA16F = 7,
    ENG_TEX_R8_UNORM = 8,
} EngTexFormat;

#define ENG_TEX_FLAG_CUBE 1U
#define ENG_TEX_MAX_LAYERS 256U

typedef struct EngTexInfo {
    EngTexFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t mip_count;
    uint32_t layer_count;
    uint32_t flags;
} EngTexInfo;

/* Fichier .tex vérifié ; pointe dans le tampon fourni à eng_tex_open. */
typedef struct EngTexView {
    EngTexInfo info;
    const uint8_t *data; /* section DATA */
    uint32_t data_size;
    const uint8_t *mips; /* section MIPS : {offset u32, size u32} par sous-ressource */
} EngTexView;

/* Octets par bloc et dimension du bloc d'un format ; false si `format` est inconnu. */
bool eng_tex_format_block(EngTexFormat format, uint32_t *block_bytes, uint32_t *block_size);

/* Octets d'un niveau de `width` x `height` texels. 0 si le format est inconnu. */
size_t eng_tex_level_bytes(EngTexFormat format, uint32_t width, uint32_t height);

/* Vérifie un fichier .tex en mémoire. false (journalisé avec `name`) s'il est refusé ; `view` est
 * alors mis à zéro. */
bool eng_tex_open(const void *data, size_t size, const char *name, EngTexView *view);

/* Octets de la sous-ressource (couche, niveau) ; false si hors limites. */
bool eng_tex_subresource(const EngTexView *view, uint32_t layer, uint32_t level,
                         const uint8_t **data, uint32_t *size);

/* Convertit la couche 0 en nouvelle image. RGBA8 : le niveau 0 seul (eng_image_create) ; BC7 :
 * toute la chaîne de mips, restée compressée (eng_image_create_encoded). Les autres formats, les
 * cubes et les tableaux de couches BC7 : false (journalisé). `srgb` reçoit true pour RGBA8_SRGB et
 * BC7_SRGB. */
bool eng_tex_to_image(const EngTexView *view, EngImage *out, bool *srgb);

/* Construit un .tex RGBA8 depuis `image` : chaîne de mips complète (moyenne en lumière linéaire
 * pour `srgb`, voir eng_image_generate_mips). Le fichier est alloué dans `arena` (valide jusqu'à
 * son reset) ; `scratch` sert aux calculs et est restituée. false (journalisé) si l'image est
 * invalide ou la mémoire insuffisante ; l'arène est alors restituée. */
bool eng_tex_encode(EngArena *arena, EngArena *scratch, const EngImage *image, bool srgb,
                    const void **out, size_t *out_size);

/* Comme eng_tex_encode, mais en BC7 (M2-03) : chaque niveau de la chaîne de mips est compressé en
 * blocs de 4 x 4 texels sur 16 octets, soit 1 octet par texel contre 4 en RGBA8 (au plus 4 fois
 * moins de mémoire ; un peu moins pour les petits niveaux, arrondis au bloc). Format BC7_SRGB si
 * `srgb` (erreur mesurée de façon perceptuelle), BC7_UNORM sinon (erreur RGB : données). La
 * compression est avec perte ; sortie déterministe. */
bool eng_tex_encode_bc7(EngArena *arena, EngArena *scratch, const EngImage *image, bool srgb,
                        const void **out, size_t *out_size);
