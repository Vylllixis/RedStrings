/* Conversion d'un arbre scanné (engine/tree_bake.h) : l'allègement d'un maillage par effondrement
 * d'arêtes (eng_bake_simplify). Ce que ces tests exigent : moins de triangles, la même forme (les
 * parties planes s'allègent sans erreur avant qu'on touche aux reliefs), un bord qui ne bouge pas,
 * des coutures d'UV respectées, aucun triangle retourné. */
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "engine/tree_bake.h"
#include "utest.h"

#define SIDE 8 /* quadrillages de SIDE x SIDE cases */

typedef struct Mesh {
    EngBakeVertex vertices[1024];
    uint32_t indices[4096 * 3];
    uint32_t vertex_count;
    uint32_t triangle_count;
} Mesh;

static Mesh g_mesh; /* hors pile */

/* Quadrillage de (x0, 0) à (x0 + width, SIDE), hauteur donnée par `height`, UV de u0 à u1 en x. */
static void add_grid(Mesh *m, float x0, float width, float u0, float u1, float (*height)(float))
{
    const uint32_t base = m->vertex_count;
    for (int j = 0; j <= SIDE; j++) {
        for (int i = 0; i <= SIDE; i++) {
            const float t = (float)i / (float)SIDE;
            const float x = x0 + (t * width);
            EngBakeVertex *v = &m->vertices[m->vertex_count++];
            *v = (EngBakeVertex){.position = {x, (float)j, height != NULL ? height(x) : 0.0F},
                                 .normal = {0.0F, 0.0F, 1.0F},
                                 .uv = {u0 + (t * (u1 - u0)), (float)j / (float)SIDE}};
        }
    }
    for (int j = 0; j < SIDE; j++) {
        for (int i = 0; i < SIDE; i++) {
            const uint32_t a = base + (uint32_t)((j * (SIDE + 1)) + i);
            const uint32_t b = a + 1U;
            const uint32_t c = a + (uint32_t)(SIDE + 1);
            const uint32_t d = c + 1U;
            const uint32_t tri[6] = {a, b, d, a, d, c};
            memcpy(&m->indices[(size_t)m->triangle_count * 3U], tri, sizeof tri);
            m->triangle_count += 2U;
        }
    }
}

static Mesh *fresh_mesh(void)
{
    memset(&g_mesh, 0, sizeof g_mesh);
    return &g_mesh;
}

/* allège le maillage ; renvoie le nombre de triangles restants */
static uint32_t simplify(Mesh *m, uint32_t target)
{
    const size_t bytes = eng_bake_simplify_work(m->vertex_count, m->triangle_count);
    void *work = malloc(bytes);
    const uint32_t kept = eng_bake_simplify(m->vertices, m->vertex_count, m->indices,
                                            m->triangle_count, target, work, bytes);
    free(work);
    return kept;
}

typedef struct Measure {
    double area;       /* aire vue de dessus (somme des aires projetées, signées) */
    double volume;     /* volume sous la surface */
    double min_normal; /* plus petite composante z des normales : < 0, un triangle est retourné */
    float low[2];
    float high[2];
} Measure;

static Measure measure(const Mesh *m, uint32_t triangles)
{
    Measure r = {.min_normal = 1.0, .low = {1e9F, 1e9F}, .high = {-1e9F, -1e9F}};
    for (uint32_t t = 0; t < triangles; t++) {
        const float *a = m->vertices[m->indices[(size_t)t * 3U]].position;
        const float *b = m->vertices[m->indices[((size_t)t * 3U) + 1U]].position;
        const float *c = m->vertices[m->indices[((size_t)t * 3U) + 2U]].position;
        const double twice = ((double)(b[0] - a[0]) * (double)(c[1] - a[1])) -
                             ((double)(c[0] - a[0]) * (double)(b[1] - a[1]));
        r.area += 0.5 * twice;
        r.volume += 0.5 * twice * ((double)a[2] + (double)b[2] + (double)c[2]) / 3.0;
        r.min_normal = twice < r.min_normal ? twice : r.min_normal;
        const float *corners[3] = {a, b, c};
        for (int k = 0; k < 3; k++) {
            for (int axis = 0; axis < 2; axis++) {
                r.low[axis] = fminf(r.low[axis], corners[k][axis]);
                r.high[axis] = fmaxf(r.high[axis], corners[k][axis]);
            }
        }
    }
    return r;
}

UTEST(eng_bake_simplify, a_flat_grid_keeps_its_outline_with_far_fewer_triangles)
{
    Mesh *m = fresh_mesh();
    add_grid(m, 0.0F, (float)SIDE, 0.0F, 1.0F, NULL);
    ASSERT_EQ(128U, m->triangle_count);
    const uint32_t kept = simplify(m, 8U);
    ASSERT_GE(kept, 2U);
    ASSERT_LE(kept, 8U); /* un plan : rien n'empêche d'atteindre la cible */
    const Measure r = measure(m, kept);
    ASSERT_NEAR(64.0, r.area, 1e-6); /* le bord n'a pas bougé : même aire, même contour */
    ASSERT_NEAR(0.0F, r.low[0], 1e-6F);
    ASSERT_NEAR(8.0F, r.high[0], 1e-6F);
    ASSERT_NEAR(0.0F, r.low[1], 1e-6F);
    ASSERT_NEAR(8.0F, r.high[1], 1e-6F);
    ASSERT_GT(r.min_normal, 0.0); /* aucun triangle retourné ni aplati */
    /* les triangles restants sont rangés en tête, et leurs index restent valides */
    for (uint32_t i = 0; i < kept * 3U; i++) {
        ASSERT_LT(m->indices[i], m->vertex_count);
    }
}

static float tent(float x)
{
    return 1.0F - (fabsf(x - 4.0F) / 4.0F); /* deux pans qui se rejoignent sur l'arête x = 4 */
}

UTEST(eng_bake_simplify, flat_parts_are_removed_before_any_relief)
{
    Mesh *m = fresh_mesh();
    add_grid(m, 0.0F, (float)SIDE, 0.0F, 1.0F, tent);
    const Measure before = measure(m, m->triangle_count);
    /* deux pans : quatre triangles suffiraient ; pour descendre à six sans rien déformer, il faut
     * effondrer chaque arête dans le bon sens (vers le sommet de l'arête du toit, jamais l'inverse)
     */
    const uint32_t kept = simplify(m, 6U);
    ASSERT_LE(kept, 6U);
    ASSERT_GE(kept, 4U);
    const Measure after = measure(m, kept);
    /* deux pans plans : on peut retirer presque tout sans rien déformer ; un allègement qui
     * toucherait à l'arête avant d'avoir vidé les pans changerait le volume */
    ASSERT_NEAR(before.volume, after.volume, 1e-4);
    ASSERT_NEAR(32.0, after.volume, 1e-4);
    ASSERT_NEAR(64.0, after.area, 1e-6);
    ASSERT_GT(after.min_normal, 0.0);
}

UTEST(eng_bake_simplify, a_texture_seam_is_never_crossed)
{
    /* deux moitiés qui se touchent en x = 4 mais dont les UV ne se suivent pas (0 à 0,4 puis 0,6 à
     * 1) : leurs sommets du milieu ont la même position, pas les mêmes UV */
    Mesh *m = fresh_mesh();
    add_grid(m, 0.0F, 4.0F, 0.0F, 0.4F, NULL);
    const uint32_t first_island = m->vertex_count;
    add_grid(m, 4.0F, 4.0F, 0.6F, 1.0F, NULL);
    ASSERT_EQ(256U, m->triangle_count);
    const uint32_t kept = simplify(m, 12U);
    ASSERT_LE(kept, 16U);
    ASSERT_GE(kept, 4U);
    double area[2] = {0.0, 0.0};
    for (uint32_t t = 0; t < kept; t++) {
        const uint32_t *i = &m->indices[(size_t)t * 3U];
        const bool left = i[0] < first_island;
        /* un triangle n'a jamais un pied de chaque côté de la couture */
        ASSERT_EQ(left, i[1] < first_island);
        ASSERT_EQ(left, i[2] < first_island);
        const float *a = m->vertices[i[0]].position;
        const float *b = m->vertices[i[1]].position;
        const float *c = m->vertices[i[2]].position;
        for (int k = 0; k < 3; k++) { /* et chaque moitié reste de son côté de x = 4 */
            const float x = m->vertices[i[k]].position[0];
            ASSERT_TRUE(left ? x <= 4.0F : x >= 4.0F);
        }
        area[left ? 0 : 1] += 0.5 * (((double)(b[0] - a[0]) * (double)(c[1] - a[1])) -
                                     ((double)(c[0] - a[0]) * (double)(b[1] - a[1])));
    }
    ASSERT_NEAR(32.0, area[0], 1e-6); /* la couture est restée une ligne droite, sans trou */
    ASSERT_NEAR(32.0, area[1], 1e-6);
}

/* Cube de 4 m, six faces de 4 x 4 cases ; les sommets des arêtes sont dédoublés (une normale par
 * face), comme dans un vrai modèle. */
static void add_cube(Mesh *m)
{
    static const float axes[6][3][3] = {
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}, {{0, 0, -1}, {0, 1, 0}, {1, 0, 0}},
        {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}};
    for (int f = 0; f < 6; f++) {
        const uint32_t base = m->vertex_count;
        for (int j = 0; j <= 4; j++) {
            for (int i = 0; i <= 4; i++) {
                EngBakeVertex *v = &m->vertices[m->vertex_count++];
                memset(v, 0, sizeof *v);
                for (int a = 0; a < 3; a++) {
                    v->position[a] = (2.0F * axes[f][0][a]) + (((float)i - 2.0F) * axes[f][1][a]) +
                                     (((float)j - 2.0F) * axes[f][2][a]);
                    v->normal[a] = axes[f][0][a];
                }
                v->uv[0] = (float)i / 4.0F;
                v->uv[1] = (float)j / 4.0F;
            }
        }
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                const uint32_t a = base + (uint32_t)((j * 5) + i);
                const uint32_t tri[6] = {a, a + 1U, a + 6U, a, a + 6U, a + 5U};
                memcpy(&m->indices[(size_t)m->triangle_count * 3U], tri, sizeof tri);
                m->triangle_count += 2U;
            }
        }
    }
}

static double signed_volume(const Mesh *m, uint32_t triangles)
{
    double volume = 0.0;
    for (uint32_t t = 0; t < triangles; t++) {
        const float *a = m->vertices[m->indices[(size_t)t * 3U]].position;
        const float *b = m->vertices[m->indices[((size_t)t * 3U) + 1U]].position;
        const float *c = m->vertices[m->indices[((size_t)t * 3U) + 2U]].position;
        volume += ((double)a[0] * (((double)b[1] * (double)c[2]) - ((double)b[2] * (double)c[1]))) -
                  ((double)a[1] * (((double)b[0] * (double)c[2]) - ((double)b[2] * (double)c[0]))) +
                  ((double)a[2] * (((double)b[0] * (double)c[1]) - ((double)b[1] * (double)c[0])));
    }
    return volume / 6.0;
}

UTEST(eng_bake_simplify, a_closed_shape_keeps_its_volume_and_stays_closed)
{
    Mesh *m = fresh_mesh();
    add_cube(m);
    ASSERT_EQ(192U, m->triangle_count);
    ASSERT_NEAR(64.0, signed_volume(m, m->triangle_count), 1e-5);
    const uint32_t kept = simplify(m, 12U);
    ASSERT_LE(kept, 40U);
    ASSERT_GE(kept, 12U); /* un cube ne se fait pas avec moins de douze triangles */
    ASSERT_NEAR(64.0, signed_volume(m, kept), 1e-4); /* ni coin rogné, ni face percée */
    for (uint32_t t = 0; t < kept;
         t++) { /* chaque triangle reste sur sa face, tourné vers dehors */
        const EngBakeVertex *a = &m->vertices[m->indices[(size_t)t * 3U]];
        const EngBakeVertex *b = &m->vertices[m->indices[((size_t)t * 3U) + 1U]];
        const EngBakeVertex *c = &m->vertices[m->indices[((size_t)t * 3U) + 2U]];
        const float u[3] = {b->position[0] - a->position[0], b->position[1] - a->position[1],
                            b->position[2] - a->position[2]};
        const float v[3] = {c->position[0] - a->position[0], c->position[1] - a->position[1],
                            c->position[2] - a->position[2]};
        const float n[3] = {(u[1] * v[2]) - (u[2] * v[1]), (u[2] * v[0]) - (u[0] * v[2]),
                            (u[0] * v[1]) - (u[1] * v[0])};
        const float along = (n[0] * a->normal[0]) + (n[1] * a->normal[1]) + (n[2] * a->normal[2]);
        ASSERT_GT(along, 0.0F);
        ASSERT_NEAR(1.0F,
                    (a->normal[0] * b->normal[0]) + (a->normal[1] * b->normal[1]) +
                        (a->normal[2] * b->normal[2]),
                    1e-6F);
    }
}

UTEST(eng_bake_simplify, nothing_changes_when_the_target_is_already_met_or_a_parameter_is_bad)
{
    Mesh *m = fresh_mesh();
    add_grid(m, 0.0F, (float)SIDE, 0.0F, 1.0F, NULL);
    uint32_t copy[128 * 3];
    memcpy(copy, m->indices, sizeof copy);
    ASSERT_EQ(128U, simplify(m, 128U));
    ASSERT_EQ(0, memcmp(copy, m->indices, sizeof copy));
    ASSERT_EQ(128U, simplify(m, 5000U));
    const size_t bytes = eng_bake_simplify_work(m->vertex_count, m->triangle_count);
    void *work = malloc(bytes);
    ASSERT_EQ(0U, eng_bake_simplify(m->vertices, m->vertex_count, m->indices, m->triangle_count, 8U,
                                    work, bytes - 1U)); /* mémoire de travail trop petite */
    ASSERT_EQ(0U, eng_bake_simplify(NULL, m->vertex_count, m->indices, 128U, 8U, work, bytes));
    ASSERT_EQ(0U, eng_bake_simplify(m->vertices, m->vertex_count, NULL, 128U, 8U, work, bytes));
    ASSERT_EQ(0U,
              eng_bake_simplify(m->vertices, m->vertex_count, m->indices, 128U, 8U, NULL, bytes));
    ASSERT_EQ(0U, eng_bake_simplify(m->vertices, 0U, m->indices, 128U, 8U, work, bytes));
    ASSERT_EQ(0U, eng_bake_simplify(m->vertices, m->vertex_count, m->indices, 0U, 8U, work, bytes));
    ASSERT_EQ(0, memcmp(copy, m->indices, sizeof copy)); /* un refus ne touche à rien */
    m->indices[7] = m->vertex_count;                     /* un index hors du tableau de sommets */
    ASSERT_EQ(0U,
              eng_bake_simplify(m->vertices, m->vertex_count, m->indices, 128U, 8U, work, bytes));
    free(work);
    /* la mémoire de travail grandit avec le maillage */
    ASSERT_GT(eng_bake_simplify_work(200U, 300U), eng_bake_simplify_work(100U, 300U));
    ASSERT_GT(eng_bake_simplify_work(100U, 600U), eng_bake_simplify_work(100U, 300U));
}

UTEST(eng_bake_simplify, triangles_reduced_to_a_point_or_a_line_are_dropped)
{
    Mesh *m = fresh_mesh();
    add_grid(m, 0.0F, (float)SIDE, 0.0F, 1.0F, NULL);
    /* un triangle dégénéré de plus : deux fois le même sommet */
    const uint32_t extra[3] = {0U, 0U, 5U};
    memcpy(&m->indices[(size_t)m->triangle_count * 3U], extra, sizeof extra);
    m->triangle_count++;
    const uint32_t kept = simplify(m, 5000U); /* cible déjà atteinte : rien n'est allégé */
    ASSERT_EQ(128U, kept);                    /* mais lui ne compte pas */
    const Measure r = measure(m, kept);
    ASSERT_NEAR(64.0, r.area, 1e-6);
}
UTEST(eng_bake_simplify, no_triangle_is_ever_turned_over)
{
    /* Une surface plane en L (un carré de 8 dont on a retiré un quart) : tout effondrement y est
     * gratuit, y compris ceux qui feraient passer un triangle par-dessus un autre autour du coin
     * rentrant. Quelle que soit la cible, chaque triangle restant garde son sens. */
    static const uint32_t targets[5] = {60U, 40U, 24U, 12U, 6U};
    for (uint32_t k = 0; k < 5U; k++) {
        Mesh *m = fresh_mesh();
        add_grid(m, 0.0F, (float)SIDE, 0.0F, 1.0F, NULL);
        uint32_t kept_triangles = 0;
        for (uint32_t t = 0; t < m->triangle_count; t++) { /* retire le quart x > 4, y > 4 */
            const float *a = m->vertices[m->indices[(size_t)t * 3U]].position;
            const float *b = m->vertices[m->indices[((size_t)t * 3U) + 1U]].position;
            const float *c = m->vertices[m->indices[((size_t)t * 3U) + 2U]].position;
            const float cx = (a[0] + b[0] + c[0]) / 3.0F;
            const float cy = (a[1] + b[1] + c[1]) / 3.0F;
            if (cx > 4.0F && cy > 4.0F) {
                continue;
            }
            memmove(&m->indices[(size_t)kept_triangles * 3U], &m->indices[(size_t)t * 3U],
                    3U * sizeof(uint32_t));
            kept_triangles++;
        }
        m->triangle_count = kept_triangles;
        ASSERT_EQ(96U, m->triangle_count);
        const uint32_t kept = simplify(m, targets[k]);
        ASSERT_GT(kept, 0U);
        ASSERT_LE(kept, targets[k]);
        const Measure r = measure(m, kept);
        ASSERT_GT(r.min_normal, 1e-9);
        ASSERT_NEAR(48.0, r.area, 1e-6); /* et le L garde son aire : ni trou, ni recouvrement */
    }
}

UTEST(eng_bake_simplify, a_narrow_ring_never_closes_on_itself)
{
    /* Un anneau plat très étroit (1 mm de large, 4 m de rayon, 32 segments) : deux bords, sans
     * extrémité. Rabattre un bord sur l'autre serait l'effondrement le moins coûteux (1 mm de
     * trajet) ; il est interdit, un sommet du bord ne glisse que le long de SON bord. L'anneau
     * s'allège donc le long de ses bords et garde presque toute sa surface ; s'il se refermait,
     * chaque effondrement emporterait deux triangles et la moitié de la surface disparaîtrait. */
    enum { SEGMENTS = 32 };
    Mesh *m = fresh_mesh();
    for (int ring = 0; ring < 2; ring++) {
        for (int i = 0; i < SEGMENTS; i++) {
            const float angle = 6.2831853F * (float)i / (float)SEGMENTS;
            const float radius = 4.0F - ((float)ring * 0.001F);
            m->vertices[m->vertex_count++] =
                (EngBakeVertex){.position = {radius * cosf(angle), radius * sinf(angle), 0.0F},
                                .normal = {0.0F, 0.0F, 1.0F},
                                .uv = {(float)i / (float)SEGMENTS, (float)ring}};
        }
    }
    for (uint32_t i = 0; i < (uint32_t)SEGMENTS; i++) {
        const uint32_t next = (i + 1U) % (uint32_t)SEGMENTS;
        const uint32_t outer = i;
        const uint32_t inner = i + (uint32_t)SEGMENTS;
        const uint32_t tri[6] = {
            outer, next, next + (uint32_t)SEGMENTS, outer, next + (uint32_t)SEGMENTS, inner};
        memcpy(&m->indices[(size_t)m->triangle_count * 3U], tri, sizeof tri);
        m->triangle_count += 2U;
    }
    const double before = fabs(measure(m, m->triangle_count).area);
    ASSERT_GT(before, 0.02); /* 2 pi x 4 m x 1 mm */
    const uint32_t kept = simplify(m, (uint32_t)SEGMENTS);
    ASSERT_LE(kept, 40U); /* de 64 triangles à 32 environ : il s'allège le long de ses bords */
    ASSERT_GE(kept, 6U);
    const double after = fabs(measure(m, kept).area);
    ASSERT_GT(after, 0.85 * before);
    for (uint32_t t = 0; t < kept; t++) { /* chaque triangle va encore d'un bord à l'autre */
        uint32_t on_inner = 0;
        for (uint32_t k = 0; k < 3U; k++) {
            on_inner += m->indices[((size_t)t * 3U) + k] >= (uint32_t)SEGMENTS ? 1U : 0U;
        }
        ASSERT_GT(on_inner, 0U);
        ASSERT_LT(on_inner, 3U);
    }
}
