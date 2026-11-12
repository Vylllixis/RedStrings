add_executable(launcher src/main.c src/demo_scene.c)
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
# dossier est petit.
add_custom_target(launcher_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/bin/assets
  COMMENT "Copie des assets")
add_dependencies(launcher launcher_assets)
