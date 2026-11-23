/* Allègement d'un maillage : le tas des effondrements candidats (le moins coûteux en tête) et les
 * règles qui rendent un effondrement permis. Voir tree_bake_simplify_interne.h. */
#include <math.h>

#include "tree_bake_simplify_interne.h"

#define FLIP_LIMIT 0.2 /* cosinus minimal entre l'ancienne et la nouvelle normale */

/* ── Tas des effondrements candidats (le moins coûteux en tête) ── */

static void heap_push(Simplifier *s, const Candidate *c)
{
    if (s->heap_count >= s->heap_capacity) {
        return; /* tas plein : ce candidat est oublié (l'allègement s'arrêtera plus tôt) */
    }
    uint32_t i = s->heap_count++;
    while (i > 0U) {
        const uint32_t parent = (i - 1U) / 2U;
        if (s->heap[parent].cost <= c->cost) {
            break;
        }
        s->heap[i] = s->heap[parent];
        i = parent;
    }
    s->heap[i] = *c;
}

Candidate bake_heap_pop(Simplifier *s)
{
    const Candidate top = s->heap[0];
    const Candidate last = s->heap[--s->heap_count];
    uint32_t i = 0;
    for (;;) {
        uint32_t child = (2U * i) + 1U;
        if (child >= s->heap_count) {
            break;
        }
        if (child + 1U < s->heap_count && s->heap[child + 1U].cost < s->heap[child].cost) {
            child++;
        }
        if (last.cost <= s->heap[child].cost) {
            break;
        }
        s->heap[i] = s->heap[child];
        i = child;
    }
    if (s->heap_count > 0U) {
        s->heap[i] = last;
    }
    return top;
}

/* ── Un effondrement est-il permis ? ── */

/* Le sommet de la classe `to` qui partage un triangle avec le sommet `vertex` (de la classe
 * `from`) ; NONE s'il n'y en a pas ou s'il y en a plusieurs. */
static uint32_t partner_of(const Simplifier *s, uint32_t from, uint32_t to, uint32_t vertex)
{
    uint32_t partner = NONE;
    for (uint32_t corner = s->corner_head[from]; corner != NONE; corner = s->corner_next[corner]) {
        const uint32_t t = corner / 3U;
        if (s->triangle_alive[t] == 0U || s->indices[corner] != vertex) {
            continue;
        }
        for (uint32_t k = 0; k < 3U; k++) {
            const uint32_t other = s->indices[(t * 3U) + k];
            if (s->class_of[other] != to) {
                continue;
            }
            if (partner != NONE && partner != other) {
                return NONE; /* deux partenaires différents : ambigu */
            }
            partner = other;
        }
    }
    return partner;
}

/* aucun triangle de `from` (hors ceux qui disparaissent) ne se retourne ni ne s'aplatit */
static bool keeps_orientation(const Simplifier *s, uint32_t from, uint32_t to)
{
    const float *target = class_position(s, to);
    for (uint32_t corner = s->corner_head[from]; corner != NONE; corner = s->corner_next[corner]) {
        const uint32_t t = corner / 3U;
        if (s->triangle_alive[t] == 0U) {
            continue;
        }
        const float *before[3];
        const float *after[3];
        bool vanishes = false;
        for (uint32_t k = 0; k < 3U; k++) {
            const uint32_t c = corner_class(s, (t * 3U) + k);
            vanishes = vanishes || c == to;
            before[k] = class_position(s, c);
            after[k] = c == from ? target : before[k];
        }
        if (vanishes) {
            continue;
        }
        double n0[3];
        double n1[3];
        const double a0 = bake_triangle_normal(before[0], before[1], before[2], n0);
        const double a1 = bake_triangle_normal(after[0], after[1], after[2], n1);
        const double along = (n0[0] * n1[0]) + (n0[1] * n1[1]) + (n0[2] * n1[2]);
        if (a1 <= 1e-20 || along < FLIP_LIMIT * a0 * a1) {
            return false;
        }
    }
    return true;
}

bool bake_can_collapse(const Simplifier *s, uint32_t from, uint32_t to, uint32_t *partners)
{
    if ((s->kind[from] & KIND_BOUNDARY) != 0U && bake_shared_triangles(s, from, to) != 1U) {
        return false; /* une classe du bord ne glisse que le long du bord */
    }
    for (uint32_t v = s->member_head[from]; v != NONE; v = s->member_next[v]) {
        const uint32_t partner = partner_of(s, from, to, v);
        if (partner == NONE) {
            return false;
        }
        if (partners != NULL) {
            partners[v] = partner;
        }
    }
    return keeps_orientation(s, from, to);
}

void bake_push_edge(Simplifier *s, uint32_t a, uint32_t b)
{
    const float *pa = class_position(s, a);
    const float *pb = class_position(s, b);
    const double length2 = ((double)(pa[0] - pb[0]) * (double)(pa[0] - pb[0])) +
                           ((double)(pa[1] - pb[1]) * (double)(pa[1] - pb[1])) +
                           ((double)(pa[2] - pb[2]) * (double)(pa[2] - pb[2]));
    /* les deux sens : garder B (A disparaît) ou garder A ; le moins coûteux qui soit permis */
    const double keep_b = bake_quadric_error(&s->quadric[a], &s->quadric[b], pb);
    const double keep_a = bake_quadric_error(&s->quadric[a], &s->quadric[b], pa);
    const bool a_first = keep_b <= keep_a;
    for (int attempt = 0; attempt < 2; attempt++) {
        const bool remove_a = (attempt == 0) == a_first;
        const uint32_t from = remove_a ? a : b;
        const uint32_t to = remove_a ? b : a;
        if (bake_can_collapse(s, from, to, NULL)) {
            /* un peu de la longueur de l'arête départage les surfaces planes (erreur nulle) */
            const Candidate c = {.cost = (float)((remove_a ? keep_b : keep_a) + (1e-7 * length2)),
                                 .from = from,
                                 .to = to,
                                 .from_version = s->version[from],
                                 .to_version = s->version[to]};
            heap_push(s, &c);
            return;
        }
    }
}
