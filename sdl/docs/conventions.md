# Conventions du projet

Ces règles sont vérifiées par les outils quand c'est possible (`tools/scripts/format.sh`,
`tools/scripts/lint.sh`, `-Werror`). Elles sont modifiables : toute exception ou changement
passe par un ADR dans `docs/adr/`.

## Langue
- Identifiants et noms de fichiers en anglais. Commentaires en français ou en anglais.

## Nommage
| Élément | Règle | Exemple |
|---|---|---|
| Fonction | `prefixe_snake_case` | `eng_arena_push`, `gm_player_update` |
| Préfixes | `eng_` moteur, `gm_` jeu, `tl_` outils | |
| Type (struct, typedef, enum) | `PascalCase` avec préfixe de module | `EngArena`, `GmPlayer` |
| Constante d'énumération, macro, constante globale | `UPPER_SNAKE` avec préfixe | `ENG_LOG_INFO` |
| Variable, paramètre, champ | `snake_case` | `delta_time` |

## Format
- 4 espaces, pas de tabulation, 100 colonnes au maximum.
- Accolade de fonction sur sa propre ligne ; accolades des `if`/`for`/`while` sur la même ligne
  et **obligatoires**, même pour une seule instruction.
- Un fichier = un module : `engine/src/foo.c` + `engine/include/engine/foo.h`.
- En-têtes : `#pragma once`. Un en-tête inclut ce dont il a besoin, pas plus.
- Ordre des `#include` : en-tête du module, puis bibliothèques tierces, puis système.
- Fonction : viser moins de 50 lignes. C'est un repère, pas une règle bloquante.

## Erreurs
- Les fonctions qui peuvent échouer retournent `bool` (`true` = succès) ; les résultats
  sortent par pointeur en dernier paramètre.
- Erreur de programmation (précondition violée) : `ENG_ASSERT` (M0-11), retiré en Release.
- Erreur d'exécution (fichier absent, GPU refusé) : retour d'erreur + `ENG_LOG_ERROR` (M0-10).
- `exit()` et `abort()` uniquement dans `launcher/`, jamais dans le moteur ni dans le jeu.

## Mémoire
- Pas de `malloc` ni de `free` dans la boucle de frame : on passe par les arènes (M0-12).
- Toute fonction qui alloue reçoit l'allocateur en paramètre explicite.
- Propriété indiquée dans le nom : `*_create` / `*_destroy` (alloue, libère), `*_init` /
  `*_shutdown` (n'alloue pas la structure elle-même).
- `game/` n'a pas d'état global modifiable : son état vit dans la mémoire fournie par le moteur
  (condition du rechargement à chaud, M0-17).

## Types et unités
- `<stdint.h>` pour les tailles fixes (`uint32_t`), `size_t` pour tailles et indices,
  `<stdbool.h>` pour les booléens.
- `float` pour le rendu et la physique ; `double` seulement pour l'accumulation du temps.
- Unités : mètres, secondes, radians. Repère main droite, Y vers le haut (précisé en M1-06).

## Outils
- Formater : `tools/scripts/format.sh` (vérifier sans modifier : `--check`).
- Analyser : `cmake --preset debug && tools/scripts/lint.sh`.
- Compiler : `-Wall -Wextra -Wpedantic -Werror` sur notre code, ASan + UBSan en Debug.
