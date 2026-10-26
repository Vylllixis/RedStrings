include(FetchContent)

# Sources partagées par tous les presets (téléchargées une seule fois, ignorées par git)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party/_deps" CACHE PATH "" FORCE)
# Ne pas contacter le réseau à chaque configuration une fois les sources présentes
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# ── SDL3 : release-3.4.18 (commit 829a65d) ──
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.4.18
  GIT_SHALLOW    ON)

# ── cglm : v0.9.7 (commit 888aca0), mode en-têtes seuls ──
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cglm
  GIT_REPOSITORY https://github.com/recp/cglm.git
  GIT_TAG        v0.9.7
  GIT_SHALLOW    ON)

# ── cgltf : v1.15 (commit 360db1a), en-tête unique, on n'utilise pas son CMake ──
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG        v1.15
  GIT_SHALLOW    ON
  SOURCE_SUBDIR  _none)

# ── stb : pas de tags, commit 003501f figé ──
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
  SOURCE_SUBDIR  _none)

FetchContent_MakeAvailable(SDL3 cglm cgltf stb)

add_library(cgltf_headers INTERFACE)
target_include_directories(cgltf_headers INTERFACE "${cgltf_SOURCE_DIR}")
add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")
