# Le jeu est une bibliothèque dynamique rechargeable à chaud (M0-17).
# Il ne lie PAS le moteur : il ne voit que ses en-têtes. Le moteur lui
# fournira ses services via une table de fonctions, ce qui évite deux copies
# de l'état du moteur (dans l'exécutable et dans la bibliothèque).
add_library(game SHARED src/game.c)
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/engine/include)
set_target_properties(game PROPERTIES PREFIX "")
target_link_libraries(game PRIVATE project_options)
