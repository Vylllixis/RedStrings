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
ow_add_test(frustum test_frustum.c)
ow_add_test(atmosphere test_atmosphere.c)
ow_add_test(fog test_fog.c)
ow_add_test(rock test_rock.c)
ow_add_test(placement test_placement.c)
ow_add_test(grass test_grass.c)
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
