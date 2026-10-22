# Contribuer à RedStrings

RedStrings est un projet étudiant (ESGI, module *Langage C avancé*) : les contributions, les retours et les rapports de bugs sont les bienvenus, dans la mesure où elles respectent l'esprit du projet : du C11 lisible, sûr, sans dépendance inutile.

## Signaler un bug ou proposer une idée

Ouvre une [issue](../../issues/new/choose) avec le modèle qui convient. Pour un bug, donne :

- la **version** (`git describe --tags` ou le hash du commit) ;
- la **version utilisée** : console (`./redstrings`) ou fenêtre (`./redstrings_csfml`, avec ou sans `"Yu-Gi-Oh!"`) ;
- ton **système** et la version de CSFML (`dpkg -s libcsfml-dev | grep Version` sous Debian/Ubuntu) ;
- les **étapes** pour reproduire, ce que tu attendais et ce qui s'est passé ;
- si possible, le contenu de `personnages.txt` quand le bug concerne une sauvegarde.

Une faille de sécurité ne se signale pas dans une issue publique : voir [SECURITY.md](SECURITY.md).

## Mettre en place l'environnement

Prérequis : `gcc` (C11), `make`, `ar`, et pour la version graphique la bibliothèque CSFML 2.6 (`sudo apt install libcsfml-dev` sous Debian/Ubuntu).

```bash
git clone https://github.com/Vylllixis/RedStrings.git
cd RedStrings
make            # ./redstrings (console)
make csfml      # ./redstrings_csfml (fenêtre), si CSFML est installée
```

## Avant de proposer un changement

```bash
make re && make tests        # compilation sans avertissement (-Werror) + tests avec sanitizers
make csfml && make csfml-tests
```

Le projet compile avec `-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow -Wmissing-prototypes` (la version graphique ajoute d'autres avertissements, voir `csfml/Makefile`). **Un changement qui ajoute un avertissement ou casse un test ne sera pas fusionné.**

Si tu modifies une règle du jeu, ajoute ou adapte un test dans `tests/tests.c`. Si tu modifies une fonction de la version graphique qui ne dépend pas de SFML (UTF-8, animations, couleurs…), fais-le dans `csfml/tests/tests.c`.

## Conventions du code

| Élément | Règle |
|---|---|
| Fonctions, variables, champs | `snake_case`, en français (`creer_personnage`) |
| Types | `snake_case_t` |
| Macros, constantes d'`enum` | `MAJUSCULES` |
| Taille d'un fichier `.c` | moins de 250 lignes : si un fichier grossit, on le découpe par responsabilité |
| Headers | un header par thème, chacun doit compiler seul ; chaque `.c` n'inclut que ce qu'il utilise |
| Copies de chaînes | toujours bornées (`snprintf`, taille passée avec le pointeur), jamais `strcpy` |
| Retours à vérifier | `malloc`, `realloc`, `fopen`, créations SFML : un échec ne doit jamais faire planter |

**Le moteur ne dépend d'aucune interface.** `src/personnage/` (classes, combat, `bataille.c`, sauvegarde) ne contient ni `printf` ni appel SFML : la console et la fenêtre le partagent. Les animations ne décident jamais d'un résultat ; elles jouent des événements déjà résolus par le moteur.

## Workflow Git

Le dépôt suit **git flow** :

1. une branche par changement, partie de `develop` (`feature/…`, `fix/…`, `docs/…`, `refactor/…`) ;
2. des commits courts avec un **gitmoji** et un message en français : `✨ feat(bataille): …`, `🐛 fix(…)`, `♻️ refactor(…)`, `✅ test(…)`, `📝 docs(…)`, `🔨 build(…)`, `🔒 fix(…)` pour la sécurité ;
3. fusion dans `develop` avec `--no-ff` (`🔀 merge(<branche>): …`) ;
4. `main` ne reçoit que les versions publiées, avec un tag `vX.Y.Z`.

Les pull requests se font **vers `develop`**, pas vers `main`.

## Ressources tierces

- N'ajoute pas d'images, de sons ou de polices dont tu ne peux pas justifier la licence.
- Les illustrations du mode `"Yu-Gi-Oh!"` ne sont **pas** dans le dépôt (voir [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)) : n'en propose pas dans une pull request.
