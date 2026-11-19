# Instancing GPU et rochers procéduraux (M3-07)

## Ce que c'est

Dessiner 50 000 rochers sans 50 000 dessins : un maillage, un buffer d'instances, **un dessin**. Le GPU lit les
données d'une instance (position, échelle, rotation, teinte) dans un second flux de sommets, avancé une fois par
instance et non par sommet.

- Renderer (`engine/renderer.h`) : `EngPipelineDesc.instance_stride` / `instance_attributes` /
  `instance_attribute_count` (le flux 1) ; `EngDrawCmd.instance_buffer` et `instance_count` ;
  `EngRendererStats.instances`.
- `engine/instancing.h` : `EngInstance` (36 octets), `eng_instanced_pipeline_desc`, `eng_instance_make_yaw`,
  `eng_instance_matrix` (référence CPU), `eng_instance_valid`, `eng_instances_upload`.
- `shaders/instanced.vert.hlsl` : mêmes sorties que `model.vert` ; le fragment shader est `model.frag`, inchangé
  (même matériau PBR, même éclairage par image, même ombre reçue si fournie).
- `eng_model_gpu_draw_ex` : `EngModelDrawOptions.instance_buffer` / `instance_count` ; `model_matrix` est ignorée.
- `engine/rock.h` : rocher procédural (voir plus bas).

## Données d'une instance

| Champ | Type | Attribut | Rôle |
|-------|------|----------|------|
| `position` + `scale` | 4 × float | `TEXCOORD4` | position monde (m), échelle **uniforme** > 0 |
| `rotation` | 4 × float | `TEXCOORD5` | quaternion unitaire (x, y, z, w) |
| `tint` | 4 × octet | `TEXCOORD6` | facteur RGBA linéaire, multiplié à la couleur du matériau |

Le sommet monde vaut `rotation(position_locale × scale) + position`. Une échelle uniforme évite d'avoir à inverser
la matrice pour les normales : la normale est simplement tournée. 50 000 instances = 1,8 Mo.

## Règles du renderer

Le renderer **rejette** (compte dans `rejected_commands`, un seul avertissement) un dessin dont :

- le pipeline a un flux d'instances mais pas de `instance_buffer`, ou un buffer périmé ;
- `instance_count` est 0 avec un flux d'instances ;
- `instance_count × instance_stride` dépasse la taille du buffer (le GPU lirait hors du buffer) ;
- un `instance_buffer` est fourni à un pipeline sans flux d'instances.

À la création, un pipeline est refusé si le flux est incohérent : stride sans attribut ou l'inverse, attribut de
format nul, offset ≥ stride, deux attributs à la même `location` (y compris celles du flux de sommets), flux
d'instances sans flux de sommets.

## Rocher procédural

`eng_rock_build` : icosaèdre subdivisé `n` fois (10 × 4ⁿ + 2 sommets, 20 × 4ⁿ triangles ; n = 1 : 42 / 80),
chaque sommet déplacé radialement par un bruit de valeur 3D à deux octaves (déterministe, graine), puis aplati
verticalement. Surface **fermée** (chaque arête partagée par exactement deux triangles orientés de façon
cohérente, Euler V − E + F = 2), étoilée par rapport à l'origine (tous les triangles orientés vers l'extérieur),
normales lissées par somme des normales de face pondérées par l'aire. Mêmes paramètres : mêmes octets. Matériau par
défaut : gris, rugosité 0,9, non métallique, sans texture.

## Dans le lanceur

`--rocks N` (0 à 1 000 000) avec `--terrain` : N rochers de sous-division 1 posés au hasard (graine fixe) sur le
terrain, enfoncés de 15 % de leur échelle, teintes grises variées, **un dessin par image**. Le journal d'arrêt écrit
`rochers : N instances, 1 dessin par image`. Ce placement est volontairement naïf (uniforme) : le placement réel
(pente, altitude, splatmap) est M3-08.

## Preuves

- `engine.instancing_gpu` :
  - 9 instances en un dessin = 9 dessins ordinaires du même maillage avec `eng_instance_matrix`, **pixel pour pixel à
    2/255 près** (le vertex shader instancié égale le chemin ordinaire) ; un seul dessin dans la file ;
  - la teinte d'instance égale un matériau de couleur multipliée ;
  - **50 000 rochers = 1 dessin, 50 000 instances** (statistiques du renderer), image non vide ; le temps est écrit
    sur stderr (informatif) ;
  - dessins incohérents rejetés (6 cas) et exactement-la-capacité accepté ; flux et instances invalides refusés.
- `engine.instancing` (CPU) : 36 octets et offsets des attributs lus par le shader ; rotation d'axe Y ; matrice =
  translation × rotation × échelle pour un quaternion quelconque ; validité.
- `engine.rock` (CPU) : formules de comptage jusqu'à n = 4, surface fermée et orientée (O(T²) sur 1280 triangles),
  triangles et normales vers l'extérieur, sphère exacte à rugosité 0 (volume par divergence), bornes selon
  rayon/rugosité/aplatissement, déterminisme et graines différentes, matériau, paramètres invalides, arène trop petite
  restituée.
- `launcher.terrain` : `--rocks 2000` journalise 2000 instances, sans ERROR ; `--rocks 1000001 | -5 | abc` refusés.

## Limites (honnêtes)

- **60 FPS non vérifié.** Le seul rasteriseur disponible ici est SwiftShader (logiciel, avec ASan en debug) : le
  lanceur avec 50 000 rochers (4 millions de triangles) a pris environ 5 s par image dans cette configuration, ce
  qui ne dit **rien** d'un vrai GPU. Ce qui est établi : 1 dessin, 1 buffer de 1,8 Mo, aucun uniform par instance,
  aucune allocation par image. À mesurer sur la machine de Théo (`--rocks 50000`, F3 / profiler).
- Pas de culling par instance (le GPU découpe ce qui est hors champ mais le vertex shader tourne pour tous les
  sommets) ; pas de LODs : les rochers lointains coûtent autant que les proches. Un culling par frustum de blocs
  d'instances, ou des LODs, sont les étapes suivantes si la mesure sur GPU réel le demande.
- Les rochers **ne projettent ni ne reçoivent d'ombre** ici (le pipeline d'ombre n'a pas de variante instanciée) et
  ne sont **pas brouillardés** (`model.frag` n'a pas le brouillard du terrain, voir docs/fog.md) : lointains, ils
  tranchent sur le terrain brumeux.
- Échelle uniforme seulement ; pas de teinte par instance dans le matériau (multiplication de la couleur de base).
- Placement du lanceur : uniforme et naïf (M3-08 fait le vrai).
