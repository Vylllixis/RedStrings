#pragma once

#include "engine/game_api.h"

/* Le jeu ne lie pas le moteur : bridge.c fournit, côté jeu, eng_log_write / eng_assert_fail,
 * qui transmettent au moteur par la table de services. Ainsi ENG_LOG_* et ENG_ASSERT s'utilisent
 * dans le jeu exactement comme dans le moteur. À appeler dans init ET reload. */
void gm_bind_services(const EngServices *services);
