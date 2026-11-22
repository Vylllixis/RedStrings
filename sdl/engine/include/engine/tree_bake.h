#ifndef ENG_TREE_BAKE_H
#define ENG_TREE_BAKE_H

/* Conversion d'un ARBRE SCANNÉ en arbre de jeu (demande de Théo, 2026-11-22 : « un rendu ultra
 * réaliste des arbres »).
 *
 * Un arbre scanné (Poly Haven, CC0) est une géométrie de film : chaque feuille, chaque aiguille
 * est faite de triangles, des centaines de milliers à plusieurs millions par arbre. Un jeu ne peut
 * pas en dessiner des centaines. La technique habituelle, reprise ici :
 *
 *   1. le FEUILLAGE (feuilles et rameaux) est découpé en GRAPPES de triangles voisins
 *      (eng_bake_cluster) ;
 *   2. chaque grappe reçoit un repère aligné sur sa forme (eng_bake_fit) et devient trois CARTES,
 *      trois rectangles perpendiculaires qui se croisent en son centre : quelle que soit la
 *      direction du regard, l'une d'elles au moins est vue de face ;
 *   3. la grappe est « photographiée » à plat sur chaque carte par un petit traceur de triangles
 *      (eng_bake_raster) : couleur, découpe (alpha = part couverte), relief (normale dans le
 *      repère de la carte), occlusion et rugosité ;
 *   4. les photographies sont rangées dans une seule image, l'ATLAS (eng_bake_pack), et leurs
 *      bords prolongés dans le vide (eng_bake_dilate) pour que les réductions de l'image ne
 *      fassent pas baver du noir ;
 *   5. le TRONC, lui, reste un maillage, allégé (eng_bake_simplify) ;
 *   6. de loin, l'arbre entier est photographié de côté et de dessus sur quelques rectangles ;
 *   7. le cœur du feuillage reçoit moins de lumière du ciel que son pourtour : une grille de
 *      densité (EngBakeGrid) donne à chaque sommet la part du ciel qu'il voit, écrite dans
 *      l'occlusion des photographies.
 *
 * Module pur : ni fichier, ni GPU. L'outil tools/src/treebake.c lit le fichier source et écrit les
 * résultats. Repère : main droite, Y vers le haut, mètres. Tests : tests/test_tree_bake.c. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── Maillage source : une soupe de triangles indexée, une matière par triangle ── */

typedef struct EngBakeVertex {
    float position[3];
    float normal[3]; /* unitaire */
    float uv[2];     /* (0, 0) en haut à gauche de l'image */
} EngBakeVertex;

typedef struct EngBakeImage {
    const uint8_t *rgba; /* width * height * 4 octets, première ligne en haut ; NULL : absente */
    uint32_t width;
    uint32_t height;
} EngBakeImage;

/* Images d'une matière. Absente : couleur blanche, occlusion 1, rugosité 1. */
typedef struct EngBakeMaterial {
    EngBakeImage color; /* couleur de base (sRGB) */
    EngBakeImage arm;   /* R : occlusion, G : rugosité, B : métal (linéaire) */
} EngBakeMaterial;

typedef struct EngBakeMesh {
    const EngBakeVertex *vertices;
    uint32_t vertex_count;
    const uint32_t *indices; /* trois par triangle */
    uint32_t triangle_count;
    const uint8_t *triangle_material; /* par triangle : indice dans `materials` */
    const EngBakeMaterial *materials;
    uint32_t material_count;
    const float *occlusion; /* par sommet : part du ciel vue, de 0 à 1 ; NULL : 1 partout */
} EngBakeMesh;

/* ── Grappes ── */

/* Regroupe `count` points (xyz) en au plus `k` grappes de points voisins (k-moyennes, départ
 * « k-means++ » sur une graine fixe : mêmes entrées, mêmes grappes). `weights` : poids de chaque
 * point (aire du triangle) ou NULL. `assignment[i]` reçoit la grappe du point i, de 0 au nombre
 * renvoyé - 1 ; `centers` (3 * k flottants) reçoit le centre de chaque grappe. Les grappes vides
 * sont retirées : le nombre renvoyé peut être inférieur à `k`. 0 si un paramètre est invalide. */
uint32_t eng_bake_cluster(const float *points, const float *weights, uint32_t count, uint32_t k,
                          uint32_t seed, uint32_t *assignment, float *centers);

/* Repère d'une grappe : son centre, trois axes orthonormés rangés du plus étendu au moins étendu
 * (axes principaux de ses points), et sa demi-étendue le long de chacun. */
typedef struct EngBakeFrame {
    float center[3];
    float axis[3][3]; /* axis[0] : grand axe ; axis[2] : l'épaisseur ; base directe */
    float half[3];    /* demi-étendue le long de chaque axe, mesurée depuis `center` */
} EngBakeFrame;

/* Ajuste le repère des `count` points (xyz, `stride` octets entre deux points). Le centre est le
 * milieu de l'étendue le long de chaque axe : tous les points tiennent dans la boîte
 * [-half, +half]. false si `count` est nul ou un point non fini. */
bool eng_bake_fit(const float *points, size_t stride, uint32_t count, EngBakeFrame *out);

/* ── Photographie d'une grappe ── */

/* Vue orthographique : on regarde le plan (origin ; axis_u, axis_v) depuis le côté +axis_n.
 * L'image couvre [-half_u, +half_u] x [-half_v, +half_v] ; +axis_v est en haut de l'image. */
typedef struct EngBakeView {
    float origin[3];
    float axis_u[3];
    float axis_v[3];
    float axis_n[3]; /* = axis_u x axis_v */
    float half_u;
    float half_v;
} EngBakeView;

/* Cible : trois images de même taille, RGBA 8 bits.
 *   color  : couleur (sRGB) et alpha = part du pixel couverte par la géométrie ;
 *   normal : normale dans le repère de la vue, (x, y, z) * 0,5 + 0,5 (x : +axis_u, y : +axis_v,
 *            z : vers l'observateur), alpha = même couverture ;
 *   arm    : R occlusion, G rugosité, B métal, alpha = même couverture. */
typedef struct EngBakeTarget {
    uint8_t *color;
    uint8_t *normal;
    uint8_t *arm;
    uint32_t width;
    uint32_t height;
} EngBakeTarget;

typedef struct EngBakeRasterOptions {
    uint32_t supersample; /* échantillons par pixel et par axe (1 à 4) ; 0 : 3 */
    float occlusion;      /* multiplie l'occlusion écrite (grappe au cœur de l'arbre) ; 0 : 1 */
    /* Neige : part de blanc ajoutée aux surfaces tournées vers le ciel (normale.y > 0,35), de 0
     * (aucune) à 1. */
    float snow;
} EngBakeRasterOptions;

/* Trace les triangles `triangles[0..count-1]` du maillage (ou tous si `triangles` est NULL :
 * `count` est alors ignoré) dans le rectangle (x, y, w, h) de la cible, vus par `view`. Le plus
 * proche de l'observateur l'emporte ; les deux faces d'un triangle sont visibles. Ce qui dépasse du
 * rectangle est coupé. `scratch` : (w * s) * (h * s) * 16 flottants au moins (s = supersample).
 * false si un paramètre est invalide. Renvoie par `covered` (peut être NULL) la part du rectangle
 * couverte, de 0 à 1. */
bool eng_bake_raster(const EngBakeMesh *mesh, const uint32_t *triangles, uint32_t count,
                     const EngBakeView *view, const EngBakeTarget *target, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const EngBakeRasterOptions *options, float *scratch,
                     float *covered);

/* Prolonge couleur, normale et occlusion des pixels couverts (alpha > 0) dans les pixels vides du
 * rectangle, de proche en proche (`passes` pixels au plus) ; l'alpha des pixels vides reste 0.
 * `work` : w * h octets. */
void eng_bake_dilate(const EngBakeTarget *target, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                     uint32_t passes, uint8_t *work);

/* ── Ombre du feuillage sur lui-même ── */

/* Grille de densité du feuillage, et part du ciel vue depuis chaque case : un rayon qui traverse
 * une longueur L de feuillage de densité d (m² de feuille par m³) garde exp(-absorption * d * L) de
 * sa lumière. Le ciel éclaire surtout d'en haut : une direction compte pour 0,25 (vers le sol) à 1
 * (vers le zénith). */
typedef struct EngBakeGrid {
    float min[3];     /* coin de la grille (m) */
    float cell;       /* côté d'une case (m) */
    uint32_t size[3]; /* cases par axe */
    float *density; /* m² de feuille par m³, size[0] * size[1] * size[2] cases (x le plus rapide) */
    float *sky;     /* part du ciel vue du centre de chaque case, de 0 à 1 */
} EngBakeGrid;

/* Construit la grille de `count` morceaux de feuillage (`points` : centres xyz ; `areas` : aires en
 * m²) : `resolution` cases (de 4 à 96) le long du plus grand côté, plus une case de marge autour.
 * `absorption` : part de la lumière arrêtée par mètre carré de feuille traversé et par m³ (0,5 pour
 * des feuilles orientées au hasard). false (grille vide) si un paramètre est invalide ou si la
 * mémoire manque. À libérer par eng_bake_grid_free. */
bool eng_bake_grid_build(EngBakeGrid *grid, const float *points, const float *areas, uint32_t count,
                         uint32_t resolution, float absorption);
/* Part du ciel vue en `position` (interpolée entre les cases ; hors de la grille : la case la plus
 * proche). 1 si la grille est vide. */
float eng_bake_grid_sky(const EngBakeGrid *grid, const float position[3]);
void eng_bake_grid_free(EngBakeGrid *grid);

/* ── Rangement dans l'atlas ── */

typedef struct EngBakeRect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} EngBakeRect;

/* Range `count` rectangles (w, h donnés, x et y écrits) dans une image de width x height, par
 * étagères, sans chevauchement, à `gap` pixels au moins les uns des autres et du bord. `order` :
 * tableau de travail de `count` éléments. false si tout ne tient pas (x et y sont alors
 * indéfinis) ou si un rectangle est vide. */
bool eng_bake_pack(EngBakeRect *rects, uint32_t count, uint32_t width, uint32_t height,
                   uint32_t gap, uint32_t *order);

/* ── Allègement d'un maillage (le tronc) ── */

/* Réduit le maillage à `target_triangles` triangles environ par effondrement d'arêtes (erreur
 * quadrique de Garland et Heckbert) : chaque étape retire le sommet dont la disparition déforme
 * le moins la surface. Les sommets gardent leurs attributs (un sommet retiré est remplacé par son
 * voisin) : les UV restent valables. Les sommets d'une COUTURE (même position, UV différents) ne
 * s'effondrent que le long de la couture ; ceux d'un BORD ne glissent que le long du
 * bord.
 *
 * Travaille sur place : `indices` est réécrit (triangles restants d'abord) et le nouveau nombre
 * de triangles est renvoyé. Les sommets ne sont pas déplacés ni retirés du tableau (certains ne
 * servent plus : eng_bake_compact). `work_bytes` de mémoire de travail à `work` :
 * eng_bake_simplify_work(vertex_count, triangle_count) au moins. 0 en cas de paramètre invalide. */
uint32_t eng_bake_simplify(const EngBakeVertex *vertices, uint32_t vertex_count, uint32_t *indices,
                           uint32_t triangle_count, uint32_t target_triangles, void *work,
                           size_t work_bytes);
size_t eng_bake_simplify_work(uint32_t vertex_count, uint32_t triangle_count);

/* Retire les sommets inutilisés : `vertices` est tassé, `indices` renumérotés. `remap` : tableau de
 * travail de `vertex_count` éléments. Renvoie le nouveau nombre de sommets. */
uint32_t eng_bake_compact(EngBakeVertex *vertices, uint32_t vertex_count, uint32_t *indices,
                          uint32_t triangle_count, uint32_t *remap);

#endif
