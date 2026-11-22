## Maths (M1-06)

Les maths passent par `engine/math.h` (cglm v0.9.7). Ne jamais inclure `<cglm/cglm.h>` directement :
`engine/math.h` refuse de compiler si la configuration de cglm n'est pas celle du projet.

| Sujet | Convention |
|---|---|
| Repère | main droite : X × Y = Z |
| Axes du monde | +X droite, +Y haut, **-Z devant** (`ENG_AXIS_RIGHT`, `ENG_AXIS_UP`, `ENG_AXIS_FORWARD`) |
| Unité | 1 unité = 1 mètre ; angles en **radians** (`glm_rad` pour convertir des degrés) |
| Rotations | règle de la main droite : +90° autour de Y envoie +Z sur +X |
| Matrices | colonne-majeur (`m[colonne][ligne]`, translation dans `m[3]`), même disposition en mémoire que GLSL/HLSL par défaut |
| Produit | `a * b` applique `b` d'abord : `proj * view * model` (`glm_mat4_mul(proj, view, pv)`) |
| Vecteurs | colonnes : `clip = M * v` (`glm_mat4_mulv`) |
| Espace de découpage | SDL_GPU, tous pilotes : x, y dans [-1, 1], **y vers le haut**, **profondeur z dans [0, 1]** (0 = plan proche, 1 = plan lointain ; pas [-1, 1] comme OpenGL) |
| Face avant | sens **antihoraire** à l'écran (y vers le haut) : valeur par défaut de `EngFrontFace` |
| Caméra | `eng_look_at(eye, target, up, view)` ; `eng_perspective(fovy, aspect, near, far, proj)` avec `fovy` vertical, `aspect` = largeur / hauteur |

**Mise en place technique** : CMake définit `CGLM_FORCE_DEPTH_ZERO_TO_ONE` sur la cible `engine` (donc
pour tout ce qui la lie) ; cglm est en main droite par défaut, `CGLM_FORCE_LEFT_HANDED` est interdit
(`#error`). `eng_project_point(m, p, ndc)` projette un point (faux s'il est derrière la caméra), utile
pour les tests, le picking et le culling.

**Côté shader** (confirmé en M1-07) : une matrice `mat4` de cglm est copiée telle quelle dans les
uniforms ; en HLSL on écrit `mul(mvp, float4(position, 1.0))`. Preuve : `tests/test_render3d.c`
(translation de +0,5 en x = +16 pixels sur 64 ; projection en perspective qui tombe sur le pixel prédit
par `eng_project_point`). Confirmé uniquement par SwiftShader/SPIR-V ; le chemin D3D12 (DXIL) et Metal
n'a jamais tourné.

**Profondeur** : plage [0, 1], comparaison LESS, effacement à 1,0, découpage aux plans proche et
lointain actif (voir `docs/renderer.md`).

**Tests** : `tests/test_math.c` (14 tests, valeurs calculées à la main) : axes et produit vectoriel,
disposition colonne-majeur, rotations, vue (caméra par défaut = identité, +X → droite), profondeur 0 à
1 avec formule `f(d-n) / (d(f-n))`, x vers la droite et y vers le haut, aspect, points derrière la
caméra, composition `proj * view * model`, sens antihoraire conservé (et inversé vu de dos), projection
inverse.
