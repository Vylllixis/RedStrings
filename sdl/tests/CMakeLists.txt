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

# ── Tests de bout en bout : on lance le vrai exécutable et on lit sa sortie ──
# (expressions régulières volontairement sans accents : indépendantes de l'encodage)
set(LAUNCHER_ENV "SDL_VIDEODRIVER=dummy")

add_test(NAME launcher.smoke COMMAND launcher --frames 3
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.smoke PROPERTIES
  ENVIRONMENT "${LAUNCHER_ENV}"
  PASS_REGULAR_EXPRESSION "propre apr.* 3 images")

add_test(NAME launcher.bad_option COMMAND launcher --log-level bruit
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(launcher.bad_option PROPERTIES
  PASS_REGULAR_EXPRESSION "usage :")
