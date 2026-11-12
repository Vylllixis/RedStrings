# Implémentations des bibliothèques à en-tête unique (cgltf, stb) : code tiers,
# donc compilé SANS project_options (pas de -Werror, pas de /WX).
add_library(engine_third_party OBJECT src/third_party_impl.c)
target_link_libraries(engine_third_party PRIVATE cgltf_headers stb_headers)
set_target_properties(engine_third_party PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  # Les avertissements du code tiers (strncpy, conversions...) ne nous concernent pas.
  target_compile_definitions(engine_third_party PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(engine_third_party PRIVATE /W0)
endif()

add_library(engine STATIC src/engine.c src/deps.c src/platform.c src/log.c src/assert.c src/arena.c src/memory.c src/array.c src/pool.c src/map.c src/clock.c src/loop.c src/game_host.c src/input.c src/input_defaults.c src/files.c src/config.c src/gpu.c src/gpu_debug.c src/shader.c src/shader_hot.c src/renderer.c src/image.c src/camera.c src/model.c src/model_gpu.c src/debug_lines.c src/debug_lines_gpu.c src/frame_stats.c src/tweak.c src/lighting.c
    src/pbr.c src/ibl.c src/shadow.c src/environment.c src/tonemap.c src/ui.c)
target_sources(engine PRIVATE $<TARGET_OBJECTS:engine_third_party>)
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PUBLIC SDL3::SDL3 cglm_headers cgltf_headers stb_headers ow_profile ow_imgui)
if(OW_UI)
  # Couche C++ des backends ImGui (SDL3, SDL_GPU) : compilée comme notre code (mêmes avertissements)
  target_sources(engine PRIVATE src/ui_backend.cpp)
  set_target_properties(engine PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endif()
# Conventions maths (docs/conventions.md) : profondeur [0,1] comme SDL_GPU ; main droite = défaut de cglm.
target_compile_definitions(engine PUBLIC CGLM_FORCE_DEPTH_ZERO_TO_ONE)
target_link_libraries(engine PRIVATE project_options)

# Rechargement à chaud des shaders : la bibliothèque shadercross (statique) compile le HLSL à
# l'exécution. Sans elle, shader_hot.c ne contient que le repli sur les shaders hors ligne.
if(OW_SHADERS_OK AND OW_SHADER_HOT_RELOAD)
  target_compile_definitions(engine PUBLIC ENG_SHADER_HOT_RELOAD)
  target_link_libraries(engine PUBLIC SDL3_shadercross::SDL3_shadercross-static)
endif()
