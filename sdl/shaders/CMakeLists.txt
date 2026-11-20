# Sources HLSL du jeu. Les sorties (SPIR-V, DXIL, MSL, JSON) vont à côté de l'exécutable, dans
# bin/shaders/, d'où le moteur les charge (engine/shader.h).
include(${CMAKE_SOURCE_DIR}/cmake/Shaders.cmake)

ow_add_shaders(shaders
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/shaders
  SOURCES
    triangle.vert.hlsl
    triangle.frag.hlsl
    cube.vert.hlsl
    textured.vert.hlsl
    textured_lit.vert.hlsl
    textured.frag.hlsl
    multitex.frag.hlsl
    model.vert.hlsl
    model.frag.hlsl
    instanced.vert.hlsl
    grass.vert.hlsl
    terrain.frag.hlsl
    water.frag.hlsl
    lines.vert.hlsl
    lines.frag.hlsl
    tonemap.vert.hlsl
    tonemap.frag.hlsl
    sky.vert.hlsl
    sky.frag.hlsl
    atmosphere.frag.hlsl
    shadow.vert.hlsl
    shadow.frag.hlsl
    model_skinned.vert.hlsl
    shadow_skinned.vert.hlsl
    shadow_instanced.vert.hlsl)
