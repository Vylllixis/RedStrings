add_executable(assetc src/assetc.c)
target_link_libraries(assetc PRIVATE engine)
target_link_libraries(assetc PRIVATE project_options)

# Carte du monde (2026-11-21) : aperçu vu de dessus de assets/world/carte.txt.
add_executable(carte src/carte.c)
target_link_libraries(carte PRIVATE engine)
target_link_libraries(carte PRIVATE project_options)

# Arbres réalistes (2026-11-22) : conversion d'un arbre scanné (des millions de triangles) en arbre
# de jeu (engine/tree_bake.h, docs/arbres-realistes.md). L'écriture d'images (stb_image_write) est du
# code tiers : compilée à part, sans project_options (pas de -Werror).
add_library(treebake_third_party OBJECT src/treebake_third_party.c)
target_link_libraries(treebake_third_party PRIVATE stb_headers)
if(MSVC)
  target_compile_definitions(treebake_third_party PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(treebake_third_party PRIVATE /W0)
endif()
add_executable(treebake src/treebake.c src/treebake_source.c src/treebake_trunk.c
               src/treebake_foliage.c src/treebake_cards.c src/treebake_far.c src/treebake_atlas.c
               src/treebake_write.c
               $<TARGET_OBJECTS:treebake_third_party>)
target_link_libraries(treebake PRIVATE engine)
target_link_libraries(treebake PRIVATE project_options)

# Assets cuisinés du jeu (M2-04) : bin/assets/cooked/, recuits seulement si une source ou assetc change.
# Les sources restent dans assets/ ; le moteur lira les .mesh / .tex cuits (gestionnaire d'assets, M2-05).
include(${CMAKE_SOURCE_DIR}/cmake/Assets.cmake)
ow_cook_assets(cooked_assets
  TOOL assetc
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/assets/cooked
  BASE_DIR ${CMAKE_SOURCE_DIR}/assets
  MODELS models/demo.gltf
  TEXTURES textures/checker.png)

# Personnages animés (M4) : KayKit (CC0), préparés par tools/scripts/kaykit_prepare.sh. Le chevalier
# porte les clips du jeu ; les ennemis partagent son squelette (même rig) et ses clips.
set(OW_CHARACTER_CLIPS idle walk run walk_back strafe_left strafe_right jump_start jump_air jump_land
    dodge_back dodge_forward dodge_left dodge_right slash_h slash_d chop stab idle_2h slash_2h chop_2h
    spin_2h stab_2h block block_raise block_hit parry hit_a hit_b death death_b cheer shoot spell
    CACHE INTERNAL "clips cuits pour les personnages")
set(ow_characters_out ${CMAKE_BINARY_DIR}/bin/assets/cooked/characters)
ow_cook_character(cooked_knight
  TOOL assetc OUTPUT_DIR ${ow_characters_out} BASE_DIR ${CMAKE_SOURCE_DIR}/assets
  NAME knight SOURCE characters/knight.glb CLIPS ${OW_CHARACTER_CLIPS})
set(ow_character_targets cooked_knight)
foreach(enemy IN ITEMS minion warrior rogue mage)
  ow_cook_character(cooked_skeleton_${enemy}
    TOOL assetc OUTPUT_DIR ${ow_characters_out} BASE_DIR ${CMAKE_SOURCE_DIR}/assets
    NAME skeleton_${enemy} SOURCE characters/skeleton_${enemy}.glb)
  list(APPEND ow_character_targets cooked_skeleton_${enemy})
endforeach()
# Armes et boucliers : chacun dans son propre repère, attaché à un os par un socket au runtime.
ow_cook_assets(cooked_props
  TOOL assetc
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/assets/cooked
  BASE_DIR ${CMAKE_SOURCE_DIR}/assets
  MODELS props/sword_1handed.gltf props/sword_2handed.gltf props/axe_2handed.gltf
         props/shield_round.gltf props/Skeleton_Blade.gltf props/Skeleton_Axe.gltf
         props/Skeleton_Shield_Small_A.gltf props/Skeleton_Crossbow.gltf props/Skeleton_Staff.gltf)
add_custom_target(cooked_characters DEPENDS ${ow_character_targets} cooked_props)
