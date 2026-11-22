# Test de la cuisson au build (M2-04) : lancé par `cmake -P`, il configure puis construit le mini-projet
# tests/cook_fixture avec le générateur de la build principale, et vérifie QUAND assetc s'exécute.
#
#   -DOW_ASSETC=<assetc> -DOW_ASSETS_CMAKE=<cmake/Assets.cmake> -DOW_FIXTURE=<tests/cook_fixture>
#   -DOW_ASSETS=<assets/> -DOW_WORK=<dossier de travail> -DOW_GENERATOR=<générateur>
foreach(var OW_ASSETC OW_ASSETS_CMAKE OW_FIXTURE OW_ASSETS OW_WORK OW_GENERATOR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "cook_incremental : ${var} manquant")
  endif()
endforeach()

set(work "${OW_WORK}")
set(build "${work}/build")
set(tool "${work}/outil/assetc")
set(src "${work}/dossier avec espaces/assets") # les espaces font partie du test
set(out "${build}/out")

function(fail message)
  message(FATAL_ERROR "cook_incremental : ${message}")
endfunction()

# Construit ; compte les cuissons (une ligne « Cuisson » par asset) et renvoie la sortie.
function(build_and_count count_var text_var)
  execute_process(COMMAND ${CMAKE_COMMAND} --build ${build}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text_err)
  set(text "${text}${text_err}")
  if(NOT rc EQUAL 0)
    fail("la build échoue :\n${text}")
  endif()
  string(REGEX MATCHALL "Cuisson [^\n]*" lines "${text}")
  list(LENGTH lines n)
  set(${count_var} ${n} PARENT_SCOPE)
  set(${text_var} "${text}" PARENT_SCOPE)
endfunction()

# Attend exactement `expected` cuissons, dont les noms sont `names` (dans un ordre quelconque).
function(expect_cooked step expected)
  build_and_count(n text)
  if(NOT n EQUAL expected)
    fail("${step} : ${n} cuisson(s) au lieu de ${expected} :\n${text}")
  endif()
  foreach(name IN LISTS ARGN)
    if(NOT text MATCHES "Cuisson ${name}")
      fail("${step} : « ${name} » n'a pas été recuit :\n${text}")
    endif()
  endforeach()
  if(expected EQUAL 0 AND NOT text MATCHES "no work to do")
    fail("${step} : ninja aurait dû n'avoir rien à faire :\n${text}")
  endif()
  message(STATUS "ok : ${step} (${n} cuisson(s))")
endfunction()

# Les horodatages des fichiers ont une résolution limitée sur certains systèmes de fichiers.
function(touch_later path)
  execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 1.1)
  file(TOUCH "${path}")
endfunction()

# ── Mise en place ──
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}/outil")
file(COPY "${OW_ASSETC}" DESTINATION "${work}/outil")
file(COPY "${OW_ASSETS}/" DESTINATION "${src}")
file(WRITE "${src}/sans_rapport.txt" "ne fait partie d'aucun asset\n")

execute_process(
  COMMAND ${CMAKE_COMMAND} -S ${OW_FIXTURE} -B ${build} -G ${OW_GENERATOR}
          -DOW_ASSETC=${tool} -DOW_ASSETS_CMAKE=${OW_ASSETS_CMAKE} -DOW_SRC=${src}
  RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text_err)
if(NOT rc EQUAL 0)
  fail("configuration impossible :\n${text}${text_err}")
endif()

# ── Première build : tout est cuit ──
expect_cooked("première build" 2 "models/demo.gltf" "textures/checker.png")
foreach(file models/demo.mesh models/demo.0.tex textures/checker.tex)
  if(NOT EXISTS "${out}/${file}")
    fail("sortie absente : ${file}")
  endif()
endforeach()

# Le fichier de dépendances protège les espaces et nomme le .bin et l'image du glTF.
file(READ "${out}/models/demo.mesh.d" depfile)
foreach(needle "dossier\\ avec\\ espaces" "demo.bin" "checker.png" "demo.gltf")
  string(FIND "${depfile}" "${needle}" at)
  if(at EQUAL -1)
    fail("fichier de dépendances sans « ${needle} » :\n${depfile}")
  endif()
endforeach()

# ── Rien n'a changé : rien n'est refait (le critère d'acceptation) ──
expect_cooked("deuxième build, rien n'a changé" 0)
expect_cooked("troisième build" 0)

# ── Ce qui doit déclencher une cuisson ──
touch_later("${src}/models/demo.bin")
expect_cooked("tampon .bin du glTF modifié" 1 "models/demo.gltf")

touch_later("${src}/textures/checker.png")
expect_cooked("image modifiée : le modèle qui l'utilise est recuit aussi" 2 "models/demo.gltf"
              "textures/checker.png")

touch_later("${src}/models/demo.gltf")
expect_cooked("glTF modifié" 1 "models/demo.gltf")

touch_later("${tool}")
expect_cooked("cuiseur modifié : tout est recuit" 2 "models/demo.gltf" "textures/checker.png")

# ── Ce qui ne doit rien déclencher ──
touch_later("${src}/sans_rapport.txt")
expect_cooked("fichier sans rapport modifié" 0)

# ── Sortie supprimée : refaite ──
file(REMOVE "${out}/textures/checker.tex")
expect_cooked("sortie supprimée" 1 "textures/checker.png")

# ── Texture périmée d'une cuisson précédente : supprimée par assetc ──
file(WRITE "${out}/models/demo.1.tex" "reste d'une cuisson avec plus de textures")
touch_later("${src}/models/demo.gltf")
expect_cooked("glTF modifié, texture périmée" 1 "models/demo.gltf")
if(EXISTS "${out}/models/demo.1.tex")
  fail("demo.1.tex, texture périmée, n'a pas été supprimée")
endif()
if(NOT EXISTS "${out}/models/demo.0.tex")
  fail("demo.0.tex, texture utile, a disparu")
endif()

# ── Échec : pas de .mesh qui paraîtrait à jour, pas de fichier de dépendances ──
file(WRITE "${work}/cassé.gltf" "{ ceci n'est pas un glTF")
file(MAKE_DIRECTORY "${work}/echec")
execute_process(COMMAND ${tool} mesh "${work}/cassé.gltf" "${work}/echec/cassé.mesh"
                        --depfile "${work}/echec/cassé.mesh.d"
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  fail("assetc a accepté un glTF invalide")
endif()
file(GLOB leftovers "${work}/echec/*")
if(leftovers)
  fail("un échec a laissé des fichiers : ${leftovers}")
endif()

# ── Options : RAW (RGBA8) et LINEAR_TEXTURES (données) changent le format cuit ──
# Formats de .tex : 2 = RGBA8_SRGB, 3 = BC7_UNORM, 4 = BC7_SRGB (docs/asset-formats.md).
set(build_variants "${work}/variantes")
execute_process(
  COMMAND ${CMAKE_COMMAND} -S ${OW_FIXTURE} -B ${build_variants} -G ${OW_GENERATOR}
          -DOW_ASSETC=${tool} -DOW_ASSETS_CMAKE=${OW_ASSETS_CMAKE} -DOW_SRC=${src} -DOW_VARIANTS=ON
  RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text_err)
if(NOT rc EQUAL 0)
  fail("configuration des variantes impossible :\n${text}${text_err}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build ${build_variants}
                RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text_err)
if(NOT rc EQUAL 0)
  fail("build des variantes impossible :\n${text}${text_err}")
endif()
function(expect_tex_format file format)
  execute_process(COMMAND ${tool} info "${build_variants}/${file}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text_err)
  if(NOT rc EQUAL 0 OR NOT text MATCHES "contenu valide : format ${format},")
    fail("${file} : format ${format} attendu :\n${text}${text_err}")
  endif()
endfunction()
expect_tex_format("couleur/textures/checker.tex" 4)  # BC7 sRGB : le défaut
expect_tex_format("lineaire/textures/checker.tex" 3) # BC7 linéaire
expect_tex_format("brut/textures/checker.tex" 2)     # RGBA8 sRGB
# le .0.tex du modèle brut est lui aussi en RGBA8 (couleur de base : sRGB)
expect_tex_format("brut/models/demo.0.tex" 2)
message(STATUS "ok : options RAW et LINEAR_TEXTURES")

message(STATUS "cook_incremental : tout est conforme")
