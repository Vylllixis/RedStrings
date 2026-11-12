# Compilation hors ligne des shaders HLSL : SPIR-V (Vulkan), DXIL (Direct3D 12), MSL (Metal), et un
# JSON de réflexion (nombre de samplers, de tampons...), par l'exécutable `shadercross`.
#
#   ow_add_shaders(<cible>
#     OUTPUT_DIR <dossier>
#     SOURCES    triangle.vert.hlsl triangle.frag.hlsl ...   (nom.étage.hlsl : vert, frag ou comp)
#     [COMMON    commun.hlsli ...])                           (inclus par les sources)
#
# Chaque source produit nom.étage.spv / .dxil / .msl / .json dans OUTPUT_DIR. Une sortie est
# régénérée dès que sa source, un fichier COMMON ou l'outil change (ni plus, ni moins).
function(ow_add_shaders target)
  cmake_parse_arguments(ARG "" "OUTPUT_DIR" "SOURCES;COMMON" ${ARGN})
  if(NOT ARG_OUTPUT_DIR OR NOT ARG_SOURCES)
    message(FATAL_ERROR "ow_add_shaders : OUTPUT_DIR et SOURCES sont obligatoires")
  endif()

  set(common_abs)
  foreach(common IN LISTS ARG_COMMON)
    cmake_path(ABSOLUTE_PATH common BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE common_path)
    list(APPEND common_abs ${common_path})
  endforeach()

  set(outputs)
  foreach(source IN LISTS ARG_SOURCES)
    cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE source_path)
    cmake_path(GET source_path FILENAME file_name)
    if(file_name MATCHES "^(.+)\\.(vert|frag|comp)\\.hlsl$")
      set(stem "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
      if(CMAKE_MATCH_2 STREQUAL "vert")
        set(stage vertex)
      elseif(CMAKE_MATCH_2 STREQUAL "frag")
        set(stage fragment)
      else()
        set(stage compute)
      endif()
    else()
      message(FATAL_ERROR "ow_add_shaders : « ${file_name} » doit s'appeler nom.vert.hlsl, nom.frag.hlsl ou nom.comp.hlsl")
    endif()

    foreach(format IN ITEMS spv:SPIRV dxil:DXIL msl:MSL json:JSON)
      string(REPLACE ":" ";" parts ${format})
      list(GET parts 0 extension)
      list(GET parts 1 destination)
      set(output "${ARG_OUTPUT_DIR}/${stem}.${extension}")
      add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ARG_OUTPUT_DIR}
        COMMAND shadercross ${source_path} --source HLSL --dest ${destination} --stage ${stage}
                --entrypoint main --output ${output}
        DEPENDS ${source_path} ${common_abs} shadercross
        COMMENT "Shader ${file_name} -> ${stem}.${extension}"
        VERBATIM)
      list(APPEND outputs ${output})
    endforeach()
  endforeach()

  add_custom_target(${target} ALL DEPENDS ${outputs})
  if(TARGET ow_shadercross_runtime) # Windows : DLL de DXC copiées avant la première compilation
    add_dependencies(${target} ow_shadercross_runtime)
  endif()
endfunction()
