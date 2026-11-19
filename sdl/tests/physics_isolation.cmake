# Test d'isolation de Jolt (M4-01) : aucun type de Jolt ne doit apparaître hors de
# engine/src/physics.c. Lancé par `cmake -P` avec -DOW_ROOT=<racine du projet>.
if(NOT DEFINED OW_ROOT)
  message(FATAL_ERROR "physics_isolation : OW_ROOT manquant")
endif()
file(GLOB_RECURSE files
  "${OW_ROOT}/game/*" "${OW_ROOT}/launcher/*" "${OW_ROOT}/tools/*"
  "${OW_ROOT}/engine/include/*" "${OW_ROOT}/engine/src/*" "${OW_ROOT}/tests/*.c")
list(LENGTH files n)
if(n LESS 20)
  message(FATAL_ERROR "physics_isolation : seulement ${n} fichiers examinés, chemin invalide ?")
endif()
set(bad "")
foreach(f IN LISTS files)
  if(f MATCHES "engine/src/physics\\.c$" OR f MATCHES "physics_isolation\\.cmake$" OR IS_DIRECTORY "${f}")
    continue()
  endif()
  file(READ "${f}" text)
  if(text MATCHES "joltc\\.h|JPH_[A-Za-z]|JoltPhysics")
    # les commentaires qui citent « Jolt » en toutes lettres sont permis, pas ses symboles
    list(APPEND bad "${f}")
  endif()
endforeach()
if(bad)
  string(REPLACE ";" "\n  " list "${bad}")
  message(FATAL_ERROR "physics_isolation : symboles Jolt hors de physics.c :\n  ${list}")
endif()
message(STATUS "physics_isolation : ${n} fichiers examinés, aucun symbole Jolt hors de physics.c")
