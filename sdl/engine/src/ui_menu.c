/* Dessin des menus : fond, barre du menu principal, listes (pause, options, contrôles), message.
 * Les cartes des parties sont dans ui_menu_cards.c. Voir ui_menu.h. */
#include "engine/ui_menu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/ui_game.h"

#ifdef OW_UI

#include "ui_backend.h"
#include "ui_menu_draw.h"

#define PI 3.14159265F

static uint32_t g_vertices;

/* ── outils ── */

float mm_clamp01(float x)
{
    return x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x);
}

float mm_smooth(float a, float b, float x)
{
    const float t = mm_clamp01((x - a) / (b - a));
    return t * t * (3.0F - (2.0F * t));
}

ImU32 mm_col(float r, float g, float b, float a)
{
    const ImVec4_c c = {r, g, b, mm_clamp01(a)};
    return igColorConvertFloat4ToU32(c);
}

ImU32 mm_ivory(float a)
{
    return mm_col(1.0F, 0.965F, 0.91F, a);
}

ImU32 mm_soft(float a)
{
    return mm_col(0.86F, 0.78F, 0.76F, a);
}

ImU32 mm_red(float a)
{
    return mm_col(0.96F, 0.13F, 0.17F, a);
}

ImU32 mm_mix(const float from[3], const float to[3], float k, float a)
{
    k = mm_clamp01(k);
    return mm_col(from[0] + ((to[0] - from[0]) * k), from[1] + ((to[1] - from[1]) * k),
                  from[2] + ((to[2] - from[2]) * k), a);
}

float mm_text_w(ImFont *font, float size, const char *s)
{
    return ImFont_CalcTextSizeA(font, size, 1e9F, 0.0F, s, NULL, NULL).x;
}

void mm_text(const MenuCtx *c, ImFont *font, float size, float x, float y, ImU32 col, const char *s,
             float shadow)
{
    const float alpha = (float)((col >> 24) & 0xFFU) / 255.0F;
    if (alpha < 0.004F || s[0] == '\0') {
        return;
    }
    if (shadow > 0.0F) {
        ImDrawList_AddText_FontPtr(
            c->dl, font, size, (ImVec2_c){x + (size * 0.05F), y + (size * 0.06F)},
            mm_col(0.0F, 0.0F, 0.0F, 0.8F * shadow * alpha), s, NULL, 0.0F, NULL);
    }
    ImDrawList_AddText_FontPtr(c->dl, font, size, (ImVec2_c){x, y}, col, s, NULL, 0.0F, NULL);
}

static const char *next_char(const char *p)
{
    p++;
    while (((unsigned char)*p & 0xC0U) == 0x80U) {
        p++;
    }
    return p;
}

float mm_tracked_w(ImFont *font, float size, const char *s, float track)
{
    float width = 0.0F;
    uint32_t count = 0;
    for (const char *p = s; *p != '\0';) {
        const char *end = next_char(p);
        width += ImFont_CalcTextSizeA(font, size, 1e9F, 0.0F, p, end, NULL).x;
        count++;
        p = end;
    }
    return count > 0U ? width + (track * (float)(count - 1U)) : 0.0F;
}

void mm_tracked(const MenuCtx *c, ImFont *font, float size, float x, float y, ImU32 col,
                const char *s, float track, float shadow)
{
    const float alpha = (float)((col >> 24) & 0xFFU) / 255.0F;
    if (alpha < 0.004F) {
        return;
    }
    const ImU32 dark = mm_col(0.0F, 0.0F, 0.0F, 0.8F * shadow * alpha);
    for (const char *p = s; *p != '\0';) {
        const char *end = next_char(p);
        if (shadow > 0.0F) {
            ImDrawList_AddText_FontPtr(c->dl, font, size,
                                       (ImVec2_c){x + (size * 0.05F), y + (size * 0.06F)}, dark, p,
                                       end, 0.0F, NULL);
        }
        ImDrawList_AddText_FontPtr(c->dl, font, size, (ImVec2_c){x, y}, col, p, end, 0.0F, NULL);
        x += ImFont_CalcTextSizeA(font, size, 1e9F, 0.0F, p, end, NULL).x + track;
        p = end;
    }
}

void mm_upper(const char *in, char *out, size_t capacity)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p != '\0' && n + 1U < capacity; p++) {
        unsigned char ch = *p;
        /* a -> A ; après l'octet 0xC3 : é -> É, à -> À, ç -> Ç... (même écart de 32) */
        const bool accented = n > 0U && (unsigned char)out[n - 1U] == 0xC3U && ch >= 0xA0U &&
                              ch <= 0xBEU && ch != 0xB7U;
        if ((ch >= 'a' && ch <= 'z') || accented) {
            ch = (unsigned char)(ch - 32U);
        }
        out[n++] = (char)ch;
    }
    out[n] = '\0';
}

float mm_fit(ImFont *font, float size, const char *s, float track, float width)
{
    const float w = mm_tracked_w(font, size, s, track);
    return w > width && w > 0.0F ? size * (width / w) : size;
}

int mm_mark(const MenuCtx *c)
{
    return c->dl->VtxBuffer.Size;
}

void mm_xform(const MenuCtx *c, int from, float pivot_x, float pivot_y, float scale, float angle,
              float dx, float dy, float alpha)
{
    const float cs = cosf(angle);
    const float sn = sinf(angle);
    ImDrawVert *v = c->dl->VtxBuffer.Data;
    for (int i = from; i < c->dl->VtxBuffer.Size; i++) {
        const float x = (v[i].pos.x - pivot_x) * scale;
        const float y = (v[i].pos.y - pivot_y) * scale;
        v[i].pos.x = pivot_x + (x * cs) - (y * sn) + dx;
        v[i].pos.y = pivot_y + (x * sn) + (y * cs) + dy;
        if (alpha < 0.999F) {
            const float a = (float)((v[i].col >> 24) & 0xFFU) * mm_clamp01(alpha);
            v[i].col = (v[i].col & 0x00FFFFFFU) | ((ImU32)(a + 0.5F) << 24);
        }
    }
}

void mm_thread(const MenuCtx *c, const ImVec2_c *points, int count, float width, float alpha)
{
    if (count < 2 || alpha < 0.004F) {
        return;
    }
    if (!c->hc) { /* le halo : deux traits larges et pâles sous le fil */
        ImDrawList_AddPolyline(c->dl, points, count, mm_red(0.09F * alpha), width * 6.5F, 0);
        ImDrawList_AddPolyline(c->dl, points, count, mm_red(0.2F * alpha), width * 3.0F, 0);
    }
    ImDrawList_AddPolyline(c->dl, points, count, mm_col(1.0F, 0.2F, 0.24F, alpha), width, 0);
    ImDrawList_AddPolyline(c->dl, points, count, mm_col(1.0F, 0.66F, 0.6F, 0.6F * alpha),
                           width * 0.34F, 0);
}

void mm_bead(const MenuCtx *c, float x, float y, float radius, float alpha)
{
    if (alpha < 0.004F) {
        return;
    }
    const ImVec2_c at = {x, y};
    if (!c->hc) {
        ImDrawList_AddCircleFilled(c->dl, at, radius * 3.2F, mm_red(0.1F * alpha), 28);
        ImDrawList_AddCircleFilled(c->dl, at, radius * 1.9F, mm_red(0.26F * alpha), 24);
    }
    ImDrawList_AddCircleFilled(c->dl, at, radius, mm_col(1.0F, 0.33F, 0.34F, alpha), 20);
    ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){x - (radius * 0.28F), y - (radius * 0.28F)},
                               radius * 0.42F, mm_col(1.0F, 0.86F, 0.8F, 0.9F * alpha), 12);
}

bool mm_mouse_in(const MenuCtx *c, float x0, float y0, float x1, float y1)
{
    return c->mouse.x >= x0 && c->mouse.x < x1 && c->mouse.y >= y0 && c->mouse.y < y1;
}

void mm_hint(const MenuCtx *c, float x, float y, const char *text, float alpha)
{
    const float size = c->px * 0.66F;
    mm_text(c, c->font, size, x - (mm_text_w(c->font, size, text) * 0.5F), y, mm_soft(0.7F * alpha),
            text, 1.0F);
}

float mm_rows(MenuCtx *c, const EngMenuItem *items, uint32_t count, int32_t selected,
              const MenuRows *rows)
{
    static const float soft[3] = {0.84F, 0.77F, 0.77F};
    static const float ivory[3] = {1.0F, 0.965F, 0.91F};
    static const float danger[3] = {1.0F, 0.42F, 0.4F};
    const EngMenuMotion *a = c->a;
    for (uint32_t i = 0; i < count; i++) {
        const EngMenuItem *it = &items[i];
        const float e = eng_stagger(rows->enter - 0.04F, i, 0.05F, 0.32F);
        const float ee = eng_ease_out_cubic(e);
        const bool sel = (int32_t)i == selected;
        const float k = rows->live ? a->emphasis[i] : (sel ? 1.0F : 0.0F);
        const float alpha = rows->presence * mm_clamp01(e * 1.6F) * (it->enabled ? 1.0F : 0.38F);
        const float size = rows->size * (1.0F + (0.06F * k));
        float x = rows->x + ((1.0F - ee) * 28.0F * c->u) + (k * 14.0F * c->u);
        const float y = rows->y + ((float)i * rows->row_h);
        if (sel && rows->live) { /* refus : la ligne tremble */
            x += sinf(a->clock * 55.0F) * a->deny * 7.0F * c->u;
        }
        const float *lit = (int32_t)i == rows->danger ? danger : ivory;
        float label_w = 0.0F;
        if (c->hc && k > 0.02F) { /* contraste élevé : la ligne choisie sur un pavé blanc */
            ImDrawList_AddRectFilled(c->dl,
                                     (ImVec2_c){rows->x - (10.0F * c->u), y - (size * 0.12F)},
                                     (ImVec2_c){rows->x + rows->width, y + (size * 1.36F)},
                                     mm_col(1.0F, 1.0F, 1.0F, 0.95F * k * alpha), 6.0F * c->u, 0);
            mm_text(c, c->bold, size, x, y, mm_col(0.0F, 0.0F, 0.0F, alpha), it->label, 0.0F);
            label_w = mm_text_w(c->bold, size, it->label);
        } else { /* le texte passe du maigre au gras avec la mise en avant */
            mm_text(c, c->font, size, x, y, mm_mix(soft, lit, k, alpha * (1.0F - k)), it->label,
                    1.0F);
            mm_text(c, c->bold, size, x, y, mm_mix(soft, lit, k, alpha * k), it->label, 1.0F);
            label_w = mm_text_w(k > 0.5F ? c->bold : c->font, size, it->label);
            if (k > 0.02F) { /* le fil se coud sous la ligne choisie */
                const ImVec2_c line[2] = {{x, y + (size * 1.32F)},
                                          {x + (label_w * mm_clamp01(k)), y + (size * 1.32F)}};
                mm_thread(c, line, 2, 2.0F * c->u, alpha * k);
            }
        }
        if (it->value[0] != '\0') {
            const float vs = rows->size * 0.7F;
            mm_text(c, c->font, vs, x + label_w + (16.0F * c->u), y + (size - vs) * 0.72F,
                    c->hc && k > 0.5F ? mm_col(0.0F, 0.0F, 0.0F, alpha) : mm_soft(0.85F * alpha),
                    it->value, c->hc && k > 0.5F ? 0.0F : 1.0F);
        }
        if (rows->live && it->enabled &&
            mm_mouse_in(c, rows->x - (26.0F * c->u), y - (rows->row_h * 0.16F),
                        rows->x + rows->width, y + (rows->row_h * 0.84F))) {
            c->hover = (int32_t)i;
        }
    }
    if (count > 0U && !c->hc) { /* la perle suit la ligne choisie */
        const float at = rows->live ? a->cursor.x : (float)selected;
        const float row =
            at < -0.4F ? -0.4F : (at > (float)count - 0.6F ? (float)count - 0.6F : at);
        mm_bead(c, rows->x - (16.0F * c->u), rows->y + (row * rows->row_h) + (rows->size * 0.66F),
                4.6F * c->u * (1.0F + (0.5F * (rows->live ? a->confirm : 0.0F))),
                rows->presence * mm_smooth(0.05F, 0.3F, rows->enter));
    }
    return rows->y + ((float)count * rows->row_h);
}

/* ── fond ── */

static bool title_context(const MenuCtx *c, EngMenuScreen screen)
{
    const EngMenuFamily family = eng_menu_family(screen);
    return family == ENG_MENU_FAMILY_RAIL || family == ENG_MENU_FAMILY_CARDS ||
           (screen != ENG_MENU_PAUSE && family == ENG_MENU_FAMILY_LIST &&
            c->m->options_return == ENG_MENU_MAIN);
}

static void draw_backdrop(const MenuCtx *c)
{
    const float open = c->a->open;
    const EngMenuScreen screen = c->m->screen != ENG_MENU_CLOSED ? c->m->screen : c->a->from;
    if (open < 0.004F) {
        return;
    }
    if (c->on_title) {
        /* la scène du titre s'assombrit elle-même ; ici seulement de quoi lire la barre du bas */
        if (c->hc) {
            ImDrawList_AddRectFilled(c->dl, (ImVec2_c){0, 0}, (ImVec2_c){c->w, c->h},
                                     mm_col(0.0F, 0.0F, 0.0F, 0.72F * open), 0.0F, 0);
        } else if (eng_menu_family(screen) == ENG_MENU_FAMILY_RAIL) {
            const ImU32 dark = mm_col(0.01F, 0.006F, 0.02F, 0.62F * open);
            const ImU32 clear = mm_col(0.01F, 0.006F, 0.02F, 0.0F);
            ImDrawList_AddRectFilledMultiColor(c->dl, (ImVec2_c){0, c->h * 0.8F},
                                               (ImVec2_c){c->w, c->h}, clear, clear, dark, dark);
        }
        return;
    }
    /* le jeu derrière : un voile ; la pause garde le jeu visible à droite */
    const bool pause = screen == ENG_MENU_PAUSE;
    const float veil = c->hc ? 0.9F : (pause ? 0.42F : 0.74F);
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){0, 0}, (ImVec2_c){c->w, c->h},
                             mm_col(0.015F, 0.012F, 0.03F, veil * open), 0.0F, 0);
    if (pause && !c->hc) {
        const ImU32 dark = mm_col(0.012F, 0.008F, 0.026F, 0.82F * open);
        const ImU32 clear = mm_col(0.012F, 0.008F, 0.026F, 0.0F);
        ImDrawList_AddRectFilledMultiColor(c->dl, (ImVec2_c){0, 0}, (ImVec2_c){c->w * 0.62F, c->h},
                                           dark, clear, clear, dark);
    }
    if (title_context(c, screen)) { /* pas de scène du titre : le titre du jeu est écrit ici */
        char title[] = "RED STRINGS";
        const float size = c->px * 2.7F;
        const float track = size * 0.2F;
        const float tw = mm_tracked_w(c->bold, size, title, track);
        const float x = (c->w - tw) * 0.5F;
        const float y = c->h * 0.07F;
        mm_tracked(c, c->bold, size, x, y, mm_ivory(open), title, track, 1.0F);
        const ImVec2_c line[2] = {{x, y + (size * 1.3F)}, {x + tw, y + (size * 1.3F)}};
        mm_thread(c, line, 2, 2.4F * c->u, open);
    }
}

/* ── barre du menu principal ── */

static float rail_wave(const MenuCtx *c, float t)
{
    const float pluck = c->a->pluck;
    const float bow = sinf(t * PI);
    return (bow * sinf(pluck * 44.0F) * 5.5F * c->u * expf(-pluck * 5.0F)) +
           (bow * sinf((t * 7.0F) + (c->a->clock * 1.3F)) * 0.7F * c->u);
}

static void draw_rail(MenuCtx *c, const EngMenu *m, int32_t selected, float enter, float presence,
                      bool live)
{
    static const float soft[3] = {0.78F, 0.71F, 0.72F};
    static const float ivory[3] = {1.0F, 0.97F, 0.92F};
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    if (n == 0U) {
        return;
    }
    const EngMenuMotion *a = c->a;
    const float aspect = c->w / c->h;
    /* la corde de l'écran de chargement s'allonge pour devenir la barre du menu */
    const float half_loading = fminf(0.36F, aspect * 0.32F) * c->h;
    const float half_menu = fminf(0.62F, aspect * 0.42F) * c->h;
    const float grow = c->on_title && live ? eng_ease_in_out_cubic(a->title_menu) : 1.0F;
    const float half = half_loading + ((half_menu - half_loading) * grow);
    const float hint_size = c->px * 0.66F;
    const float sub_size = c->px * 0.92F;
    /* elle monte du bas de l'écran, sauf à la sortie de l'écran de chargement : elle est alors
     * exactement à la place de la corde de chargement */
    const float rail_y = c->h - (10.0F * c->u) - (hint_size * 1.3F) - (sub_size * 1.35F) -
                         (c->h * 0.012F) + ((1.0F - presence) * c->h * 0.05F * grow);
    const float cx = c->w * 0.5F;
    const float cell = (2.0F * half_menu) / (float)n;
    /* le fil */
    enum { RAIL_POINTS = 72 };
    ImVec2_c points[RAIL_POINTS];
    for (int i = 0; i < RAIL_POINTS; i++) {
        const float t = (float)i / (float)(RAIL_POINTS - 1);
        points[i] = (ImVec2_c){cx - half + (2.0F * half * t), rail_y + rail_wave(c, t)};
    }
    mm_thread(c, points, RAIL_POINTS, 2.4F * c->u, presence);
    for (int end = 0; end < 2; end++) { /* les chevilles */
        ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){cx + (end == 0 ? -half : half), rail_y},
                                   5.0F * c->u, mm_col(0.78F, 0.58F, 0.48F, presence), 16);
    }
    /* les lignes, posées sur le fil */
    char label[ENG_MENU_LABEL];
    float base = c->px * 1.42F;
    for (uint32_t i = 0; i < n;
         i++) { /* toutes à la même taille : celle qui fait tenir la plus longue */
        mm_upper(items[i].label, label, sizeof label);
        base = mm_fit(c->bold, base * 1.14F, label, base * 1.14F * 0.16F, cell * 0.94F) / 1.14F;
    }
    for (uint32_t i = 0; i < n; i++) {
        const float e = eng_stagger(enter - 0.16F, i, 0.07F, 0.42F);
        const bool sel = (int32_t)i == selected;
        const float k = live ? a->emphasis[i] : (sel ? 1.0F : 0.0F);
        const float pop = sel && live ? a->confirm : 0.0F;
        const float size = base * (1.0F + (0.14F * k) + (0.07F * pop));
        const float track = size * 0.16F;
        mm_upper(items[i].label, label, sizeof label);
        const float lw = mm_tracked_w(c->bold, size, label, track);
        float x = cx - half_menu + (((float)i + 0.5F) * cell) - (lw * 0.5F);
        const float y = rail_y - (c->h * 0.024F) - size +
                        ((1.0F - eng_ease_out_back(e)) * 20.0F * c->u) - (k * 3.0F * c->u);
        if (sel && live) {
            x += sinf(a->clock * 55.0F) * a->deny * 7.0F * c->u;
        }
        const float alpha = presence * mm_clamp01(e * 1.7F) * (items[i].enabled ? 1.0F : 0.36F);
        if (c->hc && k > 0.5F) {
            ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x - (12.0F * c->u), y - (4.0F * c->u)},
                                     (ImVec2_c){x + lw + (12.0F * c->u), y + (size * 1.3F)},
                                     mm_col(1.0F, 1.0F, 1.0F, 0.95F * alpha), 6.0F * c->u, 0);
            mm_tracked(c, c->bold, size, x, y, mm_col(0.0F, 0.0F, 0.0F, alpha), label, track, 0.0F);
        } else {
            mm_tracked(c, c->bold, size, x, y, mm_mix(soft, ivory, k + pop, alpha), label, track,
                       1.0F);
        }
        const float left = cx - half_menu + ((float)i * cell);
        if (live && items[i].enabled &&
            mm_mouse_in(c, left, rail_y - (c->h * 0.1F), left + cell, rail_y + (c->h * 0.045F))) {
            c->hover = (int32_t)i;
        }
    }
    /* la perle : elle part du bout de la corde de chargement et glisse jusqu'à la ligne choisie */
    const float at = live ? a->cursor.x : (float)selected;
    const float slot = at < -0.3F ? -0.3F : (at > (float)n - 0.7F ? (float)n - 0.7F : at);
    float bead_x = cx - half_menu + ((slot + 0.5F) * cell);
    bead_x = (cx + half_loading) + ((bead_x - (cx + half_loading)) * eng_ease_out_cubic(grow));
    const float bead_t = mm_clamp01((bead_x - (cx - half)) / (2.0F * half));
    mm_bead(c, bead_x, rail_y + rail_wave(c, bead_t),
            5.6F * c->u * (1.0F + (0.5F * (live ? a->confirm : 0.0F))), presence);
    /* sous le fil : ce que fait la ligne choisie */
    if (selected >= 0 && (uint32_t)selected < n && items[selected].value[0] != '\0') {
        const char *sub = items[selected].value;
        const float sw = mm_text_w(c->font, sub_size, sub);
        float sx = bead_x - (sw * 0.5F);
        sx = sx < 16.0F * c->u ? 16.0F * c->u : sx;
        sx = sx + sw > c->w - (16.0F * c->u) ? c->w - (16.0F * c->u) - sw : sx;
        const float alpha = presence * mm_smooth(0.45F, 0.85F, enter) *
                            (live ? mm_smooth(0.03F, 0.22F, a->since_select) : 1.0F);
        mm_text(c, c->font, sub_size, sx, rail_y + (c->h * 0.012F),
                mm_col(0.97F, 0.84F, 0.78F, alpha), sub, 1.0F);
    }
    mm_hint(c, cx, c->h - (10.0F * c->u) - (hint_size * 1.1F),
            "flèches : choisir   ·   Entrée : valider", presence * mm_smooth(0.6F, 1.1F, enter));
}

/* ── listes : pause, options, contrôles ── */

static void draw_pause(MenuCtx *c, const EngMenu *m, int32_t selected, float enter, float presence,
                       bool live)
{
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    const float x = c->w * 0.085F;
    const float size = c->px * 2.5F;
    const float track = size * 0.22F;
    const float y = c->h * 0.2F;
    const float in = eng_ease_out_cubic(enter / 0.3F);
    const float tx = x - ((1.0F - in) * 36.0F * c->u);
    mm_tracked(c, c->bold, size, tx, y, mm_ivory(presence), "PAUSE", track, 1.0F);
    const float tw = mm_tracked_w(c->bold, size, "PAUSE", track);
    const ImVec2_c line[2] = {
        {tx, y + (size * 1.28F)},
        {tx + (tw * eng_ease_out_cubic((enter - 0.08F) / 0.4F)), y + (size * 1.28F)}};
    mm_thread(c, line, 2, 2.6F * c->u, presence);
    const MenuRows rows = {.x = x + (20.0F * c->u),
                           .y = y + (size * 1.28F) + (c->px * 1.6F),
                           .size = c->px * 1.3F,
                           .row_h = c->px * 1.3F * 1.85F,
                           .width = c->w * 0.38F,
                           .enter = enter - 0.1F,
                           .presence = presence,
                           .live = live,
                           .danger = -1};
    (void)mm_rows(c, items, n, selected, &rows);
    mm_hint(c, x + (20.0F * c->u) + (c->w * 0.1F), c->h - (c->px * 2.0F),
            "flèches : choisir   ·   Entrée : valider   ·   Échap : reprendre", presence);
}

/* la valeur d'une ligne d'options, à droite : curseur, bascule, choix ou texte */
static void draw_value(const MenuCtx *c, const EngMenuItem *it, float zone_x, float right,
                       float mid, float k, float alpha, bool pulse)
{
    const float size = c->px * 0.9F;
    const float bump = pulse ? c->a->adjust : 0.0F;
    const ImU32 ink = c->hc && k > 0.5F ? mm_col(0.0F, 0.0F, 0.0F, alpha)
                                        : mm_col(1.0F, 0.9F, 0.84F, alpha * (0.72F + (0.28F * k)));
    const float value_w = mm_text_w(c->font, size, it->value);
    const float text_y = mid - (size * 0.62F);
    const float column = 66.0F * c->u; /* la place du texte de la valeur */
    if (it->kind == ENG_MENU_ITEM_SLIDER) {
        const float x0 = zone_x;
        const float x1 = right - column - (12.0F * c->u);
        const float at = x0 + ((x1 - x0) * it->frac);
        ImDrawList_AddLine(c->dl, (ImVec2_c){x0, mid}, (ImVec2_c){x1, mid},
                           mm_col(1.0F, 0.9F, 0.86F, 0.24F * alpha), 2.0F * c->u, 0);
        const ImVec2_c filled[2] = {{x0, mid}, {at, mid}};
        mm_thread(c, filled, 2, 2.2F * c->u, alpha * (0.55F + (0.45F * k)));
        mm_bead(c, at, mid, 4.4F * c->u * (1.0F + (0.5F * bump)), alpha);
        mm_text(c, c->font, size, right - value_w, text_y, ink, it->value, 0.0F);
    } else if (it->kind == ENG_MENU_ITEM_TOGGLE) {
        const float pw = 36.0F * c->u;
        const float ph = 18.0F * c->u;
        const float x0 = right - column - (12.0F * c->u) - pw;
        const float on = it->frac;
        ImDrawList_AddRectFilled(
            c->dl, (ImVec2_c){x0, mid - (ph * 0.5F)}, (ImVec2_c){x0 + pw, mid + (ph * 0.5F)},
            mm_col(0.96F, 0.13F, 0.17F, alpha * (0.12F + (0.7F * on))), ph * 0.5F, 0);
        ImDrawList_AddRect(
            c->dl, (ImVec2_c){x0, mid - (ph * 0.5F)}, (ImVec2_c){x0 + pw, mid + (ph * 0.5F)},
            mm_col(1.0F, 0.9F, 0.86F, alpha * (0.5F - (0.25F * on))), ph * 0.5F, 1.5F * c->u, 0);
        ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){x0 + (ph * 0.5F) + ((pw - ph) * on), mid},
                                   (ph * 0.36F) * (1.0F + (0.35F * bump)), mm_ivory(alpha), 16);
        mm_text(c, c->font, size, right - value_w, text_y, ink, it->value, 0.0F);
    } else if (it->kind == ENG_MENU_ITEM_CHOICE) {
        const float cx = (zone_x + right) * 0.5F;
        const float span = (right - zone_x) * 0.5F;
        const float push = bump * 5.0F * c->u * (float)c->a->adjust_dir;
        const float tri = 5.0F * c->u;
        const ImU32 arrow = mm_col(1.0F, 0.4F, 0.4F, alpha * (0.35F + (0.65F * k)));
        const float lx = cx - span + (push < 0.0F ? push : 0.0F);
        const float rx = cx + span + (push > 0.0F ? push : 0.0F);
        ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){lx, mid}, (ImVec2_c){lx + tri, mid - tri},
                                     (ImVec2_c){lx + tri, mid + tri}, arrow);
        ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){rx, mid}, (ImVec2_c){rx - tri, mid - tri},
                                     (ImVec2_c){rx - tri, mid + tri}, arrow);
        mm_text(c, c->font, size, cx - (value_w * 0.5F), text_y, ink, it->value, 0.0F);
    } else if (it->value[0] != '\0') {
        const float fitted =
            mm_fit(c->font, size, it->value, 0.0F, right - zone_x + (c->w * 0.08F));
        mm_text(c, c->font, fitted, right - mm_text_w(c->font, fitted, it->value),
                mid - (fitted * 0.62F), ink, it->value, 0.0F);
    } else if (strstr(it->label, "...") != NULL) { /* ouvre un autre écran */
        const float tri = 5.0F * c->u;
        ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){right, mid},
                                     (ImVec2_c){right - tri, mid - tri},
                                     (ImVec2_c){right - tri, mid + tri},
                                     mm_col(1.0F, 0.4F, 0.4F, alpha * (0.35F + (0.65F * k))));
    }
}

static void draw_options(MenuCtx *c, const EngMenu *m, int32_t selected, float enter,
                         float presence, bool live)
{
    static const float soft[3] = {0.84F, 0.78F, 0.78F};
    static const float ivory[3] = {1.0F, 0.965F, 0.91F};
    static const float black[3] = {0.0F, 0.0F, 0.0F};
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    EngMenuMotion *a = c->a;
    const float in = eng_ease_out_cubic(enter / 0.28F);
    const float pw = fminf(c->w * 0.94F, c->h * 1.06F);
    const float x0 = (c->w - pw) * 0.5F;
    const float x1 = x0 + pw;
    const float top = (c->h * 0.2F) + ((1.0F - in) * 18.0F * c->u);
    const float bottom = c->h * 0.92F;
    const float pad = 26.0F * c->u;
    const ImU32 glass = mm_col(0.035F, 0.026F, 0.06F, (c->hc ? 0.97F : 0.78F) * presence);
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x0, top}, (ImVec2_c){x1, bottom}, glass,
                             16.0F * c->u, 0);
    ImDrawList_AddRect(c->dl, (ImVec2_c){x0, top}, (ImVec2_c){x1, bottom},
                       mm_col(1.0F, 0.9F, 0.86F, 0.16F * presence), 16.0F * c->u, 1.5F * c->u, 0);
    /* le titre de l'écran et son fil */
    char title[ENG_MENU_LABEL];
    mm_upper(eng_menu_title(m), title, sizeof title);
    const float ts = c->px * 0.95F;
    const float track = ts * 0.24F;
    mm_tracked(c, c->bold, ts, x0 + pad, top + (pad * 0.8F), mm_col(1.0F, 0.82F, 0.78F, presence),
               title, track, 0.0F);
    const float tw = mm_tracked_w(c->bold, ts, title, track);
    const float rule_y = top + (pad * 0.8F) + (ts * 1.5F);
    const ImVec2_c rule[2] = {
        {x0 + pad, rule_y},
        {x0 + pad + ((tw + (40.0F * c->u)) * eng_ease_out_cubic((enter - 0.05F) / 0.45F)), rule_y}};
    mm_thread(c, rule, 2, 2.0F * c->u, presence);
    /* les lignes, dans une zone qui défile */
    const float area_top = rule_y + (c->px * 0.7F);
    const float area_bottom = bottom - (c->px * 2.0F);
    const float row_h = c->px * 1.58F;
    uint32_t visible = (uint32_t)floorf((area_bottom - area_top) / row_h);
    visible = visible < 1U ? 1U : visible;
    if (live) {
        a->list_count = n;
        a->list_visible = visible;
    }
    const float first = live ? a->scroll.x : 0.0F;
    const float zone_x = x1 - (pw * 0.4F);
    const float right = x1 - pad;
    ImDrawList_PushClipRect(c->dl, (ImVec2_c){x0, area_top},
                            (ImVec2_c){x1, area_top + ((float)visible * row_h)}, true);
    for (uint32_t i = 0; i < n; i++) {
        const float y = area_top + (((float)i - first) * row_h);
        if (y < area_top - row_h || y > area_bottom + row_h) {
            continue;
        }
        const EngMenuItem *it = &items[i];
        const bool sel = (int32_t)i == selected;
        const float e = eng_stagger(enter - 0.06F, i > 14U ? 14U : i, 0.022F, 0.3F);
        const float k = live ? a->emphasis[i] : (sel ? 1.0F : 0.0F);
        const float alpha = presence * mm_clamp01(e * 1.6F) * (it->enabled ? 1.0F : 0.4F);
        const float mid = y + (row_h * 0.5F);
        float lx = x0 + pad + (8.0F * c->u * k) + ((1.0F - eng_ease_out_cubic(e)) * 24.0F * c->u);
        if (sel && live) {
            lx += sinf(a->clock * 55.0F) * a->deny * 6.0F * c->u;
        }
        if (k > 0.01F) { /* la bande de la ligne choisie */
            ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x0 + (pad * 0.4F), y + c->u},
                                     (ImVec2_c){x1 - (pad * 0.4F), y + row_h - c->u},
                                     c->hc ? mm_col(1.0F, 1.0F, 1.0F, 0.95F * k * alpha)
                                           : mm_red(0.17F * k * alpha),
                                     7.0F * c->u, 0);
            if (!c->hc) {
                ImDrawList_AddRectFilled(
                    c->dl, (ImVec2_c){x0 + (pad * 0.4F), y + (row_h * 0.2F)},
                    (ImVec2_c){x0 + (pad * 0.4F) + (3.0F * c->u), y + (row_h * 0.8F)},
                    mm_red(k * alpha), 1.5F * c->u, 0);
            }
        }
        mm_text(c, c->font, c->px, lx, mid - (c->px * 0.62F),
                mm_mix(soft, c->hc ? black : ivory, k, alpha), it->label, 0.0F);
        const bool waiting = m->screen == ENG_MENU_CONTROLS && m->remap == (int32_t)i;
        const float value_alpha =
            waiting ? alpha * (0.55F + (0.45F * sinf(a->clock * 6.0F))) : alpha;
        draw_value(c, it, zone_x, right, mid, k, value_alpha, sel && live);
        if (live && it->enabled && mm_mouse_in(c, x0, y, x1, y + row_h) && c->mouse.y >= area_top &&
            c->mouse.y < area_top + ((float)visible * row_h)) {
            c->hover = (int32_t)i;
            c->hover_dir = it->kind == ENG_MENU_ITEM_PLAIN
                               ? 0
                               : (c->mouse.x < (zone_x + right - (66.0F * c->u)) * 0.5F ? -1 : 1);
        }
    }
    ImDrawList_PopClipRect(c->dl);
    if (n > visible) { /* ce qui dépasse : dégradés en haut et en bas, et un repère à droite */
        const ImU32 solid = mm_col(0.035F, 0.026F, 0.06F, 0.95F * presence);
        const ImU32 clear = mm_col(0.035F, 0.026F, 0.06F, 0.0F);
        const float max_first = (float)(n - visible);
        const float list_bottom = area_top + ((float)visible * row_h);
        if (first > 0.02F) {
            ImDrawList_AddRectFilledMultiColor(
                c->dl, (ImVec2_c){x0 + pad * 0.3F, area_top},
                (ImVec2_c){x1 - pad * 0.3F, area_top + (row_h * 0.7F)}, solid, solid, clear, clear);
        }
        if (first < max_first - 0.02F) {
            ImDrawList_AddRectFilledMultiColor(
                c->dl, (ImVec2_c){x0 + pad * 0.3F, list_bottom - (row_h * 0.7F)},
                (ImVec2_c){x1 - pad * 0.3F, list_bottom}, clear, clear, solid, solid);
        }
        const float track_h = list_bottom - area_top;
        const float thumb_h = track_h * ((float)visible / (float)n);
        const float thumb_y = area_top + ((track_h - thumb_h) * mm_clamp01(first / max_first));
        const float bar_x = x1 - (pad * 0.3F);
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){bar_x, area_top},
                                 (ImVec2_c){bar_x + (3.0F * c->u), list_bottom},
                                 mm_col(1.0F, 0.9F, 0.86F, 0.1F * presence), 1.5F * c->u, 0);
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){bar_x, thumb_y},
                                 (ImVec2_c){bar_x + (3.0F * c->u), thumb_y + thumb_h},
                                 mm_red(0.85F * presence), 1.5F * c->u, 0);
    }
    mm_hint(c, c->w * 0.5F, bottom - (c->px * 1.45F),
            m->screen == ENG_MENU_OPTIONS
                ? "flèches : choisir et régler   ·   Entrée : valider   ·   Échap : retour"
                : "Entrée : changer la touche   ·   Échap : retour",
            presence);
}

static void draw_screen(MenuCtx *c, const EngMenu *m, float enter, float presence, bool live)
{
    if (presence < 0.004F) {
        return;
    }
    switch (m->screen) {
    case ENG_MENU_MAIN:
        draw_rail(c, m, m->selected, enter, presence, live);
        break;
    case ENG_MENU_PAUSE:
        draw_pause(c, m, m->selected, enter, presence, live);
        break;
    case ENG_MENU_OPTIONS:
    case ENG_MENU_CONTROLS:
        draw_options(c, m, m->selected, enter, presence, live);
        break;
    case ENG_MENU_SAVES:
    case ENG_MENU_SLOT:
    case ENG_MENU_NAME:
    case ENG_MENU_DELETE:
        mm_draw_cards_screen(c, m, enter, presence, live);
        break;
    default:
        break;
    }
}

/* le message bref (« Partie 1 sauvegardée ») : une pastille qui descend du haut de l'écran */
static void draw_toast(const MenuCtx *c)
{
    const float k = c->a->status;
    const char *text = c->a->status_text;
    if (k < 0.01F || text[0] == '\0') {
        return;
    }
    const float size = c->px * 0.86F;
    const float tw = mm_text_w(c->font, size, text);
    const float w = tw + (58.0F * c->u);
    const float h = size * 1.9F;
    const float x = (c->w - w) * 0.5F;
    const float y = (-h) + ((h + (c->h * 0.022F)) * eng_ease_out_back(k));
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x, y}, (ImVec2_c){x + w, y + h},
                             mm_col(0.045F, 0.03F, 0.07F, 0.92F * k), h * 0.5F, 0);
    ImDrawList_AddRect(c->dl, (ImVec2_c){x, y}, (ImVec2_c){x + w, y + h}, mm_red(0.7F * k),
                       h * 0.5F, 1.5F * c->u, 0);
    mm_bead(c, x + (22.0F * c->u), y + (h * 0.5F), 4.4F * c->u, k);
    mm_text(c, c->font, size, x + (40.0F * c->u), y + ((h - (size * 1.24F)) * 0.5F), mm_ivory(k),
            text, 0.0F);
}

static bool cards_alive(const EngMenuMotion *a)
{
    if (a->thread > 0.001F || a->ghost_time >= 0.0F) {
        return true;
    }
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        if (a->cards[i].alpha.x > 0.01F) {
            return true;
        }
    }
    return false;
}

EngMenuHover eng_ui_draw_menu(EngUi *ui, const EngMenu *menu, EngMenuMotion *motion,
                              const EngMenuView *view)
{
    const EngMenuHover none = {-1, 0};
    g_vertices = 0;
    if (ui == NULL || !ui->frame_open || menu == NULL || motion == NULL || view == NULL ||
        view->width <= 0 || view->height <= 0) {
        return none;
    }
    const bool leaving = motion->from != ENG_MENU_CLOSED && motion->from != menu->screen &&
                         motion->leave < ENG_MOTION_OUT_TIME;
    if (!eng_menu_is_open(menu) && !leaving && motion->open < 0.01F && !cards_alive(motion) &&
        motion->status < 0.01F) {
        return none;
    }
    MenuCtx c = {.dl = igGetForegroundDrawList_Nil(),
                 .font = igGetFont(),
                 .bold = (ImFont *)ow_ui_bold_font(),
                 .m = menu,
                 .a = motion,
                 .w = (float)view->width,
                 .h = (float)view->height,
                 .px = eng_ui_text_px(view->height, view->text_scale),
                 .hc = view->high_contrast,
                 .on_title = view->on_title,
                 .mouse = igGetIO_Nil()->MousePos,
                 .hover = -1,
                 .hover_dir = 0};
    c.bold = c.bold != NULL ? c.bold : c.font;
    c.u = c.px / 24.0F;
    const int first_vertex = c.dl->VtxBuffer.Size;
    draw_backdrop(&c);
    if (cards_alive(motion)) {
        mm_draw_cards(&c);
    }
    if (leaving) { /* l'écran quitté finit de sortir */
        EngMenu old = *menu;
        old.screen = motion->from;
        old.selected = motion->from_selected;
        old.remap = -1;
        draw_screen(&c, &old, 10.0F, 1.0F - (motion->leave / ENG_MOTION_OUT_TIME), false);
    }
    if (eng_menu_is_open(menu)) { /* l'écran courant entre quand l'autre a presque fini de sortir */
        const float wait = motion->from != ENG_MENU_CLOSED ? ENG_MOTION_OUT_TIME * 0.6F : 0.0F;
        const float enter = motion->enter - wait;
        draw_screen(&c, menu, enter, enter > 0.0F ? eng_ease_out_cubic(enter / 0.22F) : 0.0F, true);
    }
    draw_toast(&c);
    g_vertices = (uint32_t)(c.dl->VtxBuffer.Size - first_vertex);
    return (EngMenuHover){c.hover, c.hover_dir};
}

uint32_t eng_ui_menu_vertices(void)
{
    return g_vertices;
}

#else

EngMenuHover eng_ui_draw_menu(EngUi *ui, const EngMenu *menu, EngMenuMotion *motion,
                              const EngMenuView *view)
{
    (void)ui;
    (void)menu;
    (void)motion;
    (void)view;
    return (EngMenuHover){-1, 0};
}

uint32_t eng_ui_menu_vertices(void)
{
    return 0;
}

#endif
