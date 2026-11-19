# Eau simple (M3-10)

## Ce que c'est

Un plan d'eau à une altitude donnée, dont la **couleur dépend de la profondeur** et qui **reflète le ciel** (Fresnel).
Procédural : aucune texture.

- `engine/water.h` : `EngWaterParams`, `eng_water_default/valid`, `eng_water_body_color`, `eng_water_fresnel`,
  `eng_water_normal`, `eng_water_shade` (référence CPU du shader), `eng_water_pack`, `eng_water_build`,
  `eng_water_wet_cells`, `eng_water_pipeline_desc`.
- `shaders/water.frag.hlsl` : `model.vert` + ce fragment ; il applique ensuite le brouillard de `terrain.frag`.
- Lanceur : `--water` (niveau = altitude min + 20 % de la plage du champ) ou `--water-level M`.

## Le maillage

`eng_water_build` couvre les cellules du champ dont **au moins un coin est strictement sous le niveau** (le terrain
cache le reste du plan). Les sommets sont ceux du champ, à l'altitude `level` ; **`uv.x` porte la profondeur**
(`level − altitude du terrain`, bornée à 0) : le fragment shader n'a donc besoin d'aucune texture ni du champ. La
profondeur est interpolée linéairement entre sommets : exacte sur un fond plan ou incliné, approchée sur un fond
bosselé à l'échelle d'une cellule.

## Le modèle de couleur (formules)

```
t       = 1 − exp(−absorption × profondeur)         0 au bord, tend vers 1 au large
corps   = mélange(couleur peu profonde, couleur profonde, t)
n       = normale du plan perturbée par deux ondes planes (rides)
F       = f0 + (1 − f0) (1 − cos)^5,   cos = <n, vers la caméra>
r       = 2 <n, v> n − v ;  ciel = mélange(horizon, zénith, sqrt(r_y))
éclairé = corps × (ambiante + soleil × <n, soleil> / pi)
couleur = mélange(éclairé, ciel, F) + soleil × F × <r, soleil>^256
```

- **Vu de près** : on voit le fond sombre ou clair selon la profondeur (F ≈ f0 = 0,02) ; le bord, peu profond, est
  de la couleur claire verdâtre.
- **Vu de loin** (regard rasant) : F tend vers 1 et l'eau prend la couleur du ciel à l'horizon, puis le brouillard.
- Les rides : première onde (direction (0,8, 0,6)) de pente `ripple_amplitude`, seconde (direction (−0,6, 0,8),
  1,7 × le nombre d'onde, 1,3 × plus rapide) de pente moitié. La phase est ramenée dans [0, 2 pi[ côté CPU.
- Le ciel réfléchi est le dégradé horizon / zénith de `eng_atmosphere_to_sky_desc` pour le soleil courant, à
  l'exposition du ciel dessiné : l'eau suit l'heure de la journée.

## Limites (dites, pas cachées)

- **Pas de transparence** : le renderer n'a pas de mélange alpha ; le fond n'est pas vu à travers l'eau, c'est la
  couleur de profondeur qui le suggère. La rive est la ligne où le terrain perce le plan, sans écume ni dégradé
  d'alpha.
- Un **seul niveau** par plan ; pas de vagues géométriques, pas de réfraction, pas de reflet du terrain ni des
  objets (seulement du ciel), pas d'ombre sur l'eau.
- Le critère « lac crédible vu de près et de loin » est **subjectif** : vérifié ici par des images de test
  (`OW_INSTANCING_DUMP=<dossier>` écrit `water_calm.ppm`, `water_ripples.ppm`, `water_fog.ppm`; images très sombres sans tonemapping : c'est la sortie linéaire brute du test, le lanceur applique le tonemapping) et par l'égalité
  pixel pour pixel avec la référence CPU ; le jugement final est le tien.

## Tests

- `engine.water` (12, CPU) : paramètres, loi d'absorption, Fresnel, normale (plate sans rides, unitaire, bornée,
  dépend du temps, valeur exacte), composition de `eng_water_shade` (zénith, soleil, regard rasant, reflet du soleil),
  disposition des uniforms et phases, maillage (cellules mouillées exactement, profondeurs aux sommets, sens des
  triangles, creux de 3 m), champ sec (vide) / entièrement mouillé, rejets (arène restituée).
- `engine.water_gpu` (3) : le shader égale `eng_water_shade` sur une grille de pixels (lac calme ; rides et temps) ;
  le temps fait bouger les rides ; plus profond = plus sombre ; avec brouillard, égalité avec
  `eng_water_shade` puis `eng_fog_apply`.
- `launcher.terrain` : `--water` journalise le niveau et le nombre de cellules mouillées.

## Mutations (vérification des tests)

205 mutants de `water.c` (opérateurs inversés), tests CPU et GPU : 25 ne compilent pas (`-Wparentheses` sur `&&` → `||`),
les autres sont tous tués sauf 17, équivalents ou inobservables : égalités de clamp (`x < 0`, `d > 0`, `vl > 1e-12`,
profondeur nulle), bornes min/max `<` → `<=` du rectangle englobant, `+` dans des commentaires, plafonds de
sommets/indices du modèle (jamais atteints par un champ de test), boucles de remplissage `<=` (un tour de plus
dans l'arène, invisible). Les survivants du premier passage avaient révélé de vrais trous, comblés : maillage à
espacement 2 m et origine décalée (positions, profondeurs, aire des triangles, boîte englobante exacte), niveau
exactement à l'altitude du terrain (sec), couleur invariante par translation verticale, validité de chaque canal
de couleur, de chaque scalaire et du ciel passé à `eng_water_pack`, plus petit champ (2 x 2).
