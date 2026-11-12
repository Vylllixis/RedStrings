include(FetchContent)

# Sources partagées par tous les presets (téléchargées une seule fois, ignorées par git)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party/_deps" CACHE PATH "" FORCE)
# Ne pas contacter le réseau à chaque configuration une fois les sources présentes
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# ── SDL3 (release-3.4.18, commit 829a65d) ──
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.4.18
  GIT_SHALLOW    ON)

# ── cglm (v0.9.7, commit 888aca0) : mode en-têtes seuls ──
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cglm
  GIT_REPOSITORY https://github.com/recp/cglm.git
  GIT_TAG        v0.9.7
  GIT_SHALLOW    ON)

# ── cgltf (v1.15, commit bbeb5b0) : un seul en-tête, pas de CMake à utiliser ──
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG        v1.15
  GIT_SHALLOW    ON
  SOURCE_SUBDIR  _none)

# ── stb (pas de tags : commit 003501f figé) ──
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
  SOURCE_SUBDIR  _none)

# ── utest.h (tag « pre-ai », commit 0744717) : framework de tests, un seul en-tête ──
# Choix volontaire : le dernier état avant que le dépôt n'accepte du code écrit avec des outils d'IA.
if(OW_BUILD_TESTS)
  FetchContent_Declare(utest
    GIT_REPOSITORY https://github.com/sheredom/utest.h.git
    GIT_TAG        07447179856c4662d443c6690e3b408b20042e99
    SOURCE_SUBDIR  _none)
  FetchContent_MakeAvailable(utest)
  add_library(utest_headers INTERFACE)
  target_include_directories(utest_headers SYSTEM INTERFACE "${utest_SOURCE_DIR}")
endif()

FetchContent_MakeAvailable(SDL3 cglm cgltf stb)

# ── Tracy (v0.15.0, commit f2a4f25) : profileur, SEULEMENT si OW_PROFILE=ON ──
# Le client est en C++ (API C : TracyC.h). Le visualiseur doit avoir la MÊME version.
# Hors profilage, rien n'est téléchargé ni compilé : ow_profile reste une cible vide.
add_library(ow_profile INTERFACE)
if(OW_PROFILE)
  set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
  set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)      # rien n'est collecté tant qu'aucun visualiseur n'est connecté
  set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE) # n'écoute pas le réseau
  set(TRACY_STATIC ON CACHE BOOL "" FORCE)
  set(TRACY_NO_CRASH_HANDLER ON CACHE BOOL "" FORCE) # évite un conflit avec les gestionnaires d'ASan/UBSan
  set(TRACY_NO_FRAME_IMAGE ON CACHE BOOL "" FORCE)   # pas de capture d'écran : un thread de moins
  FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG        v0.15.0
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(tracy)
  target_link_libraries(ow_profile INTERFACE Tracy::TracyClient)
  target_compile_definitions(ow_profile INTERFACE OW_PROFILE=1)
endif()

# ── Dear ImGui (commit 6d32174, 1.93.0 « docking ») + cimgui (commit 2a6a93f) : OW_UI seulement ──
# cimgui fournit l'API C de Dear ImGui ; il est généré pour UNE version précise d'ImGui (ses fichiers
# ne compilent pas avec une autre) : les deux commits vont donc ensemble et ne doivent pas être
# changés séparément. ImGui est cloné en entier (GIT_SHALLOW est impossible avec un commit), les
# sous-modules de cimgui ne sont pas utilisés (ImGui est récupéré à part). cimgui n'a pas de
# backend SDL_GPU : on utilise directement les backends C++ d'ImGui (SDL3 et SDL_GPU3), pilotés par
# notre couche engine/src/ui_backend.cpp.
add_library(ow_imgui INTERFACE)
if(OW_UI)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        6d32174508fb8c518e7a832cc1d571fadd5cba76
    SOURCE_SUBDIR  _none)
  FetchContent_Declare(cimgui
    GIT_REPOSITORY https://github.com/cimgui/cimgui.git
    GIT_TAG        2a6a93fccd10f275ee9206167dc9698f6f00fac1
    GIT_SUBMODULES ""
    SOURCE_SUBDIR  _none)
  FetchContent_MakeAvailable(imgui cimgui)
  # cimgui.cpp inclut « ./imgui/imgui.h » (ImGui y est un sous-module) : on fournit ces deux
  # en-têtes par redirection plutôt que de copier ou lier ImGui dans le dossier de cimgui.
  # (file(CONFIGURE) ne réécrit le fichier que s'il change : pas de recompilation inutile.)
  set(ow_imgui_stage "${CMAKE_BINARY_DIR}/ow_cimgui_stage")
  file(CONFIGURE OUTPUT "${ow_imgui_stage}/imgui/imgui.h"
       CONTENT "#include \"${imgui_SOURCE_DIR}/imgui.h\"\n" @ONLY NEWLINE_STYLE UNIX)
  file(CONFIGURE OUTPUT "${ow_imgui_stage}/imgui/imgui_internal.h"
       CONTENT "#include \"${imgui_SOURCE_DIR}/imgui_internal.h\"\n" @ONLY NEWLINE_STYLE UNIX)
  add_library(ow_imgui_lib STATIC
    "${cimgui_SOURCE_DIR}/cimgui.cpp"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp" # vide avec IMGUI_DISABLE_DEMO_WINDOWS, mais cimgui y renvoie
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp")
  set_target_properties(ow_imgui_lib PROPERTIES POSITION_INDEPENDENT_CODE ON
                        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  target_include_directories(ow_imgui_lib PRIVATE "${ow_imgui_stage}")
  target_include_directories(ow_imgui_lib SYSTEM PUBLIC
    "${cimgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
  # Pas de fenêtres de démonstration ni d'outils de debug d'ImGui (code inutile). OBSOLETE : cimgui
  # est généré sans les fonctions obsolètes. CIMGUI_NO_EXPORT : pas de __declspec(dllexport).
  target_compile_definitions(ow_imgui_lib PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
    IMGUI_DISABLE_DEMO_WINDOWS CIMGUI_NO_EXPORT)
  target_link_libraries(ow_imgui_lib PUBLIC SDL3::SDL3)
  if(MSVC)
    target_compile_options(ow_imgui_lib PRIVATE /W0 /utf-8)
  endif()
  target_link_libraries(ow_imgui INTERFACE ow_imgui_lib)
  target_compile_definitions(ow_imgui INTERFACE OW_UI=1)
endif()

add_library(cgltf_headers INTERFACE)
target_include_directories(cgltf_headers INTERFACE "${cgltf_SOURCE_DIR}")
add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")

# ── Chaîne de compilation des shaders : SDL_shadercross + SPIRV-Cross + DXC ──
# HLSL -> SPIR-V / DXIL / MSL, hors ligne, par l'exécutable `shadercross` (voir cmake/Shaders.cmake).
#  - DXC : archives officielles de Microsoft (v1.9.2609), figées par somme de contrôle.
#  - SPIRV-Cross : tag vulkan-sdk-1.4.363.0, compilé ici en bibliothèque partagée.
#  - SDL_shadercross : commit 1ff05be (3 sept. 2026, aucune version publiée), compilé ici.
# Plateformes prises en charge : Linux x86_64 et Windows (comme la CI). Ailleurs : désactivé.
set(OW_SHADERS_OK OFF)
if(OW_BUILD_SHADERS)
  if(WIN32)
    set(ow_dxc_url "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/dxc_2026_09_29.zip")
    set(ow_dxc_hash "ad31b1fc8443175d204f77a611fdb3ef2ec42759bdc2f1167368de24a4a7e7f1")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(ow_dxc_url "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/linux_dxc_2026_09_28.x86_x64.tar.gz")
    set(ow_dxc_hash "96faadc7f5c282d2ffda49804beb4c3ee38127bc252b723234e3c5cdf7aa39a1")
  else()
    message(WARNING "OW_BUILD_SHADERS : plateforme non prise en charge (Linux x86_64 et Windows seulement), shaders désactivés")
  endif()
endif()
if(OW_BUILD_SHADERS AND DEFINED ow_dxc_url)
  set(OW_SHADERS_OK ON)
  FetchContent_Declare(dxc URL ${ow_dxc_url} URL_HASH SHA256=${ow_dxc_hash} DOWNLOAD_EXTRACT_TIMESTAMP ON)
  FetchContent_MakeAvailable(dxc)

  set(SPIRV_CROSS_SHARED ON CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_STATIC OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_SKIP_INSTALL OFF CACHE BOOL "" FORCE) # nécessaire : shadercross exporte ses cibles
  FetchContent_Declare(spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG        vulkan-sdk-1.4.363.0
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(spirv_cross)
  # shadercross cherche la cible sous ce nom (celui du paquet CMake installé)
  add_library(spirv_cross_c_shared ALIAS spirv-cross-c-shared)

  # shadercross redéfinit DirectXShaderCompiler_ROOT vers son propre dossier : on renseigne donc
  # directement les résultats de ses find_*() (variables de cache), qui sont alors ignorés.
  if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
      set(ow_dxc_arch "arm64")
    else()
      set(ow_dxc_arch "x64")
    endif()
    set(DirectXShaderCompiler_INCLUDE_PATH "${dxc_SOURCE_DIR}/inc" CACHE PATH "" FORCE)
    set(DirectXShaderCompiler_dxcompiler_BINARY "${dxc_SOURCE_DIR}/bin/${ow_dxc_arch}/dxcompiler.dll" CACHE FILEPATH "" FORCE)
    set(DirectXShaderCompiler_dxcompiler_LIBRARY "${dxc_SOURCE_DIR}/lib/${ow_dxc_arch}/dxcompiler.lib" CACHE FILEPATH "" FORCE)
    set(DirectXShaderCompiler_dxil_BINARY "${dxc_SOURCE_DIR}/bin/${ow_dxc_arch}/dxil.dll" CACHE FILEPATH "" FORCE)
  else()
    set(DirectXShaderCompiler_INCLUDE_PATH "${dxc_SOURCE_DIR}/include/dxc" CACHE PATH "" FORCE)
    set(DirectXShaderCompiler_dxcompiler_LIBRARY "${dxc_SOURCE_DIR}/lib/libdxcompiler.so" CACHE FILEPATH "" FORCE)
    set(DirectXShaderCompiler_dxil_LIBRARY "${dxc_SOURCE_DIR}/lib/libdxil.so" CACHE FILEPATH "" FORCE)
  endif()
  set(SDLSHADERCROSS_VENDORED OFF CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_DXC ON CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_SHARED OFF CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_STATIC ON CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_SPIRVCROSS_SHARED ON CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_CLI ON CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_CLI_STATIC ON CACHE BOOL "" FORCE)
  set(SDLSHADERCROSS_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(shadercross
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
    GIT_TAG        1ff05bec573988a98ef9e0260b4da44f512b8367)
  FetchContent_MakeAvailable(shadercross)
  if(WIN32)
    # Sous Windows les DLL de DXC doivent se trouver à côté de shadercross.exe (pas de RPATH).
    add_custom_target(ow_shadercross_runtime
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DirectXShaderCompiler_dxcompiler_BINARY} ${DirectXShaderCompiler_dxil_BINARY}
        $<TARGET_FILE_DIR:shadercross>
      DEPENDS shadercross)
  endif()
endif()

# Rechargement à chaud des shaders sous Windows : DXC et SPIRV-Cross sont des DLL, qui doivent
# se trouver à côté de l'exécutable (pas de RPATH). À appeler depuis le dossier de la cible.
# Sous Linux le RPATH de la build suffit : fonction sans effet.
function(ow_copy_shader_runtime target)
  if(WIN32 AND OW_SHADERS_OK AND OW_SHADER_HOT_RELOAD)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DirectXShaderCompiler_dxcompiler_BINARY} ${DirectXShaderCompiler_dxil_BINARY}
        $<TARGET_FILE:spirv-cross-c-shared> $<TARGET_FILE_DIR:${target}>)
  endif()
endfunction()

# Visible par le CMake racine (ce fichier est un sous-dossier) : décide de la cible « shaders » et des tests.
set(OW_SHADERS_OK ${OW_SHADERS_OK} PARENT_SCOPE)
