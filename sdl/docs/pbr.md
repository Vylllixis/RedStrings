# Matériaux PBR métal / rugosité (M1-15)

Le fragment shader `model.frag` évalue le modèle de matériau de glTF 2.0 (annexe B de la
spécification) : **Cook-Torrance** à distribution **GGX**, **normal map**, **occlusion**, **émissif**.
Les formules sont écrites deux fois, dans le shader et dans `engine/pbr.h` (`eng_pbr_shade`), que les
tests comparent pixel à pixel. Les paramètres d'éclairage sont décrits dans `docs/lighting.md`.

## Formules

`N` normale de shading (après normal map), `L` vers le soleil, `V` vers la caméra, `H = normalize(L + V)`.
`α = max(rugosité, 0,045)²` (le plancher évite `D` infini sur un miroir parfait).

```
D     = α² / (π ((N·H)² (α² − 1) + 1)²)                       GGX
Vis   = 0,5 / (N·L √((N·V)² (1 − α²) + α²) + N·V √((N·L)² (1 − α²) + α²))   Smith corrélé
F     = F0 + (1 − F0) (1 − V·H)⁵                              Schlick
F0    = lerp(0,04, base, métal)        c_diff = base (1 − métal)
direct   = soleil × N·L × ((1 − F) c_diff + π F D Vis)     si N·L > 0, sinon 0
ambiante = occlusion × ambiante × (c_diff × irradiance(N) + préfiltrée(R, r) × (F0 A + B))   (A, B : LUT, `docs/ibl.md`)
couleur  = direct + ambiante + émissif
```

- **Unités.** `sun_intensity = 1` vaut un éclairement de π : une surface blanche mate éclairée de
  face rend 1. Le diffus est `c_diff/π × π × intensité`, d'où l'absence de `π` sur le terme diffus ;
  le terme spéculaire garde son `π` pour la même raison.
- **Fresnel et diffus** : le diffus est pondéré par `(1 − F)` (c'est le mélange de Fresnel de la
  spécification, `fresnel_mix`), pas seulement par `(1 − métal)`.
- **Ambiante.** Depuis M1-16, éclairage basé image (`docs/ibl.md`) : le diffus vaut
  `c_diff × irradiance(N)`, le spéculaire `préfiltrée(R, r) × (F0 A + B)`, avec `(A, B)` lus dans une
  LUT du BRDF intégrée numériquement (même visibilité de Smith que le soleil). Avant M1-16 (ciel
  uniforme), `(A, B)` venait de l'approximation analytique de Karis, qui s'écartait de l'intégrale
  jusqu'à 15 % à rugosité 0,6. `eng_pbr_shade` garde l'ambiant uniforme (cas sans environnement) ;
  `eng_pbr_shade_ibl` (`engine/ibl.h`) est la référence CPU de l'éclairage basé image. L'occlusion ne touche que cette part (elle n'existe pas pour une lumière directe) :
  `occlusion = 1 + force × (texture.r − 1)`.
- **Émissif** : `facteur × texture` (× `KHR_materials_emissive_strength` s'il est présent), ajouté
  tel quel après l'éclairage.

## Matériau glTF et textures

| glTF | Usage | Format GPU |
|---|---|---|
| `baseColorTexture` × `baseColorFactor` | couleur de base | RGBA8 **sRGB** |
| `metallicRoughnessTexture` (G rugosité, B métal) × facteurs | métal, rugosité | RGBA8 **linéaire** |
| `normalTexture` (+ `scale`) | normal map, espace tangent | RGBA8 linéaire |
| `occlusionTexture` (R) (+ `strength`) | occlusion ambiante | RGBA8 linéaire |
| `emissiveTexture` × `emissiveFactor` | émissif | RGBA8 **sRGB** |

- Un matériau sans bloc `pbrMetallicRoughness`, ou sans ces facteurs, prend les défauts de la
  spécification : **métal 1, rugosité 1** (un métal très rugueux, sombre sans ambiante : ce n'est pas un
  oubli, c'est la spécification). Le matériau 0 du moteur (primitive sans matériau) fait de même.
- Une même image peut servir à deux rôles de familles différentes (par ex. couleur de base et
  métal/rugosité) : elle est alors envoyée deux fois au GPU, une fois par format
  (`EngModelGpu.textures` et `.linear_textures`). Une image qu'aucun matériau n'utilise n'a pas de pixels.
- Un rôle sans image reçoit une texture blanche 1 × 1 : le facteur seul compte. `scale` et `strength`
  ne sont lus que si la texture existe (cgltf les laisse à 0 sinon).
- Le shader lie **cinq** textures par dessin, chacune avec son sampler (SDL_GPU exige un couple
  texture + sampler par emplacement) : `EngDrawCmd.texture` (emplacement 0) puis
  `extra_textures[0..3]` (emplacements 1 à 4, sans trou). Le sampler est le même partout.

## Tangentes et normal map

L'espace tangent d'un sommet est `(T, B = cross(N, T) × w, N)`.

- **TANGENT** du fichier s'il existe (transformée par la matrice du nœud, normalisée ; le shader la
  projette sur le plan tangent).
- Sinon, **calculées** à partir des UV : pour chaque triangle, `T = dP/du` et `B = −dP/dv` (la
  bitangente d'une normal map glTF pointe vers les `v` décroissants, puisque `v` descend dans l'image),
  sommés sur les sommets sans pondération, orthogonalisés à la normale (Gram-Schmidt) ; `w` est le
  signe de `cross(N, T)·B`, ce qui prend en charge les UV en miroir. UV dégénérés ou somme nulle : un
  axe quelconque orthogonal à la normale, `w = 1`.
- Le shader reprend `T` de l'interpolation, le réorthogonalise à la normale **interpolée** (par pixel) et
  applique `tn = ((2 s.xy − 1) × scale, 2 s.z − 1)` puis `normalize(T tn.x + B tn.y + N tn.z)`.
  La normal map n'est lue que si `has_normal_map` (`emissive.w` du cbuffer) vaut 1.
- La tangente est une **direction de la surface** : elle suit la matrice du modèle, pas la matrice
  normale.

## Vérification

- `tests/test_pbr.c` : D, Vis, F, (A, B), normal map, `eng_pbr_shade` contre des valeurs **calculées à
  la main** puis contre `tools/reference` (Python indépendant en double précision, mêmes formules
  écrites d'après la spécification) ; cas limites (rugosité 0, vue rasante, face arrière, entrées non
  numériques).
- `tests/test_model_gpu.c` : le shader contre `eng_pbr_shade` sur des pixels lus après un vrai rendu :
  facteurs, pic spéculaire, face arrière, positions monde et caméra, couleurs du soleil, chaque texture
  avec son format (sRGB ou linéaire : une lecture dans le mauvais format est mesurée et rejetée), normal
  map (fichier et calculée, `w = −1`), occlusion (ambiante seule), émissif.
- `tests/test_model.c` : matériaux, tangentes (fichier, calculées, UV en miroir, cisaillés, dégénérés).
- `tests/test_texture.c` : liaison de cinq textures par dessin (couples texture + sampler, trous et
  handles périmés refusés).

### Comparaison avec un visualiseur de référence (acceptation)

DamagedHelmet (Khronos glTF-Sample-Assets), rendu 512 × 512 en lumière linéaire sans tonemapping, par
le moteur et par **three.js r170** (`MeshStandardMaterial`, WebGL dans Chromium / SwiftShader), mêmes
caméras, mêmes soleils (`DirectionalLight` d'intensité π × intensité du moteur). Quatre points de vue
et directions de soleil, **sans ambiante** (three.js n'a pas de spéculaire d'ambiante sans carte
d'environnement) : le test couvre donc le direct (Cook-Torrance, normal map, textures et leurs formats,
émissif), pas encore l'ambiante ni l'occlusion (IBL : M1-16). Reproductible : `tools/reference/README.md`.

| three.js | part de pixels à ±4 niveaux | MAE (sur 255) | PSNR linéaire |
|---|---|---|---|
| tel quel | 60 à 85 % | 2,3 à 4,2 | 25 à 31 dB |
| aligné sur l'annexe B (voir ci-dessous) | **99,5 à 99,6 %** | **0,12 à 0,25** | **45 à 52 dB** |

three.js diffère de la spécification sur deux points que le moteur ne reproduit pas ; chacun a été
isolé en ne corrigeant que lui :

- le diffus direct n'est pas pondéré par `(1 − F)` (spécification : `fresnel_mix`) : à lui seul, il
  explique l'essentiel de l'écart sur les surfaces diélectriques claires (la peinture blanche : −4 à −5 %
  de luminance dans le moteur) ;
- la rugosité est relevée (`max(r, 0,0525)` puis `+ geometryRoughness`, un anti-aliasing spéculaire
  fondé sur les dérivées de la normale) : les reflets sont plus larges et moins intenses (jusqu'à −10 %
  d'énergie totale sur les vues les plus brillantes).

« Aligné » désigne ces deux corrections appliquées au shader de three.js (`tools/reference/page.html`).
Ce n'est donc pas un visualiseur de référence pris tel quel : la comparaison valide tout le reste (GGX,
Smith, Schlick, tangentes calculées contre les dérivées d'écran de three.js, formats de texture, sRGB,
mips, orientation des UV) mais pas ces deux choix.

## Limites

- Cinq textures par dessin avec **un seul sampler** : le sampler glTF de chaque texture (adressage,
  filtres) est ignoré (répétition, trilinéaire, anisotropie).
- Ni `alphaMode` (masque, mélange), ni double face, ni `KHR_texture_transform`, ni autres extensions
  (clearcoat, transmission, spéculaire, ior…). Un seul jeu d'UV (`TEXCOORD_0`).
- Éclairage basé image : limites dans `docs/ibl.md` (pas de diffusion multiple, une sonde globale
  statique). Seule la lumière directe du soleil est ombrée (M1-17, `docs/shadows.md`).
- Les tangentes calculées sont une somme par triangle des dérivées ; dans une zone aux UV en miroir,
  `w` peut différer de la reconstruction par dérivées d'écran de certains visualiseurs.
- Les mips d'une normal map sont filtrés comme des couleurs, sans renormalisation.
- L'occlusion n'est vérifiée que sur l'ambiante seule (cas sans soleil et cas combiné contre la
  référence CPU) ; la comparaison avec un visualiseur externe de l'éclairage basé image est dans
  `docs/ibl.md`.
