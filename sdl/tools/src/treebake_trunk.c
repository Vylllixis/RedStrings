/* treebake : le tronc. Chaque matière « tronc » de la source devient une partie du modèle, allégée
 * deux fois (de près, à mi-distance) ; ses images sont recopiées. Voir treebake.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "treebake.h"

#define TRIANGLES_MIN                                                                              \
    48U /* une partie garde au moins cela, même si sa part du total est infime                    \
         */

/* les triangles de la matière `material`, avec leurs seuls sommets */
static bool extract(const TbSource *s, uint32_t material, TbMeshOut *out)
{
    memset(out, 0, sizeof *out);
    const uint32_t triangles = s->materials[material].triangles;
    uint32_t *remap = malloc((size_t)s->vertex_count * sizeof *remap);
    out->indices = malloc((size_t)triangles * 3U * sizeof *out->indices);
    if (remap == NULL || out->indices == NULL) {
        free(remap);
        tb_mesh_free(out);
        return false;
    }
    memset(remap, 0xFF, (size_t)s->vertex_count * sizeof *remap);
    uint32_t vertices = 0;
    for (uint32_t t = 0; t < s->triangle_count && out->triangle_count < triangles; t++) {
        if (s->triangle_material[t] != material) {
            continue;
        }
        for (uint32_t k = 0; k < 3U; k++) {
            const uint32_t v = s->indices[((size_t)t * 3U) + k];
            if (remap[v] == UINT32_MAX) {
                remap[v] = vertices++;
            }
            out->indices[((size_t)out->triangle_count * 3U) + k] = remap[v];
        }
        out->triangle_count++;
    }
    out->vertices = malloc((size_t)(vertices > 0U ? vertices : 1U) * sizeof *out->vertices);
    if (out->vertices == NULL) {
        free(remap);
        tb_mesh_free(out);
        return false;
    }
    for (uint32_t v = 0; v < s->vertex_count; v++) {
        if (remap[v] != UINT32_MAX) {
            out->vertices[remap[v]] = s->vertices[v];
        }
    }
    out->vertex_count = vertices;
    free(remap);
    return true;
}

static double surface(const TbMeshOut *m)
{
    double total = 0.0;
    for (uint32_t t = 0; t < m->triangle_count; t++) {
        const float *a = m->vertices[m->indices[(size_t)t * 3U]].position;
        const float *b = m->vertices[m->indices[((size_t)t * 3U) + 1U]].position;
        const float *c = m->vertices[m->indices[((size_t)t * 3U) + 2U]].position;
        const double u[3] = {(double)b[0] - (double)a[0], (double)b[1] - (double)a[1],
                             (double)b[2] - (double)a[2]};
        const double v[3] = {(double)c[0] - (double)a[0], (double)c[1] - (double)a[1],
                             (double)c[2] - (double)a[2]};
        const double n[3] = {(u[1] * v[2]) - (u[2] * v[1]), (u[2] * v[0]) - (u[0] * v[2]),
                             (u[0] * v[1]) - (u[1] * v[0])};
        total += 0.5 * sqrt((n[0] * n[0]) + (n[1] * n[1]) + (n[2] * n[2]));
    }
    return total;
}

/* allège `m` sur place jusqu'à `target` triangles environ, puis retire les sommets inutilisés */
static bool reduce(TbMeshOut *m, uint32_t target)
{
    if (m->triangle_count <= target) {
        return true;
    }
    const size_t bytes = eng_bake_simplify_work(m->vertex_count, m->triangle_count);
    void *work = malloc(bytes);
    uint32_t *remap = malloc((size_t)m->vertex_count * sizeof *remap);
    bool ok = work != NULL && remap != NULL;
    if (ok) {
        const uint32_t kept = eng_bake_simplify(m->vertices, m->vertex_count, m->indices,
                                                m->triangle_count, target, work, bytes);
        ok = kept > 0U;
        if (ok) {
            m->triangle_count = kept;
            m->vertex_count =
                eng_bake_compact(m->vertices, m->vertex_count, m->indices, kept, remap);
        }
    }
    free(work);
    free(remap);
    return ok;
}

static bool duplicate(const TbMeshOut *from, TbMeshOut *to)
{
    memset(to, 0, sizeof *to);
    to->vertices = malloc((size_t)from->vertex_count * sizeof *to->vertices);
    to->indices = malloc((size_t)from->triangle_count * 3U * sizeof *to->indices);
    if (to->vertices == NULL || to->indices == NULL) {
        tb_mesh_free(to);
        return false;
    }
    memcpy(to->vertices, from->vertices, (size_t)from->vertex_count * sizeof *to->vertices);
    memcpy(to->indices, from->indices, (size_t)from->triangle_count * 3U * sizeof *to->indices);
    to->vertex_count = from->vertex_count;
    to->triangle_count = from->triangle_count;
    return true;
}

static const char *const ROLE[3] = {"diff", "nor", "arm"}; /* couleur, relief, ARM */

/* Recopie l'image `from` de la source sous le nom <nom>_bark<partie>_<rôle>.<ext> ; la même image
 * déjà recopiée pour une partie précédente est réutilisée. Nom vide si l'image est absente. */
static bool copy_image(const TbOptions *o, TbTrunk *trunk, uint32_t part, uint32_t kind,
                       const char *from)
{
    TbTrunkPart *dst = &trunk->parts[part];
    dst->uri[kind][0] = '\0';
    (void)snprintf(dst->source[kind], TB_PATH, "%s", from);
    if (from[0] == '\0') {
        return true;
    }
    for (uint32_t p = 0; p < part; p++) {
        if (strcmp(trunk->parts[p].source[kind], from) == 0) {
            memmove(dst->uri[kind], trunk->parts[p].uri[kind], TB_NAME); /* p < part : distincts */
            return true;
        }
    }
    const char *dot = strrchr(from, '.');
    char path[TB_PATH];
    (void)snprintf(dst->uri[kind], TB_NAME, "%s_bark%u_%s%s", o->name, (unsigned)part, ROLE[kind],
                   dot != NULL ? dot : ".jpg");
    (void)snprintf(path, sizeof path, "%s/%s", o->out_dir, dst->uri[kind]);
    if (!tb_copy_file(from, path)) {
        (void)fprintf(stderr, "treebake : copie de %s vers %s impossible\n", from, path);
        return false;
    }
    return true;
}

static void normalize3(double v[3])
{
    const double length = sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
    if (length > 1e-30) {
        for (int a = 0; a < 3; a++) {
            v[a] /= length;
        }
    }
}

/* Tangentes d'un maillage à partir de ses UV, comme les calcule le moteur (engine/src/model.c :
 * T = dP/du, bitangente = -dP/dv, une voix par triangle), mais dont le w porte en plus la part du
 * ciel vue par le sommet (voir tb_mesh_add_quad) : les branches au cœur du houppier sont sombres.
 */
static bool make_tangents(TbMeshOut *m, const EngBakeGrid *grid)
{
    double *sums = calloc((size_t)(m->vertex_count > 0U ? m->vertex_count : 1U) * 6U, sizeof *sums);
    m->tangents =
        malloc((size_t)(m->vertex_count > 0U ? m->vertex_count : 1U) * 4U * sizeof(float));
    if (sums == NULL || m->tangents == NULL) {
        free(sums);
        return false;
    }
    for (uint32_t t = 0; t < m->triangle_count; t++) {
        const uint32_t *index = &m->indices[(size_t)t * 3U];
        const EngBakeVertex *v0 = &m->vertices[index[0]];
        const EngBakeVertex *v1 = &m->vertices[index[1]];
        const EngBakeVertex *v2 = &m->vertices[index[2]];
        const double du1 = (double)v1->uv[0] - (double)v0->uv[0];
        const double dv1 = (double)v1->uv[1] - (double)v0->uv[1];
        const double du2 = (double)v2->uv[0] - (double)v0->uv[0];
        const double dv2 = (double)v2->uv[1] - (double)v0->uv[1];
        const double det = (du1 * dv2) - (du2 * dv1);
        if (fabs(det) < 1e-20) {
            continue;
        }
        double tangent[3];
        double bitangent[3];
        for (int a = 0; a < 3; a++) {
            const double e1 = (double)v1->position[a] - (double)v0->position[a];
            const double e2 = (double)v2->position[a] - (double)v0->position[a];
            tangent[a] = ((e1 * dv2) - (e2 * dv1)) / det;
            bitangent[a] = -((e2 * du1) - (e1 * du2)) / det;
        }
        normalize3(tangent);
        normalize3(bitangent);
        for (uint32_t k = 0; k < 3U; k++) {
            for (int a = 0; a < 3; a++) {
                sums[((size_t)index[k] * 6U) + (size_t)a] += tangent[a];
                sums[((size_t)index[k] * 6U) + 3U + (size_t)a] += bitangent[a];
            }
        }
    }
    for (uint32_t v = 0; v < m->vertex_count; v++) {
        const float *n = m->vertices[v].normal;
        const double *sum = &sums[(size_t)v * 6U];
        const double along =
            (sum[0] * (double)n[0]) + (sum[1] * (double)n[1]) + (sum[2] * (double)n[2]);
        double tangent[3] = {sum[0] - (along * (double)n[0]), sum[1] - (along * (double)n[1]),
                             sum[2] - (along * (double)n[2])};
        if ((tangent[0] * tangent[0]) + (tangent[1] * tangent[1]) + (tangent[2] * tangent[2]) <
            1e-20) {
            /* pas d'UV utilisables : n'importe quelle direction perpendiculaire à la normale */
            const int least = fabsf(n[0]) <= fabsf(n[1]) && fabsf(n[0]) <= fabsf(n[2])
                                  ? 0
                                  : (fabsf(n[1]) <= fabsf(n[2]) ? 1 : 2);
            for (int a = 0; a < 3; a++) {
                tangent[a] = (a == least ? 1.0 : 0.0) - ((double)n[least] * (double)n[a]);
            }
        }
        normalize3(tangent);
        const double cross[3] = {((double)n[1] * tangent[2]) - ((double)n[2] * tangent[1]),
                                 ((double)n[2] * tangent[0]) - ((double)n[0] * tangent[2]),
                                 ((double)n[0] * tangent[1]) - ((double)n[1] * tangent[0])};
        const double side = (cross[0] * sum[3]) + (cross[1] * sum[4]) + (cross[2] * sum[5]);
        float sky = grid != NULL && grid->sky != NULL
                        ? eng_bake_grid_sky(grid, m->vertices[v].position)
                        : 1.0F;
        sky = sky < TB_SKY_MIN ? TB_SKY_MIN : (sky > 1.0F ? 1.0F : sky);
        float *out = &m->tangents[(size_t)v * 4U];
        for (int a = 0; a < 3; a++) {
            out[a] = (float)tangent[a];
        }
        out[3] = side < 0.0 ? -sky : sky;
    }
    free(sums);
    return true;
}

bool tb_trunk_build(TbTrunk *out, const TbSource *s, const TbOptions *o)
{
    memset(out, 0, sizeof *out);
    double area[TB_MAX_MATERIALS] = {0.0};
    double total = 0.0;
    bool ok = true;
    for (uint32_t m = 0; ok && m < s->material_count; m++) {
        const TbMaterial *material = &s->materials[m];
        if (material->role != TB_TRUNK || material->triangles == 0U) {
            continue;
        }
        TbTrunkPart *part = &out->parts[out->count];
        ok = extract(s, m, &part->lod0) &&
             copy_image(o, out, out->count, 0U, material->color_path) &&
             copy_image(o, out, out->count, 1U, material->normal_path) &&
             copy_image(o, out, out->count, 2U, material->arm_path);
        if (ok) {
            (void)snprintf(part->name, sizeof part->name, "bark%u", (unsigned)out->count);
            (void)snprintf(part->material, sizeof part->material, "%s", material->name);
            area[out->count] = surface(&part->lod0);
            total += area[out->count];
            out->count++;
        }
    }
    /* le budget de triangles est partagé entre les parties selon leur surface */
    for (uint32_t p = 0; ok && p < out->count; p++) {
        TbTrunkPart *part = &out->parts[p];
        const double share = total > 0.0 ? area[p] / total : 1.0;
        const uint32_t before = part->lod0.triangle_count;
        const uint32_t near_target = (uint32_t)((double)o->trunk_triangles * share);
        const uint32_t lod_target = (uint32_t)((double)o->trunk_triangles_lod * share);
        ok = reduce(&part->lod0, near_target > TRIANGLES_MIN ? near_target : TRIANGLES_MIN) &&
             duplicate(&part->lod0, &part->lod1) &&
             reduce(&part->lod1, lod_target > TRIANGLES_MIN ? lod_target : TRIANGLES_MIN) &&
             make_tangents(&part->lod0, &s->grid) && make_tangents(&part->lod1, &s->grid);
        if (ok) {
            (void)printf("tronc %s (%s, %.1f m²) : %u triangles -> %u de près, %u à mi-distance\n",
                         part->name, part->material, area[p], (unsigned)before,
                         (unsigned)part->lod0.triangle_count, (unsigned)part->lod1.triangle_count);
        }
    }
    if (!ok) {
        (void)fprintf(stderr, "treebake : allègement du tronc impossible\n");
        tb_trunk_free(out);
    }
    return ok;
}

void tb_trunk_free(TbTrunk *t)
{
    for (uint32_t p = 0; p < TB_MAX_MATERIALS; p++) {
        tb_mesh_free(&t->parts[p].lod0);
        tb_mesh_free(&t->parts[p].lod1);
    }
    memset(t, 0, sizeof *t);
}
