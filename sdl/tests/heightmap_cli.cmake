# Test de la ligne de commande « assetc heightmap » (M3-01) : lancé par `cmake -P`.
#   -DOW_ASSETC=<assetc> -DOW_WORK=<dossier de travail>
foreach(var OW_ASSETC OW_WORK)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "heightmap_cli : ${var} manquant")
  endif()
endforeach()

file(REMOVE_RECURSE "${OW_WORK}")
file(MAKE_DIRECTORY "${OW_WORK}")
set(dir "${OW_WORK}/sous dossier") # un espace dans le chemin fait partie du test

function(fail message)
  message(FATAL_ERROR "heightmap_cli : ${message}")
endfunction()

# Lance assetc ; renvoie le code et la sortie (stdout + stderr) dans les variables nommées.
function(assetc rc_var out_var)
  execute_process(COMMAND ${OW_ASSETC} ${ARGN}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(${rc_var} "${rc}" PARENT_SCOPE)
  set(${out_var} "${out}${err}" PARENT_SCOPE)
endfunction()

# 1. Génération avec options, PNG en prime ; le dossier de sortie est créé.
assetc(rc text heightmap "${dir}/t.hmap" --size 65 --seed 42 --spacing 2 --alt-min -5 --alt-max 95
       --wavelength 100 --octaves 4 --lacunarity 2.5 --gain 0.4 --ridged --png "${dir}/t.png")
if(NOT rc EQUAL 0)
  fail("heightmap échoue (${rc}) : ${text}")
endif()
if(NOT EXISTS "${dir}/t.hmap" OR NOT EXISTS "${dir}/t.png")
  fail("fichiers de sortie absents : ${text}")
endif()
if(NOT text MATCHES "65 x 65" OR NOT text MATCHES "graine 42")
  fail("le compte rendu ne reprend pas les options : ${text}")
endif()

# 2. info relit le fichier et le valide.
assetc(rc text info "${dir}/t.hmap")
if(NOT rc EQUAL 0)
  fail("info refuse le fichier produit : ${text}")
endif()
if(NOT text MATCHES "OWHF version 1" OR NOT text MATCHES "65 x 65 échantillons, espacement 2 m, étendue 128 m, altitudes -5 à 95 m")
  fail("info ne décrit pas le champ attendu : ${text}")
endif()

# 3. Même graine, mêmes octets ; autre graine, autres octets.
assetc(rc text heightmap "${dir}/t2.hmap" --size 65 --seed 42 --spacing 2 --alt-min -5 --alt-max 95
       --wavelength 100 --octaves 4 --lacunarity 2.5 --gain 0.4 --ridged)
if(NOT rc EQUAL 0)
  fail("seconde génération : ${text}")
endif()
file(SHA256 "${dir}/t.hmap" h1)
file(SHA256 "${dir}/t2.hmap" h2)
if(NOT h1 STREQUAL h2)
  fail("mêmes options, fichiers différents")
endif()
assetc(rc text heightmap "${dir}/t3.hmap" --size 65 --seed 43 --spacing 2 --alt-min -5 --alt-max 95
       --wavelength 100 --octaves 4 --lacunarity 2.5 --gain 0.4 --ridged)
file(SHA256 "${dir}/t3.hmap" h3)
if(h1 STREQUAL h3)
  fail("une autre graine donne le même fichier")
endif()

# 4. Aller-retour par le PNG : l'import redonne exactement le même .hmap.
assetc(rc text heightmap-import "${dir}/t.png" "${dir}/back.hmap" --spacing 2 --alt-min -5 --alt-max 95)
if(NOT rc EQUAL 0)
  fail("heightmap-import échoue : ${text}")
endif()
file(SHA256 "${dir}/back.hmap" hb)
if(NOT h1 STREQUAL hb)
  fail("PNG puis import ne redonne pas le .hmap d'origine")
endif()

# 5. Refus : option inconnue, valeur absente, valeur illisible, paramètre invalide, mauvaise
#    extension, entrée inexistante, image non carrée. Aucun fichier n'est écrit.
function(expect_failure code)
  assetc(rc text ${ARGN})
  if(rc EQUAL 0)
    fail("devrait échouer : ${ARGN}")
  endif()
  if(NOT rc EQUAL ${code})
    fail("code ${rc} au lieu de ${code} : ${ARGN} : ${text}")
  endif()
endfunction()
expect_failure(2 heightmap "${dir}/x.hmap" --inconnue 3)
expect_failure(2 heightmap "${dir}/x.hmap" --size)
expect_failure(2 heightmap "${dir}/x.hmap" --size abc)
expect_failure(2 heightmap "${dir}/x.hmap" --size -4)
expect_failure(2 heightmap "${dir}/x.hmap" --seed 1.5)
expect_failure(2 heightmap "${dir}/x.hmap" --gain x)
expect_failure(1 heightmap "${dir}/x.hmap" --size 1)
expect_failure(1 heightmap "${dir}/x.hmap" --alt-min 10 --alt-max 10)
expect_failure(1 heightmap "${dir}/x.hmap" --octaves 99)
expect_failure(2 heightmap "${dir}/x.txt")
expect_failure(1 heightmap-import "${dir}/absent.png" "${dir}/y.hmap")
expect_failure(2 heightmap-import "${dir}/t.png" "${dir}/y.bin")
expect_failure(1 heightmap-import "${dir}/t.png" "${dir}/y.hmap" --alt-min 5 --alt-max 5)
expect_failure(2 heightmap-import "${dir}/t.png" "${dir}/y.hmap" --spacing)
expect_failure(1 info "${dir}/absent.hmap")
if(EXISTS "${dir}/x.hmap" OR EXISTS "${dir}/y.hmap" OR EXISTS "${dir}/x.txt")
  fail("un échec a laissé un fichier de sortie")
endif()

message(STATUS "heightmap_cli : OK")
