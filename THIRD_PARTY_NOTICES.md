# Ressources et dépendances tierces

Ce fichier recense ce que RedStrings utilise sans l'avoir écrit. Il décrit l'état du dépôt ; il ne remplace pas les licences d'origine.

## Embarqué dans le dépôt

### Polices (`csfml/assets/fonts/`)

| Fichier | Police | Auteurs et licence |
|---|---|---|
| `NotoSans-Regular.ttf`, `NotoSans-Bold.ttf` | Noto Sans, version 2.004 | « Copyright 2015 Google LLC » (mention lue dans le fichier de police). Les polices Noto sont diffusées par leurs auteurs sous **SIL Open Font License 1.1** ; cette licence n'est pas écrite dans les fichiers : **à confirmer sur le dépôt officiel de Noto** avant toute redistribution séparée. |
| `DejaVuSans-Bold.ttf` | DejaVu Sans Bold | « Copyright (c) 2003 by Bitstream, Inc. », « Copyright (c) 2006 by Tavmjong Bah » ; les modifications DejaVu sont dans le domaine public. Licence des polices **Bitstream Vera** (texte complet intégré au fichier de police et à <https://dejavu-fonts.github.io/License.html>) : redistribution libre, y compris avec un programme, à condition de conserver les mentions de copyright ; interdiction de vendre les polices seules ; une version modifiée doit changer de nom. |

Ces polices sont utilisées telles quelles, non modifiées.

### Images du README (`.github/readme/`)

Les illustrations SVG sont générées par [`generer_svg.py`](.github/readme/outils/generer_svg.py) à partir du code du projet. Les captures `.webp` sont des enregistrements de RedStrings lui-même, en mode normal.

## Dépendances à l'installation (non redistribuées)

| Dépendance | Rôle | Licence |
|---|---|---|
| **CSFML 2.6** (binding C de SFML) | fenêtre, dessin, entrées (version graphique uniquement) | zlib/libpng |
| **SFML 2.6** | utilisée par CSFML | zlib/libpng |
| `gcc`, `make`, `ar` (GNU) | compilation | outils de construction, rien n'est lié au jeu |

CSFML et SFML sont installées par le gestionnaire de paquets (`libcsfml-dev`) et liées dynamiquement : leur code n'est pas copié dans ce dépôt. La licence zlib est permissive et compatible avec les licences libres usuelles. La version console (`./redstrings`) n'en a pas besoin.

## Mode `"Yu-Gi-Oh!"` : illustrations non incluses

Le mode caché de la version graphique affiche, pour chaque classe, l'illustration d'une carte de *Yu-Gi-Oh! Trading Card Game*.

- **Les images ne sont pas dans ce dépôt** (ignorées par `.gitignore`). Elles appartiennent à leurs ayants droit (Kazuki Takahashi, Konami). Le script `csfml/assets/yugioh/telecharger.sh` les télécharge **en local** depuis la base YGOPRODeck pour un usage personnel ; ce dépôt n'accorde aucun droit sur elles.
- Le code ne contient que des références factuelles (nom et numéro de chaque carte, cadre, attribut, niveau) dans `csfml/src/yugioh/cartes.c`.
- *Yu-Gi-Oh!* est une marque de ses propriétaires. RedStrings est un projet étudiant sans lien avec Konami ni avec les auteurs de la série, et ne prétend pas à leur approbation.
- Le nom officiel d'une carte ne remplace jamais celui d'une classe à l'écran.
- Je n'ai pas vérifié les conditions d'utilisation du site YGOPRODeck : ne redistribue pas les images téléchargées, et consulte ses conditions avant tout usage au-delà d'un usage personnel.
