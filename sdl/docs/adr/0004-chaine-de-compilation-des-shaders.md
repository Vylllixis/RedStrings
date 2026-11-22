# ADR 0004 — Chaîne de compilation des shaders

Statut : Accepté (M1-02) · Date : 2026-11

## Contexte

SDL_GPU ne lit pas le HLSL : il charge SPIR-V (Vulkan), DXIL (Direct3D 12) ou MSL (Metal). Le
projet vise Windows et Linux (CI) et veut une build reproductible depuis un clone vierge, sans
installation manuelle d'outils. SDL_shadercross, l'outil de la bibliothèque SDL pour ce besoin,
n'a publié ni version ni binaire.

## Options

1. **GLSL + glslang/glslc.** Rien pour DXIL ni MSL sans passer par SPIRV-Cross + DXC de toute
   façon.
2. **Compiler à l'exécution** (shadercross en bibliothèque). Utile pour le rechargement à chaud
   (M1-03, en Debug), mais embarque DXC dans le jeu et retarde le démarrage.
3. **Hors ligne, `shadercross` construit avec le projet**, DXC pris dans les archives officielles
   de Microsoft, SPIRV-Cross compilé depuis les sources. Retenu.
4. Compiler DXC depuis les sources (option « vendored » de shadercross). Plusieurs dizaines de
   minutes et plusieurs Go : rejeté.
5. Exiger le SDK Vulkan de LunarG sur chaque machine. Contraignant pour la CI et non
   reproductible (version libre).

## Décision

Option 3, versions et sommes de contrôle figées (`third_party/CMakeLists.txt`). Un shader
`nom.étage.hlsl` produit `.spv`, `.dxil`, `.msl` et `.json` de réflexion dans `bin/shaders/` ;
**les trois formats sont toujours produits**, pour que le même arbre serve tous les pilotes.
Le chargeur choisit selon `SDL_GetGPUShaderFormats`. La chaîne est derrière
`OW_BUILD_SHADERS` (défaut : ON) ; Linux x86_64 et Windows seulement.

## Conséquences

- Un compilateur C++ est requis par défaut (SPIRV-Cross) ; `OW_BUILD_SHADERS=OFF` l'évite (ADR 0002).
- Premier configure : environ 45 Mo de téléchargements pour DXC, plus la compilation de SPIRV-Cross
  et de shadercross (environ 35 s mesurées sur Linux, par preset).
- Un shader cassé casse la build, avec le message de DXC.
- Le rechargement à chaud (M1-03) utilise la même bibliothèque en compilation à l'exécution, liée au
  moteur seulement en Debug (`OW_SHADER_HOT_RELOAD`) : une seule voie SPIR-V → format du pilote, et
  Release n'embarque ni DXC ni SPIRV-Cross. Voir `docs/shaders.md`.
- macOS n'est pas pris en charge par cette chaîne (DXC n'y est pas distribué) ; à reprendre si Metal
  devient une cible.
