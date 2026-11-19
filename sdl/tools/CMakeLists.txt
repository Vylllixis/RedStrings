add_executable(assetc src/assetc.c)
target_link_libraries(assetc PRIVATE engine)
target_link_libraries(assetc PRIVATE project_options)

# Assets cuisinés du jeu (M2-04) : bin/assets/cooked/, recuits seulement si une source ou assetc change.
# Les sources restent dans assets/ ; le moteur lira les .mesh / .tex cuits (gestionnaire d'assets, M2-05).
include(${CMAKE_SOURCE_DIR}/cmake/Assets.cmake)
ow_cook_assets(cooked_assets
  TOOL assetc
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/assets/cooked
  BASE_DIR ${CMAKE_SOURCE_DIR}/assets
  MODELS models/demo.gltf
  TEXTURES textures/checker.png)
