#ifndef TOOLS_TREEBAKE_INTERNE_H
#define TOOLS_TREEBAKE_INTERNE_H

/* treebake : ce que se partagent les fichiers du feuillage et de la vue lointaine : les grappes
 * (treebake_foliage.c) et l'atlas (treebake_atlas.c). */

#include "treebake.h"

#define TB_PLANES 3U /* cartes par grappe */

typedef struct TbCluster {
    EngBakeFrame frame; /* orienté : axe 0 vers l'extérieur du houppier, axe 2 vers le haut */
    float leaf_area;    /* aire de son feuillage (m²) */
    uint32_t bouquet;   /* bouquet dont il reprend les photographies */
} TbCluster;

/* Les triangles de la source qui vont sur les cartes (feuilles et rameaux). */
typedef struct TbFoliage {
    uint32_t *triangles; /* numéros dans la source */
    float *centers;      /* 3 par triangle */
    float *areas;        /* m² */
    uint32_t count;
    float crown[3]; /* centre du houppier (pondéré par l'aire) */
    /* après tb_foliage_cluster : */
    TbCluster *clusters;
    uint32_t cluster_count;
    uint32_t *start;  /* par grappe : début de ses triangles dans `sorted` (cluster_count + 1) */
    uint32_t *sorted; /* numéros de triangles de la source, rangés par grappe */
} TbFoliage;

/* true avec f->count == 0 si l'arbre n'a pas de feuillage. */
bool tb_foliage_gather(TbFoliage *f, const TbSource *s);
/* Grappes de triangles voisins (o->clusters au plus), leurs repères orientés. */
bool tb_foliage_cluster(TbFoliage *f, const TbSource *s, const TbOptions *o);
void tb_foliage_free(TbFoliage *f);

typedef struct TbAtlas {
    EngBakeTarget target; /* couleur + découpe, relief, occlusion / rugosité */
    float *scratch;       /* mémoire de travail du traceur, agrandie au besoin */
    size_t scratch_floats;
} TbAtlas;

bool tb_atlas_init(TbAtlas *a, uint32_t width, uint32_t height);
void tb_atlas_free(TbAtlas *a);
/* Donne à chaque vue un rectangle de l'image (rects[i]), à la plus grande densité commune (pixels
 * par mètre) pour laquelle tout tient. Renvoie cette densité, 0 si rien ne tient. */
float tb_atlas_layout(const EngBakeView *views, uint32_t count, uint32_t width, uint32_t height,
                      EngBakeRect *rects);
/* Photographie les triangles donnés (tous si `triangles` est NULL) dans le rectangle. */
bool tb_atlas_shoot(TbAtlas *a, const EngBakeMesh *mesh, const uint32_t *triangles, uint32_t count,
                    const EngBakeView *view, const EngBakeRect *rect, float snow, float *covered);
/* Après toutes les photographies : couleur moyenne dans les vides, bords prolongés. */
bool tb_atlas_finish(TbAtlas *a, const EngBakeRect *rects, uint32_t count);
/* Écrit <nom>_<suffixe>.png, _n.jpg et _arm.jpg ; les trois noms (TB_NAME octets) sont renvoyés. */
bool tb_atlas_write(const TbAtlas *a, const TbOptions *o, const char *suffix, char *color_uri,
                    char *normal_uri, char *arm_uri);

/* Maillage de `quads` rectangles au plus (vide au départ). */
bool tb_mesh_alloc(TbMeshOut *m, uint32_t quads);

#endif
