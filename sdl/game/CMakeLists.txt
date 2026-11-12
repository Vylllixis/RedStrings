# Le jeu est une bibliothèque dynamique rechargeable à chaud (M0-17).
# Il ne lie PAS le moteur : il ne voit que ses en-têtes. Le moteur lui fournit ses services par
# une table de fonctions (engine/game_api.h) ; bridge.c relie ENG_LOG_* et ENG_ASSERT à cette
# table. Résultat : une seule copie de l'état du moteur, celle de l'exécutable.
add_library(game SHARED src/game.c src/bridge.c)
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/engine/include)
set_target_properties(game PROPERTIES
  PREFIX ""
  C_VISIBILITY_PRESET hidden) # seul eng_game_get_api est exporté (ENG_GAME_EXPORT)
target_link_libraries(game PRIVATE project_options)
