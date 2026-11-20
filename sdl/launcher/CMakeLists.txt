add_executable(launcher src/main.c src/post_bench.c src/demo_scene.c src/editor_mode.c src/play_mode.c src/play_combat.c src/world_bench.c src/skeleton_view.c src/game_flow.c src/avatar.c src/avatar_gallery.c src/world_view.c)
target_link_libraries(launcher PRIVATE engine)
add_dependencies(launcher game)
target_link_libraries(launcher PRIVATE project_options)
if(OW_SHADERS_OK)
  add_dependencies(launcher shaders)
  if(OW_SHADER_HOT_RELOAD)
    # Dossier des sources HLSL, surveillées par le rechargement à chaud (Debug seulement).
    target_compile_definitions(launcher PRIVATE OW_SHADER_SOURCE_DIR="${CMAKE_SOURCE_DIR}/shaders")
  endif()
endif()
ow_copy_shader_runtime(launcher)

# Assets du lanceur (textures de la démonstration...) : copiés à côté de l'exécutable, d'où le
# moteur les lit (chemins relatifs à l'exécutable, engine/files.h). Recopiés à chaque build : le
# dossier est petit. Le modèle de la démonstration est lu cuit : assets/cooked/ vient de cooked_assets.
add_custom_target(launcher_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/bin/assets
  COMMENT "Copie des assets")
add_dependencies(launcher launcher_assets cooked_assets cooked_characters)

# --watch-assets : le lanceur recuit le modèle de démonstration à partir des sources du dépôt (assets/)
# avec assetc, puis le recharge (M2-06).
target_compile_definitions(launcher PRIVATE
  OW_ASSETC_PATH="$<TARGET_FILE:assetc>"
  OW_ASSET_SOURCE_DIR="${CMAKE_SOURCE_DIR}/assets")
add_dependencies(launcher assetc)
