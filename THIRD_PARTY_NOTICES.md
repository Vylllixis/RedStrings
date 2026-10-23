# Ressources et dépendances tierces

Ce fichier recense ce que RedStrings utilise sans l'avoir écrit. Il décrit l'état du dépôt ; il ne remplace pas les licences d'origine.

## Périmètre de la licence MIT

Le fichier [`LICENSE`](LICENSE) (MIT, © 2026 Vyllixis) couvre **le code source écrit pour RedStrings** et la documentation du dépôt. Il ne couvre **aucune** des ressources listées ci-dessous, qui restent soumises à leurs propres conditions.

**Compatibilité.** CSFML et SFML sont sous licence zlib/libpng : permissive, compatible avec la licence MIT. Elles sont liées dynamiquement et ne sont pas copiées dans le dépôt, donc leur licence n'impose rien de plus au code de RedStrings. Les polices sont des fichiers de données séparés, chargés à l'exécution : elles ne sont pas relicenciées et gardent leurs conditions.

## Embarqué dans le dépôt

### Polices (`csfml/assets/fonts/`)

| Fichier | Police | Auteurs et licence |
|---|---|---|
| `NotoSans-Regular.ttf`, `NotoSans-Bold.ttf` | Noto Sans, version 2.004 | « Copyright 2015 Google LLC » (mention lue dans le fichier de police). Licence : **SIL Open Font License 1.1**, selon le dépôt officiel du projet Noto ([notofonts/latin-greek-cyrillic, `OFL.txt`](https://github.com/notofonts/latin-greek-cyrillic/blob/main/OFL.txt)). Le fichier de police ne contient pas le texte de la licence : une copie de ce texte officiel est fournie dans [`csfml/assets/fonts/OFL.txt`](csfml/assets/fonts/OFL.txt) (son en-tête porte le copyright du dépôt actuel, « 2022 The Noto Project Authors »). La licence autorise l'usage et la redistribution avec un programme, sans modification du nom ni vente de la police seule. |
| `DejaVuSans-Bold.ttf` | DejaVu Sans Bold | « Copyright (c) 2003 by Bitstream, Inc. », « Copyright (c) 2006 by Tavmjong Bah » ; les modifications DejaVu sont dans le domaine public. Licence des polices **Bitstream Vera** (texte complet intégré au fichier de police et à <https://dejavu-fonts.github.io/License.html>) : redistribution libre, y compris avec un programme, à condition de conserver les mentions de copyright ; interdiction de vendre les polices seules ; une version modifiée doit changer de nom. |

Ces polices sont utilisées telles quelles, non modifiées.

### Sons et musiques (`csfml/assets/audio/`)

Aucune ressource tierce : les 96 bruitages (53 standard, 43 Yu-Gi-Oh!) et les 4 musiques sont des **créations originales**, synthétisées par les scripts Python de [`csfml/assets/audio/outils/`](csfml/assets/audio/outils/) (sans échantillon extérieur) et encodés en OGG Vorbis par l'encodeur intégré à SFML. Ils sont couverts par la licence MIT du dépôt. Les clins d'œil aux jeux vidéo (dont le thème « Yu-Gi-Oh! ») reprennent des gestes et des styles, jamais un son ni une mélodie existants : détail dans [`csfml/assets/audio/LICENCES.md`](csfml/assets/audio/LICENCES.md).

### Images du README (`.github/readme/`)

Les illustrations SVG sont générées par [`generer_svg.py`](.github/readme/outils/generer_svg.py) à partir du code du projet. Les captures `.webp` sont des enregistrements de RedStrings lui-même, en mode normal.

## Dépendances à l'installation (non redistribuées)

| Dépendance | Rôle | Licence |
|---|---|---|
| **CSFML 2.6** (binding C de SFML) | fenêtre, dessin, entrées, sons et musique (version graphique uniquement) | zlib/libpng |
| **OpenAL Soft**, **libvorbis**, **libogg**, **libFLAC** (installées avec SFML Audio) | sortie audio, lecture et encodage OGG / FLAC | LGPL 2.1 (OpenAL Soft, liée dynamiquement) · BSD (Xiph) |
| **SFML 2.6** | utilisée par CSFML | zlib/libpng |
| `gcc`, `make`, `ar` (GNU) | compilation | outils de construction, rien n'est lié au jeu |

CSFML et SFML sont installées par le gestionnaire de paquets (`libcsfml-dev`) et liées dynamiquement : leur code n'est pas copié dans ce dépôt. La licence zlib est permissive et compatible avec les licences libres usuelles. La version console (`./redstrings`) n'en a pas besoin.

## Mode `"Yu-Gi-Oh!"` : illustrations non incluses

Le mode caché de la version graphique affiche, pour chaque classe, l'illustration d'une carte de *Yu-Gi-Oh! Trading Card Game*.

- **Les images ne sont pas dans ce dépôt** (ignorées par `.gitignore`) **et ne sont pas couvertes par la licence MIT.** Elles appartiennent à leurs ayants droit (Kazuki Takahashi, Konami). Le script `csfml/assets/yugioh/telecharger.sh` les télécharge **en local** depuis la base YGOPRODeck pour un usage personnel ; ce dépôt n'accorde aucun droit sur elles.
- Le code ne contient que des références factuelles (nom et numéro de chaque carte, cadre, attribut, niveau) dans `csfml/src/yugioh/cartes.c`.
- *Yu-Gi-Oh!* est une marque de ses propriétaires. RedStrings est un projet étudiant sans lien avec Konami ni avec les auteurs de la série, et ne prétend pas à leur approbation.
- Le nom officiel d'une carte ne remplace jamais celui d'une classe à l'écran.
- Je n'ai pas vérifié les conditions d'utilisation du site YGOPRODeck : ne redistribue pas les images téléchargées, et consulte ses conditions avant tout usage au-delà d'un usage personnel.
