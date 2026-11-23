# ADR 0001 — Le jeu est une bibliothèque qui ne lie pas le moteur

Statut : Accepté (M0-17) · Date : 2026-11

## Contexte

Le code du jeu doit pouvoir être recompilé et rechargé sans relancer le programme (critère de
sortie de M0). Le moteur contient de l'état global : fichier de journal, compteur d'allocations,
hooks d'assertion, mutex.

## Options

1. **Le jeu lie le moteur statiquement.** Simple, mais la bibliothèque du jeu embarque sa propre
   copie de ces globales : deux journaux, deux compteurs, des assertions qui n'atteignent pas le
   moteur. Rejeté.
2. **Le moteur est une bibliothèque dynamique partagée** par l'exécutable et le jeu. Une seule
   copie des globales, mais : symboles exportés à maintenir, bibliothèques d'import sous Windows,
   ordre de chargement, et l'exécutable dépend d'une DLL en plus. Plus lourd que le besoin.
3. **Le jeu ne lie rien ; le moteur lui passe une table de fonctions** (`EngServices`) et le jeu
   expose une seule fonction (`eng_game_get_api`). Retenu.

## Décision

Option 3. Le jeu exporte uniquement `eng_game_get_api` (visibilité cachée pour le reste).
`game/src/bridge.c` définit côté jeu `eng_log_write` et `eng_assert_fail` qui relaient vers la
table, pour que `ENG_LOG_*` et `ENG_ASSERT` restent utilisables à l'identique. Les assertions
passent par un hook : le service retourne « s'arrêter dans le débogueur » et c'est le **jeu** qui
déclenche l'arrêt, pour que le point d'arrêt tombe sur la ligne fautive.

Le moteur copie la bibliothèque avant de la charger, vérifie `api_version` et `state_size`, et
garde tout l'état du jeu dans un bloc qu'il possède (règles dans `game_api.h`).

## Conséquences

- Le jeu ne peut appeler aucune fonction du moteur hors de `EngServices`. Toute capacité
  nouvelle = une entrée de table + incrément de `ENG_GAME_API_VERSION`.
- Les modules du moteur **compilés dans le moteur** (arena, array, pool, map...) sont
  inaccessibles au jeu tant qu'on n'a pas décidé comment (voir `architecture.md`, §9).
- Pas de globale modifiable dans le jeu ; pas de pointeur vers le code du jeu dans l'état.
- Un changement de taille d'état est détecté (rechargement refusé) ; un réordonnancement à
  taille égale ne l'est pas.
