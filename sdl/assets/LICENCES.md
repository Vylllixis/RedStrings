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
