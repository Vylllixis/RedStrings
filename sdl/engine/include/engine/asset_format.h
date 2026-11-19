#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

/* Conteneur binaire des assets cuisinés (M2-01) : .mesh, .tex, .skel, .anim. Ce module ne connaît
 * QUE le conteneur (en-tête, table des sections, vérifications) ; le contenu des sections de
 * chaque type est décrit dans docs/asset-formats.md et lu par le module de ce type. Il ne dépend
 * ni du GPU ni de SDL : le cuiseur (tools/assetc, M2-02) et le moteur partagent donc la même
 * définition de la disposition.
 *
 * Disposition d'un fichier (petit-boutiste, toutes les valeurs en octets entiers) :
 *
 *   0   en-tête, 32 octets
 *   32  table des sections, 16 octets par section
 *   ... sections, chacune alignée sur 16 octets (zéros de remplissage), dans l'ordre de la table
 *
 *   En-tête : magic u32 | endian_tag u32 | version u16 | header_size u16 | flags u32 |
 *             file_size u32 | section_count u32 | reserved u32 x2
 *   Section : tag u32 | offset u32 (depuis le début du fichier) | size u32 | count u32
 *
 * Règles vérifiées au chargement (eng_asset_open), dans cet ordre : taille minimale, magic,
 * endian_tag, version EXACTEMENT égale à celle que le moteur sait lire, header_size = 32, flags et
 * reserved à zéro, file_size = taille réelle (détecte troncature et surplus), nombre de sections
 * <= 64, table dans le fichier, puis pour chaque section : tag non nul et unique, offset multiple
 * de 16, offsets croissants sans chevauchement (ni avec la table), section entièrement dans le
 * fichier. Un fichier qui passe ces règles peut être parcouru sans lecture hors du tampon ; le
 * CONTENU des sections (nombres de sommets, indices...) reste à vérifier par le module du type.
 *
 * Version : un seul numéro, augmenté à tout changement qui rend les anciens fichiers illisibles
 * (pas de compatibilité ascendante : le cuiseur régénère les assets, voir docs/asset-formats.md).
 *
 * Petit-boutiste : les sections sont lues en place (pas de conversion), ce qui n'est correct que
 * sur une machine petit-boutiste : toutes les cibles du projet (x86-64, ARM64) le sont. */

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&                                 \
    (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error                                                                                             \
    "Les assets sont petit-boutistes et lus en place : plateforme grand-boutiste non prise en charge"
#endif

/* Quatre caractères ASCII, rangés dans cet ordre dans le fichier (octet 0 = a). */
#define ENG_FOURCC(a, b, c, d)                                                                     \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) |     \
     ((uint32_t)(uint8_t)(d) << 24))

/* Magics : un par type de fichier (« OW » : monde ouvert). */
#define ENG_ASSET_MAGIC_MESH ENG_FOURCC('O', 'W', 'M', 'H')
#define ENG_ASSET_MAGIC_TEX ENG_FOURCC('O', 'W', 'T', 'X')
#define ENG_ASSET_MAGIC_SKEL ENG_FOURCC('O', 'W', 'S', 'K')
#define ENG_ASSET_MAGIC_ANIM ENG_FOURCC('O', 'W', 'A', 'N')
#define ENG_ASSET_MAGIC_HMAP ENG_FOURCC('O', 'W', 'H', 'F') /* champ de hauteurs (M3-01) */
#define ENG_ASSET_MAGIC_SKIN ENG_FOURCC('O', 'W', 'S', 'N') /* poids de skinning (M4-21) */

/* Étiquettes des sections définies par docs/asset-formats.md (INFO et STRS sont communes). */
#define ENG_ASSET_TAG_INFO ENG_FOURCC('I', 'N', 'F', 'O')
#define ENG_ASSET_TAG_STRS ENG_FOURCC('S', 'T', 'R', 'S')
#define ENG_ASSET_TAG_VERT ENG_FOURCC('V', 'E', 'R', 'T')
#define ENG_ASSET_TAG_INDX ENG_FOURCC('I', 'N', 'D', 'X')
#define ENG_ASSET_TAG_PART ENG_FOURCC('P', 'A', 'R', 'T')
#define ENG_ASSET_TAG_MATL ENG_FOURCC('M', 'A', 'T', 'L')
#define ENG_ASSET_TAG_TREF ENG_FOURCC('T', 'R', 'E', 'F')
#define ENG_ASSET_TAG_MIPS ENG_FOURCC('M', 'I', 'P', 'S')
#define ENG_ASSET_TAG_DATA ENG_FOURCC('D', 'A', 'T', 'A')
#define ENG_ASSET_TAG_JNTS ENG_FOURCC('J', 'N', 'T', 'S')
#define ENG_ASSET_TAG_IBMS ENG_FOURCC('I', 'B', 'M', 'S')
#define ENG_ASSET_TAG_CHAN ENG_FOURCC('C', 'H', 'A', 'N')
#define ENG_ASSET_TAG_TIME ENG_FOURCC('T', 'I', 'M', 'E')
#define ENG_ASSET_TAG_VALS ENG_FOURCC('V', 'A', 'L', 'S')
#define ENG_ASSET_TAG_WGHT ENG_FOURCC('W', 'G', 'H', 'T')

/* Versions actuelles de chaque type (celles que le moteur sait lire). */
#define ENG_ASSET_VERSION_MESH 1U
#define ENG_ASSET_VERSION_TEX 1U
#define ENG_ASSET_VERSION_SKEL 1U
#define ENG_ASSET_VERSION_ANIM 1U
#define ENG_ASSET_VERSION_HMAP 1U
#define ENG_ASSET_VERSION_SKIN 1U

#define ENG_ASSET_ALIGN 16U
#define ENG_ASSET_HEADER_SIZE 32U
#define ENG_ASSET_SECTION_ENTRY_SIZE 16U
#define ENG_ASSET_MAX_SECTIONS 64U
#define ENG_ASSET_ENDIAN_TAG 0x01020304U /* lu 0x04030201 par un lecteur qui se trompe d'ordre */

typedef enum EngAssetStatus {
    ENG_ASSET_OK = 0,
    ENG_ASSET_TOO_SMALL,     /* moins de 32 octets (ou pointeur nul) */
    ENG_ASSET_BAD_MAGIC,     /* ce n'est pas un fichier du type demandé */
    ENG_ASSET_BAD_ENDIAN,    /* écrit par une machine d'un autre boutisme, ou corrompu */
    ENG_ASSET_VERSION_OLDER, /* version du fichier < version lue par le moteur : recuire */
    ENG_ASSET_VERSION_NEWER, /* version du fichier > version lue par le moteur : moteur trop ancien
                              */
    ENG_ASSET_BAD_HEADER,    /* header_size, flags ou reserved inattendus */
    ENG_ASSET_SIZE_MISMATCH, /* file_size de l'en-tête != taille reçue : tronqué ou surplus */
    ENG_ASSET_BAD_TABLE,     /* trop de sections, ou table hors du fichier */
    ENG_ASSET_BAD_TAG,       /* étiquette de section nulle */
    ENG_ASSET_DUPLICATE_TAG, /* deux sections portent la même étiquette */
    ENG_ASSET_SECTION_ALIGN, /* offset non multiple de 16 */
    ENG_ASSET_SECTION_OVERLAP, /* section dans la table, ou qui chevauche la précédente */
    ENG_ASSET_SECTION_RANGE,   /* section qui dépasse la fin du fichier */
} EngAssetStatus;

/* Texte court et stable d'un statut (pour les journaux et les tests). */
const char *eng_asset_status_name(EngAssetStatus status);

/* Fichier vérifié. Les pointeurs visent le tampon fourni à eng_asset_open : il doit rester valide
 * tant que la vue ou une section en est utilisée. Les sections sont alignées sur 16 octets SI le
 * tampon l'est (eng_file_read_all et les arènes le garantissent). */
typedef struct EngAssetView {
    const uint8_t *data;
    size_t size;
    uint32_t magic;
    uint16_t version;
    uint32_t section_count;
} EngAssetView;

typedef struct EngAssetSection {
    uint32_t tag;
    const uint8_t *data; /* début de la section */
    uint32_t size;       /* octets */
    uint32_t count;      /* nombre d'éléments : sens propre à chaque section (docs) */
} EngAssetSection;

/* Vérifie un fichier en mémoire. `magic` et `version` : ceux du type attendu (ENG_ASSET_MAGIC_*,
 * ENG_ASSET_VERSION_*, version >= 1). Retourne ENG_ASSET_OK et remplit `out`, ou un statut
 * d'erreur, journalisé avec `name` (chemin ou nom du fichier, NULL accepté) et `out` mis à zéro.
 * Ne lit jamais hors de [data, data + size). */
EngAssetStatus eng_asset_open(const void *data, size_t size, uint32_t magic, uint16_t version,
                              const char *name, EngAssetView *out);

/* Même vérification, sans journal (tests, outils qui produisent leur propre message). */
EngAssetStatus eng_asset_check(const void *data, size_t size, uint32_t magic, uint16_t version,
                               EngAssetView *out);

/* Lit le fichier `path` (relatif à l'exécutable, engine/files.h) dans `arena` puis le vérifie.
 * false : fichier absent ou illisible (journalisé par files.c) ou refusé par eng_asset_open. */
bool eng_asset_load(EngArena *arena, const char *path, uint32_t magic, uint16_t version,
                    EngAssetView *out);

/* Section d'indice `index` (ordre de la table) ; false si hors de [0, section_count). */
bool eng_asset_section_at(const EngAssetView *view, uint32_t index, EngAssetSection *out);

/* Section d'étiquette `tag` ; false si absente. */
bool eng_asset_find_section(const EngAssetView *view, uint32_t tag, EngAssetSection *out);

/* ── Écriture (cuiseur, tests) ── */

typedef struct EngAssetSectionDesc {
    uint32_t tag;
    const void *data; /* `size` octets ; peut être NULL si size == 0 */
    uint32_t size;
    uint32_t count;
} EngAssetSectionDesc;

/* Taille du fichier produit par eng_asset_build pour ces sections, ou 0 si elles sont invalides
 * (plus de 64, étiquette nulle ou en double, données NULL avec une taille, fichier de plus de
 * 4 Gio). Sections dans l'ordre donné : c'est l'ordre du fichier. */
size_t eng_asset_file_size(const EngAssetSectionDesc *sections, uint32_t count);

/* Écrit le fichier dans `out` (`capacity` octets, au moins eng_asset_file_size). Le remplissage est
 * à zéro : mêmes entrées, mêmes octets (le cuiseur en dépend pour ne pas recuire pour rien).
 * `written` (facultatif) reçoit la taille. false, rien d'écrit, si les sections sont invalides,
 * `version` est 0 ou `capacity` est trop petit. */
bool eng_asset_build(uint32_t magic, uint16_t version, const EngAssetSectionDesc *sections,
                     uint32_t count, void *out, size_t capacity, size_t *written);
