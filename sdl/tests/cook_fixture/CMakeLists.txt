# Mini-projet de test de la cuisson incrémentale (tests/cook_incremental.cmake) : il utilise
# cmake/Assets.cmake tel quel, avec une COPIE d'assetc (pour pouvoir « modifier le cuiseur »
# sans toucher à la vraie build), sur une copie des sources dans un dossier à espaces.
# La cuisson est déclarée dans un SOUS-dossier (comme tools/ dans le projet) : Ninja y exprime les
# chemins autrement, et c'est là qu'une erreur de chemin dans le fichier de dépendances se voit.
cmake_minimum_required(VERSION 3.25)
project(cook_fixture NONE)

add_executable(assetc IMPORTED)
set_target_properties(assetc PROPERTIES IMPORTED_LOCATION "${OW_ASSETC}")

include("${OW_ASSETS_CMAKE}")
if(OW_VARIANTS)
  # Options RAW et LINEAR_TEXTURES : une même image cuite de trois façons (format du .tex produit).
  add_subdirectory(variants)
else()
  add_subdirectory(tools)
endif()
