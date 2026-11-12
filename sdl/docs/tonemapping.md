# HDR, tonemapping et gamma (M1-14)

La scène n'est plus dessinée directement dans l'écran : elle l'est en **HDR** (texture RGBA16F,
lumière **linéaire**, valeurs au-delà de 1,0 permises), puis une passe plein écran la ramène à
l'écran avec une exposition, une courbe de tonemapping et l'encodage sRGB. Les types et la
référence CPU sont dans `engine/tonemap.h`.

## Chaîne d'une image

```
passe principale  ──►  texture HDR RGBA16F (taille de la swapchain, profondeur comprise)
passe de résolution ─► swapchain : exposition → opérateur → encodage sRGB   (tonemap.frag.hlsl)
passe overlay      ──►  swapchain : interface de debug, dessinée par-dessus, SANS tonemapping
```

- `EngRendererDesc.hdr = true` met tout en place : `eng_gpu_enable_hdr`, un pipeline `tonemap`
  (shaders `tonemap.vert/frag`), un sampler au plus proche. Les pipelines créés **sans**
  `color_format` dessinent alors en RGBA16F (`eng_renderer_scene_format`).
- La résolution est déclenchée par la **fin de la passe principale** (`EngGpu` appelle la fonction
  installée par `eng_gpu_set_resolve`) : aussi bien quand l'overlay s'ouvre que quand l'image se
  termine. Une image sans aucun dessin est donc quand même résolue (fond uni).
- **Repli** : si les shaders `tonemap.*` manquent ou si le GPU ne sait pas dessiner en RGBA16F,
  un avertissement est journalisé et le renderer continue **sans HDR**, comme avant
  (`eng_renderer_hdr_enabled` = `false`).
- La couleur de fond (`eng_renderer_set_clear_color`) est maintenant une valeur de **scène**
  (linéaire), pas une valeur d'écran : `(0,1 ; 0,1 ; 0,1)` donne un gris visiblement plus clair
  qu'avant (≈ 0,35 une fois encodé en sRGB).

## Calcul

```
rgb    = clamp(hdr, 0, 65504) × 2^EV            exposition en EV : +1 EV double la lumière
rgb    = opérateur(rgb)                         vers [0, 1]
sortie = encodage_sRGB(rgb)                     sauf cible déjà _SRGB (le GPU encode alors)
```

| Opérateur | Courbe |
|---|---|
| `ENG_TONEMAP_NONE` | `saturate` : aucune compression, tout ce qui dépasse 1,0 est écrasé (sert de témoin) |
| `ENG_TONEMAP_ACES` (défaut) | approximation de Narkowicz de ACES filmic : `x(2,51x + 0,03) / (x(2,43x + 0,59) + 0,14)`, par canal |
| `ENG_TONEMAP_AGX` | AgX dans l'implémentation de three.js / Filament : matrices sRGB ↔ Rec.2020 et « inset/outset », log2 sur [−12,47393 ; 4,026069] EV, sigmoïde polynomiale d'ordre 6, linéarisation en 2,2. Sans le « look » optionnel |

- ACES par canal **désature** les couleurs très lumineuses vers le blanc de façon moins douce
  qu'AgX ; AgX place le gris moyen 0,18 à ≈ 0,5 une fois encodé en sRGB (test
  `agx_middle_grey_is_about_half_after_srgb_encoding`). Aucun des deux n'est « le bon » : ils se
  changent en direct dans le panneau (`tonemap : 0 aucun, 1 ACES, 2 AgX`, `exposition (EV)`).
- Entrées : négatif et NaN → 0 ; au-delà de 65504 (plus grand RGBA16F fini) → 65504. Sans ce
  plafond, un débordement `+inf` donnait `inf/inf = NaN` et un pixel **noir** avec ACES (défaut
  trouvé par les tests, corrigé en C et en HLSL).
- Exposition bornée à ±10 EV, valeur non finie → 0 (`eng_tonemap_sanitize`, appliquée par
  `eng_renderer_set_tonemap`, qui copie).

## Gamma et textures

- Tout le calcul de lumière est **linéaire** : les textures de couleur sont déjà créées en
  `ENG_TEXTURE_RGBA8_SRGB` (décodées par le GPU à la lecture, M1-08), les facteurs de matériau glTF
  et les couleurs de lumière sont linéaires. Le seul encodage sRGB est celui de la résolution.
- Le shader encode lui-même quand la cible est `UNORM` (cas de la swapchain en SDR) et laisse le GPU
  encoder quand elle est `*_UNORM_SRGB` (`R8G8B8A8` et `B8G8R8A8`) : jamais de double encodage.
- Ce qui n'a pas été converti : les couleurs **écrites à la main** dans des sommets (cubes de démo,
  lignes de debug) sont lues comme linéaires ; elles passent aussi par l'exposition et le
  tonemapping, donc leur teinte dépend de l'opérateur (une ligne rouge pure (1,0,0) devient environ
  (0,80 ; 0 ; 0) en ACES avant encodage). Un overlay de debug à couleurs exactes demanderait une
  seconde passe après la résolution.

## Vérification

- `tests/test_tonemap.c` (CPU) : valeurs ACES, NONE et sRGB **calculées à la main** ; AgX comparé à
  une implémentation Python indépendante ; monotonie, bornes, entrées non numériques.
- `tests/test_tonemap_gpu.c` : une texture RGBA16F aux valeurs exactes (2 × 2 texels lus au plus
  proche, donc quatre quadrants, aussi pour l'orientation et l'ordre des canaux) est résolue par le
  vrai pipeline vers : RGBA8, RGBA8_SRGB, BGRA8 (avec et sans _SRGB) et **RGBA32F** (comparaison
  au millième, sans arrondi 8 bits, y compris sur le segment linéaire de l'sRGB). La passe principale
  du moteur est vérifiée en relisant la texture HDR (valeurs 4,0 et 0,5 exactes, donc ni écrêtées
  ni arrondies). Chaîne complète : modèle éclairé en PBR (rouge ≈ 3,4, bien au-dessus de 1) → HDR → pixels.
- `tests/test_gpu.c` : ordre résolution → overlay, une seule résolution par image, redimensionnement,
  désactivation.

## Limites connues

- Rendu sur la vraie fenêtre non vérifié par lecture de pixels (la swapchain ne se relit pas) : la
  résolution est vérifiée vers des textures, et l'enchaînement sur la swapchain par compteurs.
- Pas d'auto-exposition, pas de bloom, pas d'anti-crénelage : l'exposition est un réglage manuel.
- Pas de dithering : une dégradé très lent peut montrer des bandes en 8 bits.
- AgX sans « look » ; ACES par canal (pas la chaîne complète AP1/RRT/ODT).
- Un `EngRenderer` HDR consomme une texture RGBA16F supplémentaire (8 octets par pixel, soit 16 Mio
  en 1920 × 1080) et une passe plein écran de plus.
- Le repli sans HDR (échec de `eng_gpu_enable_hdr` sur un GPU sans RGBA16F) n'est pas couvert par un
  test : aucun GPU de test ne refuse ce format. Seul le repli « shaders absents » l'est.
