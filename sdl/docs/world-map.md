# Carte du monde (dessinée à la main)

Demande de Théo du 2026-11-21 : une grande carte unique, **toujours la même**, dans l'esprit des
mondes ouverts : une grande plaine au centre, le château de départ sur une petite colline d'où l'on
voit presque toute la carte, et les autres régions autour, à portée de vue.

La carte est décrite dans [`assets/world/carte.txt`](../assets/world/carte.txt). Le moteur la lit
(`engine/world_map.h`), puis trame une fois au chargement le relief et les régions (environ 1 à
2 s pour 4 km × 4 km à 4 m). Rien n'est tiré au hasard : le seul bruit est un détail de surface à
graine fixe, identique à chaque lancement.

![Aperçu de la carte](devlog/img/carte-v1.png)

*Aperçu vu de dessus (nord en haut) : plaine au centre et château de départ (blanc), montagne
enneigée au nord-ouest, désert au sud-ouest, volcan au nord-est, collines au sud-est, falaises sur
le pourtour. Zones : vert facile, orange dur, rouge boss. Lignes beiges : chemins.*

## Voir la carte

```bash
build/debug/bin/carte assets/world/carte.txt apercu.ppm            # aperçu 1024 x 1024
build/debug/bin/carte assets/world/carte.txt apercu.ppm 256 -1300 -1350 50 10   # + altitudes
```

L'outil affiche aussi la pente la plus forte de chaque chemin et l'altitude de chaque zone. Avec
`x0 z0 pas n`, il imprime une grille d'altitudes pour placer les lacets d'un chemin.

## Repère

Mètres ; x vers l'est, z vers le **sud** (le nord est en haut de l'aperçu) ; le château de départ
en (0, 0). La carte couvre [-côté/2 ; côté/2] ; au-delà, le relief du bord est prolongé.

## Format de `carte.txt`

Une instruction par ligne, `#` commence un commentaire. Les erreurs donnent leur numéro de ligne.

| Instruction | Effet |
|---|---|
| `monde côté espacement` | taille de la carte et pas du relief (côté multiple de l'espacement, au plus 4097 échantillons) |
| `eau niveau` | niveau de l'eau (lacs, rivières) |
| `bordure début hauteur` | falaises du pourtour, à partir de `début` m du centre (norme « carré arrondi ») |
| `depart x z` | point de départ du joueur |
| `region biome x z portée` | graine d'une région (`plaine`, `montagne`, `desert`, `volcan`) ; frontières déformées et fondues sur ~10 % de la portée |
| `colline x z rayon hauteur [plateau]` | bosse arrondie, sommet plat de rayon `plateau` |
| `montagne x z rayon hauteur` | pic aux arêtes irrégulières |
| `crete largeur hauteur x z x z ...` | chaîne le long d'une ligne |
| `volcan x z rayon hauteur rayon_cratère profondeur` | cône, cratère à fond plat |
| `plateau x z rayon hauteur` | sommet plat, flancs raides (mesa) |
| `dunes x z rayon amplitude longueur_onde direction_deg` | vagues de sable |
| `aplanir x z rayon [hauteur]` | aire plane (château, camps, arènes) ; sans hauteur : au niveau du sol en son centre |
| `chemin largeur x z x z ...` | route : pente linéaire entre ses sommets, au plus 30 % (garde-fou), extrémités au sol |
| `lac x z rayon profondeur` | creusé sous le niveau de l'eau, berges douces |
| `riviere largeur profondeur x z ...` | lit creusé le long d'une ligne |
| `lave largeur x z ...` | coulée de lave (sillon de 1,5 m), jamais sur un chemin |
| `foret essence x z rayon espacement` | forêt (`chene`, `sapin`, `acacia`) : grille fixe décalée, lisière moins dense |
| `arbres_epars essence biome espacement` | arbres épars partout où le biome domine (acacias du désert) |
| `zone biome facile\|dur\|boss x z` | place d'une zone de combat |

Ordre de construction : régions et relief de base des biomes, formes en relief, bordure ; aires
aplanies ; chemins ; lacs et rivières ; lave. Une aire aplanie placée après une colline aplanit
donc la colline.

## Dans le jeu (`--world`)

- **Relief** : les cellules du monde ouvert (256 m, un sommet tous les 4 m) lisent la carte tramée
  (`EngWorldGen.map`) ; plus de rochers, d'arbres ni de camps procéduraux.
- **Sol à six matières** (`eng_biome_splat6`) : herbe, roche, terre, neige, puis sable et lave dans
  une seconde carte de poids (emplacements 9 à 11 après la carte d'ombre, `terrain.frag`). Plaine :
  herbe ; montagne : neige ; désert : sable ; volcan : basalte sombre et cendre ; les chemins en
  terre battue ; la lave rayonne (émissive, visible la nuit).
- **Arbres** (`eng_worldmap_trees`, `launcher/src/world_trees.c`) : chênes (forêts de la plaine),
  sapins enneigés (forêts de la montagne), acacias épars dans le désert, aucun dans le volcan ;
  environ 11 000 sur la carte. Jamais sur un chemin, dans l'eau, sur la lave, sur une pente de plus
  de 35 degrés, à moins de 70 m du départ ni de 32 m d'une zone de combat. Placement fixe : même
  résultat quel que soit le découpage en cellules. Dessin instancié par essence et par cellule ;
  version simplifiée (28 à 64 triangles) au-delà de 220 m ; ombre jusqu'à 200 m ; troncs solides
  (capsules) à moins de 40 m du joueur.
- **Vue lointaine** (`launcher/src/world_far.c`) : toute la carte, un sommet tous les 32 m, en
  morceaux de 256 m dessinés là où la cellule détaillée n'est pas chargée ; plan lointain 6 km,
  brouillard 0,00035 /m : depuis la colline on voit les montagnes, le désert et le volcan.
- **Départ** : au château de départ, sur la colline (0, 0) : remparts de 2,4 m (on voit par-dessus),
  une porte sur chaque côté, donjon, bivouac. `--at-zone N` place le joueur sur le chemin depuis
  le château, 20 m avant la zone (34 m avant un boss).
- **Zones** : positions lues dans la carte (`eng_worldmap_zones`), le reste (noms, PV, butin) dans
  `engine/map_zones`.

Modèles d'arbres : `tools/scripts/arbres_make.py` (chêne et acacia originaux, sapin enneigé tiré du
sapin KayKit CC0) ; voir `assets/LICENCES.md`.

## État

Fait : carte, relief, matières, arbres, vue lointaine, château de départ, zones. Limites : pas
d'eau dessinée (lacs et rivières sont des creux) ; la lave ne blesse pas ; pas de pont sur les
gués ; les ennemis ne contournent pas les arbres (les zones de combat en sont dégagées).
