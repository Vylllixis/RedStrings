# Suivi de la finition M0 à M8

Session de finition sur la machine de Théo (vrai GPU, vraie fenêtre, son). Une ligne par tâche,
mise à jour à chaque tâche terminée. Questions et blocages : `../QUESTIONS-THEO.md` (racine du
dépôt). Branche locale : `finition-m0-m8` (jamais poussée par moi).

Machine : Ubuntu 24.04.5, Linux 7.0, X11 ; Intel Core i7-13650HX (20 threads) ; NVIDIA GeForce
RTX 5070 Laptop GPU (8 Go, pilote 580.178.04) + Intel UHD (hybride) ; écran interne 2560x1600
à 165 Hz ; son PipeWire.

| Tâche | État avant | Ce que j'ai fait | Preuve | État après |
|---|---|---|---|---|
| P0-01 Reprise du dépôt | M2 à M9 non commités ; dernière archive (jour/nuit, progression) non extraite ; une archive M8 perdue : la build ne compilait pas | branche `finition-m0-m8` ; archive extraite ; bus du mixeur, son 3D, dernière touche pressée, `play_mode_spawn_point/teleport`, `game_flow.c` dans la build reconstruits d'après `test_game_systems` ; module crash facultatif tant que le patch M9 n'est pas là (Q1) | `cmake --build build/debug` sans avertissement ; ctest Debug : voir P0-02 | fait |
