/* Décor des zones : château fermé avec une porte à l'ouest, arènes de boss dégagées, campements. */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "engine/zone_decor.h"
#include "utest.h"

static EngMapZone g_zones[ENG_MAPZONE_COUNT];

static void build(void)
{
    const EngBiomeMap map = eng_biome_map_default();
    (void)eng_mapzones_build(&map, g_zones);
}

/* le point (x, z) est-il dans une boîte solide, élargie de `margin` ? */
static bool blocked(const EngDecorBox *b, uint32_t n, float x, float z, float margin)
{
    for (uint32_t i = 0; i < n; i++) {
        if (b[i].solid && fabsf(x - b[i].x) <= b[i].half[0] + margin &&
            fabsf(z - b[i].z) <= b[i].half[2] + margin) {
            return true;
        }
    }
    return false;
}

static bool all_valid(const EngDecorBox *b, uint32_t n)
{
    uint32_t solids = 0;
    for (uint32_t i = 0; i < n; i++) {
        for (int a = 0; a < 3; a++) {
            if (!(b[i].half[a] > 0.0F) || !isfinite(b[i].half[a])) {
                return false;
            }
        }
        if (!isfinite(b[i].x) || !isfinite(b[i].z) || !(b[i].lift >= 0.0F) ||
            b[i].color >= (uint8_t)ENG_DECOR_COLORS || (b[i].solid && b[i].yaw != 0.0F)) {
            return false;
        }
        solids += b[i].solid ? 1U : 0U;
    }
    return solids <= 64U; /* ENG_AI_MAX_OBSTACLES : les ennemis les contournent tous */
}

UTEST(zone_decor, the_castle_is_closed_except_for_its_west_and_east_gates)
{
    build();
    EngDecorBox b[ENG_DECOR_MAX];
    const uint32_t n = eng_zone_decor(&g_zones[2], b, ENG_DECOR_MAX);
    ASSERT_GE(n, 40U);
    ASSERT_LE(n, ENG_DECOR_MAX);
    EXPECT_TRUE(all_valid(b, n));
    const float half = ENG_DECOR_CASTLE_HALF;
    /* le tour des murs, tous les 25 cm : fermé partout, sauf les deux portes (ouest et est) */
    uint32_t holes = 0;
    uint32_t gate_open = 0;
    for (int i = 0; i <= 176; i++) {
        const float t = -half + (0.25F * (float)i);
        holes += blocked(b, n, t, -half, 0.0F) ? 0U : 1U; /* nord */
        holes += blocked(b, n, t, half, 0.0F) ? 0U : 1U;  /* sud */
        const bool in_gate = fabsf(t) < (0.5F * ENG_DECOR_GATE_WIDTH) - 0.01F;
        if (in_gate) {
            gate_open += blocked(b, n, -half, t, 0.0F) ? 0U : 1U;
            EXPECT_FALSE(blocked(b, n, -half, t, 0.0F));
            EXPECT_FALSE(blocked(b, n, half, t, 0.0F));
        } else {
            holes += blocked(b, n, -half, t, 0.0F) ? 0U : 1U; /* ouest hors porte */
            holes += blocked(b, n, half, t, 0.0F) ? 0U : 1U;  /* est hors porte */
        }
    }
    EXPECT_EQ(holes, 0U);
    EXPECT_GE(gate_open, 18U); /* 4,8 m ouverts */
    /* les portes laissent passer le cavalier (rayon 1 m) : on traverse le château d'ouest en est */
    for (int i = 0; i <= 216; i++) {
        EXPECT_FALSE(blocked(b, n, -half - 5.0F + (0.25F * (float)i), 0.0F, 1.0F));
    }
    /* la cour est dégagée autour du boss (12 m) ; le donjon est au fond, pas au centre */
    for (int ix = -12; ix <= 12; ix++) {
        for (int iz = -12; iz <= 12; iz++) {
            ASSERT_FALSE(blocked(b, n, (float)ix, (float)iz, 0.0F));
        }
    }
    EXPECT_TRUE(blocked(b, n, half - 6.0F, -(half - 7.0F), 0.0F)); /* le donjon, au nord-est */
    /* tours plus hautes que les murs */
    float wall_top = 0.0F;
    float tower_top = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        if (b[i].solid && b[i].color == (uint8_t)ENG_DECOR_STONE) {
            const float top = b[i].lift + (2.0F * b[i].half[1]);
            if (b[i].half[0] > 1.0F && b[i].half[2] > 1.0F) {
                tower_top = fmaxf(tower_top, top);
            } else {
                wall_top = fmaxf(wall_top, top);
            }
        }
    }
    EXPECT_NEAR(wall_top, 5.0F, 1e-4F);
    EXPECT_GT(tower_top, wall_top + 1.5F);
}

UTEST(zone_decor, every_zone_has_a_decor_that_leaves_its_fight_area_clear)
{
    build();
    EngDecorBox b[ENG_DECOR_MAX];
    uint32_t counts[ENG_MAPZONE_COUNT];
    for (uint32_t zi = 0; zi < ENG_MAPZONE_COUNT; zi++) {
        const EngMapZone *zone = &g_zones[zi];
        const uint32_t n = eng_zone_decor(zone, b, ENG_DECOR_MAX);
        counts[zi] = n;
        ASSERT_GE(n, 4U);
        ASSERT_LE(n, ENG_DECOR_MAX);
        EXPECT_TRUE(all_valid(b, n));
        /* aire de combat : 12 m autour d'un boss, 6 m autour d'un camp (ennemis à 5 m) */
        const int clear = zone->kind == ENG_MAPZONE_BOSS ? 12 : 6;
        for (int ix = -clear; ix <= clear; ix++) {
            for (int iz = -clear; iz <= clear; iz++) {
                if ((ix * ix) + (iz * iz) <= clear * clear) {
                    ASSERT_FALSE(blocked(b, n, (float)ix, (float)iz, 0.0F));
                }
            }
        }
        /* le joueur arrive par l'ouest : le chemin jusqu'au centre est libre (rayon 0,5 m) */
        for (int i = 0; i <= 120; i++) {
            ASSERT_FALSE(blocked(b, n, -30.0F + (0.25F * (float)i), 0.0F, 0.5F));
        }
        bool solid = false;
        for (uint32_t i = 0; i < n; i++) {
            solid = solid || b[i].solid;
        }
        EXPECT_TRUE(solid); /* au moins un obstacle réel */
    }
    /* chaque boss a son décor : matière propre au biome */
    const uint8_t expected[ENG_BIOME_COUNT] = {ENG_DECOR_STONE, ENG_DECOR_ICE, ENG_DECOR_SANDSTONE,
                                               ENG_DECOR_BASALT};
    for (uint32_t biome = 0; biome < ENG_BIOME_COUNT; biome++) {
        const uint32_t n = eng_zone_decor(&g_zones[(biome * 3U) + 2U], b, ENG_DECOR_MAX);
        uint32_t of_biome = 0;
        for (uint32_t i = 0; i < n; i++) {
            of_biome += b[i].color == expected[biome] ? 1U : 0U;
        }
        EXPECT_GE(of_biome, 8U);
    }
    EXPECT_GT(counts[2], counts[5]); /* le château est le plus gros décor */
    /* volcan : des brasiers (émissifs) */
    const uint32_t nv = eng_zone_decor(&g_zones[11], b, ENG_DECOR_MAX);
    uint32_t fires = 0;
    for (uint32_t i = 0; i < nv; i++) {
        fires += b[i].color == (uint8_t)ENG_DECOR_FIRE ? 1U : 0U;
    }
    EXPECT_EQ(fires, 6U);
    /* l'antre d'un dragon de 60 m : rien à moins de 70 m du centre (ses pattes sont à 20 m, sa
     * tête s'abat à 57 m), des pics de 18 m sur le pourtour */
    float tallest = 0.0F;
    for (uint32_t i = 0; i < nv; i++) {
        const float reach = b[i].half[0] > b[i].half[2] ? b[i].half[0] : b[i].half[2];
        EXPECT_GT(hypotf(b[i].x, b[i].z) - reach, 70.0F);
        EXPECT_LT(hypotf(b[i].x, b[i].z) + reach, 104.0F); /* dans l'anneau de lave (105 m) */
        tallest = fmaxf(tallest, 2.0F * b[i].half[1]);
    }
    EXPECT_NEAR(tallest, 18.0F, 0.01F);
    /* le chemin du château arrive par le sud-ouest (-x, +z) : cette diagonale est libre sur 12 m
     * de large, jusqu'au centre */
    for (int i = 0; i <= 150; i++) {
        const float along = (float)i;
        EXPECT_FALSE(blocked(b, nv, -0.7071068F * along, 0.7071068F * along, 6.0F));
    }
}

UTEST(zone_decor, the_spawn_castle_has_four_gates_and_a_free_courtyard)
{
    EngDecorBox b[ENG_DECOR_MAX];
    const uint32_t n = eng_camp_decor(b, ENG_DECOR_MAX);
    ASSERT_GE(n, 50U);
    ASSERT_LE(n, ENG_DECOR_MAX);
    EXPECT_TRUE(all_valid(b, n));
    for (int ix = -10; ix <= 10; ix++) { /* le joueur se réveille au centre de la cour, libre */
        for (int iz = -10; iz <= 6; iz++) {
            ASSERT_FALSE(blocked(b, n, (float)ix, (float)iz, 0.0F));
        }
    }
    /* une porte au milieu de chaque côté : on sort dans les quatre directions (rayon 0,5 m) */
    const float half = ENG_DECOR_CASTLE_HALF;
    for (int i = 0; i <= 120; i++) {
        const float t = -half - 4.0F + (0.4F * (float)i);
        EXPECT_FALSE(blocked(b, n, t, 0.0F, 0.5F));
        EXPECT_FALSE(blocked(b, n, 0.0F, t, 0.5F));
    }
    /* ailleurs, le mur est fermé */
    EXPECT_TRUE(blocked(b, n, -half, 10.0F, 0.0F));
    EXPECT_TRUE(blocked(b, n, 10.0F, half, 0.0F));
    bool fire = false;
    for (uint32_t i = 0; i < n; i++) {
        fire = fire || b[i].color == (uint8_t)ENG_DECOR_FIRE;
    }
    EXPECT_TRUE(fire);
    float wall_top = 0.0F; /* remparts bas : on voit la carte depuis la cour */
    for (uint32_t i = 0; i < n; i++) {
        if (b[i].solid && b[i].color == (uint8_t)ENG_DECOR_STONE &&
            (b[i].half[0] < 1.0F || b[i].half[2] < 1.0F)) {
            wall_top = fmaxf(wall_top, b[i].lift + (2.0F * b[i].half[1]));
        }
    }
    EXPECT_NEAR(wall_top, 2.4F, 1e-4F);
    /* arguments invalides, liste trop courte */
    build();
    EXPECT_EQ(eng_zone_decor(NULL, b, ENG_DECOR_MAX), 0U);
    EXPECT_EQ(eng_zone_decor(&g_zones[2], NULL, ENG_DECOR_MAX), 0U);
    EXPECT_EQ(eng_zone_decor(&g_zones[2], b, 7U), 7U);
    EXPECT_EQ(eng_camp_decor(NULL, 8U), 0U);
    EXPECT_EQ(eng_camp_decor(b, 2U), 2U);
}
