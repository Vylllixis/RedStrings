/* Écran d'attente logiciel (engine/boot_screen.h) : la corde de chargement dessinée par le
 * processeur. La référence de ces tests est une transcription DIRECTE du shader
 * (shaders/splash.frag.hlsl, section « La corde du bas » et fin de main), sans table ni raccourci :
 * le module doit donner la même image, à un niveau près. L'égalité avec le vrai shader, elle, est
 * vérifiée par le lanceur (--splash-shots). */
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "engine/boot_screen.h"
#include "utest.h"

#define PI_F 3.14159265F
#define LABEL_W 64U
#define LABEL_H 32U

typedef struct V3 {
    float r;
    float g;
    float b;
} V3;

/* ── La référence : le shader, ligne à ligne ── */

static float sat(float x)
{
    return x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x);
}

static float sstep(float a, float b, float x)
{
    const float t = sat((x - a) / (b - a));
    return t * t * (3.0F - 2.0F * t);
}

static float frac(float x)
{
    return x - floorf(x);
}

static V3 lerp3(V3 a, V3 b, float t)
{
    return (V3){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

static V3 add3(V3 a, V3 b, float k)
{
    return (V3){a.r + b.r * k, a.g + b.g * k, a.b + b.b * k};
}

static V3 scale3(V3 a, float k)
{
    return (V3){a.r * k, a.g * k, a.b * k};
}

static float ref_texel(const EngBootText *t, int x, int y)
{
    x = x < 0 ? 0 : (x >= (int)t->width ? (int)t->width - 1 : x);
    y = y < 0 ? 0 : (y >= (int)t->height ? (int)t->height - 1 : y);
    return (float)t->pixels[(size_t)y * t->width + (size_t)x] / 255.0F;
}

/* text_field(p, (0, -0,467), label_half, label_rect) : échantillonnage bilinéaire, bords prolongés
 */
static float ref_field(const EngBootText *t, float aspect, uint32_t w, uint32_t h, uint32_t x,
                       uint32_t y)
{
    const float px = (((float)x + 0.5F) / (float)w - 0.5F) * aspect;
    const float py = 0.5F - ((float)y + 0.5F) / (float)h;
    const float half_h = 0.0125F;
    const float half_w = half_h * (t->aspect > 0.01F ? t->aspect : 0.01F);
    const float qx = px / half_w;
    const float qy = (py + 0.467F) / half_h;
    if (fabsf(qx) > 1.0F || fabsf(qy) > 1.0F) {
        return 0.0F;
    }
    const float u = t->rect[0] + (t->rect[2] - t->rect[0]) * sat(qx * 0.5F + 0.5F);
    const float v = t->rect[1] + (t->rect[3] - t->rect[1]) * sat(0.5F - qy * 0.5F);
    const float tx = u * (float)t->width - 0.5F;
    const float ty = v * (float)t->height - 0.5F;
    const int x0 = (int)floorf(tx);
    const int y0 = (int)floorf(ty);
    const float fx = tx - floorf(tx);
    const float fy = ty - floorf(ty);
    const float top = ref_texel(t, x0, y0) + (ref_texel(t, x0 + 1, y0) - ref_texel(t, x0, y0)) * fx;
    const float bottom =
        ref_texel(t, x0, y0 + 1) + (ref_texel(t, x0 + 1, y0 + 1) - ref_texel(t, x0, y0 + 1)) * fx;
    return top + (bottom - top) * fy;
}

static uint32_t ref_byte(float c)
{
    const float toned = 1.0F - expf(-c * 1.25F);
    return (uint32_t)(powf(sat(toned), 1.0F / 2.2F) * 255.0F + 0.5F);
}

static uint32_t ref_pixel(const EngBootFrame *f, uint32_t x, uint32_t y)
{
    const float t = f->time_s;
    const float progress = sat(f->progress);
    const float aspect = (float)f->width / (float)f->height;
    const float uvx = ((float)x + 0.5F) / (float)f->width;
    const float uvy = ((float)y + 0.5F) / (float)f->height;
    const float px = (uvx - 0.5F) * aspect;
    const float py = 0.5F - uvy;
    const float aa = 1.5F / (float)f->height;
    const V3 thread = {1.0F, 0.07F, 0.1F};
    V3 col = {0.0F, 0.0F, 0.0F};
    const float lead = sstep(0.0F, 0.3F, t);
    const float bar_half = fminf(0.36F, aspect * 0.32F);
    const float u_bar = (px + bar_half) / (2.0F * bar_half);
    const float in_bar = (u_bar >= 0.0F ? 1.0F : 0.0F) * (1.0F >= u_bar ? 1.0F : 0.0F);
    const float pluck =
        sinf(u_bar * PI_F) * sinf(t * 13.0F) * 0.0022F * expf(-frac(t * 0.5F) * 3.0F);
    const float bar_d = fabsf(py - (-0.435F + pluck));
    const float filled = sstep(progress + 0.004F, progress - 0.004F, u_bar);
    const float string_core = sstep(0.0022F + aa, 0.0022F - aa, bar_d) * in_bar;
    col = lerp3(col, scale3((V3){0.2F, 0.03F, 0.05F}, lead), string_core * 0.9F * (1.0F - filled));
    col = add3(col, thread, (string_core + expf(-bar_d * 150.0F) * 0.5F * in_bar) * filled * lead);
    const float left = hypotf(px + bar_half, py + 0.435F);
    const float right = hypotf(px - bar_half, py + 0.435F);
    const float pegs = fminf(left, right) - 0.0055F;
    col = lerp3(col, scale3((V3){0.75F, 0.55F, 0.45F}, lead), sstep(aa, -aa, pegs));
    const float bead_x = -bar_half + progress * 2.0F * bar_half;
    const float bead_r = hypotf(px - bead_x, py - (-0.435F + pluck));
    col = add3(col, (V3){1.0F, 0.55F, 0.45F}, expf(-bead_r * 150.0F) * 1.3F * lead);
    if (f->label != NULL) {
        /* fwidth : pente mesurée dans le bloc de 2 x 2 pixels, le long de la ligne et de la colonne
         */
        const uint32_t x0 = x & ~1U;
        const uint32_t y0 = y & ~1U;
        const float label = ref_field(f->label, aspect, f->width, f->height, x, y);
        const float ddx = ref_field(f->label, aspect, f->width, f->height, x0 + 1U, y) -
                          ref_field(f->label, aspect, f->width, f->height, x0, y);
        const float ddy = ref_field(f->label, aspect, f->width, f->height, x, y0 + 1U) -
                          ref_field(f->label, aspect, f->width, f->height, x, y0);
        const float edge = (fabsf(ddx) + fabsf(ddy)) * 0.75F + 0.004F;
        col = lerp3(col, scale3((V3){0.95F, 0.8F, 0.72F}, lead),
                    sstep(0.5F - edge, 0.5F + edge, label) * (0.55F + 0.2F * sinf(t * 2.2F)));
    }
    const float vx = uvx - 0.5F;
    const float vy = uvy - 0.5F;
    col = scale3(col, 1.0F - (vx * vx + vy * vy) * 0.85F);
    return (ref_byte(col.r) << 16U) | (ref_byte(col.g) << 8U) | ref_byte(col.b);
}

/* ── Outils ── */

static uint8_t label_pixels[LABEL_W * LABEL_H];

/* une légende d'essai : un « pavé » en champ de distance dans le bas de l'image (le haut joue le
 * titre), nul sur les bords de son rectangle comme un vrai texte */
static EngBootText test_label(void)
{
    for (uint32_t y = 0; y < LABEL_H; y++) {
        for (uint32_t x = 0; x < LABEL_W; x++) {
            const float dx = fabsf((float)x - 31.5F) / 28.0F;
            const float dy = fabsf((float)y - 20.0F) / 8.0F;
            const float inside = 1.0F - (dx > dy ? dx : dy);
            const float v = y < 8U ? 200.0F : sat(inside * 2.0F) * 255.0F;
            label_pixels[y * LABEL_W + x] = (uint8_t)v;
        }
    }
    return (EngBootText){.pixels = label_pixels,
                         .width = LABEL_W,
                         .height = LABEL_H,
                         .rect = {0.0F, 0.25F, 1.0F, 1.0F},
                         .aspect = 6.0F};
}

static uint32_t zone_px[1024U * 256U];

/* dessine la zone de `f` ; false si elle ne tient pas dans le tampon d'essai */
static bool draw(const EngBootFrame *f, EngBootRect *zone)
{
    *zone = eng_boot_bounds(f->width, f->height, f->label);
    if ((size_t)zone->width * zone->height > sizeof zone_px / sizeof zone_px[0]) {
        return false;
    }
    memset(zone_px, 0xAB, sizeof zone_px);
    eng_boot_draw(f, zone_px, (size_t)zone->width * sizeof(uint32_t));
    return true;
}

/* le pixel (x, y) de l'ÉCRAN dans la zone dessinée ; noir hors de la zone */
static uint32_t at(const EngBootRect *zone, uint32_t x, uint32_t y)
{
    if (x < zone->x || y < zone->y || x >= zone->x + zone->width || y >= zone->y + zone->height) {
        return 0U;
    }
    return zone_px[(size_t)(y - zone->y) * zone->width + (x - zone->x)];
}

static uint32_t channel_gap(uint32_t a, uint32_t b)
{
    uint32_t worst = 0;
    for (uint32_t shift = 0; shift <= 16U; shift += 8U) {
        const int d = (int)((a >> shift) & 0xFFU) - (int)((b >> shift) & 0xFFU);
        const uint32_t gap = (uint32_t)(d < 0 ? -d : d);
        worst = gap > worst ? gap : worst;
    }
    return worst;
}

static uint32_t red(uint32_t c)
{
    return (c >> 16U) & 0xFFU;
}

static uint32_t green(uint32_t c)
{
    return (c >> 8U) & 0xFFU;
}

static uint32_t blue(uint32_t c)
{
    return c & 0xFFU;
}

/* colonne et ligne de l'écran d'un point du repère du shader */
static uint32_t col_of(const EngBootFrame *f, float px)
{
    const float aspect = (float)f->width / (float)f->height;
    return (uint32_t)((px / aspect + 0.5F) * (float)f->width);
}

static uint32_t row_of(const EngBootFrame *f, float py)
{
    return (uint32_t)((0.5F - py) * (float)f->height);
}

static float bar_half(const EngBootFrame *f)
{
    return fminf(0.36F, (float)f->width / (float)f->height * 0.32F);
}

/* ── Tests ── */

/* Le module donne la même image que la transcription du shader, à un niveau près (la table de la
 * courbe de ton), pour plusieurs formats d'écran, instants et progressions, avec et sans légende.
 */
UTEST(boot_screen, same_image_as_the_shader_formulas)
{
    static const uint32_t sizes[4][2] = {{1280U, 720U}, {640U, 400U}, {800U, 600U}, {360U, 640U}};
    static const float moments[6][2] = {/* temps, progression */
                                        {0.0F, 0.0F},    {0.1F, 0.004F}, {0.3F, 0.02F},
                                        {1.37F, 0.068F}, {2.9F, 0.5F},   {4.06F, 1.0F}};
    const EngBootText label = test_label();
    uint32_t worst = 0;
    uint32_t lit = 0;
    for (uint32_t s = 0; s < 4U; s++) {
        for (uint32_t m = 0; m < 12U; m++) { /* chaque instant sans légende, puis avec */
            const EngBootFrame f = {.width = sizes[s][0],
                                    .height = sizes[s][1],
                                    .time_s = moments[m / 2U][0],
                                    .progress = moments[m / 2U][1],
                                    .label = (m % 2U) == 1U ? &label : NULL};
            EngBootRect zone;
            ASSERT_TRUE(draw(&f, &zone));
            for (uint32_t y = zone.y; y < zone.y + zone.height; y++) {
                for (uint32_t x = zone.x; x < zone.x + zone.width; x++) {
                    const uint32_t got = at(&zone, x, y);
                    const uint32_t gap = channel_gap(got, ref_pixel(&f, x, y));
                    worst = gap > worst ? gap : worst;
                    lit += got != 0U ? 1U : 0U;
                    const uint32_t unused_byte = got >> 24U;
                    ASSERT_EQ(0U, unused_byte); /* l'octet de poids fort reste nul (XRGB) */
                }
            }
        }
    }
    EXPECT_LE(worst, 1U);
    EXPECT_GT(lit, 40000U); /* et l'on a bien comparé des pixels éclairés */
}

/* Hors de la zone annoncée par eng_boot_bounds, la référence est noire partout : la zone suffit,
 * quels que soient l'instant, la progression et le format (le lanceur ne redessine qu'elle). */
UTEST(boot_screen, nothing_is_lit_outside_the_bounds)
{
    static const uint32_t sizes[3][2] = {{640U, 400U}, {512U, 384U}, {300U, 500U}};
    static const float moments[5][2] = {
        {0.12F, 0.0F}, {0.5F, 1.0F}, {2.12F, 0.5F}, {3.0F, 0.02F}, {4.12F, 0.98F}};
    const EngBootText label = test_label();
    uint32_t outside = 0;
    for (uint32_t s = 0; s < 3U; s++) {
        for (uint32_t m = 0; m < 5U; m++) {
            const EngBootFrame f = {.width = sizes[s][0],
                                    .height = sizes[s][1],
                                    .time_s = moments[m][0],
                                    .progress = moments[m][1],
                                    .label = (m % 2U) == 0U ? &label : NULL};
            const EngBootRect zone = eng_boot_bounds(f.width, f.height, f.label);
            ASSERT_LE(zone.x + zone.width, f.width);
            ASSERT_LE(zone.y + zone.height, f.height);
            for (uint32_t y = 0; y < f.height; y++) {
                for (uint32_t x = 0; x < f.width; x++) {
                    const bool in_zone = x >= zone.x && y >= zone.y && x < zone.x + zone.width &&
                                         y < zone.y + zone.height;
                    outside += !in_zone && ref_pixel(&f, x, y) != 0U ? 1U : 0U;
                }
            }
        }
    }
    EXPECT_EQ(0U, outside);
}

/* La zone est une bande du bas de l'écran, pas l'écran entier : c'est ce qui rend l'animation
 * légère (au plus un cinquième de la hauteur, et pas plus large que la corde et ses lueurs). */
UTEST(boot_screen, bounds_are_a_band_at_the_bottom)
{
    const EngBootRect zone = eng_boot_bounds(2560U, 1600U, NULL);
    EXPECT_EQ(1600U, zone.y + zone.height); /* les lueurs descendent jusqu'au bas de l'écran */
    EXPECT_GT(zone.y, 1280U);               /* et ne montent pas au-dessus des 4/5 */
    EXPECT_LE(zone.height, 320U);
    /* la corde fait 0,72 hauteur d'écran, ses lueurs 0,1 de chaque côté : 0,92 x 1600 = 1472 px */
    EXPECT_GE(zone.width, 1472U);
    EXPECT_LE(zone.width, 1476U);
    EXPECT_EQ(2560U, zone.x * 2U + zone.width); /* centrée */
    /* une légende plus large que la corde élargit la zone */
    EngBootText wide = test_label();
    wide.aspect = 80.0F; /* demi-largeur 0,0125 x 80 = 1 : plus que l'écran */
    const EngBootRect all = eng_boot_bounds(2560U, 1600U, &wide);
    EXPECT_EQ(0U, all.x);
    EXPECT_EQ(2560U, all.width);
    /* ... sauf si elle est inutilisable (pas d'image, ou un rectangle qui n'est pas fini : chacune
     * de ses quatre valeurs compte) : la zone est alors celle d'un écran sans légende */
    for (uint32_t k = 0; k < 5U; k++) {
        EngBootText unusable = wide;
        if (k < 4U) {
            unusable.rect[k] = k % 2U == 0U ? NAN : INFINITY;
        } else {
            unusable.pixels = NULL;
        }
        const EngBootRect narrow = eng_boot_bounds(2560U, 1600U, &unusable);
        ASSERT_EQ(zone.x, narrow.x);
        ASSERT_EQ(zone.width, narrow.width);
        ASSERT_EQ(zone.y, narrow.y);
    }
    /* écran vide : zone vide */
    EXPECT_EQ(0U, eng_boot_bounds(0U, 720U, NULL).width);
    EXPECT_EQ(0U, eng_boot_bounds(1280U, 0U, NULL).height);
}

/* La corde part du noir et apparaît en fondu (ENG_BOOT_FADE_S), comme la fenêtre vient
 * d'apparaître. */
UTEST(boot_screen, fades_in_from_black)
{
    EngBootFrame f = {.width = 1280U, .height = 720U, .time_s = 0.0F, .progress = 1.0F};
    EngBootRect zone;
    const uint32_t x = col_of(&f, -0.1F);
    const uint32_t y = row_of(&f, -0.435F);
    ASSERT_TRUE(draw(&f, &zone));
    for (size_t i = 0; i < (size_t)zone.width * zone.height; i++) {
        ASSERT_EQ(0U, zone_px[i]); /* à l'instant 0 : tout est noir */
    }
    f.time_s = ENG_BOOT_FADE_S * 0.5F;
    ASSERT_TRUE(draw(&f, &zone));
    const uint32_t half = red(at(&zone, x, y));
    f.time_s = ENG_BOOT_FADE_S;
    ASSERT_TRUE(draw(&f, &zone));
    const uint32_t full = red(at(&zone, x, y));
    f.time_s = ENG_BOOT_FADE_S + 5.0F;
    ASSERT_TRUE(draw(&f, &zone));
    const uint32_t later = red(at(&zone, x, y));
    /* corde pleine : rouge (1 + 0,5 de lueur) x 0,84 de vignette, soit 229 après la courbe de ton
     */
    EXPECT_GT(half, 100U);
    EXPECT_LT(half, full - 20U);
    EXPECT_NEAR(229.0F, (float)full, 4.0F);
    EXPECT_LE(channel_gap(full, later), 2U); /* le fondu est fini : la corde ne s'éclaircit plus */
}

/* La partie remplie est rouge vif et lumineuse, la partie vide est une corde sombre sans lueur. */
UTEST(boot_screen, filled_part_glows_and_empty_part_is_dark)
{
    const EngBootFrame f = {.width = 1280U, .height = 720U, .time_s = 1.0F, .progress = 0.5F};
    EngBootRect zone;
    ASSERT_TRUE(draw(&f, &zone));
    const uint32_t line = row_of(&f, -0.435F);
    const uint32_t filled = at(&zone, col_of(&f, -0.5F * bar_half(&f)), line);
    const uint32_t empty = at(&zone, col_of(&f, 0.5F * bar_half(&f)), line);
    EXPECT_GT(red(filled), 215U);
    EXPECT_LT(green(filled), 130U);
    EXPECT_GT(red(empty), 80U);
    EXPECT_LT(red(empty), 150U);
    EXPECT_LT(green(empty), 70U);
    EXPECT_GT(red(empty), blue(empty) + 30U); /* sombre, mais rouge */
    /* 12 pixels au-dessus : une lueur rouge côté rempli, rien côté vide */
    EXPECT_GT(red(at(&zone, col_of(&f, -0.5F * bar_half(&f)), line - 12U)), 40U);
    EXPECT_EQ(0U, at(&zone, col_of(&f, 0.5F * bar_half(&f)), line - 12U));
    /* hors de la corde (au-delà des chevilles) : rien sur la ligne */
    EXPECT_EQ(0U, at(&zone, col_of(&f, bar_half(&f) + 0.05F), line));
}

/* Le nœud lumineux est à la progression : le point le plus clair de la corde le suit. */
UTEST(boot_screen, knot_follows_the_progress)
{
    static const float progress[3] = {0.1F, 0.5F, 0.9F};
    for (uint32_t i = 0; i < 3U; i++) {
        const EngBootFrame f = {
            .width = 1280U, .height = 720U, .time_s = 1.0F, .progress = progress[i]};
        EngBootRect zone;
        ASSERT_TRUE(draw(&f, &zone));
        const uint32_t line = row_of(&f, -0.435F);
        uint32_t best = 0;
        uint32_t best_x = 0;
        /* entre les chevilles (elles sont claires aussi) */
        for (uint32_t x = col_of(&f, -bar_half(&f) + 0.02F); x < col_of(&f, bar_half(&f) - 0.02F);
             x++) {
            const uint32_t c = at(&zone, x, line);
            const uint32_t light = red(c) + green(c) + blue(c);
            if (light > best) {
                best = light;
                best_x = x;
            }
        }
        const uint32_t expected = col_of(&f, -bar_half(&f) + progress[i] * 2.0F * bar_half(&f));
        EXPECT_LE(best_x > expected ? best_x - expected : expected - best_x, 2U);
        EXPECT_GT(best, 600U); /* presque blanc */
    }
}

/* rangée la plus lumineuse de la colonne `x` (le centre de la corde), au dixième de pixel */
static float string_row(const EngBootRect *zone, uint32_t x)
{
    float sum = 0.0F;
    float weight = 0.0F;
    for (uint32_t y = zone->y; y < zone->y + zone->height; y++) {
        const float r = (float)red(at(zone, x, y));
        if (r > 200.0F) { /* le cœur de la corde, pas sa lueur */
            sum += r * (float)y;
            weight += r;
        }
    }
    return weight > 0.0F ? sum / weight : -1.0F;
}

/* La corde vibre comme une corde pincée : son milieu bouge, ses deux bouts (les chevilles) non. */
UTEST(boot_screen, string_vibrates_between_its_fixed_ends)
{
    /* à 2,12 s : juste après une impulsion (une toutes les 2 s), au sommet d'une oscillation ;
     * à 3,8664 s : sin(13 t) = 0, la corde est droite */
    EngBootFrame f = {.width = 1000U, .height = 1000U, .time_s = 2.12F, .progress = 1.0F};
    EngBootRect zone;
    const uint32_t middle = col_of(&f, 0.0F);
    const uint32_t end = col_of(&f, -bar_half(&f) + 0.012F);
    ASSERT_TRUE(draw(&f, &zone));
    const float plucked_middle = string_row(&zone, middle);
    const float plucked_end = string_row(&zone, end);
    f.time_s = 3.8664F;
    ASSERT_TRUE(draw(&f, &zone));
    const float straight_middle = string_row(&zone, middle);
    const float straight_end = string_row(&zone, end);
    ASSERT_GT(plucked_middle, 0.0F);
    ASSERT_GT(straight_middle, 0.0F);
    /* amplitude attendue au milieu : sin(13 x 2,12) x 0,0022 x exp(-0,06 x 3) = 0,00126, soit
     * 1,26 pixel sur 1000 */
    EXPECT_NEAR(1.26F, fabsf(plucked_middle - straight_middle), 0.3F);
    EXPECT_LT(fabsf(plucked_end - straight_end), 0.15F);
    EXPECT_NEAR(935.0F, straight_middle, 0.6F); /* la corde est à 43,5 % sous le centre */
}

/* La légende est dessinée sous la corde quand on la donne, et pas sans elle. */
UTEST(boot_screen, label_is_drawn_under_the_string)
{
    const EngBootText label = test_label();
    EngBootFrame f = {.width = 1280U, .height = 720U, .time_s = 1.0F, .progress = 0.3F};
    EngBootRect zone;
    const uint32_t x = col_of(&f, 0.0F);
    const uint32_t y = row_of(&f, -0.467F);
    ASSERT_TRUE(draw(&f, &zone));
    EXPECT_EQ(0U, at(&zone, x, y)); /* sans légende : noir à cet endroit */
    f.label = &label;
    ASSERT_TRUE(draw(&f, &zone));
    const uint32_t c = at(&zone, x, y);
    EXPECT_GT(red(c), 150U); /* ivoire : rouge > vert > bleu */
    EXPECT_GT(red(c), green(c));
    EXPECT_GT(green(c), blue(c));
    /* la légende respire : son opacité change avec le temps (0,55 + 0,2 sin(2,2 t)) */
    f.time_s = 2.2F;
    ASSERT_TRUE(draw(&f, &zone));
    EXPECT_LT(red(at(&zone, x, y)), red(c) - 15U);
    /* une légende sans image ne dessine rien et ne plante pas */
    EngBootText broken = label;
    broken.pixels = NULL;
    f.label = &broken;
    ASSERT_TRUE(draw(&f, &zone));
    EXPECT_EQ(0U, at(&zone, x, y));
    /* ni une légende dont le rectangle n'est pas fini (chacune de ses quatre valeurs) */
    for (uint32_t k = 0; k < 4U; k++) {
        broken = label;
        broken.rect[k] = k % 2U == 0U ? NAN : INFINITY;
        f.label = &broken;
        ASSERT_TRUE(draw(&f, &zone));
        ASSERT_EQ(0U, at(&zone, x, y));
    }
}

/* eng_boot_draw n'écrit que dans la zone, en respectant le pas de ligne donné. */
UTEST(boot_screen, draw_stays_inside_its_zone)
{
    const EngBootFrame f = {.width = 320U, .height = 200U, .time_s = 1.0F, .progress = 0.6F};
    const EngBootRect zone = eng_boot_bounds(f.width, f.height, NULL);
    ASSERT_GT(zone.width, 0U);
    ASSERT_LE(zone.width, 320U);
    ASSERT_LE(zone.height, 60U);
    /* un pas de ligne plus grand que la zone : les colonnes en trop gardent leur valeur */
    enum { PITCH_PX = 330, ROWS = 62 };
    static uint32_t buffer[PITCH_PX * ROWS];
    for (size_t i = 0; i < sizeof buffer / sizeof buffer[0]; i++) {
        buffer[i] = 0xDEADBEEFU;
    }
    eng_boot_draw(&f, &buffer[PITCH_PX], (size_t)PITCH_PX * sizeof(uint32_t));
    uint32_t drawn = 0;
    for (uint32_t row = 0; row < (uint32_t)ROWS; row++) {
        for (uint32_t col = 0; col < (uint32_t)PITCH_PX; col++) {
            const uint32_t v = buffer[row * (uint32_t)PITCH_PX + col];
            const bool inside = row >= 1U && row < 1U + zone.height && col < zone.width;
            if (inside) {
                ASSERT_NE(0xDEADBEEFU, v);
                ASSERT_LE(channel_gap(v, ref_pixel(&f, zone.x + col, zone.y + row - 1U)), 1U);
                drawn += v != 0U ? 1U : 0U;
            } else {
                ASSERT_EQ(0xDEADBEEFU, v);
            }
        }
    }
    EXPECT_GT(drawn, 200U);
    /* un pas de ligne plus petit que la zone, ou qui n'est pas un nombre entier de pixels : rien
     * n'est écrit */
    for (size_t i = 0; i < sizeof buffer / sizeof buffer[0]; i++) {
        buffer[i] = 0xDEADBEEFU;
    }
    eng_boot_draw(&f, buffer, (size_t)(zone.width - 1U) * sizeof(uint32_t));
    eng_boot_draw(&f, buffer, (size_t)PITCH_PX * sizeof(uint32_t) + 2U);
    for (size_t i = 0; i < sizeof buffer / sizeof buffer[0]; i++) {
        ASSERT_EQ(0xDEADBEEFU, buffer[i]);
    }
    /* juste la largeur de la zone : accepté */
    eng_boot_draw(&f, buffer, (size_t)zone.width * sizeof(uint32_t));
    EXPECT_NE(0xDEADBEEFU, buffer[0]);
}

/* Paramètres absurdes : rien n'est écrit, rien ne plante ; un temps ou une progression non finis
 * valent zéro. */
UTEST(boot_screen, invalid_input_is_harmless)
{
    static uint32_t buffer[64];
    memset(buffer, 0x5A, sizeof buffer);
    const EngBootFrame empty = {.width = 0U, .height = 100U, .time_s = 1.0F, .progress = 0.5F};
    const EngBootFrame flat = {.width = 100U, .height = 0U, .time_s = 1.0F, .progress = 0.5F};
    eng_boot_draw(NULL, buffer, sizeof buffer);
    eng_boot_draw(&empty, buffer, sizeof buffer);
    eng_boot_draw(&flat, buffer, sizeof buffer);
    for (size_t i = 0; i < 64U; i++) {
        ASSERT_EQ(0x5A5A5A5AU, buffer[i]);
    }
    const EngBootFrame valid = {.width = 320U, .height = 200U, .time_s = 1.0F, .progress = 0.5F};
    eng_boot_draw(&valid, NULL, 4096U); /* pas de tampon : sans effet */

    /* temps non fini (ni un nombre, infini) ou négatif : comme l'instant 0, tout est noir */
    static const float bad_times[3] = {NAN, INFINITY, -2.0F};
    EngBootFrame f = {.width = 640U, .height = 400U, .time_s = 1.0F, .progress = 0.5F};
    EngBootRect zone;
    for (uint32_t k = 0; k < 3U; k++) {
        f.time_s = bad_times[k];
        ASSERT_TRUE(draw(&f, &zone));
        for (size_t i = 0; i < (size_t)zone.width * zone.height; i++) {
            ASSERT_EQ(0U, zone_px[i]);
        }
    }
    /* progression non finie : exactement l'image de la progression 0 */
    static uint32_t bounded[sizeof zone_px / sizeof zone_px[0]];
    f.time_s = 1.0F;
    f.progress = 0.0F;
    ASSERT_TRUE(draw(&f, &zone));
    const size_t bytes = (size_t)zone.width * zone.height * sizeof(uint32_t);
    memcpy(bounded, zone_px, bytes);
    f.progress = NAN;
    ASSERT_TRUE(draw(&f, &zone));
    const uint32_t line = row_of(&f, -0.435F);
    EXPECT_LT(red(at(&zone, col_of(&f, 0.0F), line)), 150U); /* corde vide au milieu */
    EXPECT_GT(red(at(&zone, col_of(&f, 0.0F), line)), 60U);  /* mais bien dessinée */
    EXPECT_EQ(0, memcmp(bounded, zone_px, bytes));
    /* progression hors de [0, 1] : bornée. 2 donne exactement l'image de 1 (corde pleine, nœud au
     * bout), -3 celle de 0 */
    f.progress = 1.0F;
    ASSERT_TRUE(draw(&f, &zone));
    memcpy(bounded, zone_px, bytes);
    f.progress = 2.0F;
    ASSERT_TRUE(draw(&f, &zone));
    EXPECT_GT(red(at(&zone, col_of(&f, bar_half(&f) - 0.05F), line)), 190U);
    EXPECT_EQ(0, memcmp(bounded, zone_px, bytes));
    f.progress = 0.0F;
    ASSERT_TRUE(draw(&f, &zone));
    memcpy(bounded, zone_px, bytes);
    f.progress = -3.0F;
    ASSERT_TRUE(draw(&f, &zone));
    EXPECT_LT(red(at(&zone, col_of(&f, -bar_half(&f) + 0.05F), line)), 150U);
    EXPECT_EQ(0, memcmp(bounded, zone_px, bytes));
}

/* La progression de l'attente monte sans jamais atteindre sa limite : on ne sait pas combien de
 * temps le pilote mettra, et le chargement du jeu reprend la corde là où elle en est. */
UTEST(boot_screen, progress_creeps_up_without_reaching_its_end)
{
    float p = 0.0F;
    float previous = 0.0F;
    float after_cold_start = 0.0F;
    for (uint32_t frame = 1; frame <= 3750U; frame++) { /* 60 s à 16 ms par image */
        p = eng_boot_progress(p, 0.016F);
        ASSERT_GE(p, previous);
        ASSERT_LT(p, ENG_BOOT_PROGRESS_END);
        previous = p;
        if (frame == 169U) { /* 2,7 s : le démarrage à froid mesuré */
            after_cold_start = p;
        }
    }
    EXPECT_NEAR(0.068F, after_cold_start, 0.004F); /* 0,08 x (1 - exp(-2,7 / 1,4)) */
    EXPECT_GT(p, ENG_BOOT_PROGRESS_END - 0.03F);   /* après une minute : tout près de sa limite */
    /* la première image avance déjà */
    EXPECT_GT(eng_boot_progress(0.0F, 0.016F), 0.0F);
    /* pas de temps écoulé, durée négative ou non finie : inchangée */
    EXPECT_EQ(0.03F, eng_boot_progress(0.03F, 0.0F));
    EXPECT_EQ(0.03F, eng_boot_progress(0.03F, -1.0F));
    EXPECT_EQ(0.03F, eng_boot_progress(0.03F, NAN));
    EXPECT_EQ(0.03F, eng_boot_progress(0.03F, INFINITY));
    /* un à-coup (10 s d'un coup) ne compte que pour 0,1 s */
    EXPECT_EQ(eng_boot_progress(0.01F, 0.1F), eng_boot_progress(0.01F, 10.0F));
    EXPECT_GT(eng_boot_progress(0.01F, 0.1F), eng_boot_progress(0.01F, 0.05F));
    /* elle ne recule jamais : déjà au-delà de sa limite, elle reste où elle est */
    EXPECT_EQ(0.5F, eng_boot_progress(0.5F, 0.016F));
    /* valeur absurde : bornée à [0, 1], non finie : 0 */
    EXPECT_EQ(1.0F, eng_boot_progress(7.0F, 0.016F));
    EXPECT_GT(eng_boot_progress(-4.0F, 0.016F), 0.0F);
    EXPECT_LT(eng_boot_progress(-4.0F, 0.016F), 0.01F);
    EXPECT_LT(eng_boot_progress(NAN, 0.016F), 0.01F);
}
