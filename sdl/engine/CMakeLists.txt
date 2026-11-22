# Implémentations des bibliothèques à en-tête unique (cgltf, stb) : code tiers,
# donc compilé SANS project_options (pas de -Werror, pas de /WX).
add_library(engine_third_party OBJECT src/third_party_impl.c src/third_party_bc7.c src/third_party_fnl.c)
target_link_libraries(engine_third_party PRIVATE cgltf_headers stb_headers bc7enc_headers fnl_headers)
target_include_directories(engine_third_party PRIVATE include)
set_target_properties(engine_third_party PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  # Les avertissements du code tiers (strncpy, conversions...) ne nous concernent pas.
  target_compile_definitions(engine_third_party PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(engine_third_party PRIVATE /W0)
endif()

add_library(engine STATIC src/engine.c src/deps.c src/platform.c src/log.c src/assert.c src/arena.c src/memory.c src/array.c src/pool.c src/map.c src/clock.c src/loop.c src/game_host.c src/input.c src/input_defaults.c src/files.c src/config.c src/gpu.c src/gpu_debug.c src/shader.c src/shader_hot.c src/renderer.c src/image.c src/camera.c src/orbit_camera.c src/locomotion.c src/jump.c src/stamina.c src/skeleton.c src/skin_file.c src/socket.c src/anim_clip.c src/pose.c src/anim_debug.c src/anim_blend.c src/anim_blendspace.c src/anim_graph.c src/anim_rootmotion.c src/anim_ik.c src/anim_machine.c src/actor_anim.c
    src/equipment.c
    src/ambience.c src/combat_state.c src/attack_data.c src/anim_events.c src/combat_geom.c src/hurtbox.c src/combat_hit.c src/combat_damage.c src/input_buffer.c src/fighter.c src/combat_sim.c src/combat_replay.c src/lockon.c src/damage_numbers.c src/combat_debug.c src/game_feel.c src/sfx.c src/mixer.c src/combat_audio.c src/audio_device.c src/vfx.c src/ai_world.c src/navgrid.c src/ai_perception.c src/ai_tokens.c src/ai_archetype.c src/ai_group.c src/training_dummy.c src/ai_camp.c src/ai_debug.c src/jobs.c src/biome.c src/map_zones.c src/gear.c src/creature.c src/boss_special.c src/zone_decor.c src/world_map.c src/world_map_parse.c src/world_map_build.c src/world_map_trees.c src/world_map_water.c src/world_map_grass.c src/grass_tuft.c src/ai_avoid.c src/text_sdf.c src/terrain_gpu_extra.c src/world_cell.c src/world_stream.c src/world_state.c src/world_origin.c src/upload_queue.c src/ai_area.c src/csm.c src/terrain_lod.c src/mesh_lod.c src/save_game.c src/game_menu.c src/ui_game.c src/daynight.c src/progression.c src/model.c src/model_gpu.c src/debug_lines.c src/debug_lines_gpu.c src/frame_stats.c src/tweak.c src/lighting.c
    src/pbr.c src/ibl.c src/shadow.c src/environment.c src/tonemap.c src/ui.c src/asset_format.c src/tex_file.c src/mesh_file.c src/assets.c src/asset_types.c src/asset_cook.c src/world.c src/scene.c src/editor.c src/heightmap.c src/terrain.c src/splatmap.c src/terrain_texture.c src/terrain_gpu.c src/frustum.c src/atmosphere.c src/fog.c src/instancing.c src/rock.c src/placement.c src/grass.c src/water.c src/physics.c)
target_sources(engine PRIVATE $<TARGET_OBJECTS:engine_third_party>)
# Rapport de crash (M9-03, livré par la session M9) : compilé seulement quand src/crash.c est
# présent dans le dépôt ; sinon le lanceur démarre sans gestionnaire de crash (ENG_HAVE_CRASH absent).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/crash.c)
  target_sources(engine PRIVATE src/crash.c)
  target_compile_definitions(engine PUBLIC ENG_HAVE_CRASH)
endif()
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PUBLIC SDL3::SDL3 cglm_headers cgltf_headers stb_headers ow_profile ow_imgui)
if(OW_UI)
  # Couche C++ des backends ImGui (SDL3, SDL_GPU) : compilée comme notre code (mêmes avertissements)
  target_sources(engine PRIVATE src/ui_backend.cpp)
  set_target_properties(engine PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endif()
# Jolt : lié en PRIVATE (ni en-têtes ni définitions ne se propagent) : game/ et les tests ne peuvent
# pas inclure joltc.h. Seul physics.c le voit.
if(OW_PHYSICS)
  target_link_libraries(engine PRIVATE ow_physics)
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
