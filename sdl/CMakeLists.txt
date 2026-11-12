cmake_minimum_required(VERSION 3.25)
project(openworld LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Exécutables et bibliothèques dynamiques dans le même dossier (utile au rechargement à chaud)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Options de qualité, appliquées à NOTRE code uniquement (jamais à SDL ni aux dépendances) ──
option(OW_SANITIZE "ASan + UBSan en configuration Debug" ON)
option(OW_BUILD_SHADERS "Compiler les shaders HLSL (SDL_shadercross, DXC, SPIRV-Cross)" ON)
# Recompilation des shaders à l'exécution (M1-03) : lie DXC et SPIRV-Cross au moteur, donc réservée
# par défaut au Debug ; les autres builds chargent les shaders compilés hors ligne.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(ow_hot_default ON)
else()
  set(ow_hot_default OFF)
endif()
option(OW_SHADER_HOT_RELOAD "Recompiler les shaders HLSL modifiés pendant l'exécution" ${ow_hot_default})
option(OW_PROFILE "Profileur Tracy (zones CPU, marqueurs de frame)" OFF)
option(OW_BUILD_TESTS "Compiler les tests unitaires (CTest)" ON)
# Interface de debug (M1-12) : Dear ImGui + cimgui, téléchargés à la configuration (~145 Mo, une
# fois) et compilés en C++. Désactivée : le moteur se compile sans C++ ni ImGui (eng_ui_* sont alors
# des fonctions sans effet).
option(OW_UI "Interface de debug Dear ImGui (cimgui) : panneau de statistiques et réglages" ON)

# Tracy, SPIRV-Cross (outil de compilation des shaders) et Dear ImGui sont en C++ : le compilateur
# C++ n'est requis que si l'une de ces options est active. (enable_language ici, à la racine : un appel
# dans un sous-dossier ne serait pas visible ailleurs.)
if(OW_PROFILE OR OW_BUILD_SHADERS OR OW_UI)
  enable_language(CXX)
endif()

add_library(project_options INTERFACE)
if(MSVC)
  # /utf-8 : sources ET chaînes en UTF-8 (sinon MSVC réinterprète les accents du code source)
  target_compile_options(project_options INTERFACE /W4 /WX /utf-8)
else()
  target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic -Werror)
  if(OW_SANITIZE)
    target_compile_options(project_options INTERFACE
      $<$<CONFIG:Debug>:-fsanitize=address,undefined>
      $<$<CONFIG:Debug>:-fno-sanitize-recover=undefined>
      $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)
    target_link_options(project_options INTERFACE
      $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
  endif()
endif()

# Configuration d'exemple copiée à côté de l'exécutable, SEULEMENT si elle n'y est pas déjà
# (pour ne jamais écraser les réglages de l'utilisateur).
if(NOT EXISTS ${CMAKE_BINARY_DIR}/bin/config.ini)
  file(COPY ${CMAKE_SOURCE_DIR}/config/config.ini DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()

enable_testing()

add_subdirectory(third_party)
if(OW_SHADERS_OK)
  add_subdirectory(shaders)
endif()
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(launcher)
add_subdirectory(tools)
if(OW_BUILD_TESTS)
  add_subdirectory(tests)
endif()
