# RedStrings : dossier de rendu

Jeu de combat tactique en équipe, au tour par tour, écrit en **C11**. Deux interfaces partagent le même moteur de jeu : un **terminal** et une **fenêtre graphique (CSFML)**.

Cette archive contient tout le code source, les tests et la documentation. La présentation complète du projet (règles, 28 classes, architecture, choix techniques) est dans [`README.md`](README.md).

## Compiler et lancer

Prérequis : `gcc` (C11), `make`, `ar`. La version graphique demande en plus **CSFML 2.6** (`sudo apt install libcsfml-dev` sous Debian/Ubuntu).

```bash
make              # version terminal : ./redstrings
./redstrings

make csfml        # version graphique : ./redstrings_csfml (si CSFML est installée)
./redstrings_csfml
```

La version terminal n'a besoin d'aucune bibliothèque externe.

## Vérifier

```bash
make tests          # moteur, classes, combat, sauvegarde (AddressSanitizer + UBSan)
make csfml-tests    # fonctions de la version graphique, sans fenêtre
```

Les deux commandes affichent un message de réussite et s'arrêtent sur la première vérification qui échoue.

## Ce qu'il y a à voir

| Où | Quoi |
|---|---|
| `src/personnage/bataille.c` | le moteur de combat en équipe (tours, attaque, soin, garde, IA), **sans aucun affichage** |
| `src/personnage/personnage.c` | le catalogue des 28 classes : une seule table pour la console, la fenêtre et les tests |
| `src/ui/`, `src/redstrings.c` | la version terminal |
| `csfml/` | la version graphique : `src/` rangé par thème, `include/` un en-tête par thème |
| `lib/` | `libmy.a`, 40 fonctions de bibliothèque réécrites |
| `tests/`, `csfml/tests/` | les tests unitaires |
| `Makefile`, `csfml/Makefile` | compilation avec `-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow -Wmissing-prototypes` |

## Fonctionnalités

- Création de personnages (28 classes en 4 catégories), sauvegarde et chargement dans `personnages.txt`.
- Combat en équipe de 1 à 4 contre l'ordinateur : attaquer, soigner, se mettre en garde.
- Version graphique : arène animée, fenêtre redimensionnable, plein écran (touche <kbd>F11</kbd>).
- Mode caché `./redstrings_csfml "Yu-Gi-Oh!"` : les classes deviennent des cartes. Il demande des illustrations qui ne sont pas incluses (droits), à télécharger avec `sh csfml/assets/yugioh/telecharger.sh` ; sans elles, le mode fonctionne avec les emblèmes des classes.

## Limites connues

Elles sont listées dans le [`README.md`](README.md#limites-connues) : en particulier, testé sous Linux uniquement, 10 personnages au plus dans la fenêtre, pas de compétences spéciales.

## Licence

Code sous licence **MIT** ([`LICENSE`](LICENSE)). Les polices, CSFML/SFML et les illustrations du mode « Yu-Gi-Oh! » gardent leurs propres droits : voir [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
