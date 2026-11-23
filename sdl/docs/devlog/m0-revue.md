# Revue de jalon M0 — critères de sortie

Légende : **Prouvé** = vérifié par une commande rejouable ; **À faire** = à vérifier sur ta machine.

## Critères de sortie du jalon

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Build Debug/Release depuis un clone vierge en une commande | `git clone <dépôt> && cd <dépôt>/sdl && tools/scripts/test.sh debug && tools/scripts/test.sh release` | **Partiel** : les sources d'assets n'étaient pas versionnées (`/assets/*` dans `.gitignore`), corrigé le 19/11 (P0-01) ; clone vierge pas encore rejoué |
| 2a | CI verte (Windows et Linux) | onglet *Actions* | **À faire** : dernière exécution (12/11, `m1-04-triangle`) rouge (tests Linux, compilation Windows) ; corrections MSVC du 19/11 (voir ci-dessous) à valider par un push (QUESTIONS-THEO Q3) |
| 2b | `-Werror` actif | un avertissement volontaire casse la build (vérifié en M0-06) | Prouvé |
| 2c | ASan/UBSan propres | `ctest --preset debug` (18/18) et lancement réel sous sanitizers sans rapport | Prouvé (Linux) |
| 3 | Hot reload sans perte d'état | lanceur réel : modification de `game.c`, `cmake --build --target game` pendant l'exécution → « nouveau code actif : état conservé (165 pas, x = 0.500, rechargement n°1) », 467 pas au total ; plus `engine.game_host` | Prouvé (Linux) |
| 4 | Zéro malloc dans la boucle de frame | « allocations dans la boucle de frame : 0 (sur 5000 images) » ; test `launcher.zero_alloc` | Prouvé (Linux) — portée : mise à jour de la frame, voir ADR 0003 |

## Livrable : « ouvre une fenêtre, lit clavier et manette, 60 Hz, recharge le code à chaud »

| Élément | Preuve | État |
|---|---|---|
| Fenêtre, redimensionnement, plein écran | machine de Théo (RTX 5070, X11) : fenêtre 1920 x 1080 ; F11 → 2560 x 1600 puis retour ; redimensionnée à 1000 x 600 et 1700 x 900 (captures xwd de la vraie fenêtre, swapchain recréée 6 fois, « sans fuite ») ; `fullscreen = true` → swapchain 2560 x 1600 (`--screenshot`) | **Prouvé** (19/11). Défaut corrigé : la taille journalisée était celle d'avant la bascule (SDL_SyncWindow) |
| Clavier | `--software` : carré déplacé par D (x 892 → 1460 px) ; mode combat : marche, attaque, pause, menus au clavier (xdotool, disposition AZERTY) | **Prouvé** (19/11) |
| Manette | idem avec le stick gauche, B/Cercle, X/Carré, Start | **Non vérifié** : aucune manette sur la machine (QUESTIONS-THEO Q4) |
| 60 Hz fixe | le journal final affiche « simulation : N pas à 60 Hz » ; N/durée ≈ 60 (mesuré : 467 pas en 7,8 s) | Prouvé (Linux) |
| Vsync | `vsync = true` : « présentation vsync », ≈ 163 images/s sur l'écran 165 Hz ; `vsync = false` : « présentation immédiate », 694 images/s | **Prouvé** (19/11) |
| Rechargement à chaud interactif | lanceur `--software` en marche ; `SQUARE_R` 230 → 40 dans `game/src/game.c`, `cmake --build build/release --target game` : « nouveau code actif : état conservé (274 pas, x = 0.500, rechargement n°1) », couleur du carré 230 → 40 à l'écran, puis retour à 230 (rechargement n°2) | **Prouvé** (19/11) |

Constaté au passage : le redimensionnement et le plein écran provoquent des allocations dans la
boucle de frame (28 sur 1 704 images, toutes aux images de changement de taille) ; le critère 4
reste vrai en régime établi (0 sur 3 000 images sans redimensionnement). Voir B5 dans
`suivi-finition.md`.

## Portabilité Windows (19/11)

Lecture du code pour MSVC, sans machine Windows : `/experimental:c11atomics` (`<stdatomic.h>` de
`jobs.h`), C4324 / C4127 / C4996 désactivés avec justification (`CMakeLists.txt`), `strtok_r` et
`strnlen` remplacés par leurs équivalents SDL, `__attribute__((aligned))` des tests remplacé par
`_Alignas`, deux variables masquées (C4456) et deux rétrécissements implicites (C4267) corrigés.
Contrôles automatiques faits ici : tout le code compile en C11 strict (`-std=c11`, aucune fonction
POSIX), sans VLA (`-Wvla`), sans conversion flottante implicite (`-Wfloat-conversion`). La CI produit
l'exécutable Windows en artefact (`openworld-windows-release`). **Non vérifié** : une vraie
compilation MSVC (push nécessaire).

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
