# 0014 — Champ de hauteurs : échantillons 16 bits, FastNoiseLite, format .hmap

Statut : Accepté (M3-01)

## Contexte
M3 bâtit un terrain de 1 km² par chunks. M3-01 demande une heightmap 1025 × 1025 « importée ou par bruit
procédural (FastNoiseLite en C) », avec réglages d'échelle et d'altitude, générée et sauvegardée. Les
tâches suivantes (chunks, pentes pour la splatmap, placement de la végétation, plus tard la physique) liront
cette grille : sa représentation compte.

## Options
**Stockage des hauteurs**
1. `float` par échantillon : simple, précision illimitée en pratique ; 4 octets (4,2 Mio à 1025²) ; pas de
   format d'échange avec les éditeurs d'images.
2. **`uint16` + plage d'altitudes** (retenu) : 2 octets ; 65 536 niveaux sur la plage. Pour 0–120 m, un pas
   de 1,8 mm ; pour 0–2000 m, 3 cm, sous le bruit de quantification visible. Se lit et s'écrit comme un PNG
   16 bits (GIMP, Krita, World Machine, Gaea), donc le terrain peut se peindre ou s'importer.

**Bruit**
1. Écrire un bruit (Perlin/simplex + fBm) : une journée de plus, sans valeur ajoutée.
2. **FastNoiseLite, version C** (retenu) : un seul en-tête MIT, prévu par le plan. Pas de tag dans le dépôt :
   commit figé (`785f37a`), compilé dans un seul fichier (`third_party_fnl.c`) derrière `engine/noise.h`,
   comme bc7enc (ADR 0007).

**Fichier sauvegardé**
1. PNG 16 bits seulement : interopérable, mais ne porte ni espacement ni altitudes.
2. **`.hmap` dans le conteneur des assets (magic `OWHF`) + import/export PNG** (retenu) : même vérification
   et mêmes outils (`assetc info`) que `.mesh` et `.tex` (ADR 0006), lecture en place ; le PNG reste le format
   d'échange.

**Encodeur PNG**
`stb_image_write` n'écrit pas de PNG 16 bits. Options : lodepng (nouvelle dépendance) ou un encodeur
minimal. Retenu : ~80 lignes, blocs deflate **non compressés** (valides, relus par stb et par Pillow ; le
fichier fait environ la taille des données). Une compression réelle n'apporte rien ici : le `.hmap` est le
format du jeu, le PNG un format d'échange.

## Décision
Champ carré de `uint16`, `size` × `size`, `spacing` mètres entre échantillons, plage
[`altitude_min`, `altitude_max`] ; génération par FastNoiseLite (OpenSimplex2, fBm ou crêtes), **étalée sur
toute la plage 16 bits** ; `.hmap` vérifié par le lecteur ; import et export PNG ; sous-commandes
`assetc heightmap` et `assetc heightmap-import`.

## Conséquences
- **Étalement systématique.** La plus basse valeur du bruit devient `altitude_min`, la plus haute
  `altitude_max`, quelle que soit la graine : l'altitude demandée est toujours atteinte, mais un terrain
  « presque plat » ne s'obtient pas en baissant l'amplitude du bruit ; il se règle par la plage d'altitudes.
  Deux terrains de tailles différentes n'ont pas les mêmes extrema (la normalisation se fait sur ce qui est
  généré).
- **Déterminisme.** Mêmes paramètres, même fichier, vérifié pour un même binaire (test `assets.heightmap_cli`).
  Entre compilateurs ou plateformes, les arrondis flottants (contractions FMA, `-ffast-math` absent) peuvent
  décaler une valeur de 1 sur 65 535 ; **rien ne doit donc reposer sur une régénération identique du bruit
  sur deux machines** : le `.hmap` est la source de vérité, pas la graine (M3-08 placera la végétation à
  partir du `.hmap`).
- La taille 2^k + 1 n'est pas imposée (2 à 8193) ; le découpage en chunks (M3-02) vérifiera la divisibilité.
- L'échantillonnage bilinéaire (`eng_heightmap_sample`) est continu mais pas dérivable aux nœuds : les
  normales du terrain se calculeront sur la grille (différences centrées), pas par dérivation de cette
  interpolation.
- 8193² échantillons = 128 Mio en mémoire pour un champ (au-delà : M7, streaming).
- Un 8 bits importé est étendu (`v × 257`) : il garde ses 256 marches (terrasses visibles) ; le logiciel
  d'édition doit exporter en 16 bits.
