/* Écran d'attente logiciel : la corde de chargement de shaders/splash.frag.hlsl, dessinée par le
 * processeur. Voir engine/boot_screen.h. Chaque formule reprend celle du shader (section « La
 * corde du bas » et fin de main) : toute retouche se fait des deux côtés, et --splash-shots compare
 * les deux images. */
#include "engine/boot_screen.h"

#include <math.h>
#include <stdbool.h>

#define PI_F 3.14159265F
#define BAR_Y (-0.435F)       /* hauteur de la corde */
#define BAR_HALF_MAX 0.36F    /* demi-longueur de la corde, au plus */
#define BAR_HALF_ASPECT 0.32F /* ... et pas plus que cette part de la largeur de l'écran */
#define STRING_HALF 0.0022F   /* demi-épaisseur de la corde */
#define PLUCK 0.0022F         /* amplitude de sa vibration */
#define PEG_RADIUS 0.0055F    /* les deux chevilles */
#define GLOW_FALLOFF 150.0F   /* décroissance des lueurs (corde remplie, nœud) */
#define LABEL_Y (-0.467F)     /* centre de la légende */
#define LABEL_HALF 0.0125F    /* sa demi-hauteur */
/* Au-delà de cette distance de la corde et du nœud, leurs deux lueurs réunies restent sous un
 * demi-niveau (0,5 / 255) : le pixel est noir. Calcul : (0,5 + 1,3) x exp(-150 x 0,1) x 1,25
 * = 6,9e-7, et un demi-niveau vaut (0,5 / 255)^2,2 = 1,1e-6 avant la courbe de ton. */
#define REACH 0.1F

#define TONE_SIZE 4096U /* table de la courbe de ton, indexée par la racine de la couleur */
#define TONE_MAX 4.0F /* couleur linéaire la plus forte de la table (la corde ne dépasse pas 3) */

typedef struct Rgb {
    float r;
    float g;
    float b;
} Rgb;

/* ce qui ne dépend que de la colonne du pixel */
typedef struct Column {
    float px;       /* p.x */
    float vx2;      /* (uv.x - 0,5)^2, pour la vignette */
    float line_y;   /* hauteur de la corde à cette colonne (elle vibre) */
    float filled;   /* 1 : partie déjà remplie */
    float peg_dx2;  /* carré de la distance horizontale à la cheville la plus proche */
    float bead_dx2; /* ... au nœud */
    bool in_bar;    /* entre les deux chevilles */
} Column;

/* ce qui ne dépend que de l'image */
typedef struct Scene {
    const EngBootFrame *frame;
    float aspect;
    float aa;        /* demi-largeur de l'anticrénelage */
    float bar_half;  /* demi-longueur de la corde */
    float lead;      /* fondu d'apparition, de 0 à 1 */
    float swing;     /* vibration de la corde à cet instant (à multiplier par sin(u * pi)) */
    float label_lum; /* opacité de la légende (elle respire) */
    float label_half_w;
    float progress;
} Scene;

static float saturate(float x)
{
    return x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x);
}

/* smoothstep du shader ; comme lui, accepte a > b (la courbe descend alors) */
static float smooth(float a, float b, float x)
{
    const float t = saturate((x - a) / (b - a));
    return t * t * (3.0F - (2.0F * t));
}

static float mix(float a, float b, float t)
{
    return a + ((b - a) * t);
}

static float bar_half_of(float aspect)
{
    const float by_aspect = aspect * BAR_HALF_ASPECT;
    return by_aspect < BAR_HALF_MAX ? by_aspect : BAR_HALF_MAX;
}

static float label_aspect(const EngBootText *label)
{
    return label->aspect > 0.01F ? label->aspect : 0.01F; /* max(text_info.y, 0.01) du shader */
}

/* une légende sans image, ou dont le rectangle n'est pas fini, n'est pas dessinée */
static bool label_usable(const EngBootText *label)
{
    return label != NULL && label->pixels != NULL && label->width > 0U && label->height > 0U &&
           isfinite(label->rect[0]) && isfinite(label->rect[1]) && isfinite(label->rect[2]) &&
           isfinite(label->rect[3]);
}

/* ── La légende : échantillonnage bilinéaire du champ de distance, bords prolongés ── */

static float texel(const EngBootText *t, int32_t x, int32_t y)
{
    const int32_t cx = x < 0 ? 0 : (x >= (int32_t)t->width ? (int32_t)t->width - 1 : x);
    const int32_t cy = y < 0 ? 0 : (y >= (int32_t)t->height ? (int32_t)t->height - 1 : y);
    return (float)t->pixels[((size_t)cy * t->width) + (size_t)cx] / 255.0F;
}

static float sample(const EngBootText *t, float u, float v)
{
    const float x = (u * (float)t->width) - 0.5F;
    const float y = (v * (float)t->height) - 0.5F;
    const float fx0 = floorf(x);
    const float fy0 = floorf(y);
    const int32_t x0 = (int32_t)fx0;
    const int32_t y0 = (int32_t)fy0;
    const float fx = x - fx0;
    const float fy = y - fy0;
    return mix(mix(texel(t, x0, y0), texel(t, x0 + 1, y0), fx),
               mix(texel(t, x0, y0 + 1), texel(t, x0 + 1, y0 + 1), fx), fy);
}

/* text_field du shader : la valeur du champ au point p, 0 hors du rectangle de la légende */
static float label_field(const Scene *s, float px, float py)
{
    const EngBootText *t = s->frame->label;
    const float qx = px / s->label_half_w;
    const float qy = (py - LABEL_Y) / LABEL_HALF;
    if (fabsf(qx) > 1.0F || fabsf(qy) > 1.0F) {
        return 0.0F;
    }
    return sample(t, mix(t->rect[0], t->rect[2], saturate((qx * 0.5F) + 0.5F)),
                  mix(t->rect[1], t->rect[3], saturate(0.5F - (qy * 0.5F))));
}

static float pixel_px(const Scene *s, uint32_t x)
{
    return ((((float)x + 0.5F) / (float)s->frame->width) - 0.5F) * s->aspect;
}

static float pixel_py(const Scene *s, uint32_t y)
{
    return 0.5F - (((float)y + 0.5F) / (float)s->frame->height);
}

/* opacité de la légende au pixel (x, y). Le bord des lettres s'adoucit selon la pente du champ
 * (fwidth du shader) : comme le GPU, on la mesure dans le bloc de 2 x 2 pixels qui contient le
 * pixel, le long de sa ligne (entre les deux colonnes du bloc) et le long de sa colonne. */
static float label_alpha(const Scene *s, uint32_t x, uint32_t y)
{
    const float px = pixel_px(s, x);
    const float py = pixel_py(s, y);
    const uint32_t x0 = x & ~1U;
    const uint32_t y0 = y & ~1U;
    const float field = label_field(s, px, py);
    const float across =
        label_field(s, pixel_px(s, x0 + 1U), py) - label_field(s, pixel_px(s, x0), py);
    const float along =
        label_field(s, px, pixel_py(s, y0 + 1U)) - label_field(s, px, pixel_py(s, y0));
    const float edge = ((fabsf(across) + fabsf(along)) * 0.75F) + 0.004F;
    return smooth(0.5F - edge, 0.5F + edge, field) * s->label_lum;
}

/* ── Courbe de ton et gamma : sortie = (1 - exp(-1,25 c))^(1 / 2,2), en table ── */

static uint8_t tone_table[TONE_SIZE];
static bool tone_ready;

static void tone_build(void)
{
    for (uint32_t i = 0; i < TONE_SIZE; i++) {
        const float root = (float)i / (float)(TONE_SIZE - 1U);
        const float c = root * root * TONE_MAX;
        const float out = powf(1.0F - expf(-c * 1.25F), 1.0F / 2.2F);
        tone_table[i] = (uint8_t)((out * 255.0F) + 0.5F);
    }
    tone_ready = true;
}

static uint32_t tone(float c)
{
    const float bounded = c < 0.0F ? 0.0F : (c > TONE_MAX ? TONE_MAX : c);
    const float index = sqrtf(bounded / TONE_MAX) * (float)(TONE_SIZE - 1U);
    return tone_table[(uint32_t)(index + 0.5F)];
}

/* ── L'image ── */

static bool scene_init(Scene *s, const EngBootFrame *frame)
{
    if (frame == NULL || frame->width == 0U || frame->height == 0U) {
        return false;
    }
    if (!tone_ready) {
        tone_build();
    }
    const float t = isfinite(frame->time_s) && frame->time_s > 0.0F ? frame->time_s : 0.0F;
    s->frame = frame;
    s->aspect = (float)frame->width / (float)frame->height;
    s->aa = 1.5F / (float)frame->height;
    s->bar_half = bar_half_of(s->aspect);
    s->lead = smooth(0.0F, ENG_BOOT_FADE_S, t);
    const float beat =
        (t * 0.5F) - floorf(t * 0.5F); /* frac(t * 0,5) : une impulsion toutes les 2 s */
    s->swing = sinf(t * 13.0F) * PLUCK * expf(-beat * 3.0F);
    s->label_lum = 0.55F + (0.2F * sinf(t * 2.2F));
    s->label_half_w = label_usable(frame->label) ? LABEL_HALF * label_aspect(frame->label) : 0.0F;
    s->progress = isfinite(frame->progress) ? saturate(frame->progress) : 0.0F;
    return true;
}

static void column_init(const Scene *s, uint32_t x, Column *c)
{
    const float uvx = ((float)x + 0.5F) / (float)s->frame->width;
    c->px = (uvx - 0.5F) * s->aspect;
    c->vx2 = (uvx - 0.5F) * (uvx - 0.5F);
    const float u = (c->px + s->bar_half) / (2.0F * s->bar_half);
    c->in_bar = u >= 0.0F && u <= 1.0F;
    c->line_y = BAR_Y + (sinf(u * PI_F) * s->swing);
    c->filled = smooth(s->progress + 0.004F, s->progress - 0.004F, u);
    const float left = c->px + s->bar_half;
    const float right = c->px - s->bar_half;
    c->peg_dx2 = left * left < right * right ? left * left : right * right;
    const float bead = c->px - (-s->bar_half + (s->progress * 2.0F * s->bar_half));
    c->bead_dx2 = bead * bead;
}

/* couleur du pixel de la colonne `c` à la ligne `y` (ligne et colonne de l'ÉCRAN) */
static uint32_t shade(const Scene *s, const Column *c, uint32_t x, uint32_t y)
{
    const float uvy = ((float)y + 0.5F) / (float)s->frame->height;
    const float py = 0.5F - uvy;
    const float d = fabsf(py - c->line_y);
    const float bead_dy = py - c->line_y;
    const float bead_r2 = c->bead_dx2 + (bead_dy * bead_dy);
    const float peg_dy = py - BAR_Y;
    const float peg_r2 = c->peg_dx2 + (peg_dy * peg_dy);
    const float peg_reach = PEG_RADIUS + s->aa;
    /* la partie vide de la corde n'a pas de lueur : seule son épaisseur compte */
    const bool near_string = c->in_bar && d < (c->filled > 0.0F ? REACH : STRING_HALF + s->aa);
    const bool near_bead = bead_r2 < REACH * REACH;
    const bool near_peg = peg_r2 < peg_reach * peg_reach;
    const bool in_label = s->label_half_w > 0.0F && fabsf(c->px) <= s->label_half_w &&
                          fabsf(py - LABEL_Y) <= LABEL_HALF;
    if (s->lead <= 0.0F || (!near_string && !near_bead && !near_peg && !in_label)) {
        return 0U; /* rien n'éclaire ce pixel */
    }
    Rgb col = {0.0F, 0.0F, 0.0F};
    if (near_string) {
        /* la corde : sombre tant qu'elle est vide, rouge vif et lumineuse une fois remplie */
        const float core = smooth(STRING_HALF + s->aa, STRING_HALF - s->aa, d);
        const float empty = core * 0.9F * (1.0F - c->filled);
        col.r = mix(col.r, 0.2F * s->lead, empty);
        col.g = mix(col.g, 0.03F * s->lead, empty);
        col.b = mix(col.b, 0.05F * s->lead, empty);
        const float halo = c->filled > 0.0F ? expf(-d * GLOW_FALLOFF) * 0.5F : 0.0F;
        const float glow = (core + halo) * c->filled * s->lead;
        col.r += 1.0F * glow; /* THREAD = (1, 0,07, 0,1) */
        col.g += 0.07F * glow;
        col.b += 0.1F * glow;
    }
    if (near_peg) { /* les chevilles, aux deux bouts */
        const float peg = smooth(s->aa, -s->aa, sqrtf(peg_r2) - PEG_RADIUS);
        col.r = mix(col.r, 0.75F * s->lead, peg);
        col.g = mix(col.g, 0.55F * s->lead, peg);
        col.b = mix(col.b, 0.45F * s->lead, peg);
    }
    if (near_bead) { /* le nœud qui avance avec la progression */
        const float bead = expf(-sqrtf(bead_r2) * GLOW_FALLOFF) * 1.3F * s->lead;
        col.r += 1.0F * bead;
        col.g += 0.55F * bead;
        col.b += 0.45F * bead;
    }
    if (in_label) {
        const float alpha = label_alpha(s, x, y);
        col.r = mix(col.r, 0.95F * s->lead, alpha);
        col.g = mix(col.g, 0.8F * s->lead, alpha);
        col.b = mix(col.b, 0.72F * s->lead, alpha);
    }
    const float vy = uvy - 0.5F;
    const float vignette = 1.0F - ((c->vx2 + (vy * vy)) * 0.85F);
    return (tone(col.r * vignette) << 16U) | (tone(col.g * vignette) << 8U) |
           tone(col.b * vignette);
}

EngBootRect eng_boot_bounds(uint32_t width, uint32_t height, const EngBootText *label)
{
    EngBootRect out = {0U, 0U, 0U, 0U};
    if (width == 0U || height == 0U) {
        return out;
    }
    const float aspect = (float)width / (float)height;
    float half_w = bar_half_of(aspect) + REACH;
    float top = BAR_Y + PLUCK + REACH;
    float bottom = BAR_Y - PLUCK - REACH;
    if (label_usable(label)) {
        const float label_w = LABEL_HALF * label_aspect(label);
        half_w = label_w > half_w ? label_w : half_w;
        top = LABEL_Y + LABEL_HALF > top ? LABEL_Y + LABEL_HALF : top;
        bottom = LABEL_Y - LABEL_HALF < bottom ? LABEL_Y - LABEL_HALF : bottom;
    }
    /* du repère du shader aux pixels, élargi d'un pixel de chaque côté, puis borné à l'écran */
    const float x0 = (((-half_w / aspect) + 0.5F) * (float)width) - 1.0F;
    const float x1 = (((half_w / aspect) + 0.5F) * (float)width) + 1.0F;
    const float y0 = ((0.5F - top) * (float)height) - 1.0F;
    const float y1 = ((0.5F - bottom) * (float)height) + 1.0F;
    const float fw = (float)width;
    const float fh = (float)height;
    const uint32_t left = x0 <= 0.0F ? 0U : (x0 >= fw ? width : (uint32_t)x0);
    const uint32_t right = x1 <= 0.0F ? 0U : (x1 >= fw ? width : (uint32_t)ceilf(x1));
    const uint32_t upper = y0 <= 0.0F ? 0U : (y0 >= fh ? height : (uint32_t)y0);
    const uint32_t lower = y1 <= 0.0F ? 0U : (y1 >= fh ? height : (uint32_t)ceilf(y1));
    out.x = left;
    out.y = upper;
    out.width = right > left ? right - left : 0U;
    out.height = lower > upper ? lower - upper : 0U;
    return out;
}

void eng_boot_draw(const EngBootFrame *frame, uint32_t *pixels, size_t pitch)
{
    Scene scene;
    if (pixels == NULL || !scene_init(&scene, frame)) {
        return;
    }
    const EngBootRect zone = eng_boot_bounds(frame->width, frame->height, frame->label);
    const size_t stride = pitch / sizeof(uint32_t); /* pixels par ligne du tampon */
    if (pitch % sizeof(uint32_t) != 0U || stride < zone.width) {
        return;
    }
    /* colonne par colonne : ce qui ne dépend que de la colonne n'est calculé qu'une fois */
    for (uint32_t i = 0; i < zone.width; i++) {
        Column column;
        column_init(&scene, zone.x + i, &column);
        for (uint32_t j = 0; j < zone.height; j++) {
            pixels[((size_t)j * stride) + i] = shade(&scene, &column, zone.x + i, zone.y + j);
        }
    }
}

float eng_boot_progress(float progress, float dt)
{
    const float from =
        isfinite(progress) ? (progress < 0.0F ? 0.0F : (progress > 1.0F ? 1.0F : progress)) : 0.0F;
    if (!isfinite(dt) || dt <= 0.0F) {
        return from;
    }
    /* un à-coup ne fait pas sauter la barre ; elle s'approche de sa limite sans l'atteindre */
    const float step = dt > 0.1F ? 0.1F : dt;
    const float goal = ENG_BOOT_PROGRESS_END - 0.02F;
    const float next = from + ((goal - from) * (1.0F - expf(-step / 1.4F)));
    return next > from ? next : from;
}
