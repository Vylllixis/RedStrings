# Brouillard de distance et de hauteur (M3-06)

## Ce que c'est

Le terrain lointain se fond dans le ciel : `terrain.frag` mélange la couleur éclairée de chaque pixel avec une
couleur de brouillard, selon l'épaisseur d'air traversée entre la caméra et le point. Module `engine/fog.h` /
`engine/src/fog.c` (côté CPU, référence) ; `shaders/terrain.frag.hlsl` en recopie les formules.

## Modèle

Densité exponentielle en hauteur, en 1/mètre :

    rho(y) = density x exp(-height_falloff x (y - base_height))

Le long du segment caméra -> point (longueur d, dy = p.y - cam.y), l'épaisseur optique est **l'intégrale exacte**,
sans échantillonnage :

    tau = density x exp(-height_falloff x (cam.y - base_height)) x d x (1 - exp(-x)) / x,   x = height_falloff x dy

(limite `d x density x exp(...)` quand `x -> 0` ; en pratique une série à l'ordre 1 sous `|x| < 1e-4`, où la
formule directe perd sa précision en `float`). Puis :

    f = max_opacity x (1 - exp(-tau))
    couleur = lerp(couleur éclairée, couleur du brouillard, f)

`eng_fog_optical_depth`, `eng_fog_factor` et `eng_fog_apply` (CPU) ; `eng_fog_pack` fabrique les 16 flottants que le
shader lit.

Défauts (`eng_fog_default`) : densité 0,003 /m, décroissance 0,004 /m (hauteur d'échelle 250 m), niveau 0 m,
opacité maximale 1. Au niveau du sol, `f` vaut 0,95 à 1 km. Les valeurs sont des points de départ réglables
(`--fog-density`, tweaks du lanceur), pas des valeurs « justes ».

## Couleur : celle du ciel à l'horizon

La couleur du brouillard est `eng_atmosphere_horizon(...)` (la radiance du ciel de M3-05 juste au-dessus de
l'horizon, dans l'azimut du point) multipliée par l'échelle d'exposition du ciel affiché (`sky_scale`, l'intensité
du ciel du lanceur). Là où le terrain rejoint l'horizon, il a donc exactement la couleur du ciel dessiné : pas de
couture. Le brouillard suit aussi le soleil (halo orangé du côté du soleil couchant, sombre la nuit) sans
paramètre de plus.

## Dans le moteur

- `EngTerrainGpu.fog` : `eng_terrain_gpu_set_fog(gpu, fog, atmosphere, sky_scale)` à chaque image (le soleil bouge).
- 20 flottants depuis M8-07 : le 5e float4 porte la direction du **vrai** soleil
  (`eng_fog_set_sun`). La nuit, la lumière des shaders est la lune (`apply_moonlight`), haute dans
  le ciel : la couleur du brouillard, calculée avec elle, était celle d'un ciel de jour (terrain
  blanc-bleu la nuit, B17). Test : `eng_terrain_gpu.at_night_the_fog_takes_the_sky_of_the_real_sun_not_of_the_moon`.
- Densité par défaut du lanceur : 0,0008 /m (B2 ; le moteur garde 0,003).
  `NULL` ou invalide : brouillard coupé (tout à 0 dans les uniforms ; le shader teste `fog_a.w` et `fog_a.x`).
- `EngModelDrawOptions.fragment_tail` / `fragment_tail_floats` : flottants ajoutés **après** les uniforms
  d'éclairage dans le bloc de fragment (multiple de 4, au plus 64). `eng_model_gpu_draw_ex` les recopie derrière
  l'éclairage de chaque partie. Sans queue, rien ne change pour `model.frag`.
- Lanceur : actif avec `--terrain` quand le ciel atmosphérique l'est ; `--fog-density D` (0 : coupé, refusé hors de
  [0, 1]) ; tweaks « brouillard : densité / décroissance / opacité max ». Le journal d'arrêt écrit la ligne
  `brouillard : densité ... /m, décroissance ... /m, opacité max ...`.

## Preuves

- `engine.fog` (12 tests CPU) : épaisseur uniforme = `density x d` ; formule analytique contre une intégrale
  numérique (point milieu, 20 000 pas) pour 16 couples caméra/point, y compris sous le niveau de base ; symétrie
  caméra <-> point ; continuité de part et d'autre du seuil de la série ; un observateur au-dessus de la couche voit
  moins de brouillard ; `f` croît avec la distance et reste sous le plafond ; entrées invalides ou non finies = pas
  de brouillard ; altitudes extrêmes finies ; couleur d'horizon (azimut, repli vertical, égale au ciel dessiné) ;
  disposition des 16 flottants.
- `engine.terrain_gpu` : le rendu brouillardé égale, pixel pour pixel à 3/255 près, `eng_fog_apply` appliqué au rendu
  sans brouillard (caméra à 400 m au-dessus du sol, densité 0,01) ; densité 0, opacité 0 ou brouillard invalide
  (NaN) donnent EXACTEMENT l'image sans brouillard ; un brouillard très épais donne au sol la couleur d'horizon x
  l'échelle (3/255) ; `dump_a_generated_terrain_for_the_eye` écrit aussi `terrain_fog.ppm` (OW_TERRAIN_DUMP).
- `launcher.terrain` : le journal d'arrêt donne les défauts, `--fog-density 0.0125` est pris en compte,
  `--fog-density 2 | -0.1 | abc` sont refusés (code 2).

## Limites

- Le brouillard n'est appliqué qu'au **terrain**. `model.frag` (modèle, cubes, sol de démonstration) n'en a pas : un
  objet posé sur le terrain lointain ne serait pas brouillardé. Le lanceur ne dessine que le ciel et le terrain avec
  `--terrain` ; les futurs objets (rochers, herbe de M3-07+) devront recopier `apply_fog`.
- Brouillard **homogène par couche** : pas de diffusion vers le soleil propre au brouillard (la couleur est le ciel
  de l'horizon, dont la phase de Mie donne déjà un halo côté soleil), pas de volumétrique ni d'ombres dans la brume.
- La couleur suit le ciel dessiné, pas l'éclairage par image cuit au chargement (limite de M3-05 inchangée).
- Coût non mesuré sur un vrai GPU : par pixel du terrain, quelques `exp`, une racine, un `pow` ; vérifié seulement sur
  SwiftShader (correction, pas performance).
- Le facteur est calculé par pixel avec la position monde du fragment : pas de brouillard par sommet, donc pas
  d'artefacts de maillage, mais le coût suit les pixels et non les sommets.
