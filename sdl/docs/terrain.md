# Terrain : champ de hauteurs (M3-01)

Le terrain de M3 part d'une grille de hauteurs : `engine/heightmap.h`. Décision et alternatives :
[ADR 0014](adr/0014-champ-de-hauteurs-16-bits-et-fastnoiselite.md). Les chunks (M3-02), la splatmap (M3-03) et le culling (M3-04) lisent cette grille.

## Conventions

| Sujet | Convention |
|---|---|
| Grille | carrée, `size` × `size` échantillons, `2 <= size <= 8193` |
| Position | l'échantillon `(x, z)` est en `(x * spacing, z * spacing)` mètres ; **x = colonne, z = ligne** ; la ligne 0 est la plus petite coordonnée z. L'origine est un coin de la grille, pas son centre (le placement dans le monde est du ressort de M3-02) |
| Valeur | `uint16` v ; altitude = `altitude_min + v / 65535 * (altitude_max - altitude_min)` mètres. `altitude_max > altitude_min` strictement ; les altitudes négatives sont permises |
| Taille conseillée | `size = k * C + 1` pour des chunks de `C` cellules : 1025 = 16 × 64 + 1, soit 16 × 16 chunks de 64 m à 1 m d'échantillonnage = **1 km²**. Les chunks voisins partagent leur ligne de bord, sans couture |
| Mémoire | `2 * size²` octets : 2 Mio à 1025², 128 Mio à 8193² |

## Dépendance

FastNoiseLite (version C, MIT, commit `785f37a`) est récupéré par `FetchContent` à la configuration : le
premier `cmake --preset ...` après l'installation de M3-01 a besoin du réseau (une fois).

## Génération

```
assetc heightmap terrain.hmap --png terrain.png            # 1025², 1 m, 0 à 120 m, graine 1337
assetc heightmap mont.hmap --size 2049 --alt-max 900 --ridged --wavelength 800 --seed 7
assetc heightmap-import peint.png peint.hmap --spacing 2 --alt-min 0 --alt-max 400
assetc info terrain.hmap
```

| Option | Défaut | Rôle |
|---|---|---|
| `--size N` | 1025 | côté en échantillons |
| `--spacing M` | 1 | mètres entre échantillons |
| `--alt-min A`, `--alt-max B` | 0, 120 | altitudes de la valeur 0 et de la valeur 65535 |
| `--seed S` | 1337 | graine du bruit |
| `--wavelength W` | 512 | mètres : taille des plus grands reliefs |
| `--octaves K` | 6 | 1 à 12 ; chaque octave ajoute des détails deux fois plus fins (lacunarité) |
| `--lacunarity L`, `--gain G` | 2, 0,5 | facteur de fréquence et d'amplitude d'une octave à la suivante |
| `--ridged` | FBM | crêtes (montagnes) au lieu de collines arrondies |
| `--png F` | | écrit aussi un PNG 16 bits (interopérabilité, aperçu) |

Le bruit est OpenSimplex2 (FastNoiseLite). Il est calculé **en mètres** (`x * spacing`) : changer
l'espacement change la résolution, pas la forme du monde. Il est ensuite **étalé sur toute la plage** : la
valeur la plus basse devient 0, la plus haute 65535, quelle que soit la graine. Conséquence : l'altitude
demandée est toujours atteinte, et un terrain plus doux se règle par la plage d'altitudes, pas par
l'amplitude du bruit. Un bruit constant donne un champ nul.

**Déterminisme.** Mêmes options, mêmes octets (testé : `assets.heightmap_cli`), pour un même binaire. D'un
compilateur ou d'une machine à l'autre, des arrondis flottants peuvent décaler quelques valeurs
(non mesuré : je n'ai testé qu'une plateforme). Ne comptez pas sur la graine pour retrouver un terrain :
le `.hmap` fait foi.

## Fichiers

**`.hmap`** : conteneur de `engine/asset_format.h` (magic `OWHF`, version 1, petit-boutiste), section `INFO`
(16 octets : `size` u32, `spacing` f32, `altitude_min` f32, `altitude_max` f32) puis `DATA` (`size²` u16).
Le lecteur refuse : conteneur invalide, section absente, `INFO` ≠ 16 octets, taille hors de [2, 8193],
espacement ≤ 0 ou non fini, altitudes non finies ou `max ≤ min`, `DATA` de taille ou de compte différent de
`size²`. Les échantillons sont lus **en place** (pas de copie). `assetc info` l'affiche et le valide.

**PNG 16 bits** : import (`eng_heightmap_import_image`, `assetc heightmap-import`) d'une image carrée ; une
image en couleurs est convertie en niveaux de gris ; une image **8 bits est étendue à 16 (v × 257) mais n'a
que 256 niveaux** (marches visibles sur les pentes douces : exportez en 16 bits depuis votre éditeur). Export
(`eng_heightmap_encode_png`, `--png`) : PNG gris 16 bits sans compression (taille ≈ celle des données) ; il
ne porte ni l'espacement ni les altitudes, à redonner à l'import.

## API

| Fonction | Rôle |
|---|---|
| `eng_heightmap_default_params`, `eng_heightmap_params_valid` | paramètres par défaut ; validation (journalisée) |
| `eng_heightmap_generate` | bruit → champ, alloué dans une arène (`2 * size²` octets, plus une ligne de travail de `4 * size` octets) |
| `eng_heightmap_altitude(map, x, z)` | altitude d'un échantillon ; `altitude_min` hors grille |
| `eng_heightmap_sample(map, x_m, z_m)` | altitude **bilinéaire** en mètres ; hors grille ramenée au bord ; NaN → coin (0, 0) |
| `eng_heightmap_extent` | côté en mètres : `(size - 1) * spacing` |
| `eng_heightmap_encode/open/save/load` | `.hmap` en mémoire / sur disque |
| `eng_heightmap_import_image`, `eng_heightmap_encode_png` | PNG |

## Maillage par chunks (M3-02)

`engine/terrain.h` transforme un champ en `EngModel` (donc dessiné par `eng_model_gpu_*` : PBR, ombre du
soleil, éclairage par image) : **un seul tableau de sommets** (un par échantillon), **un seul tableau
d'index**, **une partie par chunk** (`chunk_cells` × `chunk_cells` cellules, 64 par défaut ; 1025² donne
16 × 16 = 256 chunks). `EngTerrain.chunks[i]` (boîte englobante exacte) correspond à `model.parts[i]`,
`i = cz * chunks_per_side + cx`.

- **Sans couture par construction.** Deux chunks voisins partagent la rangée de sommets de leur bord (mêmes
  positions, normales, UV, tangentes). Les normales sont des différences centrées sur la grille **globale**
  (unilatérales sur le bord du terrain), jamais calculées par chunk. Le test paraboloïde en donne la preuve
  analytique ; le test GPU, une preuve visuelle (voir plus bas).
- **Repère.** Sommet `(gx, gz)` en `x = origin_x + gx·spacing`, `y = altitude`, `z = origin_z + gz·spacing` ;
  triangles anti-horaires vus d'en haut ; diagonale des cellules en damier. UV = `(gx, gz)/(size-1)` sur
  tout le terrain (coordonnées de la splatmap de M3-03). Tangente : sens des `u`, `w = +1`.
- **Mémoire.** ≈ 48 octets par échantillon (sommets) + 24 (index) : 75 Mio à 1025² dans l'arène fournie ;
  le modèle CPU est libéré après l'envoi au GPU. Limite : `size ≤ 1673` (4 194 304 sommets du modèle) ;
  au-delà, le streaming de M7.
- **Matériau.** `materials[1]` : vert-gris, rugosité 0,95, métal 0 (remplaçable par `params.material`, sans
  texture). La splatmap de M3-03 le remplace par un shader de terrain.

### Dans le lanceur

```
assetc heightmap terrain.hmap            # 1025², 1 m, 0–120 m (voir plus haut)
launcher --terrain terrain.hmap
```

La scène affiche alors **uniquement** le ciel et le terrain mélangé (splatmap par défaut calculée au chargement, 4 Mio) (centré sur l'origine, caméra à 40 m au-dessus
du centre, plan lointain 3000 m, vitesse 40 m/s ; molette pour changer). Pas d'ombre portée pour l'instant :
la carte d'ombre actuelle couvre 42 m autour de l'origine, la passe d'ombre est désactivée en mode terrain
(cascades : tâche ultérieure). Le maillage CPU demande 75 Mio, donc l'arène de niveau passe de 64 à 256 Mio
avec `--terrain`. Un fichier absent ou invalide est une ERREUR journalisée ; la démonstration reste affichée.

## Matériaux : splatmap et triplanaire (M3-03)

**Splatmap** (`engine/splatmap.h`). Un texel RGBA 8 bits par sommet du terrain : R herbe, G roche, B terre,
A neige ; les quatre poids somment **exactement 255**. Règles (fonction pure `eng_splat_weights`) :

| Couche | Poids |
|---|---|
| roche | `smoothstep(pente, 30°, 50°)` (pente = angle de la surface avec l'horizontale, différences centrées) |
| neige | `smoothstep(altitude relative, 0,70, 0,85) × (1 − roche)` : pas de neige sur une falaise |
| terre | plaques d'un bruit FBM de 24 m de longueur d'onde (30 % des surfaces plates par défaut) × `(1 − roche − neige)` |
| herbe | le reste |

Tous les seuils sont des paramètres (`EngSplatParams`) ; `eng_splatmap_generate` coûte 4 octets par échantillon.

**Textures de couche** (`engine/terrain_texture.h`). Quatre albédos 256² générés par le code, **tileables par
construction** (bruit de valeurs périodique, fonction de `(x/n, y/n)`) : herbe, roche, terre, neige. Ce sont des
**textures de remplacement** : l'art définitif passera par les assets (`EngTerrainGpuParams.layer_pixels` accepte
n'importe quels pixels).

**Shader `terrain.frag`** (`engine/terrain_gpu.h`). Copie des formules d'éclairage de `model.frag` (PBR, IBL, ombre
PCF) ; seule la couleur de base change : `Σ poids × albédo de la couche`. Chaque couche est lue par un mapping
**triplanaire** : trois projections sur les plans du monde (YZ, XZ, XY), en mètres divisés par la longueur de
répétition (8 m par défaut), mélangées par `|n|^netteté` (4 par défaut). Sur le plat, seule la projection du dessus
compte (texture à l'échelle exacte) ; sur une falaise, la projection de côté prend le relais, donc **aucune texture
n'est étirée le long de la pente** (critère d'acceptation). La splatmap est lue avec `uv·(size−1)+½)/size` : le texel
`i` est exactement au sommet `i`. Le coût : 12 lectures de texture par pixel (4 couches × 3 projections).
Slots (même sampler) : 0 splatmap, 1 à 4 couches, 5 à 7 environnement, 8 ombre.

**Duplication assumée.** HLSL sans `#include` dans notre chaîne : `terrain.frag` répète le code d'éclairage de
`model.frag`. Le test `a_uniform_layer_is_lit_exactly_like_a_plain_model` compare les deux pixel pour pixel (écart
≤ 2/255) : toute dérive les sépare.

**API de dessin.** `eng_model_gpu_draw_ex` (options : textures de remplacement, sampler, trois valeurs libres pour
le shader, **liste de parties à dessiner**) sert au terrain ; `eng_terrain_gpu_draw(..., chunks, count)` dessine
seulement les chunks listés (le culling de M3-04 n'aura qu'à fournir la liste).

## Culling par frustum (M3-04)

`engine/frustum.h`. Les six plans de la pyramide de vision sont lus dans la matrice vue-projection
(Gribb–Hartmann, profondeur [0, 1]) et normalisés ; chaque plan est `(n, d)` avec `n` unitaire tourné vers
l'intérieur, un point `p` est visible si `n·p + d >= 0`. Extraire les plans de la matrice telle quelle rend le test
indépendant de la convention de projection. Une matrice non finie ou dégénérée donne une pyramide qui **rejette
tout** (chaque plan vaut −1), jamais une pyramide qui accepte tout.

Test d'une boîte (`eng_frustum_classify_aabb`) : pour chaque plan, le coin le plus avancé selon la normale ; si
même celui-là est dehors, la boîte est `OUTSIDE`. Le test est **conservatif** : une boîte rejetée est
forcément invisible ; une boîte acceptée peut ne rien montrer (grande boîte près d'une arête de la pyramide).
Une boîte qui touche exactement un plan est acceptée. Boîte inversée ou non finie : `OUTSIDE`.

Le lanceur garde les boîtes des chunks (copiées du `EngTerrain` CPU), calcule la pyramide à chaque image et
ne dessine que les chunks visibles (`eng_terrain_gpu_draw` avec la liste). Le panneau F1 affiche
`chunks dessinés : N / M` ; l'arrêt journalise `terrain : N/M chunks dessinés à la dernière image`.
Au-delà de 1024 chunks, tout est dessiné (pas de culling) : la limite du terrain (1673 échantillons)
donne au plus 26 × 26 = 676 chunks de 64 m.

Ce que le culling ne fait pas : pas de culling par occlusion (une montagne devant n'en cache pas une autre),
pas de hiérarchie (quadtree) : 676 boîtes × 6 plans par image, négligeable à cette échelle.

## Preuves

- `engine.heightmap` (25 cas) : paramètres, déterminisme, plage 16 bits complète, continuité, échelle en
  mètres (corrélation 2D de deux échantillonnages), **référence indépendante** (30 graines : bruit, étendue,
  arrondi réécrits dans le test, égalité exacte), arène pleine, formule d'altitude, bilinéaire exact aux
  nœuds et aux milieux (z ≠ x vérifié), bords, `.hmap` (aller-retour, 13 refus de fichiers incohérents),
  PNG (fixtures produites par **Pillow**, pas par notre encodeur ; export relu : structure, CRC des chunks,
  blocs zlib non compressés bien formés et **Adler-32** vérifiés par le test, car stb_image ne le contrôle pas ;
  aller-retour pour 2, 3, 33 et 257 de côté dont un flux de plusieurs blocs), et le critère d'acceptation :
  1025 × 1025 générée, sauvegardée et relue.
- `assets.heightmap_cli` : options, comptes rendus, même graine = mêmes octets, PNG puis import = même
  `.hmap`, 15 refus (option inconnue, valeur absente ou illisible, paramètre invalide, mauvaise extension,
  entrée absente), aucun fichier laissé par un échec.
- `engine.terrain` (16 cas) : paramètres et bornes exactes (champ 2 × 2, 8192 parties, 1673/1674 de côté,
  chaque champ du matériau), comptes et disposition, couverture (chaque cellule = 2 triangles vers le haut,
  dans son propre chunk), damier des diagonales, positions/UV/origine/espacement, correspondance
  valeur 16 bits → mètres, normales **exactes** d'un plan incliné et d'un paraboloïde (la grille globale rend
  les raccords sans couture), tangentes, sommets de bord partagés, AABB exactes, arène pleine, 1 km² (256
  chunks de 64 m, aire totale 1024²).
- `engine.terrain_gpu` : rendu réel (SwiftShader) d'un champ 129² vu d'en haut : aucun pixel de fond (donc
  aucun trou aux raccords), 1 chunk = 2 × 2 chunks **pixel pour pixel**, pas de marche d'éclairage au raccord.
- `engine.splatmap` (17 cas) : paramètres (chaque refus et chaque borne), règles à valeurs exactes (smoothstep à
  mi-pente = 0,5, la neige cède à la roche, la terre cède à la roche et à la neige), mélange partout de somme 1 et
  roche croissante avec la pente (balayage 91 × 21 × 21), quantification à somme 255 (20 000 mélanges), pente d'un
  plan = `atan(gradient)` y compris aux bords, ordre lignes/colonnes de la carte, terre (part, graine, 0 % et 100 %),
  arène pleine restituée ; textures : couleur par matériau, **raccord périodique** et indépendance de la taille.
- `engine.terrain_gpu` (en plus) : un terrain de couche unie est éclairé **comme le même terrain par `model.frag`** (écart
  ≤ 2/255) ; chaque canal de la splatmap commande sa couche et un mélange 100/155 est la moyenne pondérée des
  rendus purs (tolérance 1,2 %) ; le texel est **aligné** sur le sommet (transition linéaire entre 31 et 32 m,
  mesurée) ; **falaise de 40 m : 9 bandes de texture comptées sur la paroi** (le même test échoue si le shader
  est ramené à une projection du dessus : vérifié) ; liste de chunks (un seul chunk, liste vide, indices hors
  limites, ordre quelconque = même image).
- `launcher.terrain` : `assetc heightmap` puis `launcher --terrain` (chargé, arrêt propre) ; fichier absent :
  ERREUR journalisée, la démonstration continue.
- Hors CTest : le PNG exporté a été relu avec Pillow (`I;16`, 1025², valeurs identiques à la section `DATA`).

- `engine.frustum` (10 cas) : plans unitaires et distances connues pour une caméra donnée ; points dedans / sur la
  limite / dehors pour chacun des six plans ; boîtes dehors, à cheval et dedans pour chacun des six plans ;
  **24 000 boîtes aléatoires (6 caméras) : le test par plans donne le même verdict qu'un test indépendant par
  coins en espace de découpage**, et aucune boîte rejetée ne contient un point visible ; une boîte contenant un
  point visible n'est jamais rejetée (20 000 tirages) ; cas « exactement sur un plan » (matrice identité) ;
  matrices nulles / NaN / infinies rejettent tout ; terrain 8 × 8 chunks : **caméra vers le ciel = 0 chunk**,
  vers l'horizon = quelques chunks (aucun derrière la caméra), au-dessus = les 64 ; la liste est exactement
  l'ensemble des chunks dont la boîte coupe la pyramide (40 caméras).
- `launcher.terrain` (en plus) : le journal d'arrêt donne `2/4 chunks dessinés` (caméra au centre, regard vers −Z).

## Limites

- Un seul champ, carré ; pas de streaming ni de tuiles (M7).
- Le bruit n'est ni érodé ni terrassé : collines et montagnes « de bruit », sans rivières ni vallées
  cohérentes. L'érosion hydraulique est hors M3 ; un terrain peint (import PNG) remplace le bruit.
- Pas de raccord entre deux champs voisins (bords non fondus).
- Hauteurs d'un 8 bits : 256 niveaux.
- Performances (mesurées ici, Release, un cœur de mon environnement ; votre machine sera différente) : 1025² en 0,29 s, 2049² en 1,27 s avec les paramètres par défaut (6 octaves). Le bruit est calculé deux fois (étendue puis valeurs) : c'est le prix d'éviter un tableau de flottants de la taille du champ.
