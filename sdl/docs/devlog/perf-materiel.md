# Mesures de performance sur le vrai matériel

Premières mesures sur un vrai GPU (la première passe n'avait que SwiftShader). Toutes les valeurs
sont des **temps d'image complets** (début d'image à début d'image, présentation comprise), mesurés
par le lanceur lui-même, **sans vsync** (`vsync = false`), en build Release.

## Machine

| | |
|---|---|
| Machine | portable, Ubuntu 24.04.5, noyau Linux 7.0, session X11 (GNOME) |
| CPU | Intel Core i7-13650HX (14 cœurs, 20 threads) |
| GPU | NVIDIA GeForce RTX 5070 Laptop GPU, 8 Go (pilote 580.178.04, Vulkan 1.4.312) ; un iGPU Intel est présent, SDL_GPU choisit bien le NVIDIA (journal « GPU : pilote vulkan, périphérique « NVIDIA GeForce RTX 5070 Laptop GPU » ») |
| Écran | interne 2560 x 1600 à 165 Hz |
| Build | `cmake --preset release`, commit `1cd0953` (+ instrumentation de mesure temporaire, voir « Méthode ») |

## Résultats (2026-11-19)

Moyenne / p99 / pire temps d'image, après 300 images de chauffe ; « CPU » = temps de travail du
thread principal hors soumission au GPU (la valeur du panneau F1).

| Scène (critère) | Commande | 1920 x 1080 fenêtré | 2560 x 1600 plein écran |
|---|---|---|---|
| Scène de test avec ombres (M1, « 60 FPS ») | `launcher --config C --frames 3000` | 1,44 ms (**694 FPS**) / 4,15 / 7,69 ; CPU 0,05 ms | 2,74 ms (**365 FPS**) / 5,68 / 8,18 |
| Terrain 1 km² + herbe (306 628 brins) + 71 773 touffes + 5 185 rochers + eau (M3, « 60 FPS ») | `launcher --terrain terrain.hmap --grass --placement --water` (`assetc heightmap terrain.hmap` : 1025², 1 m) | 4,65 ms (**215 FPS**) / 8,37 / 11,67 | 6,15 ms (**163 FPS**) / 9,98 / 12,29 |
| Combat dans l'arène, 3 ennemis + mannequin (M5/M6) | `launcher --combat-autoplay --no-audio` | 1,45 ms (**688 FPS**) / 4,45 / 7,57 ; CPU 0,20 ms | 2,75 ms (**364 FPS**) / 5,94 / 11,32 |
| Combat sur terrain doux + herbe + placement | `launcher --combat-autoplay --terrain doux.hmap --grass --placement` | 8,05 ms (**124 FPS**) / 11,77 / 16,23 ; CPU 0,24 ms | non mesuré |

Aucune image au-dessus de 16,7 ms dans ces séries (hors image de capture d'écran : `--screenshot`
attend le GPU et écrit le fichier, 88 à 106 ms, exclue).

Avec vsync (`vsync = true`) : présentation calée sur l'écran, 1 025 images en 6,3 s ≈ 163 Hz pour un
écran à 165 Hz ; journal « présentation vsync ». Sans : « présentation immédiate ».

### Traversée du monde (M7-14, CPU seulement)

`launcher --world-bench 10 --world-bench-report perf.txt` : rapport complet dans
[`perf-world-rtx5070.txt`](perf-world-rtx5070.txt). Thread principal : moyenne 0,018 ms, p99 0,41 ms,
pire 1,13 ms (budget 25 ms) ; 0 trou ; 285 cellules chargées, 35 emplacements au plus ; 16 threads
de travail. **Ce benchmark ne rend rien** : le rendu du monde streamé n'existe pas encore (M7).

## Méthode

- `--perf-report` n'existe pas encore (il viendra avec le premier patch M9). Pour avoir le p99 et le
  pire sur toute la série, j'ai compilé **temporairement** une version du lanceur qui vide chaque
  durée d'image dans un fichier (`OW_FRAMES_DUMP`), puis je l'ai retirée : rien de cette
  instrumentation n'est commité. Les moyennes sont cohérentes avec le titre de la fenêtre et le
  panneau F1.
- Configurations : `resolution = 1920x1080`, `fullscreen = false`, `vsync = false` ; et
  `resolution = 2560x1600`, `fullscreen = true`, `vsync = false` (swapchain vérifiée en 2560 x 1600 par
  `--screenshot`).

## Ce que ces chiffres disent

- Les critères « 60 FPS » de M1 (scène avec ombres) et de M3 (1 km² avec herbe, végétation,
  rochers, eau) sont **largement tenus** sur ce GPU, y compris en 2560 x 1600 (pire image 12,3 ms).
- Le temps CPU affiché par F1 ne compte pas l'enregistrement des commandes GPU (`eng_renderer_end_frame`
  est hors de la mesure) : il sous-estime le coût CPU réel du rendu.
- Non mesuré : un GPU plus modeste (aucune autre machine ici), le temps GPU par passe (SDL_GPU n'a pas de
  requêtes de temps ; voir Tracy / RenderDoc).

## Monde streamé rendu (M7, `--world`) — 2026-11-19

Mesuré par le lanceur lui-même (résumé « MONDE : ... » à l'arrêt, sans instrumentation ajoutée),
Release, RTX 5070 Laptop, graine 7, pilote automatique vers +X (le personnage glisse sur le relief),
25 cellules de 256 m chargées autour du joueur. Les 30 premières images (chargement) sont exclues.

| Commande | Distance | Images | Moyenne | p99 | Pire | > 25 ms | Envois | Recentrages |
|---|---|---|---|---|---|---|---|---|
| `--world 7 --world-autopilot 60 --frames 6000` (vsync 165 Hz, ombre unique), 2 passes | 2 182 m | 5 969 | — | < 12 ms | 15,7 / 16,7 ms | 0 / 0 | 70 | 4 |
| `--world 7 --world-autopilot 100 --frames 9900` (vsync, ombre unique) | 6 000 m | 9 869 | — | < 11 ms | 16,6 ms | 0 | 140 | 11 |
| idem, 4 cascades d'ombre | 6 002 m | 9 869 | — | < 11 ms | 25,4 ms | 1 | 140 | 11 |
| `--config 1080p-sans-vsync --world 7 --world-autopilot 100 --frames 20000`, 4 cascades | 3 140 m | 19 969 | 1,57 ms | < 5 ms | 15,9 ms | 0 | 85 | 6 |

- La seule image au-dessus de 25 ms (25,4 ms) n'avait ni envoi de cellule ni recentrage, et 0,2 ms
  de CPU : c'est la présentation (vsync), pas le streaming. Une première passe a aussi montré une
  image de 62,8 ms jamais reproduite ensuite (4 passes).
- Coût d'un envoi de cellule sur le thread principal : ~0,9 ms en régime (maillage 0,3 ms,
  splatmap 0,4 ms, ressources GPU, corps Jolt 0,3 ms), mais la création des ressources GPU monte
  parfois à 10-15 ms : c'est le pire temps d'image. À traiter si un écran à 60 Hz montre des
  saccades (créer les tampons d'avance, ou pré-allouer les transferts).
- Chargement initial (9 cellules) : 110 ms.
