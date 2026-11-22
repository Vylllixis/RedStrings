#ifndef ENG_GAME_MENU_INTERNAL_H
#define ENG_GAME_MENU_INTERNAL_H

/* Partagé par game_menu.c (navigation) et game_menu_items.c (lignes des écrans). */

#include "engine/game_menu.h"

/* Actions remappables de l'écran Contrôles, dans l'ordre des lignes. */
uint32_t eng_menu_remap_count(void);
EngAction eng_menu_remap_action(uint32_t index);

int32_t eng_menu_valid_slot(int32_t slot);
/* Premier emplacement libre, -1 s'ils sont tous occupés. */
int32_t eng_menu_free_slot(const EngMenu *m);

#endif
