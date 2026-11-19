# Test du lanceur avec un terrain (M3-02, culling M3-04) : lancé par `cmake -P`.
#   -DOW_ASSETC=<assetc> -DOW_LAUNCHER=<launcher> -DOW_WORK=<dossier de travail>
# 1. génère un champ 129 x 129 (4 chunks) puis lance le lanceur avec --terrain : le terrain doit être
#    chargé (journal) et l'arrêt propre ;
# 2. --terrain avec un fichier absent : ERREUR journalisée, mais le lanceur retombe sur la
#    démonstration et s'arrête proprement (code 0).
foreach(var OW_ASSETC OW_LAUNCHER OW_WORK)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "launcher_terrain : ${var} manquant")
  endif()
endforeach()
file(REMOVE_RECURSE "${OW_WORK}")
file(MAKE_DIRECTORY "${OW_WORK}")

execute_process(COMMAND ${OW_ASSETC} heightmap "${OW_WORK}/t.hmap" --size 129 --wavelength 128
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "launcher_terrain : assetc heightmap échoue (${rc}) : ${out}${err}")
endif()

set(ENV{SDL_VIDEODRIVER} offscreen)
execute_process(COMMAND ${OW_LAUNCHER} --frames 5 --env-samples 16 --terrain "${OW_WORK}/t.hmap"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "launcher_terrain : le lanceur échoue (${rc}) : ${text}")
endif()
if(text MATCHES "ERROR")
  message(FATAL_ERROR "launcher_terrain : ERREUR dans le journal : ${text}")
endif()
if(text MATCHES "affichage logiciel")
  message(STATUS "launcher_terrain : pas de GPU, test ignoré (arrêt propre vérifié seulement)")
  return()
endif()
if(NOT text MATCHES "128 m de côté, 4 chunks")
  message(FATAL_ERROR "launcher_terrain : le terrain n'a pas été chargé : ${text}")
endif()
if(NOT text MATCHES "propre")
  message(FATAL_ERROR "launcher_terrain : arrêt non propre : ${text}")
endif()
# culling (M3-04) : la caméra est au centre et regarde vers -Z : la moitié du terrain est derrière
# elle, donc strictement moins de chunks dessinés que de chunks (4 ici), et au moins un.
if(NOT text MATCHES "terrain : ([0-9]+)/([0-9]+) chunks dessinés")
  message(FATAL_ERROR "launcher_terrain : compteur de chunks absent du journal : ${text}")
endif()
set(drawn "${CMAKE_MATCH_1}")
set(total "${CMAKE_MATCH_2}")
if(NOT total EQUAL 4 OR NOT drawn LESS total OR drawn LESS 1)
  message(FATAL_ERROR "launcher_terrain : culling : ${drawn}/${total} chunks dessinés (attendu 1 à 3 sur 4)")
endif()

# brouillard (M3-06) : par défaut densité 0,0030 ; --fog-density la règle, hors de [0, 1] refusée
if(NOT text MATCHES "brouillard : densité 0.0030 /m, décroissance 0.0040 /m, opacité max 1.00")
  message(FATAL_ERROR "launcher_terrain : brouillard par défaut absent du journal : ${text}")
endif()
execute_process(COMMAND ${OW_LAUNCHER} --frames 3 --env-samples 16 --terrain "${OW_WORK}/t.hmap"
                        --fog-density 0.0125
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT rc EQUAL 0 OR NOT text MATCHES "brouillard : densité 0.0125 /m")
  message(FATAL_ERROR "launcher_terrain : --fog-density non pris en compte (${rc}) : ${text}")
endif()
foreach(bad 2 -0.1 abc)
  execute_process(COMMAND ${OW_LAUNCHER} --frames 1 --fog-density ${bad}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 2)
    message(FATAL_ERROR "launcher_terrain : --fog-density ${bad} doit être refusé (code 2), reçu ${rc}")
  endif()
endforeach()

# rochers instanciés (M3-07) : 2000 rochers = 1 dessin ; --rocks hors de [0, 1000000] refusé
execute_process(COMMAND ${OW_LAUNCHER} --frames 3 --env-samples 16 --terrain "${OW_WORK}/t.hmap"
                        --rocks 2000
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT rc EQUAL 0 OR NOT text MATCHES "rochers : 2000 instances, 1 dessin par image")
  message(FATAL_ERROR "launcher_terrain : --rocks 2000 non pris en compte (${rc}) : ${text}")
endif()
if(text MATCHES "ERROR")
  message(FATAL_ERROR "launcher_terrain : ERREUR dans le journal avec --rocks : ${text}")
endif()
foreach(bad 1000001 -5 abc)
  execute_process(COMMAND ${OW_LAUNCHER} --frames 1 --rocks ${bad}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 2)
    message(FATAL_ERROR "launcher_terrain : --rocks ${bad} doit être refusé (code 2), reçu ${rc}")
  endif()
endforeach()

# placement procédural (M3-08) : rochers sur la roche, touffes sur l'herbe, deux dessins ; mêmes
# nombres à chaque lancement (même graine) ; graine invalide refusée
execute_process(COMMAND ${OW_LAUNCHER} --frames 3 --env-samples 16 --terrain "${OW_WORK}/t.hmap"
                        --placement --placement-seed 7
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT rc EQUAL 0 OR NOT text MATCHES "placement : ([0-9]+) rochers sur la roche, ([0-9]+) touffes sur l'herbe \\(graine 7\\)")
  message(FATAL_ERROR "launcher_terrain : --placement non pris en compte (${rc}) : ${text}")
endif()
set(first_line "${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
if(CMAKE_MATCH_2 EQUAL 0)
  message(FATAL_ERROR "launcher_terrain : --placement n'a placé aucune végétation : ${text}")
endif()
if(text MATCHES "ERROR")
  message(FATAL_ERROR "launcher_terrain : ERREUR dans le journal avec --placement : ${text}")
endif()
execute_process(COMMAND ${OW_LAUNCHER} --frames 1 --env-samples 16 --terrain "${OW_WORK}/t.hmap"
                        --placement --placement-seed 7
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
string(REGEX MATCH "placement : ([0-9]+) rochers sur la roche, ([0-9]+) touffes" again "${text}")
if(NOT "${CMAKE_MATCH_1}/${CMAKE_MATCH_2}" STREQUAL "${first_line}")
  message(FATAL_ERROR "launcher_terrain : --placement non déterministe : ${first_line} puis ${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
endif()
foreach(bad abc -1 99999999999)
  execute_process(COMMAND ${OW_LAUNCHER} --frames 1 --placement-seed ${bad}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 2)
    message(FATAL_ERROR "launcher_terrain : --placement-seed ${bad} doit être refusé (code 2), reçu ${rc}")
  endif()
endforeach()

# herbe animée (M3-09) : un champ de brins, UN dessin par image
execute_process(COMMAND ${OW_LAUNCHER} --frames 3 --env-samples 16 --terrain "${OW_WORK}/t.hmap"
                        --grass
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT rc EQUAL 0 OR NOT text MATCHES "herbe : ([0-9]+) brins \\(10 triangles chacun\\)")
  message(FATAL_ERROR "launcher_terrain : --grass non pris en compte (${rc}) : ${text}")
endif()
if(CMAKE_MATCH_1 EQUAL 0)
  message(FATAL_ERROR "launcher_terrain : --grass n'a placé aucun brin : ${text}")
endif()
if(NOT text MATCHES "herbe : [0-9]+ brins, 1 dessin par image" OR text MATCHES "ERROR")
  message(FATAL_ERROR "launcher_terrain : --grass : arrêt ou journal incorrect : ${text}")
endif()

# eau (M3-10) : un plan au niveau demandé, un dessin par image
execute_process(COMMAND ${OW_LAUNCHER} --frames 3 --env-samples 16 --terrain "${OW_WORK}/t.hmap"
                        --water-level 40
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT rc EQUAL 0 OR NOT text MATCHES "eau : niveau 40\\.0 m, ([0-9]+) cellules mouillées")
  message(FATAL_ERROR "launcher_terrain : --water-level non pris en compte (${rc}) : ${text}")
endif()
if(NOT text MATCHES "eau : niveau 40\\.0 m, [0-9]+ cellules mouillées, 1 dessin par image" OR text MATCHES "ERROR")
  message(FATAL_ERROR "launcher_terrain : --water : arrêt ou journal incorrect : ${text}")
endif()
execute_process(COMMAND ${OW_LAUNCHER} --frames 1 --terrain "${OW_WORK}/t.hmap" --water-level abc
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 2)
  message(FATAL_ERROR "launcher_terrain : --water-level invalide doit sortir en 2 (${rc})")
endif()

execute_process(COMMAND ${OW_LAUNCHER} --frames 5 --env-samples 16 --terrain "${OW_WORK}/absent.hmap"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "launcher_terrain : le lanceur doit survivre à un terrain absent (${rc}) : ${text}")
endif()
if(NOT text MATCHES "chargement impossible")
  message(FATAL_ERROR "launcher_terrain : l'échec n'est pas journalisé : ${text}")
endif()
message(STATUS "launcher_terrain : OK")
