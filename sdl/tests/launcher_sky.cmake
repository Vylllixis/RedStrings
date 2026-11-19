# Test du lanceur avec le ciel atmosphérique (M3-05) : lancé par `cmake -P`.
#   -DOW_LAUNCHER=<launcher>
# Le lanceur journalise à l'arrêt la lumière directe, la visibilité du soleil et l'ambiante de la
# dernière image. On lance trois hauteurs du soleil (--sun-elevation) et on vérifie que la couleur
# suit : blanche à 60°, rouge-orangé à 3°, soleil éteint et ambiante nulle à -20°. Une hauteur
# hors de [-90, 90] est refusée (code 2). Sans GPU (affichage logiciel), pas de ciel : ignoré.
if(NOT DEFINED OW_LAUNCHER)
  message(FATAL_ERROR "launcher_sky : OW_LAUNCHER manquant")
endif()
set(ENV{SDL_VIDEODRIVER} offscreen)

# "0.37" -> 37, "60.0" -> 600 : le point retiré, valeurs entières (CMake compare mal les flottants)
function(hundredths out text)
  string(REPLACE "." "" digits "${text}")
  string(REGEX REPLACE "^(-?)0+([0-9])" "\\1\\2" digits "${digits}")
  set(${out} "${digits}" PARENT_SCOPE)
endfunction()

function(run_sun elevation)
  execute_process(COMMAND ${OW_LAUNCHER} --frames 5 --env-samples 16 --sun-elevation ${elevation}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(text "${out}${err}")
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "launcher_sky : le lanceur échoue à ${elevation}° (${rc}) : ${text}")
  endif()
  if(text MATCHES "ERROR")
    message(FATAL_ERROR "launcher_sky : ERREUR dans le journal à ${elevation}° : ${text}")
  endif()
  if(text MATCHES "affichage logiciel")
    set(SKY_SKIP TRUE PARENT_SCOPE)
    return()
  endif()
  if(NOT text MATCHES "ciel : analytique, soleil à (-?[0-9.]+)°, lumière directe \\((-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+)\\) x (-?[0-9.]+), ambiante \\((-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+)\\)")
    message(FATAL_ERROR "launcher_sky : ligne du ciel absente à ${elevation}° : ${text}")
  endif()
  hundredths(r "${CMAKE_MATCH_2}")
  hundredths(g "${CMAKE_MATCH_3}")
  hundredths(b "${CMAKE_MATCH_4}")
  hundredths(v "${CMAKE_MATCH_5}")
  hundredths(ar "${CMAKE_MATCH_6}")
  hundredths(ag "${CMAKE_MATCH_7}")
  hundredths(ab "${CMAKE_MATCH_8}")
  # la hauteur journalisée (un chiffre après la virgule : dixièmes) est celle demandée, à 0,1° près
  hundredths(shown "${CMAKE_MATCH_1}")
  math(EXPR want "${elevation} * 10")
  math(EXPR delta "${shown} - ${want}")
  if(delta LESS -1 OR delta GREATER 1)
    message(FATAL_ERROR "launcher_sky : hauteur ${CMAKE_MATCH_1}° au lieu de ${elevation}°")
  endif()
  set(SKY_R ${r} PARENT_SCOPE)
  set(SKY_G ${g} PARENT_SCOPE)
  set(SKY_B ${b} PARENT_SCOPE)
  set(SKY_V ${v} PARENT_SCOPE)
  set(SKY_AR ${ar} PARENT_SCOPE)
  set(SKY_AG ${ag} PARENT_SCOPE)
  set(SKY_AB ${ab} PARENT_SCOPE)
endfunction()

run_sun(60)
if(SKY_SKIP)
  message(STATUS "launcher_sky : pas de GPU, test ignoré")
  return()
endif()
# haut dans le ciel : lumière presque blanche, pleinement visible, ambiante forte
if(SKY_R LESS 90 OR SKY_G LESS 90 OR SKY_B LESS 90 OR NOT SKY_V EQUAL 100 OR SKY_AG LESS 80)
  message(FATAL_ERROR "launcher_sky : 60° : (${SKY_R} ${SKY_G} ${SKY_B}) x ${SKY_V}, ambiante ${SKY_AG}")
endif()

run_sun(3)
# bas sur l'horizon : rouge-orangé (rouge > 2 x vert > 4 x bleu environ), ambiante faible
math(EXPR green_x2 "${SKY_G} * 2")
if(SKY_R LESS 25 OR SKY_B GREATER 10 OR SKY_R LESS ${green_x2} OR NOT SKY_V EQUAL 100 OR SKY_AG GREATER 30)
  message(FATAL_ERROR "launcher_sky : 3° : (${SKY_R} ${SKY_G} ${SKY_B}) x ${SKY_V}, ambiante ${SKY_AG}")
endif()

run_sun(-20)
# nuit : plus de soleil, ambiante pratiquement nulle
if(NOT SKY_V EQUAL 0 OR SKY_AR GREATER 2 OR SKY_AG GREATER 2 OR SKY_AB GREATER 2)
  message(FATAL_ERROR "launcher_sky : -20° : soleil ${SKY_V}, ambiante (${SKY_AR} ${SKY_AG} ${SKY_AB})")
endif()

# options invalides : refusées
foreach(bad "--sun-elevation;95" "--sun-elevation;abc" "--sun-azimuth;181")
  execute_process(COMMAND ${OW_LAUNCHER} --frames 1 ${bad}
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 2)
    message(FATAL_ERROR "launcher_sky : « ${bad} » doit être refusé (code 2), reçu ${rc}")
  endif()
endforeach()
message(STATUS "launcher_sky : OK")
