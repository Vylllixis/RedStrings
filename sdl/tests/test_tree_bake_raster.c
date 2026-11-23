/* Conversion d'un arbre scanné (engine/tree_bake.h) : la « photographie » des triangles sur une
 * carte (eng_bake_raster) et le prolongement de ses bords (eng_bake_dilate). */
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "engine/tree_bake.h"
#include "utest.h"

#define W 16U
#define H 16U

typedef struct Shot { /* une cible de W x H et sa mémoire de travail */
    uint8_t color[W * H * 4U];
    uint8_t normal[W * H * 4U];
    uint8_t arm[W * H * 4U];
    float scratch[W * 4U * H * 4U * 16U];
    EngBakeTarget target;
} Shot;

static Shot g_shot; /* ~1 Mo : hors pile */

static Shot *fresh_shot(void)
{
    memset(&g_shot, 0x55, sizeof g_shot);
    g_shot.target = (EngBakeTarget){
        .color = g_shot.color, .normal = g_shot.normal, .arm = g_shot.arm, .width = W, .height = H};
    return &g_shot;
}

/* vue de face : x vers la droite, y vers le haut, on regarde depuis +z ; 8 m de large et de haut */
static EngBakeView front_view(void)
{
    return (EngBakeView){.origin = {0.0F, 0.0F, 0.0F},
                         .axis_u = {1.0F, 0.0F, 0.0F},
                         .axis_v = {0.0F, 1.0F, 0.0F},
                         .axis_n = {0.0F, 0.0F, 1.0F},
                         .half_u = 4.0F,
                         .half_v = 4.0F};
}

/* un rectangle [x0, x1] x [y0, y1] à la profondeur z, normale `nz` (+1 ou -1 le long de z) */
static void quad(EngBakeVertex *v, uint32_t *indices, uint32_t base, float x0, float y0, float x1,
                 float y1, float z, float nz)
{
    const float corner[4][2] = {{x0, y1}, {x0, y0}, {x1, y0}, {x1, y1}}; /* HG, BG, BD, HD */
    const float uv[4][2] = {{0.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 1.0F}, {1.0F, 0.0F}};
    for (uint32_t k = 0; k < 4U; k++) {
        v[base + k] = (EngBakeVertex){.position = {corner[k][0], corner[k][1], z},
                                      .normal = {0.0F, 0.0F, nz},
                                      .uv = {uv[k][0], uv[k][1]}};
    }
    const uint32_t order[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    for (uint32_t i = 0; i < 6U; i++) {
        indices[i] = base + order[i];
    }
}

static const uint8_t *pixel(const uint8_t *image, uint32_t x, uint32_t y)
{
    return &image[(((size_t)y * W) + x) * 4U];
}

/* une couleur unie de 1 x 1 */
static EngBakeMaterial plain(const uint8_t *rgba)
{
    return (EngBakeMaterial){.color = {rgba, 1U, 1U}, .arm = {NULL, 0U, 0U}};
}

UTEST(eng_bake_raster, a_quad_is_drawn_where_it_is_and_nowhere_else)
{
    static const uint8_t red[4] = {200U, 40U, 20U, 255U};
    const EngBakeMaterial material = plain(red);
    EngBakeVertex vertices[4];
    uint32_t indices[6];
    quad(vertices, indices, 0U, -2.0F, -1.0F, 2.0F, 3.0F, 0.0F, 1.0F); /* 4 x 4 m sur 8 x 8 m */
    const EngBakeMesh mesh = {.vertices = vertices,
                              .vertex_count = 4U,
                              .indices = indices,
                              .triangle_count = 2U,
                              .materials = &material,
                              .material_count = 1U};
    const EngBakeView view = front_view();
    Shot *s = fresh_shot();
    float covered = -1.0F;
    ASSERT_TRUE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch,
                                &covered));
    ASSERT_NEAR(0.25F, covered, 1e-4F); /* un quart de l'image */
    /* x de -2 à 2 m : colonnes 4 à 11 ; y de -1 à 3 m, le haut de l'image étant +4 m : lignes 2 à 9
     */
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            const bool inside = x >= 4U && x <= 11U && y >= 2U && y <= 9U;
            const uint8_t *c = pixel(s->color, x, y);
            ASSERT_EQ(inside ? 255U : 0U, (uint32_t)c[3]);
            ASSERT_EQ(inside ? 200U : 0U, (uint32_t)c[0]);
            ASSERT_EQ(inside ? 20U : 0U, (uint32_t)c[2]);
            /* les trois images ont la même découpe ; le vide : normale de face, occlusion 1 */
            ASSERT_EQ((uint32_t)c[3], (uint32_t)pixel(s->normal, x, y)[3]);
            ASSERT_EQ((uint32_t)c[3], (uint32_t)pixel(s->arm, x, y)[3]);
            ASSERT_EQ(255U, (uint32_t)pixel(s->normal, x, y)[2]);
            ASSERT_EQ(128U, (uint32_t)pixel(s->normal, x, y)[0]);
            ASSERT_EQ(255U, (uint32_t)pixel(s->arm, x, y)[0]);
        }
    }
}

UTEST(eng_bake_raster, the_surface_nearest_to_the_viewer_wins)
{
    static const uint8_t red[4] = {255U, 0U, 0U, 255U};
    static const uint8_t blue[4] = {0U, 0U, 255U, 255U};
    const EngBakeMaterial materials[2] = {plain(red), plain(blue)};
    EngBakeVertex vertices[8];
    uint32_t indices[12];
    quad(vertices, &indices[0], 0U, -4.0F, -4.0F, 4.0F, 4.0F, -1.0F, 1.0F); /* rouge, au fond */
    quad(vertices, &indices[6], 4U, -4.0F, -4.0F, 0.0F, 4.0F, 2.0F,
         1.0F); /* bleu, devant, à gauche */
    const uint8_t triangle_material[4] = {0U, 0U, 1U, 1U};
    EngBakeMesh mesh = {.vertices = vertices,
                        .vertex_count = 8U,
                        .indices = indices,
                        .triangle_count = 4U,
                        .triangle_material = triangle_material,
                        .materials = materials,
                        .material_count = 2U};
    const EngBakeView view = front_view();
    Shot *s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    ASSERT_EQ(255U, (uint32_t)pixel(s->color, 3U, 8U)[2]); /* à gauche : le bleu, plus proche */
    ASSERT_EQ(0U, (uint32_t)pixel(s->color, 3U, 8U)[0]);
    ASSERT_EQ(255U, (uint32_t)pixel(s->color, 12U, 8U)[0]); /* à droite : le rouge seul */
    /* l'ordre de dessin ne compte pas : le bleu d'abord donne la même image */
    uint8_t first[W * H * 4U];
    memcpy(first, s->color, sizeof first);
    const uint32_t reversed[4] = {2U, 3U, 0U, 1U};
    s = fresh_shot();
    ASSERT_TRUE(eng_bake_raster(&mesh, reversed, 4U, &view, &s->target, 0U, 0U, W, H, NULL,
                                s->scratch, NULL));
    ASSERT_EQ(0, memcmp(first, s->color, sizeof first));
    /* une liste de triangles : seuls ceux-là sont dessinés (ici le bleu) */
    const uint32_t only_blue[2] = {2U, 3U};
    s = fresh_shot();
    ASSERT_TRUE(eng_bake_raster(&mesh, only_blue, 2U, &view, &s->target, 0U, 0U, W, H, NULL,
                                s->scratch, NULL));
    ASSERT_EQ(0U, (uint32_t)pixel(s->color, 12U, 8U)[3]);
    ASSERT_EQ(255U, (uint32_t)pixel(s->color, 3U, 8U)[3]);
}

UTEST(eng_bake_raster, the_texture_is_read_with_its_top_left_origin)
{
    /* 2 x 2 : rouge en haut à gauche, vert en haut à droite, bleu en bas à gauche, blanc */
    static const uint8_t texture[16] = {255U, 0U, 0U,   255U, 0U,   255U, 0U,   255U,
                                        0U,   0U, 255U, 255U, 255U, 255U, 255U, 255U};
    const EngBakeMaterial material = {.color = {texture, 2U, 2U}, .arm = {NULL, 0U, 0U}};
    EngBakeVertex vertices[4];
    uint32_t indices[6];
    quad(vertices, indices, 0U, -4.0F, -4.0F, 4.0F, 4.0F, 0.0F, 1.0F);
    const EngBakeMesh mesh = {.vertices = vertices,
                              .vertex_count = 4U,
                              .indices = indices,
                              .triangle_count = 2U,
                              .materials = &material,
                              .material_count = 1U};
    const EngBakeView view = front_view();
    Shot *s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    /* au centre de chaque quart (loin des transitions bilinéaires) */
    ASSERT_GT((uint32_t)pixel(s->color, 4U, 4U)[0], 200U); /* haut gauche (+y) : rouge */
    ASSERT_LT((uint32_t)pixel(s->color, 4U, 4U)[1], 60U);
    ASSERT_GT((uint32_t)pixel(s->color, 12U, 4U)[1], 200U); /* haut droite : vert */
    ASSERT_LT((uint32_t)pixel(s->color, 12U, 4U)[0], 60U);
    ASSERT_GT((uint32_t)pixel(s->color, 4U, 12U)[2], 200U); /* bas gauche : bleu */
    ASSERT_LT((uint32_t)pixel(s->color, 4U, 12U)[0], 60U);
    ASSERT_GT((uint32_t)pixel(s->color, 12U, 12U)[0], 200U); /* bas droite : blanc */
    ASSERT_GT((uint32_t)pixel(s->color, 12U, 12U)[2], 200U);
}

UTEST(eng_bake_raster, normals_are_written_in_the_frame_of_the_view_facing_the_viewer)
{
    EngBakeVertex vertices[4];
    uint32_t indices[6];
    quad(vertices, indices, 0U, -4.0F, -4.0F, 4.0F, 4.0F, 0.0F, 1.0F);
    const float tilted[3] = {0.6F, 0.0F, 0.8F}; /* penchée vers +x */
    for (int k = 0; k < 4; k++) {
        memcpy(vertices[k].normal, tilted, sizeof tilted);
    }
    const EngBakeMesh mesh = {
        .vertices = vertices, .vertex_count = 4U, .indices = indices, .triangle_count = 2U};
    const EngBakeView view = front_view();
    Shot *s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    const uint8_t *n = pixel(s->normal, 8U, 8U);
    ASSERT_NEAR(((0.6F * 0.5F) + 0.5F) * 255.0F, (float)n[0], 1.5F); /* x : +axis_u */
    ASSERT_NEAR(127.5F, (float)n[1], 1.5F);
    ASSERT_NEAR(((0.8F * 0.5F) + 0.5F) * 255.0F, (float)n[2], 1.5F);
    /* sans matière : blanc, occlusion 1, rugosité 1, métal 0 */
    ASSERT_EQ(255U, (uint32_t)pixel(s->color, 8U, 8U)[1]);
    ASSERT_EQ(255U, (uint32_t)pixel(s->arm, 8U, 8U)[1]);
    ASSERT_EQ(0U, (uint32_t)pixel(s->arm, 8U, 8U)[2]);
    /* l'autre face du triangle (normale vers le fond) se voit aussi, retournée vers l'observateur
     */
    for (int k = 0; k < 4; k++) {
        vertices[k].normal[0] = -0.6F;
        vertices[k].normal[2] = -0.8F;
    }
    s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    n = pixel(s->normal, 8U, 8U);
    ASSERT_EQ(255U, (uint32_t)n[3]);
    ASSERT_NEAR(((0.6F * 0.5F) + 0.5F) * 255.0F, (float)n[0], 1.5F);
    ASSERT_GT((uint32_t)n[2], 200U);
    /* une vue tournée : la même normale monde s'écrit dans SES axes */
    const EngBakeView turned = {.origin = {0.0F, 0.0F, 0.0F},
                                .axis_u = {0.0F, 1.0F, 0.0F},
                                .axis_v = {-1.0F, 0.0F, 0.0F},
                                .axis_n = {0.0F, 0.0F, 1.0F},
                                .half_u = 4.0F,
                                .half_v = 4.0F};
    s = fresh_shot();
    ASSERT_TRUE(eng_bake_raster(&mesh, NULL, 0U, &turned, &s->target, 0U, 0U, W, H, NULL,
                                s->scratch, NULL));
    n = pixel(s->normal, 8U, 8U);
    ASSERT_NEAR(127.5F, (float)n[0], 1.5F);                           /* rien le long de +y monde */
    ASSERT_NEAR(((-0.6F * 0.5F) + 0.5F) * 255.0F, (float)n[1], 1.5F); /* +axis_v = -x monde */
}

UTEST(eng_bake_raster, the_alpha_is_the_covered_part_of_the_pixel)
{
    EngBakeVertex vertices[4];
    uint32_t indices[6];
    /* le bord droit tombe au quart du pixel 9 (x de 0,5 à 1 m, soit 0,125 m dans le pixel) */
    quad(vertices, indices, 0U, -4.0F, -4.0F, 0.625F, 4.0F, 0.0F, 1.0F);
    const EngBakeMesh mesh = {
        .vertices = vertices, .vertex_count = 4U, .indices = indices, .triangle_count = 2U};
    const EngBakeView view = front_view();
    const EngBakeRasterOptions four = {.supersample = 4U};
    Shot *s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, &four, s->scratch, NULL));
    ASSERT_EQ(255U, (uint32_t)pixel(s->color, 8U, 8U)[3]);
    ASSERT_EQ(64U, (uint32_t)pixel(s->color, 9U, 8U)[3]);  /* 1 colonne d'échantillons sur 4 */
    ASSERT_EQ(255U, (uint32_t)pixel(s->color, 9U, 8U)[0]); /* la couleur n'est pas assombrie */
    ASSERT_EQ(0U, (uint32_t)pixel(s->color, 10U, 8U)[3]);
    /* sans réglage : trois échantillons par axe, un seul des trois est dans le rectangle */
    s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    ASSERT_EQ(85U, (uint32_t)pixel(s->color, 9U, 8U)[3]);
    /* des réglages donnés, mais le suréchantillonnage laissé à 0 : trois aussi */
    const EngBakeRasterOptions unset = {.occlusion = 1.0F};
    s = fresh_shot();
    ASSERT_TRUE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, &unset,
                                s->scratch, NULL));
    ASSERT_EQ(85U, (uint32_t)pixel(s->color, 9U, 8U)[3]);
    const EngBakeRasterOptions one = {.supersample = 1U};
    s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, &one, s->scratch, NULL));
    ASSERT_EQ(0U,
              (uint32_t)pixel(s->color, 9U, 8U)[3]); /* un échantillon au centre : hors du quad */
    ASSERT_EQ(255U, (uint32_t)pixel(s->color, 8U, 8U)[3]);
}

UTEST(eng_bake_raster, occlusion_and_snow_are_baked)
{
    static const uint8_t green[4] = {40U, 120U, 30U, 255U};
    static const uint8_t arm_texel[4] = {200U, 100U, 0U, 255U}; /* occlusion 200, rugosité 100 */
    const EngBakeMaterial material = {.color = {green, 1U, 1U}, .arm = {arm_texel, 1U, 1U}};
    EngBakeVertex vertices[4];
    uint32_t indices[6];
    quad(vertices, indices, 0U, -4.0F, -4.0F, 4.0F, 4.0F, 0.0F, 1.0F);
    const float sky[4] = {0.5F, 0.5F, 0.5F, 0.5F};
    EngBakeMesh mesh = {.vertices = vertices,
                        .vertex_count = 4U,
                        .indices = indices,
                        .triangle_count = 2U,
                        .materials = &material,
                        .material_count = 1U,
                        .occlusion = sky};
    const EngBakeView view = front_view();
    const EngBakeRasterOptions half = {.occlusion = 0.5F};
    Shot *s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, &half, s->scratch, NULL));
    ASSERT_EQ(50U, (uint32_t)pixel(s->arm, 8U, 8U)[0]);  /* 200 x 0,5 (ciel) x 0,5 (grappe) */
    ASSERT_EQ(100U, (uint32_t)pixel(s->arm, 8U, 8U)[1]); /* la rugosité n'est pas touchée */
    mesh.occlusion = NULL;
    s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    ASSERT_EQ(200U, (uint32_t)pixel(s->arm, 8U, 8U)[0]);
    ASSERT_EQ(120U, (uint32_t)pixel(s->color, 8U, 8U)[1]); /* sans neige demandée : rien */
    const EngBakeRasterOptions heavy = {.snow = 1.0F};
    s = fresh_shot();
    ASSERT_TRUE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, &heavy,
                                s->scratch, NULL));
    /* même sous une neige épaisse, une face de côté (normale horizontale) reste verte */
    ASSERT_EQ(120U, (uint32_t)pixel(s->color, 8U, 8U)[1]);
    ASSERT_EQ(40U, (uint32_t)pixel(s->color, 8U, 8U)[0]);
    /* la neige blanchit ce qui regarde le ciel : la même surface, normale vers le haut */
    for (int k = 0; k < 4; k++) {
        vertices[k].normal[0] = 0.0F;
        vertices[k].normal[1] = 1.0F;
        vertices[k].normal[2] = 0.0F;
    }
    const EngBakeRasterOptions snow = {.snow = 1.0F};
    s = fresh_shot();
    ASSERT_TRUE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, &snow, s->scratch, NULL));
    ASSERT_GT((uint32_t)pixel(s->color, 8U, 8U)[0], 225U);
    ASSERT_GT((uint32_t)pixel(s->color, 8U, 8U)[2], 225U);
    const EngBakeRasterOptions light_snow = {.snow = 0.5F};
    s = fresh_shot();
    ASSERT_TRUE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, &light_snow,
                                s->scratch, NULL));
    ASSERT_NEAR(40.0F + (0.5F * (236.0F - 40.0F)), (float)pixel(s->color, 8U, 8U)[0], 1.5F);
}

UTEST(eng_bake_raster, only_the_given_rectangle_is_written)
{
    EngBakeVertex vertices[4];
    uint32_t indices[6];
    quad(vertices, indices, 0U, -4.0F, -4.0F, 4.0F, 4.0F, 0.0F, 1.0F);
    const EngBakeMesh mesh = {
        .vertices = vertices, .vertex_count = 4U, .indices = indices, .triangle_count = 2U};
    const EngBakeView view = front_view();
    Shot *s = fresh_shot();
    float covered = 0.0F;
    ASSERT_TRUE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 5U, 3U, 6U, 4U, NULL,
                                s->scratch, &covered));
    ASSERT_NEAR(1.0F, covered, 1e-5F); /* la vue entière tient dans le rectangle */
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            const bool inside = x >= 5U && x < 11U && y >= 3U && y < 7U;
            /* hors du rectangle : le motif d'origine (0x55) est intact */
            ASSERT_EQ(inside ? 255U : 0x55U, (uint32_t)pixel(s->color, x, y)[3]);
            ASSERT_EQ(inside ? 255U : 0x55U, (uint32_t)pixel(s->normal, x, y)[2]);
        }
    }
    /* rectangle hors de la cible, vide, vue invalide, paramètre manquant : refusés */
    ASSERT_FALSE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 12U, 0U, 5U, 4U, NULL,
                                 s->scratch, NULL));
    ASSERT_FALSE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 14U, 4U, 3U, NULL,
                                 s->scratch, NULL));
    ASSERT_FALSE(eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, 0U, 4U, NULL,
                                 s->scratch, NULL));
    EngBakeView flat = view;
    flat.half_u = 0.0F;
    ASSERT_FALSE(
        eng_bake_raster(&mesh, NULL, 0U, &flat, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    flat = view;
    flat.axis_n[2] = NAN;
    ASSERT_FALSE(
        eng_bake_raster(&mesh, NULL, 0U, &flat, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    ASSERT_FALSE(
        eng_bake_raster(NULL, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, s->scratch, NULL));
    ASSERT_FALSE(
        eng_bake_raster(&mesh, NULL, 0U, &view, &s->target, 0U, 0U, W, H, NULL, NULL, NULL));
    /* un triangle ou un sommet hors du maillage est ignoré, sans plantage */
    const uint32_t beyond[2] = {0U, 9U};
    uint32_t bad_indices[6];
    memcpy(bad_indices, indices, sizeof bad_indices);
    bad_indices[4] = 77U;
    const EngBakeMesh broken = {
        .vertices = vertices, .vertex_count = 4U, .indices = bad_indices, .triangle_count = 2U};
    s = fresh_shot();
    ASSERT_TRUE(eng_bake_raster(&broken, beyond, 2U, &view, &s->target, 0U, 0U, W, H, NULL,
                                s->scratch, &covered));
    ASSERT_NEAR(0.5F, covered, 0.07F); /* seul le premier triangle est valide : une moitié */
}

/* ── Prolongement des bords ── */

UTEST(eng_bake_dilate, colors_grow_one_pixel_per_pass_and_alpha_stays_empty)
{
    Shot *s = fresh_shot();
    memset(s->color, 0, sizeof s->color);
    memset(s->normal, 0, sizeof s->normal);
    memset(s->arm, 0, sizeof s->arm);
    const uint8_t leaf[4] = {30U, 150U, 60U, 255U};
    memcpy(&s->color[(((size_t)8U * W) + 8U) * 4U], leaf,
           4U); /* un seul pixel couvert, en (8, 8) */
    memcpy(&s->normal[(((size_t)8U * W) + 8U) * 4U], (const uint8_t[4]){10U, 20U, 30U, 255U}, 4U);
    memcpy(&s->arm[(((size_t)8U * W) + 8U) * 4U], (const uint8_t[4]){70U, 80U, 90U, 255U}, 4U);
    uint8_t work[W * H];
    eng_bake_dilate(&s->target, 0U, 0U, W, H, 2U, work);
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            const uint32_t dx = x > 8U ? x - 8U : 8U - x;
            const uint32_t dy = y > 8U ? y - 8U : 8U - y;
            const uint32_t reach = dx > dy ? dx : dy; /* voisinage de 8 : distance de Tchebychev */
            const uint8_t *c = pixel(s->color, x, y);
            ASSERT_EQ(reach <= 2U ? 150U : 0U, (uint32_t)c[1]);
            ASSERT_EQ(reach == 0U ? 255U : 0U, (uint32_t)c[3]); /* l'alpha des vides reste 0 */
            ASSERT_EQ(reach <= 2U ? 20U : 0U, (uint32_t)pixel(s->normal, x, y)[1]);
            ASSERT_EQ(reach <= 2U ? 90U : 0U, (uint32_t)pixel(s->arm, x, y)[2]);
        }
    }
}

UTEST(eng_bake_dilate, a_gap_between_two_colors_takes_their_average_and_the_rectangle_bounds_it)
{
    Shot *s = fresh_shot();
    memset(s->color, 0, sizeof s->color);
    memset(s->normal, 0, sizeof s->normal);
    memset(s->arm, 0, sizeof s->arm);
    memcpy(&s->color[(((size_t)4U * W) + 4U) * 4U], (const uint8_t[4]){100U, 0U, 0U, 255U}, 4U);
    memcpy(&s->color[(((size_t)4U * W) + 6U) * 4U], (const uint8_t[4]){200U, 0U, 0U, 255U}, 4U);
    uint8_t work[W * H];
    eng_bake_dilate(&s->target, 2U, 2U, 6U, 6U, 1U, work); /* rectangle de (2, 2) à (7, 7) */
    ASSERT_EQ(150U, (uint32_t)pixel(s->color, 5U, 4U)[0]); /* entre les deux : la moyenne */
    ASSERT_EQ(100U, (uint32_t)pixel(s->color, 3U, 4U)[0]);
    ASSERT_EQ(200U, (uint32_t)pixel(s->color, 7U, 4U)[0]);
    ASSERT_EQ(0U, (uint32_t)pixel(s->color, 8U, 4U)[0]);   /* hors du rectangle : intact */
    ASSERT_EQ(100U, (uint32_t)pixel(s->color, 4U, 4U)[0]); /* un pixel couvert ne change pas */
    /* rectangle hors de la cible ou paramètre manquant : sans effet, sans plantage */
    eng_bake_dilate(&s->target, 12U, 12U, 8U, 8U, 4U, work);
    eng_bake_dilate(NULL, 0U, 0U, W, H, 4U, work);
    eng_bake_dilate(&s->target, 0U, 0U, W, H, 4U, NULL);
    ASSERT_EQ(0U, (uint32_t)pixel(s->color, 14U, 14U)[0]);
}
