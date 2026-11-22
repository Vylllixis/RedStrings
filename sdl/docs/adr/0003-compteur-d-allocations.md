# ADR 0003 — Compteur d'allocations et portée du « zéro malloc »

Statut : Accepté (M0-12) · Date : 2026-11

## Contexte

Critère de sortie de M0 : zéro `malloc` dans la boucle de frame. Il faut un moyen de le
**vérifier**, pas seulement de le souhaiter.

## Options

1. Remplacer `malloc` par interposition (LD_PRELOAD, `--wrap`). Spécifique à la plateforme,
   fragile avec les sanitizers.
2. Compter via `SDL_SetMemoryFunctions` (`eng_mem_track_install`). Portable, mais ne voit que SDL
   et ce qui passe par `SDL_malloc`.
3. Option 2 **plus** une règle lint interdisant `malloc`/`free` de la libc dans notre code.

## Décision

Option 3. Le compteur est installé au tout début de `main`. `tools/scripts/lint.sh` refuse
`malloc`, `calloc`, `realloc`, `free` dans les `.c` et `.h` de `engine/`, `game/` et `launcher/`
(recherche de texte, commentaires inclus).

## Conséquences

- **Portée du compteur** : il couvre l'étape de mise à jour de la frame (événements, jeu). Hors
  compteur : le contrôle de rechargement du jeu (outil de développement) et l'affichage logiciel
  provisoire (`SDL_UpdateWindowSurface` alloue 2 fois par image dans le pilote dummy, mesuré). Le
  compteur couvrira l'image entière avec le rendu GPU (M1).
- Les allocations paresseuses de SDL (file d'événements, surface) sont déclenchées avant la
  première frame (échauffement), sinon elles seraient comptées à tort.
- Une dépendance qui appelle `malloc` directement n'est pas vue (Tracy alloue dans son propre
  allocateur) : c'est accepté tant qu'elle reste optionnelle et hors du code du jeu.
- Le test « zéro allocation » existe en CTest, et `eng_memory_end_frame` avertit une fois en cas
  de dépassement.
