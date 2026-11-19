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
        text(dl, px * 0.75F, x0, y, rgba(1, 1, 1, 1), buf, true);
        bar(dl, x0 + text_width(px * 0.75F, buf) + 10.0F * u, y + 5.0F * u, 200.0F * u, 8.0F * u,
            hud->xp_frac, hc ? rgba(0.4F, 0.8F, 1, 1) : rgba(0.35F, 0.6F, 0.95F, 0.9F), hc);
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
    /* nombres de dégâts, au-dessus des têtes */
    for (uint32_t i = 0; i < hud->number_count && i < ENG_HUD_NUMBERS; i++) {
        const EngHudNumber *n = &hud->numbers[i];
        const ImU32 col =
            n->to_player ? rgba(1, 0.35F, 0.3F, n->alpha) : rgba(1, 1, 0.85F, n->alpha);
        const float npx = px * 1.1F;
        text(dl, npx, n->x - text_width(npx, n->text) * 0.5F, n->y, col, n->text, true);
    }
    if (hud->message[0] != '\0') {
        const float mpx = px * 1.6F;
        text(dl, mpx, ((float)width - text_width(mpx, hud->message)) * 0.5F, (float)height * 0.62F,
             rgba(1, 1, 1, 1), hud->message, true);
    }
    if (hud->clock[0] != '\0') {
        text(dl, px, (float)width - text_width(px, hud->clock) - 24.0F * u, 20.0F * u,
             rgba(1, 1, 1, hc ? 1.0F : 0.85F), hud->clock, true);
    }
    if (hud->hint[0] != '\0') {
        text(dl, px * 0.7F, 24.0F * u, (float)height - 40.0F * u, rgba(1, 1, 1, hc ? 1.0F : 0.75F),
             hud->hint, true);
    }
}

void eng_ui_draw_menu(EngUi *ui, const EngMenu *menu, int width, int height, float text_scale,
                      bool hc)
{
    if (ui == NULL || !ui->frame_open || menu == NULL || !eng_menu_is_open(menu) || width <= 0 ||
        height <= 0) {
        return;
    }
    ImDrawList *dl = igGetForegroundDrawList_Nil();
    const float px = eng_ui_text_px(height, text_scale);
    const float u = px / 24.0F;
    ImDrawList_AddRectFilled(dl, (ImVec2_c){0, 0}, (ImVec2_c){(float)width, (float)height},
                             rgba(0.02F, 0.03F, 0.05F, 0.72F), 0, 0);
    const char *title = eng_menu_title(menu);
    const float tpx = px * 2.2F;
    text(dl, tpx, ((float)width - text_width(tpx, title)) * 0.5F, (float)height * 0.16F,
         rgba(1, 1, 1, 1), title, true);
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(menu, items, ENG_MENU_MAX_ITEMS);
    const float row = px * 1.7F;
    const float col_w = 760.0F * u;
    const float x0 = ((float)width - col_w) * 0.5F;
    float y = (float)height * 0.16F + tpx * 1.8F;
    for (uint32_t i = 0; i < n; i++) {
        const bool sel = (int32_t)i == menu->selected;
        if (sel) {
            ImDrawList_AddRectFilled(dl, (ImVec2_c){x0 - 16.0F * u, y - row * 0.18F},
                                     (ImVec2_c){x0 + col_w + 16.0F * u, y + row * 0.82F},
                                     hc ? rgba(1, 1, 1, 0.95F) : rgba(0.85F, 0.25F, 0.2F, 0.85F),
                                     6.0F * u, 0);
        }
        const ImU32 fg = sel && hc          ? rgba(0, 0, 0, 1)
                         : items[i].enabled ? rgba(1, 1, 1, 1)
                                            : rgba(0.55F, 0.55F, 0.55F, 1);
        text(dl, px, x0, y, fg, items[i].label, !sel || !hc);
        if (items[i].value[0] != '\0') {
            text(dl, px, x0 + col_w - text_width(px, items[i].value), y, fg, items[i].value,
                 !sel || !hc);
        }
        y += row;
    }
    const char *help =
        menu->screen == ENG_MENU_OPTIONS
            ? "haut/bas : choisir   gauche/droite : régler   Espace/A : valider   Ctrl/B : retour"
            : "haut/bas : choisir   Espace/A : valider   Ctrl/B : retour";
    const float hpx = px * 0.7F;
    text(dl, hpx, ((float)width - text_width(hpx, help)) * 0.5F, (float)height - 60.0F * u,
         rgba(1, 1, 1, 0.8F), help, true);
    if (menu->status[0] != '\0') {
        text(dl, px, ((float)width - text_width(px, menu->status)) * 0.5F, y + row * 0.5F,
             rgba(0.6F, 1, 0.6F, 1), menu->status, true);
    }
}

#else

void eng_ui_draw_hud(EngUi *ui, const EngHud *hud, int width, int height)
{
    (void)ui;
    (void)hud;
    (void)width;
    (void)height;
}

void eng_ui_draw_menu(EngUi *ui, const EngMenu *menu, int width, int height, float text_scale,
                      bool high_contrast)
{
    (void)ui;
    (void)menu;
    (void)width;
    (void)height;
    (void)text_scale;
    (void)high_contrast;
}

#endif
