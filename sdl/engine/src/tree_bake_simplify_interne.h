#ifndef ENG_TREE_BAKE_SIMPLIFY_INTERNE_H
#define ENG_TREE_BAKE_SIMPLIFY_INTERNE_H

/* Allègement d'un maillage (engine/tree_bake.h) : ce que se partagent tree_bake_simplify.c (la
 * boucle et l'effondrement), tree_bake_simplify_build.c (classes de position, coins, quadriques) et
 * tree_bake_simplify_edge.c (le tas des candidats et ce qui rend un effondrement permis). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/tree_bake.h"

#define NONE UINT32_MAX
#define KIND_BOUNDARY 1U

typedef struct Quadric { /* matrice symétrique 4 x 4 : xx xy xz xw yy yz yw zz zw ww */
    double m[10];
} Quadric;

typedef struct Candidate { /* effondrement de `from` sur `to`, valable pour ces versions */
    float cost;
    uint32_t from;
    uint32_t to;
    uint32_t from_version;
    uint32_t to_version;
} Candidate;

typedef struct Simplifier {
    const EngBakeVertex *vertices;
    uint32_t *indices;
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t alive_triangles;
    uint32_t *class_of;    /* par sommet */
    uint32_t *member_next; /* par sommet : sommet suivant de sa classe */
    uint32_t *member_head; /* par classe : premier sommet (il donne la position de la classe) */
    uint32_t *corner_next; /* par coin (3 par triangle) : coin suivant de la même classe */
    uint32_t *corner_head; /* par classe */
    uint32_t *corner_tail;
    uint32_t *version; /* par classe : change quand son voisinage change */
    uint32_t *table;   /* table de hachage des positions, puis partenaires d'un effondrement */
    Quadric *quadric;  /* par classe */
    Candidate *heap;
    uint8_t *kind;        /* par classe : KIND_BOUNDARY si elle est sur le bord */
    uint8_t *class_alive; /* par classe */
    uint8_t *triangle_alive;
    uint32_t table_size;
    uint32_t heap_count;
    uint32_t heap_capacity;
} Simplifier;

static inline const float *class_position(const Simplifier *s, uint32_t c)
{
    return s->vertices[c].position; /* tous les sommets d'une classe ont la même position */
}

static inline uint32_t corner_class(const Simplifier *s, uint32_t corner)
{
    return s->class_of[s->indices[corner]];
}

/* Classes de position, listes de coins, quadriques et bords : l'état de départ. */
void bake_build(Simplifier *s);
/* nombre de triangles vivants de la classe `a` qui contiennent aussi la classe `b` */
uint32_t bake_shared_triangles(const Simplifier *s, uint32_t a, uint32_t b);
/* normale (non unitaire) du triangle de sommets a, b, c ; renvoie le double de son aire */
double bake_triangle_normal(const float a[3], const float b[3], const float c[3], double n[3]);
/* erreur de la position `p` pour la somme des quadriques `a` et `b` */
double bake_quadric_error(const Quadric *a, const Quadric *b, const float p[3]);
/* Met dans le tas l'effondrement le moins coûteux de l'arête (a, b), s'il y en a un de permis. */
void bake_push_edge(Simplifier *s, uint32_t a, uint32_t b);
/* `from` peut-il s'effondrer sur `to` ? `partners` (peut être NULL) reçoit, à l'indice de chaque
 * sommet de `from`, son remplaçant. */
bool bake_can_collapse(const Simplifier *s, uint32_t from, uint32_t to, uint32_t *partners);
Candidate bake_heap_pop(Simplifier *s);

#endif
