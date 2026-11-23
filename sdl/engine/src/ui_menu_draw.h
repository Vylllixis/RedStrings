#ifndef ENG_UI_MENU_DRAW_H
#define ENG_UI_MENU_DRAW_H

/* Dessin des menus : ce que se partagent ui_menu.c (barre, listes, fond) et ui_menu_cards.c
 * (cartes des parties et leur panneau). Seulement avec OW_UI. */

#ifdef OW_UI

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "engine/ui_menu.h"

typedef struct MenuCtx {
    ImDrawList *dl;
    ImFont *font; /* texte courant */
    ImFont *bold; /* titres, lignes choisies (la police courante si aucune grasse n'est chargée) */
    const EngMenu *m;
    EngMenuMotion *a;
    float w; /* écran, pixels */
    float h;
    float px; /* taille du texte de base */
    float u;  /* unité d'interface : px / 24 */
    bool hc;  /* contraste élevé */
    bool on_title;
    ImVec2_c mouse;
    int32_t hover; /* ligne survolée (écran courant seulement) */
    int32_t hover_dir;
} MenuCtx;

/* Une colonne de lignes simples (pause, actions d'une partie, boutons). */
typedef struct MenuRows {
    float x;
    float y;
    float size;     /* taille du texte */
    float row_h;    /* hauteur d'une ligne */
    float width;    /* largeur cliquable */
    float enter;    /* secondes depuis l'entrée de l'écran (cascade) */
    float presence; /* 0 à 1 : opacité de l'ensemble */
    bool live;      /* écran courant : survol de la souris, mises en avant animées */
    int32_t danger; /* ligne « dangereuse » (Supprimer), -1 si aucune : choisie, elle rougit */
} MenuRows;

float mm_clamp01(float x);
float mm_smooth(float a, float b, float x);
ImU32 mm_col(float r, float g, float b, float a);
ImU32 mm_ivory(float a); /* texte clair */
ImU32 mm_soft(float a);  /* texte secondaire */
ImU32 mm_red(float a);   /* le rouge du fil */
ImU32 mm_mix(const float from[3], const float to[3], float k, float a);

float mm_text_w(ImFont *font, float size, const char *s);
/* `shadow` (0 à 1) : ombre portée pour rester lisible sur une image claire */
void mm_text(const MenuCtx *c, ImFont *font, float size, float x, float y, ImU32 col, const char *s,
             float shadow);
/* texte aux lettres espacées de `track` pixels */
float mm_tracked_w(ImFont *font, float size, const char *s, float track);
void mm_tracked(const MenuCtx *c, ImFont *font, float size, float x, float y, ImU32 col,
                const char *s, float track, float shadow);
/* majuscules (ASCII et lettres accentuées du français) */
void mm_upper(const char *in, char *out, size_t capacity);
/* la plus grande taille <= `size` pour que `s` tienne dans `width` */
float mm_fit(ImFont *font, float size, const char *s, float track, float width);

/* Transformation d'un groupe déjà dessiné : tous les sommets émis depuis mm_mark sont mis à
 * l'échelle et tournés autour du pivot, déplacés, et leur opacité multipliée. */
int mm_mark(const MenuCtx *c);
void mm_xform(const MenuCtx *c, int from, float pivot_x, float pivot_y, float scale, float angle,
              float dx, float dy, float alpha);

/* le fil rouge : un trait net et son halo */
void mm_thread(const MenuCtx *c, const ImVec2_c *points, int count, float width, float alpha);
/* la perle (le nœud) qui marque la ligne choisie */
void mm_bead(const MenuCtx *c, float x, float y, float radius, float alpha);
bool mm_mouse_in(const MenuCtx *c, float x0, float y0, float x1, float y1);

/* Les lignes ; renvoie le bas de la dernière. */
float mm_rows(MenuCtx *c, const EngMenuItem *items, uint32_t count, int32_t selected,
              const MenuRows *rows);
/* une petite ligne d'aide, centrée en `x` */
void mm_hint(const MenuCtx *c, float x, float y, const char *text, float alpha);

/* ui_menu_cards.c : le fil et les trois cartes (toujours, selon leur animation), puis le panneau
 * d'un écran de la famille des cartes. */
void mm_draw_cards(MenuCtx *c);
void mm_draw_cards_screen(MenuCtx *c, const EngMenu *m, float enter, float presence, bool live);

#endif
#endif
