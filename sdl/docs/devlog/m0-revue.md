# Revue de jalon M0 — critères de sortie

Légende : **Prouvé** = vérifié par une commande rejouable ; **À faire** = à vérifier sur ta machine.

## Critères de sortie du jalon

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Build Debug/Release depuis un clone vierge en une commande | `git clone <dépôt> && cd <dépôt>/sdl && tools/scripts/test.sh debug && tools/scripts/test.sh release` | À faire |
| 2a | CI verte (Windows et Linux) | onglet *Actions* : dernière exécution de `main` | À faire |
| 2b | `-Werror` actif | un avertissement volontaire casse la build (vérifié en M0-06) | Prouvé |
| 2c | ASan/UBSan propres | `ctest --preset debug` (18/18) et lancement réel sous sanitizers sans rapport | Prouvé (Linux) |
| 3 | Hot reload sans perte d'état | lanceur réel : modification de `game.c`, `cmake --build --target game` pendant l'exécution → « nouveau code actif : état conservé (165 pas, x = 0.500, rechargement n°1) », 467 pas au total ; plus `engine.game_host` | Prouvé (Linux) |
| 4 | Zéro malloc dans la boucle de frame | « allocations dans la boucle de frame : 0 (sur 5000 images) » ; test `launcher.zero_alloc` | Prouvé (Linux) — portée : mise à jour de la frame, voir ADR 0003 |

## Livrable : « ouvre une fenêtre, lit clavier et manette, 60 Hz, recharge le code à chaud »

| Élément | Preuve | État |
|---|---|---|
| Fenêtre, redimensionnement, plein écran | lancer `launcher` avec un vrai écran, redimensionner, basculer `fullscreen = true` dans `config.ini` | À faire |
| Clavier | déplacer le carré avec ZQSD/WASD, Maj pour courir, Ctrl pour l'esquive | À faire |
| Manette | idem avec le stick gauche, B/Cercle, X/Carré, Start | À faire |
| 60 Hz fixe | le journal final affiche « simulation : N pas à 60 Hz » ; N/durée ≈ 60 (mesuré : 467 pas en 7,8 s) | Prouvé (Linux) |
| Vsync | la ligne de démarrage ne contient pas « vsync indisponible » sur ta machine | À faire |
| Rechargement à chaud interactif | changer `SQUARE_R` ou `MOVE_SPEED` dans `game/src/game.c` pendant que le jeu tourne | À faire |

## Hygiène du dépôt (restes de M0)

- [ ] `main` verte après fusion de M0-21 et M0-22 ; un commit volontairement cassé rend la CI rouge (M0-08)
- [ ] CI : branches m0-09, m0-10, m0-11, m0-14 vérifiées, MSVC compris
- [ ] `docs/dependencies.md` : ligne Tracy ajoutée
- [ ] `docs/conventions.md` : sections mémoire (arènes, handles) et rechargement à chaud
- [ ] anciens `sdl/Makefile` et `sdl/src` supprimés
- [ ] `docs/architecture.md` relu : chaque affirmation vraie / fausse / dépassée
- [ ] tableau de suivi : toutes les tâches M0 à jour (M0-13 cochée par erreur, M0-14 à cocher)

## Clôture

```bash
git switch main && git pull
git tag -a v0.0 -m "M0 : fondations et outillage"
git push origin v0.0
```

Ne tague qu'une fois les lignes « À faire » du tableau des critères de sortie confirmées.
