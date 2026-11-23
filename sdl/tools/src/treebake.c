/* treebake : convertit un ARBRE SCANNÉ (glTF de plusieurs millions de triangles, par exemple un
 * arbre de Poly Haven, CC0) en arbre de jeu. Voir engine/tree_bake.h et docs/arbres-realistes.md.
 *
 *   treebake <source.gltf> <dossier> <nom> [options]
 *
 *   --node FRAGMENT   ne lire que les nœuds dont le nom contient FRAGMENT
 *   --trunk a,b       matières gardées en maillage (fragments de noms) ; les autres : feuillage
 *   --drop a,b        matières ignorées
 *   --height M        hauteur finale en mètres (défaut : celle de la source)
 *   --sink P          part de la hauteur enfoncée dans le sol (défaut 0.02)
 *   --clusters N      grappes de feuillage, autant d'emplacements de cartes (défaut 200)
 *   --bouquets N      grappes photographiées, reprises par les autres (défaut 20)
 *   --atlas N         côté de l'atlas du feuillage, en pixels (défaut 1024)
 *   --far-atlas N     côté de l'atlas de la vue lointaine (défaut 512)
 *   --trunk-tris N    triangles du tronc de près (défaut 8000)
 *   --trunk-lod N     triangles du tronc à mi-distance (défaut 1500)
 *   --snow X          neige sur le feuillage, de 0 à 1 (défaut 0)
 *   --absorption X    ombre du feuillage sur lui-même (défaut 0.5 ; 0 : aucune)
 *   --seed N          graine des grappes (défaut 1)
 *
 * Écrit dans <dossier> : <nom>.gltf et <nom>.bin, UN modèle dont les parties sont, dans l'ordre,
 * l'arbre de près (tronc allégé, feuillage en cartes), le même à mi-distance (plus léger) et la vue
 * lointaine (quatre rectangles) ; et leurs images.
 * Code de retour : 0 réussite, 1 échec, 2 mauvais usage. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "treebake.h"

#define BASE_BAND 0.08F /* part basse de l'arbre qui donne la position du pied */

static int usage(void)
{
    (void)fprintf(stderr, "usage : treebake <source.gltf> <dossier> <nom> [--node F] [--trunk a,b] "
                          "[--drop a,b] [--height M] [--sink P] [--clusters N] [--bouquets N] "
                          "[--atlas N] [--far-atlas N] [--trunk-tris N] [--trunk-lod N] [--snow X] "
                          "[--absorption X] [--seed N]\n");
    return 2;
}

static bool parse_uint(const char *text, uint32_t low, uint32_t high, uint32_t *out)
{
    char *end = NULL;
    const unsigned long value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value < low || value > high) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

static bool parse_float(const char *text, float low, float high, float *out)
{
    char *end = NULL;
    const float value = strtof(text, &end);
    if (end == text || *end != '\0' || !(value >= low && value <= high)) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse(int argc, char **argv, TbOptions *o)
{
    *o = (TbOptions){.sink = 0.02F,
                     .clusters = 200U,
                     .bouquets = 20U,
                     .atlas = 1024U,
                     .far_atlas = 512U,
                     .trunk_triangles = 8000U,
                     .trunk_triangles_lod = 1500U,
                     .absorption = 0.5F,
                     .seed = 1U};
    if (argc < 4) {
        return false;
    }
    o->source = argv[1];
    o->out_dir = argv[2];
    o->name = argv[3];
    if (strlen(o->name) == 0U || strlen(o->name) > 48U || strchr(o->name, '/') != NULL) {
        return false;
    }
    for (int i = 4; i + 1 < argc; i += 2) {
        const char *key = argv[i];
        const char *value = argv[i + 1];
        bool ok = true;
        if (strcmp(key, "--node") == 0) {
            o->node = value;
        } else if (strcmp(key, "--trunk") == 0) {
            o->trunk = value;
        } else if (strcmp(key, "--drop") == 0) {
            o->drop = value;
        } else if (strcmp(key, "--height") == 0) {
            ok = parse_float(value, 0.1F, 200.0F, &o->height);
        } else if (strcmp(key, "--sink") == 0) {
            ok = parse_float(value, 0.0F, 0.5F, &o->sink);
        } else if (strcmp(key, "--clusters") == 0) {
            ok = parse_uint(value, 1U, 4096U, &o->clusters);
        } else if (strcmp(key, "--bouquets") == 0) {
            ok = parse_uint(value, 1U, 256U, &o->bouquets);
        } else if (strcmp(key, "--atlas") == 0) {
            ok = parse_uint(value, 64U, 8192U, &o->atlas);
        } else if (strcmp(key, "--far-atlas") == 0) {
            ok = parse_uint(value, 64U, 4096U, &o->far_atlas);
        } else if (strcmp(key, "--trunk-tris") == 0) {
            ok = parse_uint(value, 1U, 2000000U, &o->trunk_triangles);
        } else if (strcmp(key, "--trunk-lod") == 0) {
            ok = parse_uint(value, 1U, 2000000U, &o->trunk_triangles_lod);
        } else if (strcmp(key, "--snow") == 0) {
            ok = parse_float(value, 0.0F, 1.0F, &o->snow);
        } else if (strcmp(key, "--absorption") == 0) {
            ok = parse_float(value, 0.0F, 8.0F, &o->absorption);
        } else if (strcmp(key, "--seed") == 0) {
            ok = parse_uint(value, 0U, 0xFFFFFFFFU, &o->seed);
        } else {
            ok = false;
        }
        if (!ok) {
            (void)fprintf(stderr, "treebake : option « %s %s » invalide\n", key, value);
            return false;
        }
    }
    return (argc - 4) % 2 == 0;
}

static bool kept(const TbSource *s, uint32_t triangle)
{
    const uint32_t m = s->triangle_material[triangle];
    return m < s->material_count && s->materials[m].role != TB_DROPPED;
}

/* Pose l'arbre dans le repère du jeu : le pied du tronc en (0, 0, 0), légèrement enfoncé, et la
 * hauteur demandée. Renvoie la hauteur finale, 0 si l'arbre est vide. */
static float place(TbSource *s, const TbOptions *o)
{
    float low = 0.0F;
    float high = 0.0F;
    bool any = false;
    for (uint32_t t = 0; t < s->triangle_count; t++) {
        for (uint32_t k = 0; kept(s, t) && k < 3U; k++) {
            const float y = s->vertices[s->indices[((size_t)t * 3U) + k]].position[1];
            low = !any || y < low ? y : low;
            high = !any || y > high ? y : high;
            any = true;
        }
    }
    if (!any || high - low < 1e-6F) {
        return 0.0F;
    }
    /* le pied : centre des sommets du bas du tronc (de tout l'arbre s'il n'a pas de tronc) */
    double sum[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
    double count[2] = {0.0, 0.0};
    for (uint32_t t = 0; t < s->triangle_count; t++) {
        for (uint32_t k = 0; kept(s, t) && k < 3U; k++) {
            const float *p = s->vertices[s->indices[((size_t)t * 3U) + k]].position;
            if (p[1] > low + (BASE_BAND * (high - low))) {
                continue;
            }
            const int trunk = s->materials[s->triangle_material[t]].role == TB_TRUNK ? 1 : 0;
            sum[trunk][0] += (double)p[0];
            sum[trunk][1] += (double)p[2];
            count[trunk] += 1.0;
        }
    }
    const int from = count[1] > 0.0 ? 1 : 0;
    const float origin[3] = {count[from] > 0.0 ? (float)(sum[from][0] / count[from]) : 0.0F,
                             low + (o->sink * (high - low)),
                             count[from] > 0.0 ? (float)(sum[from][1] / count[from]) : 0.0F};
    const float scale = o->height > 0.0F ? o->height / (high - low) : 1.0F;
    for (uint32_t v = 0; v < s->vertex_count; v++) {
        for (int a = 0; a < 3; a++) {
            s->vertices[v].position[a] = (s->vertices[v].position[a] - origin[a]) * scale;
        }
    }
    return (high - low) * scale;
}

/* Un seul modèle, ses parties dans l'ordre : celles de près (une par matière du tronc, puis le
 * feuillage), les MÊMES à mi-distance, puis la vue lointaine. Les trois niveaux partagent leurs
 * images. Le jeu retrouve les niveaux par ce rangement : 2 P + 1 parties (world_trees_real.c). */
static bool write_model(const TbOptions *o, const TbTrunk *trunk, const TbCards *cards,
                        const TbFar *distant)
{
    TbPart parts[(2U * (TB_MAX_MATERIALS + 1U)) + 1U];
    uint32_t n = 0;
    for (uint32_t lod = 0; lod < 2U; lod++) {
        for (uint32_t p = 0; p < trunk->count; p++) {
            const TbTrunkPart *t = &trunk->parts[p];
            parts[n++] = (TbPart){.mesh = lod == 0U ? &t->lod0 : &t->lod1,
                                  .material_name = t->name,
                                  .color_uri = t->uri[0],
                                  .normal_uri = t->uri[1],
                                  .arm_uri = t->uri[2]};
        }
        parts[n++] = (TbPart){.mesh = lod == 0U ? &cards->lod0 : &cards->lod1,
                              .material_name = "leaves",
                              .color_uri = cards->color_uri,
                              .normal_uri = cards->normal_uri,
                              .arm_uri = cards->arm_uri,
                              .foliage = true};
    }
    parts[n++] = (TbPart){.mesh = &distant->mesh,
                          .material_name = "far",
                          .color_uri = distant->color_uri,
                          .normal_uri = distant->normal_uri,
                          .arm_uri = distant->arm_uri,
                          .foliage = true};
    return tb_write_gltf(o->out_dir, o->name, parts, n);
}

int main(int argc, char **argv)
{
    static TbSource source; /* quelques dizaines de Kio : hors pile */
    static TbTrunk trunk;
    static TbCards cards;
    static TbFar distant;
    TbOptions o;
    if (!parse(argc, argv, &o)) {
        return usage();
    }
    if (!tb_source_load(&source, o.source, o.node, o.trunk, o.drop)) {
        return 1;
    }
    static const char *const role_name[] = {"feuillage", "tronc", "ignorée"};
    for (uint32_t m = 0; m < source.material_count; m++) {
        if (source.materials[m].triangles > 0U) {
            (void)printf("matière %s : %u triangles, %s\n", source.materials[m].name,
                         (unsigned)source.materials[m].triangles,
                         role_name[source.materials[m].role]);
        }
    }
    const float height = place(&source, &o);
    bool ok = height > 0.0F;
    if (!ok) {
        (void)fprintf(stderr, "treebake : l'arbre est vide\n");
    }
    ok = ok && tb_foliage_occlusion(&source, &o) && tb_trunk_build(&trunk, &source, &o) &&
         tb_cards_build(&cards, &source, &o) && tb_far_build(&distant, &source, &o) &&
         write_model(&o, &trunk, &cards, &distant);
    if (ok) {
        uint32_t near_triangles = cards.lod0.triangle_count;
        uint32_t lod_triangles = cards.lod1.triangle_count;
        for (uint32_t p = 0; p < trunk.count; p++) {
            near_triangles += trunk.parts[p].lod0.triangle_count;
            lod_triangles += trunk.parts[p].lod1.triangle_count;
        }
        (void)printf("%s : %.2f m de haut ; %u triangles dans la source -> %u de près, %u à "
                     "mi-distance, %u de loin\n",
                     o.name, (double)height, (unsigned)source.triangle_count,
                     (unsigned)near_triangles, (unsigned)lod_triangles,
                     (unsigned)distant.mesh.triangle_count);
    } else {
        (void)fprintf(stderr, "treebake : échec de la conversion de %s\n", o.source);
    }
    tb_far_free(&distant);
    tb_cards_free(&cards);
    tb_trunk_free(&trunk);
    tb_source_free(&source);
    return ok ? 0 : 1;
}
