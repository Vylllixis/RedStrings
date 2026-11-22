#ifndef TOOLS_TREEBAKE_H
#define TOOLS_TREEBAKE_H

/* treebake : ce que se partagent la lecture de l'arbre source (treebake_source.c), le tronc
 * (treebake_trunk.c), le feuillage en cartes (treebake_cards.c), la vue lointaine (treebake_far.c),
 * l'écriture des fichiers (treebake_write.c) et le programme (treebake.c). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/tree_bake.h"

#define TB_MAX_MATERIALS 16U
#define TB_NAME 128U
#define TB_PATH 1024U
#define TB_FAR_VIEWS 4U /* trois vues de côté, une de dessus */

typedef enum TbRole {
    TB_FOLIAGE = 0, /* feuilles et rameaux : rendus sur des cartes */
    TB_TRUNK,       /* tronc et branches : gardés en maillage, allégé */
    TB_DROPPED      /* ignoré */
} TbRole;

typedef struct TbMaterial {
    char name[TB_NAME];
    char color_path[TB_PATH]; /* fichiers de la source ; vide : absent */
    char normal_path[TB_PATH];
    char arm_path[TB_PATH];
    TbRole role;
    uint32_t triangles;
} TbMaterial;

/* L'arbre source, à plat : positions déjà dans le repère du monde de la source. */
typedef struct TbSource {
    EngBakeVertex *vertices;
    uint32_t vertex_count;
    uint32_t *indices;
    uint32_t triangle_count;
    uint8_t *triangle_material;
    float *occlusion; /* par sommet : part du ciel vue (tb_foliage_occlusion) ; NULL avant */
    EngBakeGrid grid; /* la grille dont elle vient (vide sans feuillage) */
    TbMaterial materials[TB_MAX_MATERIALS];
    /* couleur et ARM décodées de chaque matière (cartes, vues lointaines) */
    EngBakeMaterial images[TB_MAX_MATERIALS];
    uint8_t *pixels[TB_MAX_MATERIALS][2]; /* leurs pixels, propriété de la source */
    uint32_t material_count;
} TbSource;

/* Lit les nœuds du fichier glTF dont le nom contient `node_filter` (NULL : tous). `trunk` et
 * `drop` : listes de fragments de noms de matières séparés par des virgules (NULL : aucune) ; les
 * autres matières sont du feuillage. false (message sur stderr) en cas d'échec. */
bool tb_source_load(TbSource *s, const char *path, const char *node_filter, const char *trunk,
                    const char *drop);
void tb_source_free(TbSource *s);
/* Le maillage de la source vu par le cœur (engine/tree_bake.h). */
EngBakeMesh tb_source_mesh(const TbSource *s);

typedef struct TbOptions {
    const char *source;
    const char *out_dir;
    const char *name;
    const char *node;  /* fragment du nom des nœuds à lire, ou NULL */
    const char *trunk; /* fragments des matières gardées en maillage */
    const char *drop;  /* fragments des matières ignorées */
    float height;      /* hauteur finale en mètres ; 0 : celle de la source */
    float sink;        /* part de la hauteur enfoncée dans le sol (le pied ne flotte pas) */
    uint32_t clusters; /* grappes de feuillage : autant d'emplacements de cartes */
    uint32_t bouquets; /* grappes photographiées : les autres reprennent la plus ressemblante */
    uint32_t atlas;    /* côté de l'atlas du feuillage (pixels) */
    uint32_t far_atlas;
    uint32_t trunk_triangles;     /* tronc de près */
    uint32_t trunk_triangles_lod; /* tronc à mi-distance */
    float snow;                   /* neige sur le feuillage, de 0 à 1 */
    float absorption;             /* ombre du feuillage sur lui-même (0 : aucune) */
    uint32_t seed;
} TbOptions;

/* Un maillage produit. */
typedef struct TbMeshOut {
    EngBakeVertex *vertices;
    float *tangents; /* 4 par sommet, ou NULL : le moteur les calcule */
    uint32_t vertex_count;
    uint32_t *indices;
    uint32_t triangle_count;
} TbMeshOut;
void tb_mesh_free(TbMeshOut *m);

/* ── Tronc : une partie par matière « tronc » de la source ── */

typedef struct TbTrunkPart {
    TbMeshOut lod0;         /* de près */
    TbMeshOut lod1;         /* à mi-distance */
    char name[TB_NAME];     /* nom de la matière écrite : bark0, bark1... */
    char material[TB_NAME]; /* nom de la matière de la source */
    char uri[3]
            [TB_NAME]; /* couleur, relief, ARM : fichiers écrits dans la sortie ; vide : absent */
    char source[3][TB_PATH]; /* les fichiers de la source dont ils sont la copie */
} TbTrunkPart;

typedef struct TbTrunk {
    TbTrunkPart parts[TB_MAX_MATERIALS];
    uint32_t count;
} TbTrunk;

/* Allège chaque matière « tronc » et recopie ses images dans le dossier de sortie.
 * Les tangentes écrites portent la part du ciel vue par chaque sommet (voir tb_mesh_add_quad). */
bool tb_trunk_build(TbTrunk *out, const TbSource *s, const TbOptions *o);
void tb_trunk_free(TbTrunk *t);

/* ── Feuillage : grappes, cartes, atlas ── */

typedef struct TbCards {
    TbMeshOut lod0; /* de près : trois cartes par grappe */
    TbMeshOut lod1; /* à mi-distance : deux cartes par grappe, un peu agrandies */
    char color_uri[TB_NAME];
    char normal_uri[TB_NAME];
    char arm_uri[TB_NAME];
    uint32_t clusters;
    uint32_t bouquets;
    float density; /* pixels par mètre dans l'atlas */
    float crown_center[3];
} TbCards;

/* Part du ciel vue par chaque sommet de la source (s->occlusion) et la grille dont elle vient
 * (s->grid) : à appeler avant le tronc, les cartes et la vue lointaine. Sans feuillage, ou avec une
 * absorption nulle : 1 partout, grille vide. */
bool tb_foliage_occlusion(TbSource *s, const TbOptions *o);
/* true sans rien produire (out->lod0.triangle_count == 0) si l'arbre n'a pas de feuillage. */
bool tb_cards_build(TbCards *out, const TbSource *s, const TbOptions *o);
void tb_cards_free(TbCards *c);

/* ── Vue lointaine : l'arbre entier sur trois plans verticaux et un plan horizontal ── */

typedef struct TbFar {
    TbMeshOut mesh;
    char color_uri[TB_NAME];
    char normal_uri[TB_NAME];
    char arm_uri[TB_NAME];
} TbFar;

bool tb_far_build(TbFar *out, const TbSource *s, const TbOptions *o);
void tb_far_free(TbFar *f);

typedef enum TbFlip {
    TB_FLIP_NONE = 0,
    TB_FLIP_U, /* image retournée de gauche à droite */
    TB_FLIP_V  /* image retournée de haut en bas */
} TbFlip;

/* Part du ciel la plus faible écrite dans une tangente : son signe doit rester lisible. */
#define TB_SKY_MIN 0.05F

/* Ajoute à `m` le rectangle de la vue `view` (deux triangles, normale = axis_n), avec les UV du
 * rectangle `rect` d'une image de `atlas_w` x `atlas_h`, éventuellement retournée (`flip` : le
 * relief suit). `m` doit avoir la place (4 sommets, 2 triangles de plus).
 *
 * La tangente écrite porte DEUX informations : xyz, la direction des u croissants ; w, le signe
 * de la bitangente (glTF) MULTIPLIÉ par la part du ciel vue par le sommet (de TB_SKY_MIN à 1 ;
 * `grid` vide ou NULL : 1). Le shader des arbres lit |w| comme une occlusion (le cœur du houppier
 * est plus sombre) : c'est le seul attribut de sommet libre du moteur. */
void tb_mesh_add_quad(TbMeshOut *m, const EngBakeView *view, const EngBakeRect *rect,
                      uint32_t atlas_w, uint32_t atlas_h, TbFlip flip, const EngBakeGrid *grid);

/* ── Écriture ── */

/* Une partie d'un modèle écrit : ses sommets, ses triangles et sa matière. */
typedef struct TbPart {
    const TbMeshOut *mesh;
    const char *material_name;
    const char *color_uri; /* relatifs au fichier .gltf ; NULL ou vide : absent */
    const char *normal_uri;
    const char *arm_uri;
    bool foliage; /* découpe (alpha) et deux faces */
} TbPart;

/* Écrit `<dir>/<name>.gltf` et `<name>.bin`. Les parties vides sont ignorées. */
bool tb_write_gltf(const char *dir, const char *name, const TbPart *parts, uint32_t part_count);
/* Écrit une image RGBA : PNG (avec alpha) ou JPEG (sans, `quality` de 1 à 100). */
bool tb_write_png(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height);
bool tb_write_jpg(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height,
                  int quality);
bool tb_copy_file(const char *from, const char *to);

#endif
