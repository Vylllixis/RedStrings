ow_cook_assets(cooked_color TOOL assetc OUTPUT_DIR ${CMAKE_BINARY_DIR}/couleur BASE_DIR "${OW_SRC}"
  TEXTURES textures/checker.png)
ow_cook_assets(cooked_linear TOOL assetc OUTPUT_DIR ${CMAKE_BINARY_DIR}/lineaire BASE_DIR "${OW_SRC}"
  LINEAR_TEXTURES textures/checker.png)
ow_cook_assets(cooked_raw TOOL assetc OUTPUT_DIR ${CMAKE_BINARY_DIR}/brut BASE_DIR "${OW_SRC}"
  RAW TEXTURES textures/checker.png)
ow_cook_assets(cooked_raw_mesh TOOL assetc OUTPUT_DIR ${CMAKE_BINARY_DIR}/brut BASE_DIR "${OW_SRC}"
  RAW MODELS models/demo.gltf)
