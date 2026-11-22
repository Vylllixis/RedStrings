/* Ce que l'outil treebake écrit (tools/src/treebake.c), relu par le lecteur glTF du moteur : les
 * tests CTest tools.treebake_cards et tools.treebake_mesh ont converti le petit chêne du dépôt
 * dans OW_TREEBAKE_OUT, une fois tout en cartes, une fois tout en maillage. Le jeu attend d'un
 * arbre : ses parties de près, les mêmes à mi-distance, puis la vue lointaine (2 P + 1 parties). */
#include <math.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/model.h"
#include "utest.h"

typedef struct Loaded {
    EngArena arena;
    EngArena scratch;
    EngModel model;
} Loaded;

static bool load(Loaded *l, const char *name)
{
    char path[1024];
    (void)snprintf(path, sizeof path, "%s/%s.gltf", OW_TREEBAKE_OUT, name);
    return eng_arena_init(&l->arena, "arbre", (size_t)4 << 20) &&
           eng_arena_init(&l->scratch, "arbre-lecture", (size_t)8 << 20) &&
           eng_model_load(&l->arena, &l->scratch, path, &l->model);
}

static void unload(Loaded *l)
{
    eng_model_free(&l->model);
    eng_arena_shutdown(&l->scratch);
    eng_arena_shutdown(&l->arena);
}

static uint32_t triangles(const EngModel *m, uint32_t part)
{
    return m->parts[part].index_count / 3U;
}

UTEST(treebake_output, a_tree_in_cards_has_its_three_levels_and_cut_out_materials)
{
    static Loaded l;
    ASSERT_TRUE(load(&l, "cartes"));
    const EngModel *m = &l.model;
    ASSERT_EQ(3U, m->part_count);          /* feuillage de près, à mi-distance, vue lointaine */
    ASSERT_EQ(36U * 2U, triangles(m, 0U)); /* 12 grappes x 3 cartes */
    ASSERT_EQ(24U * 2U, triangles(m, 1U)); /* 12 grappes x 2 cartes */
    ASSERT_EQ(4U * 2U, triangles(m, 2U));  /* trois vues de côté, une de dessus */
    for (uint32_t p = 0; p < 3U; p++) {
        const EngModelMaterial *mat = &m->materials[m->parts[p].material];
        ASSERT_NEAR(0.5F, mat->alpha_cutoff, 1e-6F); /* matière découpée, deux faces */
        ASSERT_TRUE(mat->double_sided);
        ASSERT_GE(mat->image, 0);
        ASSERT_GE(mat->normal_image, 0);
        ASSERT_GE(mat->occlusion_image, 0);
        ASSERT_EQ(mat->occlusion_image, mat->metallic_roughness_image); /* une image ARM */
    }
    /* les deux niveaux de feuillage partagent leurs images ; la vue lointaine a les siennes */
    ASSERT_EQ(m->materials[m->parts[0].material].image, m->materials[m->parts[1].material].image);
    ASSERT_NE(m->materials[m->parts[0].material].image, m->materials[m->parts[2].material].image);
    /* l'atlas du feuillage : 256 x 256, des pixels couverts et des pixels vides */
    const EngImage *atlas = &m->images[m->materials[m->parts[0].material].image];
    ASSERT_EQ(256U, atlas->width);
    ASSERT_EQ(256U, atlas->height);
    uint32_t covered = 0;
    uint32_t empty = 0;
    for (size_t i = 0; i < (size_t)256 * 256; i++) {
        covered += atlas->pixels[(i * 4U) + 3U] == 255U ? 1U : 0U;
        empty += atlas->pixels[(i * 4U) + 3U] == 0U ? 1U : 0U;
    }
    ASSERT_GT(covered, 2000U);
    ASSERT_GT(empty, 2000U);
    /* l'arbre est posé : pied à l'origine (un peu enfoncé), 5 m de haut */
    ASSERT_NEAR(5.0F, m->bounds_max[1] - m->bounds_min[1], 0.6F);
    ASSERT_LT(m->bounds_min[1], 0.3F);
    uint32_t mirrored = 0;
    uint32_t shaded = 0;
    for (uint32_t v = 0; v < m->vertex_count; v++) {
        const EngModelVertex *vertex = &m->vertices[v];
        const float n2 = (vertex->normal[0] * vertex->normal[0]) +
                         (vertex->normal[1] * vertex->normal[1]) +
                         (vertex->normal[2] * vertex->normal[2]);
        ASSERT_NEAR(1.0F, n2, 1e-3F);
        ASSERT_GE(vertex->uv[0], 0.0F); /* dans l'atlas */
        ASSERT_LE(vertex->uv[0], 1.0F);
        ASSERT_GE(vertex->uv[1], 0.0F);
        ASSERT_LE(vertex->uv[1], 1.0F);
        /* |w| de la tangente : la part du ciel vue par le sommet, jamais nulle (le signe reste) */
        ASSERT_GE(fabsf(vertex->tangent[3]), 0.05F - 1e-6F);
        ASSERT_LE(fabsf(vertex->tangent[3]), 1.0F + 1e-6F);
        mirrored += vertex->tangent[3] < 0.0F ? 1U : 0U;
        shaded += fabsf(vertex->tangent[3]) < 0.98F ? 1U : 0U;
    }
    /* une grappe sur deux porte ses photographies en miroir : sa bitangente change de signe */
    ASSERT_GT(mirrored, 0U);
    ASSERT_LT(mirrored, m->vertex_count);
    ASSERT_GT(shaded,
              0U); /* et le feuillage se fait de l'ombre : des sommets voient moins de ciel */
    unload(&l);
}

UTEST(treebake_output, a_tree_in_mesh_keeps_opaque_parts_and_a_cut_out_far_view)
{
    static Loaded l;
    ASSERT_TRUE(load(&l, "maille"));
    const EngModel *m = &l.model;
    ASSERT_EQ(3U, m->part_count); /* le maillage de près, à mi-distance, la vue lointaine */
    const EngModelMaterial *near_mat = &m->materials[m->parts[0].material];
    const EngModelMaterial *mid_mat = &m->materials[m->parts[1].material];
    const EngModelMaterial *far_mat = &m->materials[m->parts[2].material];
    ASSERT_EQ(0.0F, near_mat->alpha_cutoff); /* un tronc est opaque */
    ASSERT_EQ(0.0F, mid_mat->alpha_cutoff);
    ASSERT_FALSE(near_mat->double_sided);
    ASSERT_NEAR(0.5F, far_mat->alpha_cutoff, 1e-6F);
    ASSERT_EQ(near_mat->image, mid_mat->image); /* la même écorce aux deux niveaux */
    ASSERT_GE(near_mat->image, 0);
    ASSERT_LE(triangles(m, 0U), 436U);
    ASSERT_LE(triangles(m, 1U), triangles(m, 0U)); /* à mi-distance : pas plus de triangles */
    ASSERT_GT(triangles(m, 1U), 0U);
    ASSERT_EQ(8U, triangles(m, 2U));
    ASSERT_NEAR(5.0F, m->bounds_max[1] - m->bounds_min[1], 0.6F);
    /* sans feuillage, rien ne cache le ciel : |w| = 1 partout, et des tangentes unitaires */
    for (uint32_t v = 0; v < m->vertex_count; v++) {
        const float *tangent = m->vertices[v].tangent;
        ASSERT_NEAR(1.0F, fabsf(tangent[3]), 1e-6F);
        ASSERT_NEAR(
            1.0F, (tangent[0] * tangent[0]) + (tangent[1] * tangent[1]) + (tangent[2] * tangent[2]),
            1e-3F);
    }
    unload(&l);
}
