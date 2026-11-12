# Éclairage (M1-13, matériaux PBR depuis M1-15)

Une **lumière directionnelle** (le soleil) et un **environnement** (éclairage basé image, M1-16 :
`docs/ibl.md`), calculés dans le
fragment shader `model.frag` en espace **monde**. Le calcul de surface est celui du PBR
(`docs/pbr.md`) ; ce document décrit les paramètres de lumière (`engine/lighting.h`). Avant M1-15,
le modèle était un Blinn-Phong global (`shininess`, `specular_strength`) : ces deux champs ont
disparu, la brillance vient maintenant du matériau glTF (métal, rugosité).

## Modèle

`couleur = direct + ambiante + émissif`, avec

- `direct = ombre × soleil × N·L × ((1 − F) c_diff + π F D Vis)` (Cook-Torrance GGX) ; `ombre` ∈ [0, 1] est
  le facteur de la carte d'ombre (`docs/shadows.md`), 1 sans ombre ;
- `ambiante = occlusion × ambiante × (c_diff × irradiance(N) + préfiltrée(R, r) × (F0 A + B))`, l'éclairage
  de l'environnement (`docs/ibl.md`) ; avec l'environnement par défaut (ciel uniforme de radiance 1),
  `irradiance = préfiltrée = 1` et on retrouve `c_diff + F0 A + B` d'avant M1-16 ;

où `soleil = sun_color × sun_intensity` et `ambiante = ambient_color × ambient_intensity`
(formules détaillées dans `docs/pbr.md`). **`sun_intensity = 1` : une surface blanche mate éclairée de
face rend 1** (éclairement π).

- Une face tournée à l'opposé du soleil (`N·L ≤ 0`) ne reçoit aucune lumière directe.
- Les valeurs sont en lumière **linéaire** et écrites telles quelles dans la cible **HDR** (RGBA16F,
  M1-14) : une intensité totale > 1 n'est plus écrêtée à ce stade ; l'exposition, le tonemapping et
  le gamma sont appliqués ensuite par la passe de résolution (`docs/tonemapping.md`). Avec un
  renderer sans HDR (repli), la valeur est écrite telle quelle dans la cible 8 bits et sature à 1.

## `EngLighting`

| Champ | Sens | Défaut |
|---|---|---|
| `sun_direction` | du point VERS le soleil, monde, **unitaire** | azimut 30°, hauteur 45° |
| `sun_color`, `sun_intensity` | couleur linéaire, intensité ≥ 0 | blanc, 1 |
| `ambient_color`, `ambient_intensity` | idem pour l'environnement (1 : le ciel tel quel) | blanc, 0,25 (démo : 1) |
| `camera_position` | monde, mètres (pour `V`) : **à mettre à jour à chaque image** | origine |

`eng_lighting_default` remplit ces valeurs ; `eng_lighting_set_sun_angles(l, azimut, hauteur)` oriente
le soleil (0° = vers +Z, 90° = vers +X ; hauteur 90° = zénith, −90° = la lumière vient du sol ;
hauteur bornée, valeur non numérique refusée).

`eng_model_gpu_draw(..., &lighting)` copie l'éclairage dans la file, **après correction** par
`eng_lighting_sanitize` (direction normalisée ou +Y si nulle, valeurs négatives ou non numériques
ramenées à 0) : l'éclairage de l'appelant n'est pas modifié. `NULL` : défaut. Sans
cette correction, une direction non unitaire donnerait des images fausses
ou des NaN, sans message.

## Conventions

- Espace monde (`docs/conventions-maths.md`) : main droite, Y vers le haut. La lumière ne suit pas
  la caméra.
- Normales : `normal_matrix` (inverse transposée du 3x3 du modèle) puis renormalisation par pixel
  (l'interpolation raccourcit les normales). Un modèle à échelle non uniforme est donc bien éclairé.
- La position du fragment vient de `model × position` (`ModelUniforms.model`).

## Régler en direct

Panneau de debug (F1, `docs/debug-ui.md`) : azimut et hauteur du soleil, intensité du soleil,
intensité de l'environnement (`environnement : intensité`, 0–4 ; elle règle aussi le ciel dessiné), et
l'ombre du soleil (`ombre : …`, `docs/shadows.md`). Les cubes texturés de la démo ne sont pas éclairés
(shader `textured.*`) ; seul le modèle glTF (tore et sphère) l'est.

## Vérification

- `tests/test_lighting.c` : maths CPU (angles, correction des valeurs), valeurs calculées **à la main**.
- `tests/test_model_gpu.c` : le shader est comparé aux valeurs à la main et à `eng_pbr_shade` (`docs/pbr.md`)
  sur des pixels lus après un vrai rendu (position monde, position de la caméra, couleurs et intensités
  du soleil et de l'ambiante, face arrière).
- `tests/test_model.c`, `tests/test_render3d.c` : contenu et alignement des blocs d'uniforms.

## Limites

- Une seule lumière directionnelle, une seule carte d'ombre statique (`docs/shadows.md` : pas de
  cascades avant M7, l'éclairage par image n'est pas ombré), pas de lumières ponctuelles, ambiante
  une seule sonde globale statique (`docs/ibl.md`).
- Pas de visualisation de Sponza dans le dépôt (le modèle n'y est pas) : la comparaison avec un
  visualiseur de référence porte sur DamagedHelmet (`docs/pbr.md`).
