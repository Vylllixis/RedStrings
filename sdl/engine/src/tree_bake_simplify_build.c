/* Allègement d'un maillage : l'état de départ. Classes de position (sommets dédoublés par une
 * couture), liste des coins de chaque classe, quadriques d'erreur et bords. Voir
 * tree_bake_simplify_interne.h. */
#include <math.h>
#include <string.h>

#include "tree_bake_simplify_interne.h"

#define BORDER_WEIGHT 8.0 /* poids des plans qui retiennent un bord à sa place */

/* ── Classes de position ── */

static uint32_t hash_position(const float p[3])
{
    uint32_t h = 2166136261U;
    for (int a = 0; a < 3; a++) {
        const float value = p[a] == 0.0F ? 0.0F : p[a]; /* -0 et +0 : la même position */
        uint32_t bits;
        memcpy(&bits, &value, sizeof bits);
        h = (h ^ bits) * 16777619U;
        h ^= h >> 15U;
    }
    return h;
}

static bool same_position(const float a[3], const float b[3])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static void build_classes(Simplifier *s)
{
    memset(s->table, 0xFF, (size_t)s->table_size * sizeof(uint32_t));
    for (uint32_t v = 0; v < s->vertex_count; v++) {
        const float *p = s->vertices[v].position;
        uint32_t slot = hash_position(p) & (s->table_size - 1U);
        while (s->table[slot] != NONE && !same_position(s->vertices[s->table[slot]].position, p)) {
            slot = (slot + 1U) & (s->table_size - 1U);
        }
        s->member_next[v] = NONE;
        s->corner_head[v] = NONE;
        s->corner_tail[v] = NONE;
        s->version[v] = 0;
        s->kind[v] = 0;
        memset(&s->quadric[v], 0, sizeof s->quadric[v]);
        if (s->table[slot] == NONE) { /* nouvelle classe : elle porte le numéro de son 1er sommet */
            s->table[slot] = v;
            s->class_of[v] = v;
            s->member_head[v] = v;
            s->class_alive[v] = 1U;
        } else {
            const uint32_t owner = s->table[slot];
            s->class_of[v] = owner;
            s->member_head[v] = NONE;
            s->class_alive[v] = 0U;
            s->member_next[v] = s->member_head[owner]; /* ajouté en tête de la liste */
            s->member_head[owner] = v;
        }
    }
}

static void build_corners(Simplifier *s)
{
    s->alive_triangles = 0;
    for (uint32_t t = 0; t < s->triangle_count; t++) {
        const uint32_t a = corner_class(s, t * 3U);
        const uint32_t b = corner_class(s, (t * 3U) + 1U);
        const uint32_t c = corner_class(s, (t * 3U) + 2U);
        s->triangle_alive[t] = a != b && b != c && a != c ? 1U : 0U;
        s->alive_triangles += s->triangle_alive[t];
        for (uint32_t k = 0; k < 3U; k++) {
            const uint32_t corner = (t * 3U) + k;
            const uint32_t owner = corner_class(s, corner);
            s->corner_next[corner] = NONE;
            if (s->triangle_alive[t] == 0U) {
                continue;
            }
            if (s->corner_tail[owner] == NONE) {
                s->corner_head[owner] = corner;
            } else {
                s->corner_next[s->corner_tail[owner]] = corner;
            }
            s->corner_tail[owner] = corner;
        }
    }
}

/* ── Quadriques ── */

static void add_plane(Quadric *q, const double n[3], double d, double weight)
{
    q->m[0] += weight * n[0] * n[0];
    q->m[1] += weight * n[0] * n[1];
    q->m[2] += weight * n[0] * n[2];
    q->m[3] += weight * n[0] * d;
    q->m[4] += weight * n[1] * n[1];
    q->m[5] += weight * n[1] * n[2];
    q->m[6] += weight * n[1] * d;
    q->m[7] += weight * n[2] * n[2];
    q->m[8] += weight * n[2] * d;
    q->m[9] += weight * d * d;
}

double bake_quadric_error(const Quadric *a, const Quadric *b, const float p[3])
{
    double m[10];
    for (int i = 0; i < 10; i++) {
        m[i] = a->m[i] + b->m[i];
    }
    const double x = (double)p[0];
    const double y = (double)p[1];
    const double z = (double)p[2];
    const double error = (m[0] * x * x) + (2.0 * m[1] * x * y) + (2.0 * m[2] * x * z) +
                         (2.0 * m[3] * x) + (m[4] * y * y) + (2.0 * m[5] * y * z) +
                         (2.0 * m[6] * y) + (m[7] * z * z) + (2.0 * m[8] * z) + m[9];
    return error > 0.0 ? error : 0.0;
}

double bake_triangle_normal(const float a[3], const float b[3], const float c[3], double n[3])
{
    const double u[3] = {(double)b[0] - (double)a[0], (double)b[1] - (double)a[1],
                         (double)b[2] - (double)a[2]};
    const double v[3] = {(double)c[0] - (double)a[0], (double)c[1] - (double)a[1],
                         (double)c[2] - (double)a[2]};
    n[0] = (u[1] * v[2]) - (u[2] * v[1]);
    n[1] = (u[2] * v[0]) - (u[0] * v[2]);
    n[2] = (u[0] * v[1]) - (u[1] * v[0]);
    return sqrt((n[0] * n[0]) + (n[1] * n[1]) + (n[2] * n[2]));
}

uint32_t bake_shared_triangles(const Simplifier *s, uint32_t a, uint32_t b)
{
    uint32_t count = 0;
    for (uint32_t corner = s->corner_head[a]; corner != NONE; corner = s->corner_next[corner]) {
        const uint32_t t = corner / 3U;
        if (s->triangle_alive[t] == 0U) {
            continue;
        }
        for (uint32_t k = 0; k < 3U; k++) {
            count += corner_class(s, (t * 3U) + k) == b ? 1U : 0U;
        }
    }
    return count;
}

static void build_quadrics(Simplifier *s)
{
    for (uint32_t t = 0; t < s->triangle_count; t++) {
        if (s->triangle_alive[t] == 0U) {
            continue;
        }
        const uint32_t c[3] = {corner_class(s, t * 3U), corner_class(s, (t * 3U) + 1U),
                               corner_class(s, (t * 3U) + 2U)};
        double n[3];
        const double twice_area = bake_triangle_normal(
            class_position(s, c[0]), class_position(s, c[1]), class_position(s, c[2]), n);
        if (twice_area <= 0.0) {
            continue;
        }
        for (int a = 0; a < 3; a++) {
            n[a] /= twice_area;
        }
        const float *p0 = class_position(s, c[0]);
        const double d =
            -((n[0] * (double)p0[0]) + (n[1] * (double)p0[1]) + (n[2] * (double)p0[2]));
        for (int k = 0; k < 3; k++) {
            add_plane(&s->quadric[c[k]], n, d, 0.5 * twice_area);
        }
        /* arête de bord (un seul triangle) : un plan perpendiculaire au triangle, passant par
         * l'arête, retient ses deux extrémités */
        for (int k = 0; k < 3; k++) {
            const uint32_t a = c[k];
            const uint32_t b = c[(k + 1) % 3];
            if (bake_shared_triangles(s, a, b) != 1U) {
                continue;
            }
            s->kind[a] |= KIND_BOUNDARY;
            s->kind[b] |= KIND_BOUNDARY;
            const float *pa = class_position(s, a);
            const float *pb = class_position(s, b);
            const double e[3] = {(double)pb[0] - (double)pa[0], (double)pb[1] - (double)pa[1],
                                 (double)pb[2] - (double)pa[2]};
            double m[3] = {(e[1] * n[2]) - (e[2] * n[1]), (e[2] * n[0]) - (e[0] * n[2]),
                           (e[0] * n[1]) - (e[1] * n[0])};
            const double length = sqrt((m[0] * m[0]) + (m[1] * m[1]) + (m[2] * m[2]));
            if (length <= 0.0) {
                continue;
            }
            for (int i = 0; i < 3; i++) {
                m[i] /= length;
            }
            const double dm =
                -((m[0] * (double)pa[0]) + (m[1] * (double)pa[1]) + (m[2] * (double)pa[2]));
            const double weight = BORDER_WEIGHT * ((e[0] * e[0]) + (e[1] * e[1]) + (e[2] * e[2]));
            add_plane(&s->quadric[a], m, dm, weight);
            add_plane(&s->quadric[b], m, dm, weight);
        }
    }
}

void bake_build(Simplifier *s)
{
    build_classes(s);
    build_corners(s);
    build_quadrics(s);
}
