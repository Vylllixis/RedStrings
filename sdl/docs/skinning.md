# Skinning GPU (M4-15)

Un personnage = un **maillage** (`EngModel`) + des **poids** (`EngSkinVertex`) + un **squelette** (M4-12) animé
par des **clips** (M4-13) évalués en **matrices de skinning** (M4-14). Ce document décrit le chemin des poids
jusqu'au pixel.

## Chargement (CPU)

`eng_model_parse_skinned(arena, scratch, glb, taille, base, skin_index, remap, remap_count, &model, &skin)`
(`engine/skin.h`) :

- ne charge que les nœuds qui utilisent le skin `skin_index` ; **leur matrice de nœud n'est pas appliquée**
  (règle de la spécification glTF pour un maillage skinné) : les sommets sont dans la pose de liaison, l'espace
  où `inverse_bind` les attend ;
- lit `JOINTS_0` / `WEIGHTS_0` (obligatoires, `vec4`, un par sommet) ; les indices du skin sont recodés avec le
  `remap` rendu par `eng_skeleton_from_gltf` pour **le même skin** (le squelette range les os parents d'abord) ;
- poids en 8 bits normalisés, **somme exactement 255** (renormalisés ; l'écart d'arrondi va à la plus forte
  influence) ; une influence de poids nul porte l'indice 0 ;
- refus (journalisés, arène restituée) : poids absents, négatifs, non finis ou de somme nulle ; indice d'os hors
  du skin (seulement s'il porte un poids non nul) ; `remap` incohérent ; skin inexistant.
  `JOINTS_1` / `WEIGHTS_1` (plus de 4 influences) sont ignorés avec un avertissement.

## GPU

- `eng_model_gpu_create_skinned` entrelace les poids aux sommets : **56 octets par sommet**
  (`EngModelVertex` 48 + joints 4 + poids 4), un seul flux.
- Pipelines : `eng_model_skinned_pipeline_desc` (`model_skinned.vert` + le `model.frag` ordinaire, donc le même
  éclairage PBR, les mêmes ombres reçues) et `eng_model_skinned_shadow_pipeline_desc` (`shadow_skinned.vert`).
- **Palette** : `EngModelDrawOptions.skin_matrices` (matrices 4×4 contiguës = `eng_pose_skin_matrices`) et
  `skin_joint_count`. Seules les 3 premières lignes de chaque matrice sont envoyées (48 octets par os) à la suite
  des uniforms ordinaires : 65 os (Mixamo) ≈ 3,3 Ko ; 256 os ≈ 12,5 Ko, d'où `ENG_MAX_UNIFORM_BYTES` relevé de
  4 096 à 16 384 (le minimum garanti par Vulkan pour un bloc d'uniforms).
- Le vertex shader déforme le sommet dans l'espace du personnage (`Σ wᵢ·Mᵢ·p`), puis `model` le place dans le
  monde. Normales et tangentes suivent la **partie linéaire** de la matrice mélangée, renormalisée : exact sans
  échelle non uniforme dans les os (cas des squelettes importés ; l'échelle 0,01 d'`Armature` est repliée dans la
  racine et annulée par `inverse_bind`). Une échelle non uniforme par os donnerait des normales légèrement fausses.
- Refus (0 dessin, journalisé) : modèle skinné sans palette, palette de 0 ou plus de 256 matrices, valeur non
  finie, palette fournie à un modèle ordinaire, ombre ordinaire demandée pour un modèle skinné.
- La palette est **copiée à chaque dessin (donc à chaque partie)** dans la file d'uniforms. Un personnage de
  5 parties à 65 os envoie ≈ 16 Ko par image : sans conséquence pour 10 personnages (160 Ko, la file par défaut
  fait 256 Ko), à surveiller au-delà (une palette partagée entre parties serait la suite logique).

## Ce qui a été vérifié

`tests/test_skinning_gpu.c`, sur SwiftShader (rasteriseur logiciel) : le maillage skinné dessiné avec une palette
**identité** donne la même image que le maillage ordinaire (≤ 2/255) ; avec un os **translaté**, **tourné** ou les
deux, la même image (≤ 3/255) que le même maillage **déformé sur le CPU** par un calcul écrit dans le test ; l'os
d'indice **255** d'une palette de **256** matrices fonctionne (et déplacer l'os 254 change l'image). `tests/
test_skin.c` couvre le chargement. Mutations : voir le message de livraison.

## Ce qui n'a pas été vu

- Aucun personnage réel (Mixamo) : maillage de test = une colonne de 24 sommets, deux os. Les poids multi-os
  sur un vrai maillage à plusieurs milliers de sommets, la déformation d'un coude ou d'un genou, l'aspect
  « candy wrapper » du skinning linéaire : **pas vus**.
- Rien n'est affiché dans la fenêtre du jeu (pas encore branché au launcher : M4-16 et suivants).
- **« 10 personnages à 60 FPS » n'est pas mesuré** : SwiftShader est logiciel, son temps ne dit rien d'un vrai
  GPU. Ce qui est mesuré : le coût **CPU** de 10 personnages de 65 os (échantillonnage de 65 canaux à 60 clés,
  matrices monde, matrices de skinning) = **0,056 ms par image en release, 0,52 ms sous ASan/UBSan** (un cœur,
  `pose.ten_characters_of_65_joints_cost_little_cpu_per_frame`), plus la taille des uniforms (ci-dessus). Le coût
  **GPU** (10 × 65 os × quelques milliers de sommets) n'est pas mesuré : c'est une estimation, très en dessous de
  ce que fait un GPU moderne — à mesurer sur ta machine avec le panneau de statistiques (F1) quand le launcher les affichera.
- Le cuiseur (`assetc mesh`) ne sait pas encore écrire les poids dans un `.mesh` : le personnage se charge
  directement depuis le glTF/GLB. À reprendre avec le pipeline Mixamo (M4-21).
