#include "engine/ui_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

float eng_ui_text_px(int height, float text_scale)
{
    const float h = height > 0 ? (float)height : 1080.0F;
    const float s = isfinite(text_scale) && text_scale > 0.0F ? text_scale : 1.0F;
    const float px = 24.0F * (h / 1080.0F) * s;
    return px < 12.0F ? 12.0F : px;
}

#ifdef OW_UI

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

static ImU32 rgba(float r, float g, float b, float a)
{
    const ImVec4_c c = {r, g, b, a};
    return igColorConvertFloat4ToU32(c);
}

static void text(ImDrawList *dl, float px, float x, float y, ImU32 col, const char *s, bool shadow)
{
    ImFont *font = igGetFont();
    if (shadow) {
        ImDrawList_AddText_FontPtr(dl, font, px, (ImVec2_c){x + px * 0.06F, y + px * 0.06F},
                                   rgba(0, 0, 0, 0.85F), s, NULL, 0.0F, NULL);
    }
    ImDrawList_AddText_FontPtr(dl, font, px, (ImVec2_c){x, y}, col, s, NULL, 0.0F, NULL);
}

static float text_width(float px, const char *s)
{
    const ImVec2_c size = ImFont_CalcTextSizeA(igGetFont(), px, 1e9F, 0.0F, s, NULL, NULL);
    return size.x;
}

/* Fond sombre arrondi derrière un texte (B4 : l'heure et l'aide étaient illisibles sur un ciel
 * clair, même avec l'ombre portée) ; plus opaque en contraste élevé. */
static void plate(ImDrawList *dl, float px, float x, float y, const char *s, bool contrast)
{
    const float pad = px * 0.35F;
    const float w = text_width(px, s);
    ImDrawList_AddRectFilled(dl, (ImVec2_c){x - pad, y - (pad * 0.4F)},
                             (ImVec2_c){x + w + pad, y + (px * 1.15F) + (pad * 0.4F)},
                             rgba(0, 0, 0, contrast ? 0.85F : 0.45F), pad, 0);
}

static void bar(ImDrawList *dl, float x, float y, float w, float h, float frac, ImU32 fill,
                bool contrast)
{
    frac = !isfinite(frac) ? 0.0F : (frac < 0.0F ? 0.0F : (frac > 1.0F ? 1.0F : frac));
    ImDrawList_AddRectFilled(dl, (ImVec2_c){x, y}, (ImVec2_c){x + w, y + h},
                             rgba(0, 0, 0, contrast ? 0.95F : 0.55F), h * 0.3F, 0);
    if (frac > 0.0F) {
        ImDrawList_AddRectFilled(dl, (ImVec2_c){x + 2.0F, y + 2.0F},
                                 (ImVec2_c){x + 2.0F + (w - 4.0F) * frac, y + h - 2.0F}, fill,
                                 h * 0.25F, 0);
    }
    if (contrast) {
        ImDrawList_AddRect(dl, (ImVec2_c){x, y}, (ImVec2_c){x + w, y + h}, rgba(1, 1, 1, 1),
                           h * 0.3F, 2.0F, 0); /* arrondi, épaisseur, drapeaux */
    }
}

void eng_ui_draw_hud(EngUi *ui, const EngHud *hud, int width, int height)
{
    if (ui == NULL || !ui->frame_open || hud == NULL || width <= 0 || height <= 0) {
        return;
    }
    ImDrawList *dl = igGetBackgroundDrawList_Nil();
    const float px = eng_ui_text_px(height, hud->text_scale);
    const float u = px / 24.0F; /* unité d'interface */
    const bool hc = hud->high_contrast;
    /* joueur : en haut à gauche */
    const float x0 = 32.0F * u;
    float y = 32.0F * u;
    bar(dl, x0, y, 360.0F * u, 22.0F * u, hud->hp / hud->hp_max,
        hc ? rgba(1, 0.2F, 0.2F, 1) : rgba(0.8F, 0.15F, 0.15F, 0.95F), hc);
    char buf[64];
    (void)snprintf(buf, sizeof buf, "%.0f / %.0f", (double)hud->hp, (double)hud->hp_max);
    text(dl, px * 0.75F, x0 + 8.0F * u, y + 1.0F * u, rgba(1, 1, 1, 1), buf, true);
    y += 28.0F * u;
    bar(dl, x0, y, 300.0F * u, 14.0F * u, hud->stamina / hud->stamina_max,
        hc ? rgba(0.2F, 1, 0.2F, 1) : rgba(0.25F, 0.75F, 0.3F, 0.95F), hc);
    y += 20.0F * u;
    bar(dl, x0, y, 300.0F * u, 10.0F * u, hud->posture / hud->posture_max,
        hc ? rgba(1, 1, 0, 1) : rgba(0.9F, 0.75F, 0.2F, 0.9F), hc);
    if (hud->level > 0U) { /* niveau et expérience (M8-08) */
        y += 16.0F * u;
        (void)snprintf(buf, sizeof buf, "Niv. %u", (unsigned)hud->level);
        plate(dl, px * 0.75F, x0, y, buf, hc);
        text(dl, px * 0.75F, x0, y, rgba(1, 1, 1, 1), buf, true);
        bar(dl, x0 + text_width(px * 0.75F, buf) + 10.0F * u, y + 5.0F * u, 200.0F * u, 8.0F * u,
            hud->xp_frac, hc ? rgba(0.4F, 0.8F, 1, 1) : rgba(0.35F, 0.6F, 0.95F, 0.9F), hc);
    }
    if (hud->weapon[0] != '\0') { /* arme en main (M8-08) */
        y += 22.0F * u;
        plate(dl, px * 0.7F, x0, y, hud->weapon, hc);
        text(dl, px * 0.7F, x0, y, rgba(1, 1, 1, 0.95F), hud->weapon, true);
    }
    /* cible : en haut au centre */
    if (hud->has_target) {
        const float tw = 520.0F * u;
        const float tx = ((float)width - tw) * 0.5F;
        const float ty = 28.0F * u;
        text(dl, px, ((float)width - text_width(px, hud->target_name)) * 0.5F, ty, rgba(1, 1, 1, 1),
             hud->target_name, true);
        bar(dl, tx, ty + px * 1.15F, tw, 16.0F * u, hud->target_hp / hud->target_hp_max,
            rgba(0.85F, 0.2F, 0.2F, 0.95F), hc);
        bar(dl, tx + tw * 0.2F, ty + px * 1.15F + 20.0F * u, tw * 0.6F, 8.0F * u,
            hud->target_posture / hud->target_posture_max, rgba(0.95F, 0.8F, 0.2F, 0.9F), hc);
    }
    /* M6-09 : boss : nom et longue barre en bas au centre */
    if (hud->has_boss && hud->boss_hp_max > 0.0F) {
        const float bw = 760.0F * u;
        const float bx = ((float)width - bw) * 0.5F;
        const float by = (float)height - 110.0F * u;
        plate(dl, px, ((float)width - text_width(px, hud->boss_name)) * 0.5F, by - px * 1.2F,
              hud->boss_name, hc);
        text(dl, px, ((float)width - text_width(px, hud->boss_name)) * 0.5F, by - px * 1.2F,
             rgba(1, 0.92F, 0.8F, 1), hud->boss_name, true);
        bar(dl, bx, by, bw, 14.0F * u, hud->boss_hp / hud->boss_hp_max,
            rgba(0.75F, 0.1F, 0.08F, 0.95F), hc);
    }
    /* nombres de dégâts, au-dessus des têtes */
    for (uint32_t i = 0; i < hud->number_count && i < ENG_HUD_NUMBERS; i++) {
        const EngHudNumber *n = &hud->numbers[i];
        const ImU32 col =
            n->to_player ? rgba(1, 0.35F, 0.3F, n->alpha) : rgba(1, 1, 0.85F, n->alpha);
        const float npx = px * 1.1F;
        text(dl, npx, n->x - text_width(npx, n->text) * 0.5F, n->y, col, n->text, true);
    }
    if (hud->letterbox > 0.0F) { /* cinématique : bandes noires */
        const float band = (float)height * 0.11F * (hud->letterbox > 1.0F ? 1.0F : hud->letterbox);
        ImDrawList_AddRectFilled(dl, (ImVec2_c){0.0F, 0.0F}, (ImVec2_c){(float)width, band},
                                 rgba(0, 0, 0, 1), 0.0F, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2_c){0.0F, (float)height - band},
                                 (ImVec2_c){(float)width, (float)height}, rgba(0, 0, 0, 1), 0.0F,
                                 0);
    }
    if (hud->message[0] != '\0') {
        const float mpx = px * 1.6F;
        text(dl, mpx, ((float)width - text_width(mpx, hud->message)) * 0.5F, (float)height * 0.62F,
             rgba(1, 1, 1, 1), hud->message, true);
    }
    if (hud->clock[0] != '\0') {
        const float cx = (float)width - text_width(px, hud->clock) - 24.0F * u;
        plate(dl, px, cx, 20.0F * u, hud->clock, hc);
        text(dl, px, cx, 20.0F * u, rgba(1, 1, 1, 1), hud->clock, true);
    }
    if (hud->hint[0] != '\0') {
        const float hy = (float)height - 40.0F * u;
        plate(dl, px * 0.7F, 24.0F * u, hy, hud->hint, hc);
        text(dl, px * 0.7F, 24.0F * u, hy, rgba(1, 1, 1, hc ? 1.0F : 0.9F), hud->hint, true);
    }
}

void eng_ui_draw_fade(EngUi *ui, float alpha, int width, int height)
{
    if (ui == NULL || !ui->frame_open || !(alpha > 0.0F) || width <= 0 || height <= 0) {
        return;
    }
    ImDrawList_AddRectFilled(igGetForegroundDrawList_Nil(), (ImVec2_c){0, 0},
                             (ImVec2_c){(float)width, (float)height},
                             rgba(0, 0, 0, alpha > 1.0F ? 1.0F : alpha), 0, 0);
}

#else

void eng_ui_draw_hud(EngUi *ui, const EngHud *hud, int width, int height)
{
    (void)ui;
    (void)hud;
    (void)width;
    (void)height;
}

void eng_ui_draw_fade(EngUi *ui, float alpha, int width, int height)
{
    (void)ui;
    (void)alpha;
    (void)width;
    (void)height;
}

#endif
