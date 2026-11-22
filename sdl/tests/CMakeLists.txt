# Un exécutable par module testé : chacun est une entrée CTest (ctest -R log, etc.).
function(ow_add_test name)
  add_executable(test_${name} ${ARGN} utest_main.c)
  target_link_libraries(test_${name} PRIVATE engine utest_headers project_options)
  set_target_properties(test_${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin)
  add_test(NAME engine.${name} COMMAND test_${name}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

ow_add_test(log test_log.c)
ow_add_test(assert test_assert.c)
ow_add_test(arena test_arena.c)
ow_add_test(memory test_memory.c)
ow_add_test(array test_array.c)
ow_add_test(pool test_pool.c)
ow_add_test(map test_map.c)
ow_add_test(assets test_assets.c)
ow_add_test(asset_cook test_asset_cook.c)
ow_add_test(world test_world.c)
ow_add_test(scene test_scene.c)
ow_add_test(editor test_editor.c)
ow_add_test(heightmap test_heightmap.c)
ow_add_test(terrain test_terrain.c)
ow_add_test(splatmap test_splatmap.c)
ow_add_test(biome test_biome.c)
ow_add_test(map_zones test_map_zones.c)
ow_add_test(gear test_gear.c)
ow_add_test(creature test_creature.c)
ow_add_test(boss_special test_boss_special.c)
ow_add_test(dragon_flight test_dragon_flight.c)
ow_add_test(zone_decor test_zone_decor.c)
ow_add_test(world_map test_world_map.c)
target_compile_definitions(test_world_map PRIVATE
  OW_WORLD_MAP_FILE="${CMAKE_SOURCE_DIR}/assets/world/carte.txt")
ow_add_test(ai_avoid test_ai_avoid.c)
ow_add_test(frustum test_frustum.c)
ow_add_test(atmosphere test_atmosphere.c)
ow_add_test(fog test_fog.c)
ow_add_test(rock test_rock.c)
ow_add_test(placement test_placement.c)
ow_add_test(grass test_grass.c)
ow_add_test(text_sdf test_text_sdf.c)
target_compile_definitions(test_text_sdf PRIVATE
  OW_FONT_PATH="${CMAKE_SOURCE_DIR}/assets/fonts/NotoSans-Bold.ttf")
ow_add_test(water test_water.c)
ow_add_test(physics test_physics.c)
ow_add_test(physics_layers test_physics_layers.c)
ow_add_test(physics_world test_physics_world.c)
ow_add_test(physics_debug test_physics_debug.c)
ow_add_test(physics_fixed test_physics_fixed.c)
ow_add_test(character test_character.c)
ow_add_test(instancing test_instancing.c)
target_compile_definitions(test_asset_cook PRIVATE OW_CMAKE_COMMAND="${CMAKE_COMMAND}")
ow_add_test(clock test_clock.c)
ow_add_test(loop test_loop.c)
ow_add_test(input test_input.c)
ow_add_test(files test_files.c)
ow_add_test(asset_format test_asset_format.c)
ow_add_test(tex_file test_tex_file.c)
ow_add_test(mesh_file test_mesh_file.c)

# Isolation de Jolt (M4-01) : joltc.h / JPH_ n'apparaissent que dans engine/src/physics.c
add_test(NAME physics.isolation
  COMMAND ${CMAKE_COMMAND} -DOW_ROOT=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/physics_isolation.cmake)

# Cuisson incrémentale (M2-04) : construit un mini-projet avec le générateur de la build principale
# et vérifie quand assetc s'exécute. Le fichier de dépendances n'existe qu'avec Ninja ou Makefiles.
if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
  add_test(NAME assets.cook_incremental
    COMMAND ${CMAKE_COMMAND}
      -DOW_ASSETC=$<TARGET_FILE:assetc>
      -DOW_ASSETS_CMAKE=${CMAKE_SOURCE_DIR}/cmake/Assets.cmake
      -DOW_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/cook_fixture
      -DOW_ASSETS=${CMAKE_SOURCE_DIR}/assets
      -DOW_WORK=${CMAKE_CURRENT_BINARY_DIR}/cook_incremental
      -DOW_GENERATOR=${CMAKE_GENERATOR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cook_incremental.cmake)
  set_tests_properties(assets.cook_incremental PROPERTIES TIMEOUT 300)
endif()
# Ligne de commande de génération des champs de hauteurs (M3-01) : assetc heightmap / heightmap-import.
add_test(NAME assets.heightmap_cli
  COMMAND ${CMAKE_COMMAND}
    -DOW_ASSETC=$<TARGET_FILE:assetc>
    -DOW_WORK=${CMAKE_CURRENT_BINARY_DIR}/heightmap_cli
    -P ${CMAKE_CURRENT_SOURCE_DIR}/heightmap_cli.cmake)
set_tests_properties(assets.heightmap_cli PROPERTIES TIMEOUT 120)
ow_add_test(config test_config.c)
ow_add_test(profile test_profile.c)
ow_add_test(gpu test_gpu.c)
ow_add_test(gpu_debug test_gpu_debug.c)
ow_copy_shader_runtime(test_gpu_debug)
# Les tests de test_gpu_debug.c observent les appels reçus par SDL en remplaçant ses fonctions à
# l'édition de liens (--wrap : éditeur de liens GNU/LLVM sous Linux ; absent de macOS et de MSVC).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_definitions(test_gpu_debug PRIVATE OW_GPU_DEBUG_WRAP=1)
  foreach(fn SDL_PushGPUDebugGroup SDL_PopGPUDebugGroup SDL_InsertGPUDebugLabel SDL_SetGPUTextureName
             SDL_SetGPUBufferName SDL_CreateGPUSampler SDL_CreateGPUGraphicsPipeline SDL_CreateGPUShader)
    target_link_options(test_gpu_debug PRIVATE "LINKER:--wrap=${fn}")
  endforeach()
endif()
ow_add_test(shader test_shader.c)
ow_add_test(shader_hot test_shader_hot.c)
ow_copy_shader_runtime(test_shader_hot)
ow_add_test(math test_math.c)
ow_add_test(camera test_camera.c)
ow_add_test(orbit_camera test_orbit_camera.c)
ow_add_test(locomotion test_locomotion.c)
ow_add_test(jump test_jump.c)
ow_add_test(stamina test_stamina.c)
ow_add_test(skeleton test_skeleton.c)
target_compile_definitions(test_skeleton PRIVATE OW_ASSETC_PATH="$<TARGET_FILE:assetc>")
add_dependencies(test_skeleton assetc)
# engine.skeleton laisse skeleton_demo.skel : le test launcher.skeleton l'affiche (fixture CTest)
set_tests_properties(engine.skeleton PROPERTIES FIXTURES_SETUP skeleton_demo)
ow_add_test(pose test_pose.c)
ow_add_test(anim_debug test_anim_debug.c)
ow_add_test(anim_blend test_anim_blend.c)
ow_add_test(anim_blendspace test_anim_blendspace.c)
ow_add_test(anim_rootmotion test_anim_rootmotion.c)
ow_add_test(anim_ik test_anim_ik.c)
ow_add_test(combat_state test_combat_state.c)
ow_add_test(attack_data test_attack_data.c)
ow_add_test(anim_events test_anim_events.c)
ow_add_test(combat_geom test_combat_geom.c)
ow_add_test(hurtbox test_hurtbox.c)
ow_add_test(combat_hit test_combat_hit.c)
ow_add_test(combat_damage test_combat_damage.c)
ow_add_test(input_buffer test_input_buffer.c)
ow_add_test(fighter test_fighter.c)
ow_add_test(training_dummy test_training_dummy.c)
ow_add_test(combat_rig test_combat_rig.c)
ow_add_test(ai_core test_ai_core.c)
ow_add_test(ai_camp test_ai_camp.c)
ow_add_test(jobs test_jobs.c)
ow_add_test(world_stream test_world_stream.c)
ow_add_test(world_ai test_world_ai.c)
ow_add_test(world_lod test_world_lod.c)
ow_add_test(game_systems test_game_systems.c)
ow_add_test(menu_motion test_menu_motion.c)
ow_add_test(daynight test_daynight.c)
ow_add_test(progression test_progression.c)
ow_add_test(combat_sim test_combat_sim.c)
ow_add_test(combat_replay test_combat_replay.c)
ow_add_test(lockon test_lockon.c)
ow_add_test(game_feel test_game_feel.c)
ow_add_test(damage_numbers test_damage_numbers.c)
ow_add_test(combat_debug test_combat_debug.c)
ow_add_test(audio test_audio.c)
ow_add_test(audio_device test_audio_device.c)
ow_add_test(vfx test_vfx.c)
ow_add_test(anim_graph test_anim_graph.c)
ow_add_test(anim_machine test_anim_machine.c)
ow_add_test(actor_anim test_actor_anim.c)
ow_add_test(equipment test_equipment.c)
ow_add_test(ambience test_ambience.c)
# M4 : vrais fichiers cuits du chevalier (cible cooked_characters) et données de combat de la source.
ow_add_test(character_motion test_character_motion.c)
target_compile_definitions(test_character_motion PRIVATE
  OW_COOKED_DIR="${CMAKE_BINARY_DIR}/bin/assets/cooked"
  OW_ATTACKS_FILE="${CMAKE_SOURCE_DIR}/assets/combat/attacks.txt")
add_dependencies(test_character_motion cooked_characters)
ow_add_test(skin test_skin.c)
ow_add_test(socket test_socket.c)
ow_add_test(skin_file test_skin_file.c)
target_compile_definitions(test_skin_file PRIVATE OW_ASSETC_PATH="$<TARGET_FILE:assetc>")
add_dependencies(test_skin_file assetc)
ow_add_test(anim test_anim.c)
target_compile_definitions(test_anim PRIVATE OW_ASSETC_PATH="$<TARGET_FILE:assetc>")
add_dependencies(test_anim assetc)
# engine.anim laisse anim_demo.skel et anim_demo.anim : le test launcher.anim les affiche (fixture CTest)
set_tests_properties(engine.anim PROPERTIES FIXTURES_SETUP anim_demo)
ow_add_test(camera_collision test_camera_collision.c)
ow_add_test(model test_model.c)
ow_add_test(debug_lines test_debug_lines.c)
ow_add_test(frame_stats test_frame_stats.c)
ow_add_test(tweak test_tweak.c)
ow_add_test(lighting test_lighting.c)
ow_add_test(pbr test_pbr.c)
ow_add_test(ibl test_ibl.c)
ow_add_test(shadow test_shadow.c)
ow_add_test(tonemap test_tonemap.c)
ow_add_test(image test_image.c)
ow_add_test(renderer test_renderer.c)
ow_copy_shader_runtime(test_renderer)
ow_add_test(render3d test_render3d.c)
ow_copy_shader_runtime(test_render3d)
ow_add_test(texture test_texture.c)
ow_copy_shader_runtime(test_texture)
ow_add_test(model_gpu test_model_gpu.c)
ow_copy_shader_runtime(test_model_gpu)
ow_add_test(terrain_gpu test_terrain_gpu.c)
ow_copy_shader_runtime(test_terrain_gpu)
ow_add_test(skinning_gpu test_skinning_gpu.c)
ow_copy_shader_runtime(test_skinning_gpu)
ow_add_test(instancing_gpu test_instancing_gpu.c)
ow_copy_shader_runtime(test_instancing_gpu)
ow_add_test(grass_gpu test_grass_gpu.c)
ow_copy_shader_runtime(test_grass_gpu)
ow_add_test(water_gpu test_water_gpu.c)
ow_copy_shader_runtime(test_water_gpu)
ow_add_test(atmosphere_gpu test_atmosphere_gpu.c)
ow_copy_shader_runtime(test_atmosphere_gpu)
ow_add_test(mesh_file_gpu test_mesh_file_gpu.c)
ow_copy_shader_runtime(test_mesh_file_gpu)
ow_add_test(assets_gpu test_assets_gpu.c)
ow_copy_shader_runtime(test_assets_gpu)
ow_add_test(shadow_gpu test_shadow_gpu.c)
ow_copy_shader_runtime(test_shadow_gpu)
ow_add_test(environment_gpu test_environment_gpu.c)
ow_copy_shader_runtime(test_environment_gpu)
ow_add_test(tonemap_gpu test_tonemap_gpu.c)
ow_copy_shader_runtime(test_tonemap_gpu)
ow_add_test(ui test_ui.c)
ow_add_test(editor_ui test_editor_ui.c)
ow_add_test(debug_lines_gpu test_debug_lines_gpu.c)
ow_copy_shader_runtime(test_debug_lines_gpu)
if(OW_SHADERS_OK)
  target_compile_definitions(test_shader PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_shader shaders)
  target_compile_definitions(test_shader_hot PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_shader_hot shaders)
  target_compile_definitions(test_renderer PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_renderer shaders)
  target_compile_definitions(test_render3d PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_render3d shaders)
  target_compile_definitions(test_texture PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_texture shaders)
  target_compile_definitions(test_model_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_model_gpu shaders)
  target_compile_definitions(test_terrain_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_terrain_gpu shaders)
  target_compile_definitions(test_instancing_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_instancing_gpu shaders)
  target_compile_definitions(test_skinning_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_skinning_gpu shaders)
  target_compile_definitions(test_grass_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_grass_gpu shaders)
  target_compile_definitions(test_water_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_water_gpu shaders)
  target_compile_definitions(test_atmosphere_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_atmosphere_gpu shaders)
  target_compile_definitions(test_mesh_file_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_mesh_file_gpu shaders)
  target_compile_definitions(test_assets_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_assets_gpu shaders assetc)
  target_compile_definitions(test_assets_gpu PRIVATE OW_ASSETC_PATH="$<TARGET_FILE:assetc>")
  target_compile_definitions(test_shadow_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_shadow_gpu shaders)
  target_compile_definitions(test_environment_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_environment_gpu shaders)
  target_compile_definitions(test_tonemap_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_tonemap_gpu shaders)
  target_compile_definitions(test_debug_lines_gpu PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_debug_lines_gpu shaders)
  target_compile_definitions(test_gpu_debug PRIVATE OW_SHADER_DIR="${CMAKE_BINARY_DIR}/bin/shaders")
  add_dependencies(test_gpu_debug shaders)
  if(OW_SHADER_HOT_RELOAD)
    target_compile_definitions(test_renderer PRIVATE OW_SHADER_SOURCE_DIR="${CMAKE_SOURCE_DIR}/shaders")
  endif()
endif()
ow_add_test(platform_input test_platform_input.c)

# ── Tests de bout en bout : on lance le vrai exécutable et on lit sa sortie ──
# (expressions régulières volontairement sans accents : indépendantes de l'encodage)
set(LAUNCHER_ENV "SDL_VIDEODRIVER=dummy")

add_test(NAME launcher.smoke COMMAND launcher --frames 3 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.smoke PROPERTIES
  ENVIRONMENT "${LAUNCHER_ENV}"
  PASS_REGULAR_EXPRESSION "propre apr.* 3 images")

# Objectif M0-12 : aucune allocation dans la boucle de frame (60 images, pilote video factice)
add_test(NAME launcher.zero_alloc COMMAND launcher --frames 60 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.zero_alloc PROPERTIES
  ENVIRONMENT "${LAUNCHER_ENV}"
  PASS_REGULAR_EXPRESSION "boucle de frame : 0 ")


# Fenêtre sans écran (pilote « offscreen ») : avec un GPU, rendu GPU ; sans GPU, repli logiciel.
# Dans les deux cas le lanceur doit tourner 30 images et s'arrêter proprement.
add_test(NAME launcher.offscreen COMMAND launcher --frames 30 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.offscreen PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  PASS_REGULAR_EXPRESSION "propre apr.* 30 images")

# La démonstration charge assets/textures/checker.png ; si l'image est introuvable, le lanceur
# se rabat sur une texture de remplacement et journalise une ERREUR : le test échoue alors.
if(OW_SHADERS_OK)
# Ombre du soleil (M1-17) : chaque image ouvre une passe d'ombre (carte D32F rendue avant la scène).
# Le lanceur affiche leur nombre à l'arrêt : 10 images, 10 passes.
add_test(NAME launcher.shadow COMMAND launcher --frames 10 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.shadow PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  PASS_REGULAR_EXPRESSION "10 image.s., 10 passe.s. d.ombre")

# Squelette de debug (M4-12) : le lanceur lit un .skel cuit par assetc (engine.skeleton) et dessine
# ses os pendant 10 images. Le test vérifie la lecture (4 articulations) et l'arrêt propre ; il ne
# vérifie PAS l'alignement à l'écran (voir docs/skeleton.md).
add_test(NAME launcher.skeleton COMMAND launcher --frames 10 --env-samples 16 --skeleton skeleton_demo.skel
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.skeleton PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  FIXTURES_REQUIRED skeleton_demo
  PASS_REGULAR_EXPRESSION "squelette : [^\n]*4 articulations.*propre apr.* 10 images"
  FAIL_REGULAR_EXPRESSION "ERROR")

# Débogueur d'animation (M4-16) : lecture du clip sur le squelette, os animés superposés au modèle.
add_test(NAME launcher.anim COMMAND launcher --frames 10 --env-samples 16 --skeleton anim_demo.skel
         --anim anim_demo.anim WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.anim PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  FIXTURES_REQUIRED anim_demo
  PASS_REGULAR_EXPRESSION "animation : [^\n]*anim_demo.anim.*propre apr.* 10 images"
  FAIL_REGULAR_EXPRESSION "ERROR")

# Sockets (M4-22) : deux points d'attache dessinés sur le squelette animé (repères RVB).
add_test(NAME launcher.sockets COMMAND launcher --frames 10 --env-samples 16 --skeleton anim_demo.skel
         --anim anim_demo.anim --sockets anim_demo.sockets WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.sockets PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  FIXTURES_REQUIRED anim_demo
  PASS_REGULAR_EXPRESSION "sockets : [^\n]*anim_demo.sockets, 2 points.*propre apr.* 10 images"
  FAIL_REGULAR_EXPRESSION "ERROR")
add_test(NAME launcher.sockets_bad COMMAND launcher --frames 10 --env-samples 16 --skeleton anim_demo.skel
         --anim anim_demo.anim --sockets anim_demo.skel WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.sockets_bad PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  FIXTURES_REQUIRED anim_demo
  PASS_REGULAR_EXPRESSION "sockets : [^\n]*anim_demo.skel : ligne 1")

# Redimensionnements (M1-18) : la fenêtre change de taille à CHAQUE image pendant 120 images, avec 1, 2
# puis 3 images en vol ; le lanceur vérifie à l'arrêt qu'aucune barrière ni cible n'a fui (code de
# sortie 1 sinon) et qu'aucune image n'a été ignorée.
foreach(n 1 2 3)
  add_test(NAME launcher.resize_${n} COMMAND launcher --stress-resize --frames-in-flight ${n}
           --frames 120 --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.resize_${n} PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    PASS_REGULAR_EXPRESSION "0 image.s. ignor.*, [1-3] image.s. en vol au plus : sans fuite")
endforeach()

# (nécessite un GPU : sans GPU le lanceur journalise « GPU indisponible », ce que ce test refuse)
# Interface de debug affichée dès le départ (--ui) : 30 images avec le panneau, sans erreur (avec
# un GPU : le panneau est construit et dessiné à chaque image par la passe overlay).
add_test(NAME launcher.ui COMMAND launcher --ui --frames 30 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.ui PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  FAIL_REGULAR_EXPRESSION "ERROR|indisponible : "
  PASS_REGULAR_EXPRESSION "propre apr.* 30 images")

# Éditeur (M2-10b), de bout en bout avec un vrai GPU : le lanceur charge assets/scenes/demo.scene, projette
# le modèle central à l'écran, le sélectionne et tire sa poignée X de 3 m avec la machine à états de la
# souris (rayons issus de pixels), enregistre la scène puis la relit dans un monde neuf (--editor-selftest).
# Les événements SDL réels (SDL_GetMouseState) ne sont pas simulés : voir docs/editor.md.
add_test(NAME launcher.editor COMMAND launcher --editor-selftest --frames 30 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.editor PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  FAIL_REGULAR_EXPRESSION "ERROR|ÉCHEC"
  PASS_REGULAR_EXPRESSION "AUTOTEST ÉDITEUR : OK")

# Une scène illisible n'est jamais écrasée : l'éditeur le dit (ERREUR attendue) et désactive l'enregistrement.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/bad.scene
     "openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2\n")
add_test(NAME launcher.editor_bad_scene
         COMMAND launcher --editor --scene ${CMAKE_CURRENT_BINARY_DIR}/bad.scene --frames 10 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.editor_bad_scene PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  PASS_REGULAR_EXPRESSION "ligne 4 : Transform.position : trois valeurs sont attendues\\) : enregistrement désactivé")

# Rechargement à chaud (M2-06) : --watch-assets surveille les sources du modèle pendant 30 images (sans
# rien modifier : ni cuisson ni rechargement), le lanceur s'arrête proprement, sans ERREUR.
add_test(NAME launcher.watch_assets COMMAND launcher --watch-assets --frames 30 --env-samples 16
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.watch_assets PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
  FAIL_REGULAR_EXPRESSION "ERROR|WARN  demo_scene"
  PASS_REGULAR_EXPRESSION "sources surveill")

# Terrain (M3-02) : --terrain charge un champ de hauteurs généré par assetc et le dessine ; un fichier
# absent est une ERREUR journalisée mais la démonstration continue.
add_test(NAME launcher.terrain
  COMMAND ${CMAKE_COMMAND}
    -DOW_ASSETC=$<TARGET_FILE:assetc>
    -DOW_LAUNCHER=$<TARGET_FILE:launcher>
    -DOW_WORK=${CMAKE_CURRENT_BINARY_DIR}/launcher_terrain
    -P ${CMAKE_CURRENT_SOURCE_DIR}/launcher_terrain.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.terrain PROPERTIES TIMEOUT 180)

# Ciel atmosphérique (M3-05) : la lumière du soleil et l'ambiante suivent sa hauteur.
add_test(NAME launcher.sky
  COMMAND ${CMAKE_COMMAND} -DOW_LAUNCHER=$<TARGET_FILE:launcher>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/launcher_sky.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.sky PROPERTIES TIMEOUT 240)

foreach(variant IN ITEMS default no_mipmaps anisotropy_4)
  if(variant STREQUAL "no_mipmaps")
    set(variant_args --no-mipmaps)
  elseif(variant STREQUAL "anisotropy_4")
    set(variant_args --anisotropy 4)
  else()
    set(variant_args)
  endif()
  add_test(NAME launcher.texture_${variant} COMMAND launcher --frames 10 --env-samples 16 ${variant_args}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.texture_${variant} PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|remplacement"
    PASS_REGULAR_EXPRESSION "propre apr.* 10 images")
endforeach()
endif()

add_test(NAME launcher.bad_option COMMAND launcher --log-level bruit
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.bad_option PROPERTIES
  PASS_REGULAR_EXPRESSION "usage :")

# ── Hôte de rechargement à chaud : trois faux jeux (A, B, « gros ») que le test copie à la ──
# ── place de la bibliothèque surveillée.                                                     ──
foreach(variant IN ITEMS 1:variant_a 2:variant_b 3:variant_big)
  string(REPLACE ":" ";" parts ${variant})
  list(GET parts 0 variant_number)
  list(GET parts 1 variant_name)
  add_library(${variant_name} SHARED test_game_lib.c ${CMAKE_SOURCE_DIR}/game/src/bridge.c)
  target_include_directories(${variant_name} PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/include ${CMAKE_SOURCE_DIR}/game/src)
  target_compile_definitions(${variant_name} PRIVATE TEST_VARIANT=${variant_number})
  target_link_libraries(${variant_name} PRIVATE project_options)
  set_target_properties(${variant_name} PROPERTIES
    PREFIX ""
    C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin)
endforeach()

ow_add_test(game_host test_game_host.c)
add_dependencies(test_game_host variant_a variant_b variant_big)
target_compile_definitions(test_game_host PRIVATE
  TEST_LIB_DIR="${CMAKE_BINARY_DIR}/tests/bin"
  TEST_LIB_EXT="${CMAKE_SHARED_LIBRARY_SUFFIX}")

target_compile_definitions(test_config PRIVATE OW_CONFIG_EXAMPLE="${CMAKE_SOURCE_DIR}/config/config.ini")

# ── Tests GPU sur un pilote Vulkan LOGICIEL (lavapipe, SwiftShader) ──
# Les tests ouvrent une fenêtre « offscreen » (sans écran). Avec le pilote NVIDIA 580 sur cette
# configuration, la création de la swapchain y plante (SEGV dans libnvidia-glcore) ; lavapipe et
# SwiftShader s'en accommodent. Un pilote logiciel donne en plus des images identiques d'une machine
# à l'autre, ce qui convient aux tests qui lisent des pixels. Le lanceur lancé à la main, lui, utilise
# toujours le vrai GPU. Désactiver avec -DOW_TEST_SOFTWARE_VULKAN=OFF ; forcer un fichier ICD avec
# -DOW_SOFTWARE_VULKAN_ICD=/chemin/vers/icd.json.
option(OW_TEST_SOFTWARE_VULKAN "Tests GPU sur un pilote Vulkan logiciel s'il est installé" ON)
if(OW_TEST_SOFTWARE_VULKAN)
  find_file(OW_SOFTWARE_VULKAN_ICD
    NAMES lvp_icd.json lvp_icd.x86_64.json vk_swiftshader_icd.json
    PATHS /usr/share/vulkan/icd.d /etc/vulkan/icd.d /usr/local/share/vulkan/icd.d
    DOC "Fichier ICD Vulkan logiciel pour les tests GPU")
  if(OW_SOFTWARE_VULKAN_ICD)
    message(STATUS "Tests GPU : pilote Vulkan logiciel ${OW_SOFTWARE_VULKAN_ICD}")
    set(ow_gpu_tests engine.gpu engine.gpu_debug engine.shader engine.shader_hot engine.renderer engine.render3d
        engine.texture engine.model_gpu engine.terrain_gpu engine.instancing_gpu engine.skinning_gpu engine.grass_gpu engine.water_gpu engine.atmosphere_gpu engine.mesh_file_gpu engine.assets_gpu engine.shadow_gpu engine.environment_gpu engine.tonemap_gpu engine.ui engine.editor_ui engine.debug_lines_gpu launcher.offscreen launcher.shadow launcher.skeleton launcher.anim launcher.sockets launcher.sockets_bad launcher.resize_1 launcher.resize_2 launcher.resize_3
        launcher.ui launcher.editor launcher.editor_bad_scene launcher.watch_assets launcher.terrain launcher.sky launcher.texture_default launcher.texture_no_mipmaps launcher.texture_anisotropy_4)
    foreach(t IN LISTS ow_gpu_tests)
      if(TEST ${t})
        set_property(TEST ${t} APPEND PROPERTY ENVIRONMENT_MODIFICATION
          "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}"
          "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
      endif()
    endforeach()
  else()
    message(STATUS "Tests GPU : aucun pilote Vulkan logiciel trouvé (sudo apt install mesa-vulkan-drivers)")
  endif()
endif()

# Mode jouable (M4) : personnage, saut, sprint, endurance, caméra et collisions assemblés, par des
# entrées scriptées, sans fenêtre (--play-selftest). Demande Jolt.
if(OW_PHYSICS)
  add_test(NAME launcher.play COMMAND launcher --play-selftest
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.play PROPERTIES
    ENVIRONMENT "${LAUNCHER_ENV}"
    PASS_REGULAR_EXPRESSION "AUTOTEST JEU : OK")
endif()

# Combat dans le mode jouable (raccord M4 + M5 + M6, --combat-selftest) : mannequin, verrouillage,
# esquive, camp de 3 ennemis (au plus 2 attaquants), victoire. Demande Jolt.
if(OW_PHYSICS)
  add_test(NAME launcher.combat COMMAND launcher --combat-selftest
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.combat PROPERTIES
    ENVIRONMENT "${LAUNCHER_ENV}"
    PASS_REGULAR_EXPRESSION "AUTOTEST COMBAT : OK")
  # Grande carte : boss spéciaux (cavalier : cheval, cinématique, à pied ; dragon : flammes,
  # corps à corps), sans fenêtre.
  add_test(NAME launcher.bosses COMMAND launcher --boss-selftest
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.bosses PROPERTIES
    ENVIRONMENT "${LAUNCHER_ENV}"
    PASS_REGULAR_EXPRESSION "AUTOTEST BOSS : OK")
  # Le même avec les personnages KayKit (M4) : hurtboxes sur les os, lame animée (M5-05, M5-06).
  add_test(NAME launcher.combat_rig COMMAND launcher --combat-rig-selftest
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.combat_rig PROPERTIES
    ENVIRONMENT "${LAUNCHER_ENV}"
    PASS_REGULAR_EXPRESSION "AUTOTEST COMBAT : OK \\([0-9]+ mesures, [0-9]+ ticks, rig animé\\)")
  # M6-03 : le camp sur un relief marqué (60 m, longueur d'onde 100 m : la moitié des cellules
  # dépasse 45°, les 3 postes du camp tombent sur une pente) : postes déplacés, îlots retirés, les
  # ennemis atteignent le joueur sans jamais remonter sur une pente.
  if(UNIX)
    add_test(NAME launcher.camp_terrain
             COMMAND sh -c "\"$1\" heightmap \"$3\" --size 257 --spacing 2 --alt-max 60 --wavelength 100 && \"$2\" --combat-terrain-selftest \"$3\""
                     sh $<TARGET_FILE:assetc> $<TARGET_FILE:launcher> ${CMAKE_CURRENT_BINARY_DIR}/camp_terrain.hmap
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(launcher.camp_terrain PROPERTIES
      ENVIRONMENT "${LAUNCHER_ENV}"
      PASS_REGULAR_EXPRESSION "AUTOTEST CAMP TERRAIN : OK")
  endif()
  # Galerie des états d'animation (M4) : personnages cuits chargés sur le GPU et dessinés.
  add_test(NAME launcher.avatar_gallery COMMAND launcher --avatar-gallery --frames 10
           --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.avatar_gallery PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite"
    PASS_REGULAR_EXPRESSION "5/5 corps, 9/9 armes")
  if(OW_SOFTWARE_VULKAN_ICD) # comme les autres tests GPU (voir plus haut)
    set_property(TEST launcher.avatar_gallery APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
  endif()
  # M8-07 : l'éclairage par image est recuit pendant le jeu quand le soleil bouge (journée de 30 s :
  # le soleil avance de 12° par seconde) ; au moins une recuisson envoyée.
  add_test(NAME launcher.ibl_rebake COMMAND launcher --combat-autoplay --no-audio --day-length 30
           --frames 400 --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.ibl_rebake PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu"
    PASS_REGULAR_EXPRESSION "éclairage par image : [1-9][0-9]* recuisson")
  if(OW_SOFTWARE_VULKAN_ICD)
    set_property(TEST launcher.ibl_rebake APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
  endif()
  # Combat rendu sur le GPU (M4/M5) : arène en volumes, 5 personnages skinnés avec leurs ombres ;
  # aucun dessin ne doit être perdu (B11 : la file d'ombre en perdait 16 par image).
  add_test(NAME launcher.combat_render COMMAND launcher --combat-autoplay --no-audio --frames 60
           --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.combat_render PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|allocation\\(s\\) pendant l'image"
    PASS_REGULAR_EXPRESSION "renderer : 60 image\\(s\\), 60 passe\\(s\\) d'ombre")
  if(OW_SOFTWARE_VULKAN_ICD)
    set_property(TEST launcher.combat_render APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
  endif()
endif()

# Combat RENDU sur un terrain (B8) : arbres et rochers KayKit placés (clairière au centre), ombres en
# cascades (personnages, arbres) ; aucun dessin perdu ni rejeté.
if(OW_SHADERS_OK)
  add_test(NAME launcher.combat_terrain_render
           COMMAND sh -c "\"$1\" heightmap \"$3\" --size 257 --spacing 2 --alt-max 25 --wavelength 500 && \"$2\" --combat-autoplay --terrain \"$3\" --placement --no-audio --frames 60 --env-samples 16"
                   sh $<TARGET_FILE:assetc> $<TARGET_FILE:launcher> ${CMAKE_CURRENT_BINARY_DIR}/combat_terrain.hmap
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.combat_terrain_render PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|dessin ignoré|inaccessible|allocation\\(s\\) pendant l'image"
    PASS_REGULAR_EXPRESSION "arbres \\(KayKit\\).*ombres : 4 cascades.*renderer : 60 image\\(s\\), 60 passe\\(s\\) d'ombre")
  if(OW_SOFTWARE_VULKAN_ICD)
    set_property(TEST launcher.combat_terrain_render APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
  endif()
endif()

# B18 : la capture d'écran passe par une texture à nous (la swapchain ne se relit pas) : aucune
# erreur de validation (couches de validation actives en Debug), image écrite.
if(OW_SHADERS_OK)
  add_test(NAME launcher.screenshot COMMAND launcher --frames 6 --no-audio --env-samples 16
           --screenshot ${CMAKE_CURRENT_BINARY_DIR}/screenshot_test.ppm 3
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.screenshot PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|Validation Error|non détruite"
    PASS_REGULAR_EXPRESSION "capture : .*screenshot_test.ppm")
  if(OW_SOFTWARE_VULKAN_ICD)
    set_property(TEST launcher.screenshot APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
  endif()
endif()

# B19 : le plein écran enregistré dans options.txt est appliqué au lancement (il était écrasé par
# l'état de la fenêtre avant d'être lu). Options isolées dans un dossier du test.
if(OW_SHADERS_OK)
  add_test(NAME launcher.options_fullscreen
           COMMAND sh -c "rm -rf \"$1\" && mkdir -p \"$1/RedStrings/game\" && printf 'graphics.fullscreen = 1\\n' > \"$1/RedStrings/game/options.txt\" && XDG_DATA_HOME=\"$1\" \"$2\" --combat --new-game --no-audio --frames 3 --env-samples 16"
                   sh ${CMAKE_CURRENT_BINARY_DIR}/options_fullscreen $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.options_fullscreen PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|plein écran non"
    PASS_REGULAR_EXPRESSION "options graphiques : .*plein écran oui")
  if(OW_SOFTWARE_VULKAN_ICD)
    set_property(TEST launcher.options_fullscreen APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
  endif()
endif()

# Grande carte : zones d'ennemis dans le monde ouvert. (1) départ devant la zone 0 (avant-poste de
# la plaine) : elle se charge et ses ennemis s'engagent ; (2) traversée à 200 m/s : les zones se
# chargent dans l'ordre du chemin, l'origine est recentrée et le camp reste à sa place.
if(OW_PHYSICS AND OW_SHADERS_OK)
  add_test(NAME launcher.world_combat COMMAND launcher --world 7 --combat-autoplay --at-zone 0
           --no-audio --frames 500 --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_combat PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|TROU"
    PASS_REGULAR_EXPRESSION "zone 0 « Plaine : avant-poste des soldats » chargée.*monde : combat : [1-9][0-9]* ennemi\\(s\\) engagé")
  add_test(NAME launcher.world_combat_shift COMMAND launcher --world 7 --combat-autoplay
           --world-autopilot 200 --no-audio --frames 240 --env-samples 16
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_combat_shift PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|TROU|a bougé"
    PASS_REGULAR_EXPRESSION "carte dessinée à la main.*château de départ.*zone 2 « Plaine : château du chevalier » chargée")
  # Eau et lave de la carte : au milieu du grand lac le joueur est à gué (au sol sur le fond de gué,
  # dans l'eau) ; sur l'anneau de lave du cratère, la lave le brûle.
  # ATTENTION : jamais de point-virgule dans une expression attendue : CMake y coupe la liste, et
  # UNE seule des deux moitiés suffirait à faire passer le test (un « . » le remplace).
  add_test(NAME launcher.world_water COMMAND launcher --world 7 --combat --new-game
           --at-pos 1250 780 --no-audio --frames 60 --env-samples 16
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_water PROPERTIES
    PASS_REGULAR_EXPRESSION "eau au niveau 3.0 m.*MONDE : joueur au sol.*MONDE : eau : [1-9][0-9]* cellules mouillées . joueur dans l'eau, à gué")
  add_test(NAME launcher.world_lava COMMAND launcher --world 7 --combat --new-game
           --at-pos 1350 -1245 --no-audio --frames 60 --env-samples 16
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_lava PROPERTIES
    PASS_REGULAR_EXPRESSION "combat : la lave brûle le joueur")
  # Décor de la carte (2026-11-22) : au château de départ, des buissons et des rochers chargés et
  # dessinés autour de la caméra, de l'herbe, et aucune touffe sous les murs du château (des
  # touffes écartées) ; devant la zone 3 (dans la forêt de sapins), les troncs proches sont donnés
  # aux ennemis du camp, qui les évitent (engine/ai_avoid.h).
  add_test(NAME launcher.world_grass COMMAND launcher --world 7 --combat --new-game
           --no-audio --frames 40 --env-samples 16
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_grass PROPERTIES
    PASS_REGULAR_EXPRESSION "herbe : [1-9][0-9]* touffes autour de la caméra.*MONDE : décor : [1-9][0-9]* buisson\\(s\\) et rocher\\(s\\) chargé\\(s\\), [1-9][0-9]* dessiné\\(s\\) autour de la caméra . herbe : [1-9][0-9][0-9][0-9]+ touffe\\(s\\), [1-9][0-9]* écartée")
  # Sauvegarde dans le monde ouvert (2026-11-22) : devant la zone 7 (désert, à 1,5 km du départ),
  # la partie est enregistrée, le joueur renvoyé au château, puis la partie reprise : il revient à
  # sa position MONDE (à 0,5 m près), au sol, alors que l'origine a été recentrée entre-temps.
  add_test(NAME launcher.world_save COMMAND launcher --world 7 --combat --new-game --at-zone 7
           --no-audio --frames 30 --env-samples 16 --save-selftest
           --save-dir ${CMAKE_CURRENT_BINARY_DIR}/saves-world
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_save PROPERTIES
    PASS_REGULAR_EXPRESSION "carte de la partie : lieu « Désert », 4 boss . au château : « Château ».*AUTOTEST SAUVEGARDE MONDE : OK.*MONDE : joueur au sol")
  add_test(NAME launcher.world_trees_avoid COMMAND launcher --world 7 --combat --new-game
           --at-zone 3 --no-audio --frames 35 --env-samples 16
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_trees_avoid PROPERTIES
    PASS_REGULAR_EXPRESSION "monde : [1-9][0-9]* tronc\\(s\\) et rocher\\(s\\) à éviter pour les ennemis du camp")
  # Écran de chargement animé (2026-11-22). 1) Le chargement réel avec l'écran : les étapes longues
  # s'exécutent dans le fil de chargement PENDANT que le fil principal anime l'écran (des images
  # sont présentées pendant le chargement), puis le jeu tourne (joueur au sol), sans erreur ni
  # ressource perdue. 2) Les images de l'animation, mesurées : fondu, fil, titre, corde, sortie.
  add_test(NAME launcher.splash COMMAND launcher --world 7 --combat --new-game --no-audio
           --splash --frames 3 --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.splash PROPERTIES
    PASS_REGULAR_EXPRESSION "écran de chargement : [1-9][0-9]* image\\(s\\) dont [1-9][0-9]* pendant le chargement \\(4 travaux dans le fil de chargement.*MONDE : joueur au sol")
  add_test(NAME launcher.splash_shots COMMAND launcher --splash-shots ${CMAKE_CURRENT_BINARY_DIR}
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.splash_shots PROPERTIES
    PASS_REGULAR_EXPRESSION "AUTOTEST CHARGEMENT : OK")
  # Écran-titre et gestion des parties (2026-11-22), de bout en bout, le menu piloté touche par
  # touche (--menu-script, un tick par image). 1) Avec la scène du titre : créer la partie « Aube »,
  # jouer, Échap, « Sauvegarder et quitter », puis la renommer, la dupliquer et supprimer la copie.
  # 2) Sans scène (pas d'écran de chargement) : créer « Brume », jouer, puis de VRAIS événements
  # Échap (ils passent par la plateforme) : pause, « Sauvegarder », reprendre, pause de nouveau :
  # le jeu ne se ferme pas et finit en pause. La partie est créée en mode créatif (flèches sur la
  # ligne « Mode de jeu »). Dossiers de parties propres au test. ATTENTION : pas de point-virgule dans le texte attendu (CMake).
  add_test(NAME launcher.title_menu
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --combat --no-audio --splash --frames 380 --env-samples 16 --save-dir \"$1\" --menu-script '40:ok,50:text=Aube,60:ok,150:back,160:down,164:down,168:down,172:ok,230:right,236:ok,250:ok,256:down,260:ok,266:text=Zénith,272:ok,280:down,284:ok,300:ok,306:down,310:down,314:down,318:ok,322:down,326:ok,340:back'"
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-title $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.title_menu PROPERTIES TIMEOUT 600
    PASS_REGULAR_EXPRESSION "nouvelle partie « Aube » dans l'emplacement 1.*MENU : écran 1, ligne 1 . 1 emplacement\\(s\\) occupé\\(s\\) . partie en cours « Zénith » . scène du titre : [1-9][0-9][0-9] image")
  add_test(NAME launcher.menu_plain
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --combat --no-audio --frames 90 --env-samples 16 --save-dir \"$1\" --menu-script '10:ok,14:text=Brume,15:up,16:right,17:down,18:ok,40:esc,46:down,50:ok,60:esc,66:esc,72:down,76:down'"
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-plain $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.menu_plain PROPERTIES
    PASS_REGULAR_EXPRESSION "nouvelle partie « Brume » \\(mode créatif\\) dans l'emplacement 1.*MENU : écran 2, ligne 2 . 1 emplacement\\(s\\) occupé\\(s\\) . partie en cours « Brume » en mode créatif \\(en jeu\\) . scène du titre : 0 image")
  # --new-game --creative : la partie lancée sans passer par le menu est en mode créatif.
  add_test(NAME launcher.creative_flag
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --combat --new-game --creative --no-audio --frames 5 --env-samples 16 --save-dir \"$1\""
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-creative $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.creative_flag PROPERTIES
    PASS_REGULAR_EXPRESSION "mode de jeu : créatif.*MENU : écran 0, ligne [0-9] . 0 emplacement\\(s\\) occupé\\(s\\) . partie en cours « Partie 1 » en mode créatif \\(en jeu\\)")
  # 3) À la souris (fenêtre de 1280 x 720) : survoler « Parties » la choisit, le clic l'ouvre ; un
  # clic sur la première carte (libre) ouvre la saisie du nom ; le clavier écrit (de vrais événements
  # de texte, par la plateforme), Entrée crée.
  add_test(NAME launcher.menu_mouse
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --combat --no-audio --frames 90 --env-samples 16 --save-dir \"$1\" --menu-script '10:mouse=0.413/0.895,14:click,30:mouse=0.306/0.52,34:click,40:type=Souris,46:ok'"
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-mouse $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.menu_mouse PROPERTIES
    PASS_REGULAR_EXPRESSION "nouvelle partie « Souris » dans l'emplacement 1.*MENU : écran 0, ligne [0-9] . 1 emplacement\\(s\\) occupé\\(s\\) . partie en cours « Souris » \\(en jeu\\)")
  # Demande de Théo du 2026-11-22 (soir) : vitesse de vol du mode créatif réglable de 0,1 à
  # 1000 m/s. La molette (de vrais crans, par la plateforme) : trois crans, puis un cran en
  # arrière : 25 x 1,25 x 1,25 = 39,06 m/s ; --creative-speed 400 puis deux crans : 625 m/s.
  add_test(NAME launcher.creative_speed
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --combat --new-game --creative --no-audio --frames 14 --env-samples 16 --save-dir \"$1\" --menu-script '5:wheel=3,9:wheel=-1'"
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-speed $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.creative_speed PROPERTIES
    PASS_REGULAR_EXPRESSION "en mode créatif \\(en jeu\\).*CRÉATIF : vitesse de vol 39.06 m/s")
  add_test(NAME launcher.creative_speed_start
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --combat --new-game --creative --creative-speed 400 --no-audio --frames 12 --env-samples 16 --save-dir \"$1\" --menu-script '5:wheel=2'"
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-speed-start $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.creative_speed_start PROPERTIES
    PASS_REGULAR_EXPRESSION "CRÉATIF : vitesse de vol 625.00 m/s")
  # « Plus de petits mobs, pour les farmer » : le pilote automatique devant la meute n° 5 de la
  # carte (zone 17) : cinq osselets, il en vainc, la meute revient. Avec les vrais personnages :
  # l'épée atteint bien ces petits ennemis.
  add_test(NAME launcher.world_pack COMMAND launcher --world 7 --combat-autoplay --at-pack 5
           --no-audio --frames 300 --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_pack PROPERTIES TIMEOUT 900
    PASS_REGULAR_EXPRESSION "12 zones sur la carte et [1-9][0-9] meute\\(s\\) de petits monstres.*zone 17 « Plaine : meute d'osselets » chargée \\(plaine, meute, 5 ennemi\\(s\\), 12 PV.*meute vaincue, elle revient.*monde : combat : [1-9][0-9]* ennemi\\(s\\) engagé\\(s\\) au plus, [0-9]+ coup\\(s\\) reçu\\(s\\), [1-9][0-9]* ennemi\\(s\\) vaincu")
  # Le dragon, dix fois plus grand, et qui vole : devant son antre (zone 11, à 150 m), 87 s de jeu
  # jouées d'un coup (--warp) : 35 s au sol, il décolle, fait ses passages, revient se poser.
  add_test(NAME launcher.world_dragon
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --world 7 --combat --new-game --creative --at-zone 11 --no-audio --frames 8 --warp 5200 --env-samples 16 --save-dir \"$1\""
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-dragon $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_dragon PROPERTIES TIMEOUT 900
    PASS_REGULAR_EXPRESSION "joueur à 22[0-9].[0-9] m d'altitude.*zone 11 « Volcan : antre du dragon » chargée \\(volcan, boss, 1 ennemi.*le Dragon de lave décolle.*le Dragon de lave se pose")
  # On le voit de loin : à 400 m de l'antre (au nord-est, hors de toute autre zone), sa zone est
  # chargée (portée de 420 m) et le décor de l'antre est posé.
  add_test(NAME launcher.world_dragon_far
           COMMAND sh -c "rm -rf \"$1\" && \"$2\" --world 7 --combat --new-game --creative --at-pos 1633 -1633 --no-audio --frames 12 --env-samples 16 --save-dir \"$1\""
                   sh ${CMAKE_CURRENT_BINARY_DIR}/saves-dragon-far $<TARGET_FILE:launcher>
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_dragon_far PROPERTIES TIMEOUT 600
    PASS_REGULAR_EXPRESSION "décor posé : 22 boîte\\(s\\) \\(zone 11\\).*zone 11 « Volcan : antre du dragon » chargée")
  foreach(t launcher.world_water launcher.world_lava launcher.world_grass
          launcher.world_trees_avoid launcher.world_save launcher.splash launcher.splash_shots
          launcher.title_menu launcher.menu_plain launcher.menu_mouse launcher.creative_flag
          launcher.creative_speed launcher.creative_speed_start launcher.world_pack
          launcher.world_dragon launcher.world_dragon_far)
    set_property(TEST ${t} PROPERTY ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
    set_property(TEST ${t} PROPERTY FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|TROU")
  endforeach()
  # Décor des zones : devant la zone 2, le château du chevalier (51 boîtes) est posé sur le relief ;
  # le château de départ, à plus de 300 m, ne l'est pas.
  add_test(NAME launcher.world_decor COMMAND launcher --world 7 --combat --at-zone 2
           --no-audio --frames 20 --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_decor PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|WARN  play_decor|non détruite|perdu|TROU"
    PASS_REGULAR_EXPRESSION "décor posé : 51 boîte\\(s\\) \\(zone 2\\)")
  # Carte du monde (sol à six matières, arbres, vue lointaine) : devant la zone 0 (plaine), ni sable
  # ni lave, des chênes ; devant la zone 7 (désert), du sable et des acacias ; devant la zone 3
  # (montagne), ni sable ni lave, des sapins chargés, dessinés et solides ; devant la zone 11
  # (volcan), de la lave, aucun arbre, la vue lointaine dessinée.
  add_test(NAME launcher.world_layers_plain COMMAND launcher --world 7 --combat --at-zone 0
           --no-audio --frames 20 --env-samples 16
           --screenshot ${CMAKE_CURRENT_BINARY_DIR}/world_layers_plain.ppm 15
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_layers_plain PROPERTIES
    PASS_REGULAR_EXPRESSION "couches de biome : 0 cellule\\(s\\) avec sable, 0 avec lave.*carte du monde : [1-9][0-9]* arbre")
  # ... et devant la zone 7 (désert) le sol dessiné est du sable (dominante chaude, pas verte).
  add_test(NAME launcher.world_layers_desert COMMAND launcher --world 7 --combat --at-zone 7
           --no-audio --frames 20 --env-samples 16
           --screenshot ${CMAKE_CURRENT_BINARY_DIR}/world_layers_desert.ppm 15
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_layers_desert PROPERTIES
    PASS_REGULAR_EXPRESSION "dominante chaude.*couches de biome : [1-9][0-9]* cellule\\(s\\) avec sable.*carte du monde : [1-9][0-9]* arbre")
  add_test(NAME launcher.world_layers_mountain COMMAND launcher --world 7 --combat --at-zone 3
           --no-audio --frames 20 --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_layers_mountain PROPERTIES
    PASS_REGULAR_EXPRESSION "couches de biome : 0 cellule\\(s\\) avec sable, 0 avec lave.*carte du monde : [1-9][0-9]* arbre\\(s\\) chargé\\(s\\), [1-9][0-9]* dessiné\\(s\\), [1-9][0-9]* tronc")
  # (120 images et « joueur au sol » : à 215 m d'altitude, un relief écrêté ferait tomber le joueur)
  # Départ dans le cratère, à 34 m du centre de l'antre (là où --at-zone 11 posait le joueur avant
  # que le dragon devienne colossal : --at-zone 11 commence maintenant à 150 m, sur la pente).
  add_test(NAME launcher.world_layers_volcano COMMAND launcher --world 7 --combat --new-game
           --at-pos 1326 -1326 --no-audio --frames 120 --env-samples 16
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_layers_volcano PROPERTIES
    PASS_REGULAR_EXPRESSION "joueur à 21[0-9].[0-9] m d'altitude.*avec sable, [1-9][0-9]* avec lave.*carte du monde : 0 arbre.*vue lointaine : [1-9][0-9]* morceau.*MONDE : joueur au sol")
  foreach(t launcher.world_layers_plain launcher.world_layers_desert launcher.world_layers_mountain
          launcher.world_layers_volcano)
    set_property(TEST ${t} PROPERTY ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
    set_property(TEST ${t} PROPERTY FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|TROU")
  endforeach()
  if(OW_SOFTWARE_VULKAN_ICD)
    foreach(t launcher.world_combat launcher.world_combat_shift launcher.world_decor
            launcher.world_water launcher.world_lava launcher.world_grass
            launcher.world_trees_avoid launcher.world_save launcher.splash
            launcher.splash_shots launcher.title_menu launcher.menu_plain launcher.menu_mouse
            launcher.creative_flag launcher.creative_speed launcher.creative_speed_start
            launcher.world_pack launcher.world_dragon launcher.world_dragon_far
            launcher.world_layers_plain
            launcher.world_layers_desert launcher.world_layers_mountain
            launcher.world_layers_volcano)
      set_property(TEST ${t} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
    endforeach()
  endif()
endif()

# Coût de chaque effet de post-process (finition M8-06) : la mesure parcourt ses 7 configurations
# (FXAA, bloom, SSAO, échelles 75 et 50 %) et quitte d'elle-même.
if(OW_SHADERS_OK)
  add_test(NAME launcher.post_bench COMMAND launcher --post-bench 5 --no-audio --env-samples 16
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.post_bench PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu"
    PASS_REGULAR_EXPRESSION "mesure du post-process : 7 configurations.*tout, échelle 50 %")
  if(OW_SOFTWARE_VULKAN_ICD)
    set_property(TEST launcher.post_bench APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
  endif()
endif()

# Monde ouvert RENDU (M7, --world) : cellules streamées envoyées au GPU, corps Jolt par cellule,
# origine flottante (traversée de 1 km au moins), personnage posé sur le relief à l'arrêt.
if(OW_PHYSICS AND OW_SHADERS_OK)
  add_test(NAME launcher.world_render COMMAND launcher --world 7 --world-autopilot 200 --frames 240
           --no-audio --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_render PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|TROU"
    PASS_REGULAR_EXPRESSION "MONDE : [0-9]+ m parcourus.* [1-9][0-9]* recentrage")
  add_test(NAME launcher.world_ground COMMAND launcher --world 7 --frames 240 --no-audio
           --env-samples 16 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.world_ground PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    FAIL_REGULAR_EXPRESSION "ERROR|non détruite|perdu|TROU"
    PASS_REGULAR_EXPRESSION "MONDE : joueur au sol")
  if(OW_SOFTWARE_VULKAN_ICD)
    foreach(t launcher.world_render launcher.world_ground)
      set_property(TEST ${t} APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "VK_DRIVER_FILES=set:${OW_SOFTWARE_VULKAN_ICD}" "VK_ICD_FILENAMES=set:${OW_SOFTWARE_VULKAN_ICD}")
    endforeach()
  endif()
endif()

# Monde ouvert (M7-14) : traversée de 2 km à 30 m/s, budget de 25 ms par image, aucun trou sous le
# joueur (--world-bench 2). Sans fenêtre ni GPU.
add_test(NAME launcher.world_bench COMMAND launcher --world-bench 2
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.world_bench PROPERTIES
  ENVIRONMENT "${LAUNCHER_ENV}"
  PASS_REGULAR_EXPRESSION "BENCHMARK MONDE : OK")

# Personnage sur le terrain procédural (raccord M3 + M4) : un champ de hauteurs de 257² (512 m, 40 m de relief) généré par
# assetc, collision Jolt, le personnage s'y pose, le suit en marchant, caméra au-dessus du sol.
if(OW_PHYSICS AND UNIX)
  add_test(NAME launcher.play_terrain
           # chemins passés en arguments positionnels : le dépôt peut contenir des espaces
           COMMAND sh -c "\"$1\" heightmap \"$3\" --size 257 --spacing 2 --alt-max 40 --wavelength 300 && \"$2\" --play-terrain-selftest \"$3\""
                   sh $<TARGET_FILE:assetc> $<TARGET_FILE:launcher> ${CMAKE_CURRENT_BINARY_DIR}/play_terrain.hmap
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.play_terrain PROPERTIES
    ENVIRONMENT "${LAUNCHER_ENV}"
    PASS_REGULAR_EXPRESSION "AUTOTEST TERRAIN : OK")
endif()

# Partie complète (M8) : menu principal, nouvelle partie, sauvegarde, menu principal, relance,
# Continuer : position, PV, ennemis et options restitués (--game-selftest). Demande Jolt.
if(OW_PHYSICS)
  add_test(NAME launcher.game COMMAND launcher --game-selftest
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.game PROPERTIES
    ENVIRONMENT "${LAUNCHER_ENV}"
    PASS_REGULAR_EXPRESSION "AUTOTEST PARTIE : OK")
endif()

# Rapport de crash (M9-03) : plantage volontaire, le rapport doit contenir le signal et la pile.
# Sous ASan, ses propres gestionnaires sont coupés pour que celui du jeu s'exécute.
if(UNIX AND EXISTS ${CMAKE_SOURCE_DIR}/engine/src/crash.c)
  add_test(NAME launcher.crash_report
           COMMAND sh -c "rm -f $<TARGET_FILE_DIR:launcher>/logs/crash-*.txt; $<TARGET_FILE:launcher> --crash-test; grep -q SIGSEGV $<TARGET_FILE_DIR:launcher>/logs/crash-*.txt && grep -q 'pile' $<TARGET_FILE_DIR:launcher>/logs/crash-*.txt && echo CRASH_REPORT_OK"
           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(launcher.crash_report PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;ASAN_OPTIONS=handle_segv=0:handle_abort=0"
    PASS_REGULAR_EXPRESSION "CRASH_REPORT_OK")
endif()
