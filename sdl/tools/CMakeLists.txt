add_executable(assetc src/assetc.c)
target_link_libraries(assetc PRIVATE engine)
target_link_libraries(assetc PRIVATE project_options)
