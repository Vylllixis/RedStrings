#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/asset_format.h"
#include "engine/model.h"

/* Fichiers .mesh (M2-02) : un EngModel cuisiné. Disposition exacte : docs/asset-formats.md §3.
 * Le chargement remplit le MÊME EngModel que le chargement glTF (engine/model.h) : le renderer
 * (engine/model_gpu.h) ne voit aucune différence, d'où un rendu identique par construction.
 *
 * Ce qui change par rapport au glTF : plus d'analyse JSON, plus de calcul de tangentes ni de
 * matrices de nœuds, plus de décodage PNG/JPEG (les textures sont des .tex déjà décodés, voir
 * engine/tex_file.h). Les textures ne sont pas dans le .mesh : il les désigne par chemin (TREF).
 *
 * Le décodeur vérifie TOUT le contenu avant de le copier : nombres cohérents avec les tailles de
 * section, indices < nombre de sommets, parties dans la liste d'indices (multiple de 3), matériaux
 * et textures référencés qui existent, chemins sûrs (relatifs, sans « .. »), facteurs et boîtes
 * finis. Il ne vérifie pas que chaque coordonnée de sommet est un nombre fini (coût proportionnel
 * à la taille du modèle, sans protéger contre autre chose qu'un fichier déjà corrompu). */

/* Une texture désignée par le .mesh. */
typedef struct EngMeshTexture {
    const char *path; /* UTF-8, relatif au dossier du .mesh, séparateur « / » */
    bool srgb; /* true : données de couleur (couleur de base, émissif) ; false : linéaire */
} EngMeshTexture;

/* Résultat du décodage : le modèle (images allouées mais SANS pixels : image_count = texture_count)
 * et la liste des textures, dans le même ordre que model.images. */
typedef struct EngMeshFile {
    EngModel model;
    EngMeshTexture *textures;
    uint32_t texture_count;
} EngMeshFile;

#define ENG_MESH_MAX_TEXTURES 4096U
#define ENG_MESH_MAX_PATH 255U

/* Construit un .mesh depuis `model`. Les champs `*_image` des matériaux sont ici des indices dans
 * `textures` (ou -1), pas dans model->images (qui n'est pas lu). Le fichier est alloué dans
 * `arena` (valide jusqu'à son reset) ; `scratch` sert aux calculs et est restituée. Modèle ou
 * textures invalides (mêmes règles que le décodeur), mémoire insuffisante : false (journalisé),
 * arène restituée. Sortie déterministe : mêmes entrées, mêmes octets. */
bool eng_mesh_encode(EngArena *arena, EngArena *scratch, const EngModel *model,
                     const EngMeshTexture *textures, uint32_t texture_count, const void **out,
                     size_t *out_size);

/* Vérifie puis décode un .mesh en mémoire ; les tableaux sont alloués dans `arena` (le tampon
 * `data` peut être libéré ensuite). `name` : pour le journal. Refusé (false, journalisé) : `out`
 * est mis à zéro et `arena` restituée. */
bool eng_mesh_decode(EngArena *arena, const void *data, size_t size, const char *name,
                     EngMeshFile *out);

/* Charge un modèle cuisiné complet : lit le .mesh (chemin relatif à l'exécutable, comme
 * eng_model_load), puis chaque .tex qu'il désigne (relatif au dossier du .mesh), converti en
 * EngImage. Le résultat est un EngModel ordinaire (eng_model_free pour ses images). `scratch` sert
 * à la lecture des fichiers et est restituée. Échec : false (journalisé), `model` mis à zéro,
 * `arena` restituée, rien n'est à libérer. Une texture au mauvais format (couleur attendue en sRGB
 * et fichier linéaire, ou l'inverse) est refusée : le cuiseur écrit toujours le bon. */
bool eng_mesh_load(EngArena *arena, EngArena *scratch, const char *path, EngModel *model);

/* Comme eng_mesh_load ; `visit` (peut être NULL) reçoit, avant sa lecture, le chemin complet de
 * chaque .tex que le .mesh désigne (celui qu'on passe à eng_file_read_all) : c'est la liste des
 * fichiers dont dépend le modèle (rechargement à chaud, M2-06). Le chemin n'est valide que pendant
 * l'appel. Appelée aussi pour une texture qui échoue ensuite. */
typedef void (*EngMeshFileVisitor)(void *user, const char *path);
bool eng_mesh_load_visit(EngArena *arena, EngArena *scratch, const char *path, EngModel *model,
                         EngMeshFileVisitor visit, void *user);

/* ── Préparation de la cuisson ── */

/* Un modèle venu du glTF désigne des IMAGES ; un .mesh désigne des TEXTURES, c'est-à-dire une image
 * dans un espace de couleur (sRGB pour la couleur de base et l'émissif, linéaire pour le reste).
 * Une image utilisée dans les deux rôles donne donc deux textures cuites, chacune avec ses mips
 * calculés dans le bon espace. */
typedef struct EngMeshPlan {
    EngModel model;           /* copie superficielle de la source, matériaux réécrits */
    EngMeshTexture *textures; /* path = « <stem>.<k>.tex », rôle de chaque texture */
    uint32_t *source_images;  /* par texture : indice de son image dans le modèle source */
    uint32_t texture_count;
} EngMeshPlan;

/* Établit la liste des textures utilisées par `source`, dans l'ordre de première utilisation
 * (matériau par matériau : couleur de base, métal/rugosité, normale, occlusion, émissif), sans
 * doublon, et réécrit les indices d'images des matériaux en indices de textures. Les tableaux du
 * plan sont alloués dans `arena` ; ses sommets, index et parties sont ceux de `source`. `stem` :
 * préfixe des noms de fichiers (au plus 200 caractères sûrs). Échec (false, journalisé) : source
 * incohérente, `stem` invalide, arène trop petite ; `plan` mis à zéro, arène restituée. */
bool eng_mesh_plan(EngArena *arena, const EngModel *source, const char *stem, EngMeshPlan *plan);
