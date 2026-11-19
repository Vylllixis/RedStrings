#ifndef ENG_UI_GAME_H
#define ENG_UI_GAME_H

/* Interface du jeu (M8-03) et rendu des menus (M8-04), par-dessus la scène, avec Dear ImGui (listes
 * de dessin, pas de fenêtres : la logique des menus est dans game_menu, sans souris). Tailles
 * proportionnelles à la hauteur de l'écran (référence 1080 px) × l'échelle du texte : lisible en
 * 1080p comme en 4K. Sans OW_UI : ne dessine rien. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/game_menu.h"
#include "engine/ui.h"

#define ENG_HUD_NUMBERS 32U

typedef struct EngHudNumber {
    float x;
    float y; /* pixels */
    char text[16];
    float alpha;
    bool to_player; /* dégât subi (rouge) ou infligé (blanc) */
} EngHudNumber;

typedef struct EngHud {
    float hp;
    float hp_max;
    float stamina;
    float stamina_max;
    float posture;
    float posture_max;
    bool has_target;
    char target_name[32];
    float target_hp;
    float target_hp_max;
    float target_posture;
    float target_posture_max;
    EngHudNumber numbers[ENG_HUD_NUMBERS];
    uint32_t number_count;
    char message[96]; /* centré en bas (« Tu es mort », « Camp vaincu ») */
    char hint[128];   /* rappel des commandes, en bas à gauche */
    char clock[8];    /* heure du jour « HH:MM », en haut à droite (vide : rien) */
    uint32_t level;   /* niveau du joueur (0 : rien d'affiché) */
    float xp_frac;    /* progression dans le niveau, [0, 1] */
    float text_scale;
    bool high_contrast;
} EngHud;

void eng_ui_draw_hud(EngUi *ui, const EngHud *hud, int width, int height);
void eng_ui_draw_menu(EngUi *ui, const EngMenu *menu, int width, int height, float text_scale,
                      bool high_contrast);

/* Taille de texte en pixels pour une hauteur d'écran (testable sans GPU). */
float eng_ui_text_px(int height, float text_scale);

#endif
