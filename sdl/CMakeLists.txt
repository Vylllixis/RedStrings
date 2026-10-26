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

add_library(project_options INTERFACE)
if(MSVC)
  target_compile_options(project_options INTERFACE /W4 /WX)
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

add_subdirectory(third_party)
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(launcher)
add_subdirectory(tools)
