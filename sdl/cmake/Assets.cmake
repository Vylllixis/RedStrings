# Cuisson des assets au build (M2-04) par l'outil `assetc` : glTF -> .mesh + .k.tex, image -> .tex.
#
#   ow_cook_assets(<cible>
#     TOOL       <cible assetc>              (défaut : assetc)
#     OUTPUT_DIR <dossier>                   (les sorties y reprennent le chemin relatif de la source)
#     [BASE_DIR  <dossier>]                  (racine des sources ; défaut : le dossier courant)
#     [MODELS    models/a.gltf ...]          (-> OUTPUT_DIR/models/a.mesh et a.<k>.tex)
#     [TEXTURES  textures/b.png ...]         (-> OUTPUT_DIR/textures/b.tex, couleur sRGB)
#     [LINEAR_TEXTURES textures/n.png ...]   (-> idem, en données linéaires : normales, rugosité...)
#     [RAW])                                 (RGBA8 non compressé au lieu de BC7)
#
# Un asset n'est recuit que si sa source, un fichier qu'elle référence (.bin, images du glTF : liste
# écrite par `assetc --depfile`, lue par Ninja) ou l'outil lui-même a changé. Rien d'autre ne
# déclenche une cuisson : une seconde build ne lance pas `assetc`.
#
# Le fichier de dépendances n'existe que pour les générateurs Ninja et Makefile : avec un autre
# (Visual Studio...), seule la source principale est suivie, et un message le signale.
function(ow_cook_assets target)
  cmake_parse_arguments(ARG "RAW" "TOOL;OUTPUT_DIR;BASE_DIR" "MODELS;TEXTURES;LINEAR_TEXTURES" ${ARGN})
  if(NOT ARG_OUTPUT_DIR)
    message(FATAL_ERROR "ow_cook_assets : OUTPUT_DIR est obligatoire")
  endif()
  if(NOT ARG_MODELS AND NOT ARG_TEXTURES AND NOT ARG_LINEAR_TEXTURES)
    message(FATAL_ERROR "ow_cook_assets : rien à cuire (MODELS, TEXTURES ou LINEAR_TEXTURES)")
  endif()
  if(NOT ARG_TOOL)
    set(ARG_TOOL assetc)
  endif()
  if(NOT ARG_BASE_DIR)
    set(ARG_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set(use_depfile OFF)
  if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    set(use_depfile ON)
  else()
    message(WARNING "ow_cook_assets : générateur « ${CMAKE_GENERATOR} » sans fichier de dépendances : "
                    "modifier un .bin ou une image référencée ne recuit pas l'asset (utiliser Ninja).")
  endif()

  set(raw_flag)
  if(ARG_RAW)
    set(raw_flag --raw)
  endif()

  set(outputs)
  foreach(kind IN ITEMS MODELS TEXTURES LINEAR_TEXTURES)
    foreach(source IN LISTS ARG_${kind})
      cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY ${ARG_BASE_DIR} OUTPUT_VARIABLE source_path)
      cmake_path(RELATIVE_PATH source_path BASE_DIRECTORY ${ARG_BASE_DIR} OUTPUT_VARIABLE relative)
      cmake_path(REMOVE_EXTENSION relative LAST_ONLY OUTPUT_VARIABLE relative_stem)
      if(kind STREQUAL "MODELS")
        set(command mesh)
        set(output "${ARG_OUTPUT_DIR}/${relative_stem}.mesh")
      else()
        set(command tex)
        set(output "${ARG_OUTPUT_DIR}/${relative_stem}.tex")
      endif()
      set(options ${raw_flag})
      if(kind STREQUAL "LINEAR_TEXTURES")
        list(APPEND options --linear)
      endif()

      # Chemins ABSOLUS : CMake (générateur Ninja) retraduit lui-même le fichier de dépendances en
      # chemins relatifs au dossier de build ; un chemin relatif donnerait une cible que Ninja ne
      # reconnaît pas (la cuisson serait alors refaite à chaque build).
      set(depfile_args)
      set(depfile_option)
      if(use_depfile)
        set(depfile_option DEPFILE ${output}.d)
        set(depfile_args --depfile ${output}.d)
      endif()

      add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ARG_OUTPUT_DIR}
        COMMAND ${ARG_TOOL} ${command} ${source_path} ${output} ${options} ${depfile_args}
        DEPENDS ${source_path} ${ARG_TOOL}
        ${depfile_option}
        COMMENT "Cuisson ${relative}"
        VERBATIM)
      list(APPEND outputs ${output})
    endforeach()
  endforeach()

  add_custom_target(${target} ALL DEPENDS ${outputs})
endfunction()

# Cuisson d'un personnage animé (M4) : squelette, maillage skinné (+ poids .skin et textures), et
# un .anim par clip. Les clips sont choisis par NOM d'animation glTF (assetc anim --anim-name).
#
#   ow_cook_character(<cible>
#     TOOL assetc OUTPUT_DIR <dossier> BASE_DIR <dossier des sources>
#     NAME knight                          (-> OUTPUT_DIR/knight.skel, knight.mesh, knight.skin)
#     SOURCE characters/knight.glb
#     [SKELETON_PATH assets/cooked/characters/knight.skel]  (chemin inscrit dans les clips, relatif
#                                                           à l'exécutable ; défaut ci-contre)
#     [CLIPS idle walk run ...]            (-> OUTPUT_DIR/knight/<clip>.anim)
#     [NO_SKELETON])                       (maillage et clips d'un squelette déjà cuit ailleurs)
#
# La cible dépend de tout ; à ajouter aux dépendances du lanceur (add_dependencies).
function(ow_cook_character target)
  cmake_parse_arguments(ARG "NO_SKELETON" "TOOL;OUTPUT_DIR;BASE_DIR;NAME;SOURCE;SKELETON_PATH" "CLIPS"
                        ${ARGN})
  foreach(required IN ITEMS OUTPUT_DIR NAME SOURCE)
    if(NOT ARG_${required})
      message(FATAL_ERROR "ow_cook_character : ${required} est obligatoire")
    endif()
  endforeach()
  if(NOT ARG_TOOL)
    set(ARG_TOOL assetc)
  endif()
  if(NOT ARG_BASE_DIR)
    set(ARG_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
  if(NOT ARG_SKELETON_PATH)
    set(ARG_SKELETON_PATH assets/cooked/characters/${ARG_NAME}.skel)
  endif()
  cmake_path(ABSOLUTE_PATH ARG_SOURCE BASE_DIRECTORY ${ARG_BASE_DIR} OUTPUT_VARIABLE source_path)
  set(out ${ARG_OUTPUT_DIR}/${ARG_NAME})
  set(outputs)
  if(NOT ARG_NO_SKELETON)
    add_custom_command(
      OUTPUT ${out}.skel
      COMMAND ${CMAKE_COMMAND} -E make_directory ${ARG_OUTPUT_DIR}
      COMMAND ${ARG_TOOL} skel ${source_path} ${out}.skel --skin 0
      DEPENDS ${source_path} ${ARG_TOOL}
      COMMENT "Squelette ${ARG_NAME}"
      VERBATIM)
    list(APPEND outputs ${out}.skel)
  endif()
  add_custom_command(
    OUTPUT ${out}.mesh
    BYPRODUCTS ${out}.skin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ARG_OUTPUT_DIR}
    COMMAND ${ARG_TOOL} mesh ${source_path} ${out}.mesh --skin 0
    DEPENDS ${source_path} ${ARG_TOOL}
    COMMENT "Personnage ${ARG_NAME}"
    VERBATIM)
  list(APPEND outputs ${out}.mesh)
  foreach(clip IN LISTS ARG_CLIPS)
    add_custom_command(
      OUTPUT ${out}/${clip}.anim
      COMMAND ${CMAKE_COMMAND} -E make_directory ${out}
      COMMAND ${ARG_TOOL} anim ${source_path} ${out}/${clip}.anim --anim-name ${clip}
              --skeleton ${ARG_SKELETON_PATH}
      DEPENDS ${source_path} ${ARG_TOOL}
      COMMENT "Clip ${ARG_NAME}/${clip}"
      VERBATIM)
    list(APPEND outputs ${out}/${clip}.anim)
  endforeach()
  add_custom_target(${target} ALL DEPENDS ${outputs})
endfunction()

# Maillage statique cuit SANS sa hiérarchie de scène (arme rangée sous un os : voir
# tools/scripts/gltf_extract.py --root-node) : comme ow_cook_assets MODELS, sous un autre nom.
function(ow_cook_props target)
  cmake_parse_arguments(ARG "" "TOOL;OUTPUT_DIR;BASE_DIR" "MODELS" ${ARGN})
  ow_cook_assets(${target} TOOL ${ARG_TOOL} OUTPUT_DIR ${ARG_OUTPUT_DIR} BASE_DIR ${ARG_BASE_DIR}
                 MODELS ${ARG_MODELS})
endfunction()
