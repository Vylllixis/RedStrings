add_executable(launcher src/main.c)
target_link_libraries(launcher PRIVATE engine)
add_dependencies(launcher game)
target_link_libraries(launcher PRIVATE project_options)
