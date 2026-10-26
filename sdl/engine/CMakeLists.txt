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

add_library(engine STATIC src/engine.c src/deps.c src/platform.c src/log.c)
target_sources(engine PRIVATE $<TARGET_OBJECTS:engine_third_party>)
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PUBLIC SDL3::SDL3 cglm_headers cgltf_headers stb_headers)
target_link_libraries(engine PRIVATE project_options)
