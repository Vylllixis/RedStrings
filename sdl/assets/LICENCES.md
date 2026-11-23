# Licences des assets

Chaque asset tiers du dossier `assets/` est listé ici avec sa source et sa licence. Les assets
écrits pour le projet (données de combat, scènes, `models/demo.gltf`, `textures/checker.png`) sont
sous la licence du dépôt.

| Fichiers | Auteur | Source | Licence | Modifications |
|---|---|---|---|---|
| `characters/knight.glb` | Kay Lousberg (KayKit) | « KayKit Character Pack : Adventurers 1.0 » — https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0 (`Characters/gltf/Knight.glb`) | CC0 1.0 (domaine public) | armes et boucliers retirés, 33 animations gardées et renommées (`tools/scripts/kaykit_prepare.sh`, `gltf_extract.py`) |
| `characters/skeleton_minion.glb`, `skeleton_warrior.glb`, `skeleton_rogue.glb`, `skeleton_mage.glb` | Kay Lousberg (KayKit) | « KayKit Character Pack : Skeletons 1.0 » — https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Skeletons-1.0 (`Characters/gltf/Skeleton_*.glb`) | CC0 1.0 | animations retirées (le jeu utilise celles du chevalier : même squelette) |
| `props/sword_1handed.*`, `sword_2handed.*`, `axe_2handed.*`, `shield_round.*`, `knight_texture.png`, `barbarian_texture.png` | Kay Lousberg (KayKit) | Adventurers 1.0, `Assets/gltf/` | CC0 1.0 | aucune |
| `props/Skeleton_Blade.*`, `Skeleton_Axe.*`, `Skeleton_Shield_Small_A.*`, `Skeleton_Crossbow.*`, `Skeleton_Staff.*`, `skeleton_texture.png` | Kay Lousberg (KayKit) | Skeletons 1.0, `Assets/gltf/` | CC0 1.0 | aucune |

Texte de la licence KayKit (extrait du `LICENSE.txt` des deux packs) :

> License: (Creative Commons Zero, CC0) http://creativecommons.org/publicdomain/zero/1.0/
> This content is free to use in personal, educational and commercial projects.
> Support me by crediting Kay Lousberg, www.kaylousberg.com (this is not mandatory)

Le crédit n'est pas obligatoire ; il figure tout de même dans les crédits du jeu.

Pour régénérer les fichiers réduits : `tools/scripts/kaykit_prepare.sh` (télécharge les packs,
puis les réduit avec `tools/scripts/gltf_extract.py`).

## Police : Noto Sans (SIL Open Font License 1.1)

- Fichiers : `fonts/NotoSans-Regular.ttf`, `fonts/NotoSans-Bold.ttf` (texte de la licence :
  `fonts/OFL.txt`).
- Auteurs : The Noto Project Authors (https://github.com/notofonts/latin-greek-cyrillic),
  copyright 2022.
- Licence : SIL OFL 1.1 : utilisation, modification et redistribution libres, y compris dans un
  jeu vendu ; la police ne peut pas être vendue seule ; le texte de la licence l'accompagne.
- Empreintes SHA-256 : Regular `478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823`,
  Bold `1df075a380fc7cb898acf64c1f7b3b4dd780de3caa860178bf929de35817a913`.
- Utilisation : police par défaut de l'interface et du HUD (M8-03).

## Décor : KayKit Medieval Hexagon Pack 1.0 (CC0)

- Fichiers : `props/nature/tree_single_A`, `tree_single_B`, `rock_single_A`, `rock_single_B`,
  `rock_single_C` (`.gltf` + `.bin`), texture commune `props/nature/hexagons_medieval.png`, texte
  de la licence `props/nature/LICENSE.txt`.
- Auteur : Kay Lousberg (www.kaylousberg.com), dépôt GitHub officiel
  KayKit-Game-Assets/KayKit-Medieval-Hexagon-Pack-1.0 ; téléchargés par
  `tools/scripts/kaykit_nature_prepare.sh` (qui vérifie la mention CC0).
- Licence : Creative Commons Zero (domaine public).
- Utilisation : arbres et rochers du placement procédural (`--placement`, B8).

## Arbres de la carte du monde (CC0)

- Fichiers : `props/nature/oak`, `acacia`, `pine_snow` et leurs versions simplifiées `*_lod`,
  `bush`, `bush_snow`, `bush_dry`, `rock` (`.gltf` + `.bin`), palette commune
  `props/nature/trees_palette.png`.
- Fabriqués par `tools/scripts/arbres_make.py` (déterministe) : le chêne et l'acacia sont des
  modèles originaux de ce projet (CC0) ; le sapin enneigé reprend la géométrie de
  `tree_single_B` (KayKit Medieval Hexagon Pack 1.0, CC0, Kay Lousberg), recolorée (neige sur le
  dessus des branches).
- Les buissons (plaine, enneigé, sec) et le rocher sont aussi des modèles originaux de ce projet
  (CC0), fabriqués par le même script.
- Utilisation : forêts, arbres épars, buissons et rochers de la carte du monde
  (`assets/world/carte.txt`).

## Arbres réalistes : arbres scannés de Poly Haven (CC0)

- Fichiers : `props/nature/real/oak.*`, `pine.*`, `acacia.*` (`.gltf` + `.bin`) et leurs images
  (`*_leaves*`, `*_far*`, `*_bark*`).
- Sources (Poly Haven, https://polyhaven.com, modèles en glTF 1k) :
  - `oak` : « Jacaranda Tree », Rob Tuytel et Rico Cilliers — https://polyhaven.com/a/jacaranda_tree
  - `pine` : « Fir Tree 01 » (le premier des trois arbres du fichier), Rob Tuytel et Rico
    Cilliers — https://polyhaven.com/a/fir_tree_01
  - `acacia` : « Quiver Tree 01 », James Ray Cock, Dario Barresi et Rico Cilliers —
    https://polyhaven.com/a/quiver_tree_01
- Licence : Creative Commons Zero 1.0 (domaine public), comme tous les assets de Poly Haven
  (https://polyhaven.com/license). Le crédit n'est pas obligatoire ; il figure ici.
- Modifications : conversion par l'outil du dépôt `treebake` (tronc allégé, feuillage remplacé par
  des cartes photographiées, vue lointaine, mise à l'échelle, neige sur le sapin) ; les images
  d'écorce (`*_bark*_diff/nor/arm.jpg`) sont celles de la source, recopiées telles quelles.
- Pour les refaire : `tools/scripts/arbres_realistes.sh` (télécharge les sources hors du dépôt,
  vérifie leurs empreintes, lance `treebake`). Voir `docs/arbres-realistes.md`.
- Utilisation : chênes, sapins et arbres du désert de la carte du monde.
