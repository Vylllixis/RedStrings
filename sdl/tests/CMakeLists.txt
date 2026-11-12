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
ow_add_test(clock test_clock.c)
ow_add_test(loop test_loop.c)
ow_add_test(input test_input.c)
ow_add_test(files test_files.c)
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
ow_add_test(shadow_gpu test_shadow_gpu.c)
ow_copy_shader_runtime(test_shadow_gpu)
ow_add_test(environment_gpu test_environment_gpu.c)
ow_copy_shader_runtime(test_environment_gpu)
ow_add_test(tonemap_gpu test_tonemap_gpu.c)
ow_copy_shader_runtime(test_tonemap_gpu)
ow_add_test(ui test_ui.c)
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
        engine.texture engine.model_gpu engine.shadow_gpu engine.environment_gpu engine.tonemap_gpu engine.ui engine.debug_lines_gpu launcher.offscreen launcher.shadow launcher.resize_1 launcher.resize_2 launcher.resize_3
        launcher.ui launcher.texture_default launcher.texture_no_mipmaps launcher.texture_anisotropy_4)
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
