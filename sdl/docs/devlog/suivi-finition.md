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
| P0-02 État des lieux (vrai matériel) | jamais lancé sur un vrai GPU, aucune mesure | builds Debug et Release, ctest dans les deux ; lancement réel piloté (menus, nouvelle partie, marche, attaque, pause, sauvegarde, menu principal, Continuer) ; son enregistré à la sortie PipeWire ; mesures sans vsync de 4 scènes ; benchmark monde | ctest Release 150/150, Debug 150/150 ; captures de la vraie fenêtre (xwd) ; `launcher.log` « partie reprise (version 2, 6 s de jeu) » ; 22 pics sonores aux coups (crête 0,35 à 0,49) ; `docs/devlog/perf-materiel.md`, `perf-world-rtx5070.txt` | fait |
| M8-07 Durée du jour | 1440 s (24 min) | `ENG_DAYNIGHT_DAY_SECONDS` = 1200 ; test « une journée en 20 min » (72 000 pas de 1/60 s) | `engine.daynight` ; mutation 1440 s : 2 tests échouent | fait (commit `5266871`) |
| M0 Vérifs machine + CI | « À faire » partout dans m0-revue ; CI rouge depuis M1 | F11, redimensionnement, clavier, vsync, rechargement à chaud vérifiés dans la vraie fenêtre ; correctif plein écran (SDL_SyncWindow) ; portabilité MSVC (atomiques, C4324/C4127/C4996, SDL_strtok_r, _Alignas, masquages) ; artefact Windows dans la CI | captures xwd, journaux ; C11 strict / -Wvla / -Wconversion / -Wshadow propres ; CI non vérifiable sans push (Q3) | partiel (CI : Théo) (commit `2587041`) |
| M4 Personnage animé | capsules en fil de fer ; aucun modèle animé | chevalier + 4 squelettes KayKit CC0 (préparés, versionnés, cuits) ; maillages rigides sous les os ; animateur de combat (`actor_anim`) ; roulade procédurale ; armes aux sockets ; ligne `anim` dans attacks.txt ; hurtboxes sur les os et lame animée dans la simulation ; portées de l'IA bornées par les vraies lames ; galerie `--avatar-gallery` ; limite de samplers 32 -> 128 | `engine.actor_anim` (10), `engine.character_motion` (sync 11 attaques, glissement 1-10 %, transitions 7,5 cm), `engine.skin` (rigide), `engine.anim` (par nom), `engine.attack_data`, `engine.combat_rig` (lame), `launcher.combat_rig`, `launcher.avatar_gallery` ; mutations : 4 + 3 + 3 + 2 détectées ; capture `docs/devlog/img/m4-galerie.png` | fait sauf ressenti (Q5) |

## Phase 0 — ce qui est cassé, laid ou illisible (première liste de bugs)

| # | Constat | Gravité | Suite |
|---|---|---|---|
| B1 | Personnage et ennemis = capsules en fil de fer | majeur | **corrigé** (M4, KayKit) |
| B2 | Image délavée : ciel et sol presque blancs, brouillard qui écrase tout (toutes les scènes) ; chaîne sRGB vérifiée correcte : c'est le réglage (exposition, brouillard) | majeur (laid) | réglage avec M8-07 (jour/nuit) |
| B3 | Arène de M4 dessinée en fil de fer : on marche sur des marches et une rampe invisibles | moyen | rendre les boîtes de l'arène (M4) |
| B4 | HUD : police bitmap fine, petite ; heure presque invisible sur le ciel clair | moyen | M8-03 (stb_truetype, ombre/fond) |
| B5 | 26 allocations dans la boucle de frame en partie interactive (dès l'image 1 : menus ImGui, sauvegarde, reprise) ; 28 au plein écran / redimensionnement | moyen (ADR 0003) | à traiter : préchauffage de l'UI, E/S hors image |
| B6 | Menus : pas de flèches ni d'Entrée ; Échap quitte le jeu sans confirmation, même en partie | mineur (UX) | M8 (menus) |
| B7 | Journal « fenêtre ouverte : 2494 x 1531 » en plein écran : taille lue avant la bascule (asynchrone sous X11) ; la swapchain est bien en 2560 x 1600 | mineur | **corrigé** (M0, SDL_SyncWindow) |
| B8 | M3 : végétation = sphères vertes, rochers = sphères blanches | moyen (laid) | après M4 (assets CC0) |
| B9 | Démarrage ~5 s : création du périphérique Vulkan 2,8 s, IBL recalculée à chaque lancement 2 s | mineur | à voir (cache IBL) |
| B10 | Pas de manette sur la machine : non testable ici | — | Théo |

Mesures (détail : `perf-materiel.md`) : M1 avec ombres 694 FPS (1080p) / 365 FPS (2560 x 1600) ;
M3 1 km² complet 215 / 163 FPS, pire 12,3 ms ; combat 688 / 364 FPS ; combat sur terrain avec herbe
124 FPS. **Aucun critère 60 FPS n'est menacé** : l'ordre de la phase 1 du prompt reste valable.
