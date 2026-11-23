/* Arbre scanné -> arbre de jeu : allègement d'un maillage par effondrement d'arêtes (erreur
 * quadrique de Garland et Heckbert, 1997). Voir engine/tree_bake.h.
 *
 * Les sommets de même position (dédoublés par une couture d'UV ou une arête vive) forment une
 * CLASSE : la surface est suivie par classes, les attributs par sommets. Effondrer la classe A sur
 * sa voisine B remplace chaque sommet de A par le sommet de B qui partage un triangle avec lui : A
 * prend la position et les attributs de B, rien n'est recalculé. C'est permis si :
 *   - chaque sommet de A a exactement un tel partenaire (une couture ne s'effondre donc que le
 *     long d'elle-même, côté par côté) ;
 *   - une classe du BORD de la surface ne glisse que le long du bord (jamais vers l'intérieur,
 *     ni vers un autre bord : un ruban étroit ne se referme pas sur lui-même) ;
 *   - aucun triangle voisin ne se retourne.
 * Le coût d'un effondrement est l'erreur quadrique de la position gardée : la somme, pondérée par
 * l'aire, des carrés des distances aux plans des triangles que les deux classes ont représentés.
 * Le long d'un bord, le coût retient aussi le contour : chaque arête de bord ajoute à ses deux
 * extrémités un plan perpendiculaire à son triangle et passant par elle. */
#include <math.h>
#include <string.h>

#include "tree_bake_simplify_interne.h"

static size_t align8(size_t n)
{
    return (n + 7U) & ~(size_t)7U;
}

static uint32_t table_size_for(uint32_t vertex_count)
{
    uint32_t size = 16;
    while (size < vertex_count * 2U && size < (1U << 30U)) {
        size *= 2U;
    }
    return size;
}

static uint32_t heap_capacity_for(uint32_t vertex_count, uint32_t triangle_count)
{
    const uint64_t wanted = (3ULL * triangle_count) + (24ULL * vertex_count) + 16ULL;
    return wanted > 0x7FFFFFFFULL ? 0x7FFFFFFFU : (uint32_t)wanted;
}

size_t eng_bake_simplify_work(uint32_t vertex_count, uint32_t triangle_count)
{
    const size_t v = vertex_count;
    const size_t t = triangle_count;
    size_t bytes = 0;
    bytes += align8(v * sizeof(uint32_t)) * 6U; /* tableaux par sommet */
    bytes += align8(t * 3U * sizeof(uint32_t)); /* corner_next */
    bytes += align8((size_t)table_size_for(vertex_count) * sizeof(uint32_t));
    bytes += align8(v * sizeof(Quadric));
    bytes += align8((size_t)heap_capacity_for(vertex_count, triangle_count) * sizeof(Candidate));
    bytes += align8(v) * 2U; /* kind, class_alive */
    bytes += align8(t);      /* triangle_alive */
    return bytes;
}

static void *take(uint8_t **cursor, size_t bytes)
{
    void *block = *cursor;
    *cursor += align8(bytes);
    return block;
}

/* ── L'effondrement ── */

static void push_neighbours(Simplifier *s, uint32_t center)
{
    for (uint32_t corner = s->corner_head[center]; corner != NONE;
         corner = s->corner_next[corner]) {
        const uint32_t t = corner / 3U;
        if (s->triangle_alive[t] == 0U) {
            continue;
        }
        for (uint32_t k = 0; k < 3U; k++) {
            const uint32_t other = corner_class(s, (t * 3U) + k);
            if (other != center) {
                bake_push_edge(s, center, other);
            }
        }
    }
}

static void collapse(Simplifier *s, uint32_t from, uint32_t to, const uint32_t *partners)
{
    /* chaque coin de `from` passe au partenaire de son sommet ; un triangle qui avait les deux
     * classes disparaît */
    for (uint32_t corner = s->corner_head[from]; corner != NONE; corner = s->corner_next[corner]) {
        const uint32_t t = corner / 3U;
        if (s->triangle_alive[t] == 0U) {
            continue;
        }
        bool has_target = false;
        for (uint32_t k = 0; k < 3U; k++) {
            has_target = has_target || corner_class(s, (t * 3U) + k) == to;
        }
        if (has_target) {
            s->triangle_alive[t] = 0U;
            s->alive_triangles--;
        } else {
            s->indices[corner] = partners[s->indices[corner]];
        }
    }
    /* les coins de `from` rejoignent la liste de `to` */
    if (s->corner_head[from] != NONE) {
        if (s->corner_tail[to] == NONE) {
            s->corner_head[to] = s->corner_head[from];
        } else {
            s->corner_next[s->corner_tail[to]] = s->corner_head[from];
        }
        s->corner_tail[to] = s->corner_tail[from];
    }
    s->corner_head[from] = NONE;
    s->corner_tail[from] = NONE;
    for (int i = 0; i < 10; i++) {
        s->quadric[to].m[i] += s->quadric[from].m[i];
    }
    s->kind[to] |= s->kind[from];
    s->class_alive[from] = 0U;
    s->version[to]++;
    s->version[from]++;
    push_neighbours(s, to);
}

uint32_t eng_bake_simplify(const EngBakeVertex *vertices, uint32_t vertex_count, uint32_t *indices,
                           uint32_t triangle_count, uint32_t target_triangles, void *work,
                           size_t work_bytes)
{
    if (vertices == NULL || indices == NULL || work == NULL || vertex_count == 0U ||
        triangle_count == 0U || work_bytes < eng_bake_simplify_work(vertex_count, triangle_count)) {
        return 0;
    }
    for (size_t i = 0; i < (size_t)triangle_count * 3U; i++) {
        if (indices[i] >= vertex_count) {
            return 0;
        }
    }
    Simplifier s = {.vertices = vertices,
                    .indices = indices,
                    .vertex_count = vertex_count,
                    .triangle_count = triangle_count};
    uint8_t *cursor = work;
    s.class_of = take(&cursor, (size_t)vertex_count * sizeof(uint32_t));
    s.member_next = take(&cursor, (size_t)vertex_count * sizeof(uint32_t));
    s.member_head = take(&cursor, (size_t)vertex_count * sizeof(uint32_t));
    s.corner_head = take(&cursor, (size_t)vertex_count * sizeof(uint32_t));
    s.corner_tail = take(&cursor, (size_t)vertex_count * sizeof(uint32_t));
    s.version = take(&cursor, (size_t)vertex_count * sizeof(uint32_t));
    s.corner_next = take(&cursor, (size_t)triangle_count * 3U * sizeof(uint32_t));
    s.table_size = table_size_for(vertex_count);
    s.table = take(&cursor, (size_t)s.table_size * sizeof(uint32_t));
    s.quadric = take(&cursor, (size_t)vertex_count * sizeof(Quadric));
    s.heap_capacity = heap_capacity_for(vertex_count, triangle_count);
    s.heap = take(&cursor, (size_t)s.heap_capacity * sizeof(Candidate));
    s.kind = take(&cursor, vertex_count);
    s.class_alive = take(&cursor, vertex_count);
    s.triangle_alive = take(&cursor, triangle_count);

    bake_build(&s);
    for (uint32_t t = 0; t < triangle_count; t++) {
        if (s.triangle_alive[t] == 0U) {
            continue;
        }
        for (uint32_t k = 0; k < 3U; k++) {
            const uint32_t a = corner_class(&s, (t * 3U) + k);
            const uint32_t b = corner_class(&s, (t * 3U) + ((k + 1U) % 3U));
            if (a < b || bake_shared_triangles(&s, a, b) == 1U) { /* chaque arête une seule fois */
                bake_push_edge(&s, a, b);
            }
        }
    }
    /* la table des positions ne sert plus : elle reçoit les partenaires (par sommet) */
    uint32_t *partners = s.table;
    while (s.alive_triangles > target_triangles && s.heap_count > 0U) {
        const Candidate c = bake_heap_pop(&s);
        if (s.class_alive[c.from] == 0U || s.class_alive[c.to] == 0U ||
            s.version[c.from] != c.from_version || s.version[c.to] != c.to_version) {
            continue; /* périmé : le voisinage a changé depuis */
        }
        if (bake_can_collapse(&s, c.from, c.to, partners)) {
            collapse(&s, c.from, c.to, partners);
        }
    }
    /* triangles restants d'abord */
    uint32_t kept = 0;
    for (uint32_t t = 0; t < triangle_count; t++) {
        if (s.triangle_alive[t] != 0U) {
            for (uint32_t k = 0; k < 3U; k++) {
                indices[(kept * 3U) + k] = indices[(t * 3U) + k];
            }
            kept++;
        }
    }
    return kept;
}
