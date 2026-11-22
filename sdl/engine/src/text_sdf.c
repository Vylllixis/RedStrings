/* Texte en champ de distance : voir engine/text_sdf.h. */
#include "engine/text_sdf.h"

#include <math.h>
#include <string.h>

#include <stb_truetype.h>

#include "engine/log.h"

#define MAX_GLYPHS 128U

typedef struct Glyph {
    int glyph; /* indice dans la police */
    float pen; /* position horizontale de son origine (pixels) */
} Glyph;

/* point de code suivant d'une chaîne UTF-8 (0 à la fin ; U+FFFD pour une suite invalide) */
static uint32_t next_codepoint(const char **text)
{
    const unsigned char *p = (const unsigned char *)*text;
    if (p[0] == 0U) {
        return 0U;
    }
    uint32_t cp = 0xFFFDU;
    size_t len = 1;
    if (p[0] < 0x80U) {
        cp = p[0];
    } else if ((p[0] & 0xE0U) == 0xC0U && (p[1] & 0xC0U) == 0x80U) {
        cp = ((uint32_t)(p[0] & 0x1FU) << 6U) | (uint32_t)(p[1] & 0x3FU);
        len = 2;
    } else if ((p[0] & 0xF0U) == 0xE0U && (p[1] & 0xC0U) == 0x80U && (p[2] & 0xC0U) == 0x80U) {
        cp = ((uint32_t)(p[0] & 0x0FU) << 12U) | ((uint32_t)(p[1] & 0x3FU) << 6U) |
             (uint32_t)(p[2] & 0x3FU);
        len = 3;
    }
    *text += len;
    return cp;
}

static bool fail(EngTextSdf *out, const char *why)
{
    *out = (EngTextSdf){0};
    ENG_LOG_ERROR("texte en champ de distance : %s", why);
    return false;
}

bool eng_text_sdf_build(EngArena *arena, const uint8_t *font, size_t font_size, const char *text,
                        float pixel_height, float tracking, float spread, EngTextSdf *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngTextSdf){0};
    if (arena == NULL || font == NULL || font_size < 12U || text == NULL ||
        !(pixel_height >= 8.0F) || pixel_height > 512.0F || !(tracking >= 0.0F) ||
        !(spread >= 2.0F) || spread > 64.0F) {
        return fail(out, "paramètre invalide (police, texte, hauteur 8 à 512, champ 2 à 64)");
    }
    stbtt_fontinfo info;
    const int offset = stbtt_GetFontOffsetForIndex(font, 0);
    if (offset < 0 || stbtt_InitFont(&info, font, offset) == 0) {
        return fail(out, "police illisible");
    }
    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    /* mise en ligne : origine de chaque glyphe, crénage et espacement compris */
    Glyph glyphs[MAX_GLYPHS];
    uint32_t count = 0;
    float pen = 0.0F;
    int previous = 0;
    const char *cursor = text;
    for (uint32_t cp = next_codepoint(&cursor); cp != 0U && count < MAX_GLYPHS;
         cp = next_codepoint(&cursor)) {
        const int g = stbtt_FindGlyphIndex(&info, (int)cp);
        if (previous != 0) {
            pen += scale * (float)stbtt_GetGlyphKernAdvance(&info, previous, g);
        }
        int advance = 0;
        int bearing = 0;
        stbtt_GetGlyphHMetrics(&info, g, &advance, &bearing);
        glyphs[count++] = (Glyph){.glyph = g, .pen = pen};
        pen += (scale * (float)advance) + tracking;
        previous = g;
    }
    if (count == 0U) {
        return fail(out, "texte vide");
    }
    const float margin = spread + 1.0F;
    const float text_width = pen - tracking;
    const uint32_t width = (uint32_t)ceilf(text_width + (2.0F * margin));
    const uint32_t height = (uint32_t)ceilf((scale * (float)(ascent - descent)) + (2.0F * margin));
    if (width > ENG_TEXT_SDF_MAX_SIDE || height > ENG_TEXT_SDF_MAX_SIDE) {
        return fail(out, "image trop grande");
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    uint8_t *pixels = ENG_ARENA_NEW_ARRAY(arena, uint8_t, (size_t)width * height);
    if (pixels == NULL) {
        return fail(out, "arène trop petite");
    }
    memset(pixels, 0, (size_t)width * height);
    const float baseline = margin + (scale * (float)ascent);
    const int pad = (int)ceilf(spread);
    uint32_t drawn = 0;
    for (uint32_t i = 0; i < count; i++) {
        int w = 0;
        int h = 0;
        int xoff = 0;
        int yoff = 0;
        unsigned char *sdf =
            stbtt_GetGlyphSDF(&info, scale, glyphs[i].glyph, pad, (unsigned char)ENG_TEXT_SDF_EDGE,
                              (float)ENG_TEXT_SDF_EDGE / spread, &w, &h, &xoff, &yoff);
        if (sdf == NULL) {
            continue; /* espace : aucun contour */
        }
        drawn++;
        const int x0 = (int)lroundf(margin + glyphs[i].pen) + xoff;
        const int y0 = (int)lroundf(baseline) + yoff;
        for (int y = 0; y < h; y++) {
            const int py = y0 + y;
            if (py < 0 || py >= (int)height) {
                continue;
            }
            for (int x = 0; x < w; x++) {
                const int px = x0 + x;
                if (px < 0 || px >= (int)width) {
                    continue;
                }
                uint8_t *dst = &pixels[((size_t)py * width) + (size_t)px];
                const uint8_t v = sdf[((size_t)y * (size_t)w) + (size_t)x];
                *dst = v > *dst ? v : *dst; /* deux lettres voisines : la plus proche gagne */
            }
        }
        stbtt_FreeSDF(sdf, NULL);
    }
    if (drawn == 0U) {
        eng_arena_reset_to(arena, mark);
        return fail(out, "aucun glyphe visible");
    }
    out->pixels = pixels;
    out->width = width;
    out->height = height;
    out->spread = spread;
    out->baseline = baseline;
    return true;
}

float eng_text_sdf_sample(const EngTextSdf *sdf, float x, float y)
{
    if (sdf == NULL || sdf->pixels == NULL || sdf->width == 0U || sdf->height == 0U ||
        !isfinite(x) || !isfinite(y)) {
        return 0.0F;
    }
    const float fx = fminf(fmaxf(x - 0.5F, 0.0F), (float)(sdf->width - 1U));
    const float fy = fminf(fmaxf(y - 0.5F, 0.0F), (float)(sdf->height - 1U));
    const uint32_t x0 = (uint32_t)fx;
    const uint32_t y0 = (uint32_t)fy;
    const uint32_t x1 = x0 + 1U < sdf->width ? x0 + 1U : x0;
    const uint32_t y1 = y0 + 1U < sdf->height ? y0 + 1U : y0;
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;
    const float a = (float)sdf->pixels[((size_t)y0 * sdf->width) + x0];
    const float b = (float)sdf->pixels[((size_t)y0 * sdf->width) + x1];
    const float c = (float)sdf->pixels[((size_t)y1 * sdf->width) + x0];
    const float d = (float)sdf->pixels[((size_t)y1 * sdf->width) + x1];
    const float top = a + ((b - a) * tx);
    const float bottom = c + ((d - c) * tx);
    return (top + ((bottom - top) * ty)) / 255.0F;
}
