add_library(engine STATIC src/engine.c src/deps.c)
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PUBLIC SDL3::SDL3 cglm_headers cgltf_headers stb_headers)
