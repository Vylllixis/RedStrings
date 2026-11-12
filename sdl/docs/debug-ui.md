# Interface de debug (M1-12)

Un panneau Dear ImGui par-dessus la scène : images par seconde, temps CPU, dessins, mémoire des
arènes, et des **réglages en direct** de variables du programme.

| Touche | Effet |
|---|---|
| F1 | affiche / masque le panneau (masqué au départ ; `--ui` l'affiche dès le lancement). À l'ouverture la souris est **libérée** (sinon on ne peut pas cliquer), et reprise à la fermeture si elle était capturée avant |
| F2 | libère / reprend la souris (comme avant) |
| F3 | debug draw (la case « debug draw (F3) » du panneau règle la même variable) |

Masqué, le panneau ne coûte **rien** : aucune image d'interface, aucune passe de rendu ajoutée.

## Ce que montre le panneau

- **Images** : FPS, durée moyenne et pire durée d'une image, temps CPU (moyenne et pire) sur les
  120 dernières images, et deux graphiques. *Durée de l'image* = de début d'image à début d'image,
  attente de la synchronisation verticale comprise. *CPU* = travail du processeur dans l'image
  (événements, simulation, jeu, enregistrement des dessins **et construction du panneau**), sans
  l'attente de présentation. Le temps **GPU** n'est pas mesuré : SDL_GPU n'offre pas de requêtes de
  temps (à voir avec un outil externe : RenderDoc, Tracy GPU…).
- **Rendu** : dessins, changements de pipeline et envois d'uniforms de la **dernière image** ;
  dessins rejetés et perdus (cumulés, doivent rester à 0).
- **Mémoire** : occupation, capacité et pic des trois arènes (`permanent`, `level`, `frame`), et le
  nombre d'allocations vues pendant les images depuis le début (objectif : 0, M0-12).
- **Réglages** : voir ci-dessous.

## Réglages en direct (`engine/tweak.h`)

Le code enregistre l'**adresse** d'une de ses variables, un nom et des bornes ; le panneau en fait un
curseur (`float`, `int`) ou une case (`bool`). Le registre n'a pas de copie : il écrit dans la
variable elle-même, donc ce que le panneau montre suit aussi les changements faits par le
programme (ex. la molette qui règle la vitesse de la caméra déplace le curseur correspondant).

```c
EngTweaks tweaks;
eng_tweaks_init(&tweaks);
eng_tweaks_add_float(&tweaks, "champ de vision (deg)", &fov_degrees, 30.0F, 110.0F);
eng_tweaks_add_bool(&tweaks, "debug draw (F3)", &platform.debug_draw);
```

Toute modification passe par `eng_tweaks_set_*` : valeur **bornée**, NaN refusé. Refus à
l'enregistrement : nom nul, vide ou déjà pris, adresse nulle, bornes invalides, valeur initiale hors
bornes, table pleine (`ENG_TWEAK_MAX` = 64). Pas d'allocation ; les noms ne sont pas copiés
(littéraux). « Tout réinitialiser » remet chaque variable à sa valeur d'enregistrement.

Réglages du lanceur : vitesse de la démo (rotation, 0–4), champ de vision (30–110°), vitesse de la
caméra (0,5–50 m/s), couleur de fond (3 curseurs), debug draw, et l'éclairage du modèle (M1-13) :
azimut et hauteur du soleil, intensité du soleil, intensité de l'environnement
(`docs/lighting.md`), l'ombre du soleil (M1-17) : case `ombre : active`, biais constant, de pente et normal,
rayon du filtre (0–3), force (`docs/shadows.md`), puis le tonemapping (M1-14) : opérateur (0 aucun, 1 ACES, 2 AgX, curseur
entier) et exposition en EV (−10 à +10) (`docs/tonemapping.md`). Les curseurs « fond » règlent
maintenant une couleur de scène linéaire (plus claire une fois encodée en sRGB qu'avant M1-14).

Dans un champ saisi à la main (Ctrl + clic sur un curseur) la valeur est bornée aussi.

## Architecture

```
SDL_Event ──► platform.event_hook ──► eng_ui_event_hook ──► backend SDL3 d'ImGui
                    (consommé ? le jeu ne voit pas l'événement)

image :  scène → debug draw → eng_ui_begin_frame / eng_ui_draw_panel / eng_ui_end_frame
         eng_renderer_end_frame :
            flush des buffers de streaming          ┐ passes de COPIE, avant toute
            overlay_prepare  (sommets ImGui → GPU)  ┘ passe de rendu
            passe 1 : scène (couleur + profondeur, effacée)
            passe 2 : overlay (couleur CONSERVÉE, sans profondeur) → backend SDL_GPU d'ImGui
```

- **cimgui** fournit l'API C de Dear ImGui (`ui.c` est du C). cimgui n'a pas de backend SDL_GPU : on
  appelle directement les backends C++ d'ImGui (`imgui_impl_sdl3`, `imgui_impl_sdlgpu3`) depuis
  `engine/src/ui_backend.cpp`, la seule couche C++ du moteur.
- **Passe à part** : le backend SDL_GPU d'ImGui dessine sans profondeur ; la scène a une
  profondeur. Une passe overlay (`eng_gpu_begin_overlay_pass`) charge la couleur existante
  (`LOAD`, pas `CLEAR`) et n'a aucune cible de profondeur. Les pipelines d'ImGui ne déclarent donc
  aucun format de profondeur. La préparation (envoi des sommets) doit précéder toute passe de rendu :
  c'est le rôle de `EngOverlayPrepareFn`, qui retourne `false` s'il n'y a rien à dessiner (passe
  non ouverte).
- **Entrées** : `EngPlatform.event_hook` reçoit chaque événement SDL avant tout autre traitement.
  Il retourne `true` pour un événement que l'interface veut (souris sur un panneau, clavier dans un
  champ de saisie) : le jeu ne le voit pas. Les **relâchements** (touche, bouton) sont toujours
  transmis aux entrées du jeu, même consommés : une touche pressée avant l'ouverture d'un champ de
  saisie ne reste pas « coincée ».
- **Mémoire** : ImGui alloue par `SDL_malloc` (visible du compteur d'allocations). En régime établi,
  la construction du panneau n'alloue rien (testé). Le backend SDL_GPU, lui, alloue à chaque image
  (tampons de transfert) : l'interface est construite **après** `eng_memory_end_frame`, hors du
  compteur de M0-12, mais ses événements passent par `eng_platform_pump`, qui est dans le compteur :
  l'ouverture du panneau peut donc faire apparaître l'avertissement « allocations dans la boucle de
  frame » une fois (file d'événements d'ImGui). Masqué : aucun effet.

## Dépendances et compilation

- Option CMake `OW_UI` (activée par défaut). Elle télécharge, à la configuration et **une seule
  fois**, Dear ImGui (commit `6d32174`, 1.93.0 « docking », cloné en entier : environ 145 Mo) et
  cimgui (commit `2a6a93f`), et demande un **compilateur C++**. Les deux commits vont ensemble :
  cimgui est généré pour une version précise d'ImGui et ne compile pas avec une autre.
- Désactivée (`-DOW_UI=OFF`) : ni C++ ni ImGui, `eng_ui_available()` retourne `false` et toutes
  les fonctions `eng_ui_*` sont sans effet.
- ImGui est compilé sans fenêtres de démonstration (`IMGUI_DISABLE_DEMO_WINDOWS`), sans `imgui.ini`
  (la disposition est décidée par le code).
- Code tiers : compilé sans nos options d'avertissements (ADR 0002). `ui_backend.cpp` est notre code
  et suit nos options.

## Limites connues

- **Le jeu (`game.so`) ne peut pas régler de variables** : il ne lie pas le moteur (ADR 0001) et ne
  voit ni ImGui ni le registre. Les réglages sont ceux du lanceur et de la scène de démo.
  Un service de réglages dans `EngServices` (version d'API 3, avec une nouvelle entrée du jeu pour
  déclarer ses variables) est la suite naturelle ; pas fait ici.
- **Rendu sur la vraie fenêtre non vérifié par lecture de pixels** : la swapchain ne se relit pas.
  Le panneau est vérifié en texture hors écran, avec les mêmes réglages de passe que l'overlay
  (`eng_gpu_fill_overlay_target`) ; l'enchaînement scène → overlay sur la swapchain est vérifié par
  compteurs (`overlay_passes`, ordre prepare / dessins / overlay), pas par l'image. À confirmer à
  l'œil sur une vraie fenêtre.
- Panneau et souris capturée (F2 pendant que le panneau est affiché) : les clics au-dessus du
  panneau restent consommés par l'interface.
- Police ImGui par défaut (ProggyClean, taille 13 px), pas de mise à l'échelle HiDPI de la police.
- Pas de graphique du temps GPU, pas de panneau par système (physique, IA… : M3+).
- Dessins de la **dernière** image : le panneau est construit avant `end_frame` de l'image courante.

## Vérifier

```
cmake --preset debug && cmake --build build/debug
ctest --test-dir build/debug -R "ui|frame_stats|tweak|platform_input|renderer|gpu|launcher.ui"
./build/debug/bin/launcher --ui        # panneau affiché dès le départ ; F1 le masque
OW_UI_DUMP=/tmp/panneau.ppm build/debug/tests/bin/test_ui   # enregistre l'image du panneau
```
