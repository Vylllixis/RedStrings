# Écran de chargement animé (2026-11-22)

## Le problème

Entre l'ouverture de la fenêtre et la première image du jeu, il se passe plusieurs secondes de
chargement (éclairage d'ambiance, carte du monde, cellules, personnages). Pendant ce temps la
fenêtre n'était jamais dessinée : sous X11 elle montrait une copie figée de ce qu'elle recouvrait.

## Ce qui est fait

- **Dès que le GPU est prêt**, le fil principal dessine une scène animée, pendant que les étapes
  longues du chargement s'exécutent **dans un fil à part**. La fenêtre vit pendant tout le
  chargement (les événements sont traités : on peut la déplacer, la fermer).
- La scène est un seul shader plein écran, `shaders/splash.frag.hlsl`, sans autre image que le
  titre : l'aube sur la carte du monde (montagnes enneigées à gauche, volcan à droite, château sur
  sa colline au centre), le chevalier sur sa falaise, et le **fil rouge** qui part de son écharpe,
  rejoint le mât du château puis le cratère du volcan (le chemin de l'aventure) en soulignant le
  titre « RED STRINGS ».
- Déroulé : fondu depuis le noir ; le fil se déroule (0,35 à 3,2 s) ; le titre apparaît de gauche à
  droite derrière un éclat (1,1 à 2,6 s) ; puis la scène vit en boucle tant que le chargement dure
  (rayons du soleil, nuages, brume, fumée et braises du volcan, coulées de lave, dragon qui passe,
  herbe, cape et bannière au vent, lucioles, étoiles, reflet qui balaie le titre) ; la corde du bas
  se remplit avec la progression ; à la fin, le soleil éclate, la scène rougit et s'éteint, et le
  menu apparaît en fondu.
- Durée : au moins 3,4 s (le temps que le titre apparaisse) puis 0,9 s de sortie. Une touche ou un
  clic après la première seconde **passe** l'animation dès que le chargement est fini.
- Le titre et la légende sont rasterisés au lancement en **champ de distance**
  (`engine/text_sdf.h`, stb_truetype, police Noto Sans) : nets à toute résolution, et la distance
  sert au halo et à l'apparition.

## Fils et règles de SDL_GPU

| Fil | Fait |
|---|---|
| principal | la fenêtre, les événements, l'image de la fenêtre (`SDL_WaitAndAcquireGPUSwapchainTexture`), le dessin de l'écran avec ses propres tampons de commandes |
| chargement | `splash_run(travail)` : `eng_renderer_init` + `demo_scene_init` ; `start_world` ; `play_mode_enable_combat` ; chargement des personnages. Il ne fait que créer et remplir des ressources. |

Pendant un `splash_run`, le fil principal ne touche ni au renderer du moteur ni à l'état d'`EngGpu`
(seulement au périphérique et à la fenêtre). L'interface (Dear ImGui), le menu et les options
restent initialisés sur le fil principal, entre deux travaux. Vérifié en Debug (ASan, UBSan,
couches de validation Vulkan) : aucun message.

La progression affichée est une **estimation par étapes** (rendu 50 %, monde 85 %, combat 92 %,
personnages 98 %) qui avance en douceur dans chaque étape : ce n'est pas une mesure exacte.

## Quand l'écran s'affiche

- Par défaut pour une partie lancée à la main.
- **Jamais** pour les lancements automatiques (`--frames`, autotests, mesures) : le chargement y
  est direct, sur le fil principal, exactement comme avant (`splash_run` appelle alors le travail
  sans créer de fil).
- `--no-splash` le retire ; `--splash` le force (tests) ; `--splash-shots DOSSIER` écrit 8 images
  de l'animation en PPM, les mesure (AUTOTEST CHARGEMENT) et quitte.

## La fenêtre n'apparaît qu'une fois dessinée (X11)

Avec l'écran de chargement, sous X11 et hors plein écran, la fenêtre est créée **cachée**
(`EngWindowDesc.hidden_until_shown`), le GPU et l'écran sont préparés, puis elle est affichée
(`eng_platform_show_window`) et sa première image suit aussitôt. Le plein écran enregistré dans les
options du jeu est maintenant appliqué dès l'ouverture de la fenêtre (avant : au milieu du
chargement).

## Après le chargement (2026-11-22)

- Vers l'**écran-titre** : la scène ne s'éteint plus. `splash_finish(..., false)` laisse le titre
  apparaître puis rend la main ; l'écran-titre (`docs/title-menu.md`) reprend la même scène au même
  instant, dans la boucle du jeu.
- Vers le jeu directement (`--new-game`) : la sortie d'avant (éclat du soleil, fondu au noir).
- Les ressources (pipeline, image du titre) ne sont plus libérées à la fin du chargement : elles
  servent à l'écran-titre, jusqu'à l'arrêt du jeu (`splash_shutdown`).
- Échap pendant le chargement passe l'animation (il ne ferme plus le jeu).

## Tests

- `engine.text_sdf` (4) : champ de distance, marges, espacement, accents, lecture bilinéaire, refus.
- `launcher.splash` : chargement réel avec l'écran : des images sont présentées PENDANT le
  chargement (quatre travaux dans le fil de chargement), puis le jeu tourne, sans erreur.
- `launcher.splash_shots` : 8 images mesurées : il sort du noir, le fil part puis traverse le
  ciel, le titre apparaît à moitié puis en entier, la corde se remplit, tout s'éteint.

## Limites (dites clairement)

- **Pas essayé sur un vrai écran** : tout a été vérifié hors écran, en rendu logiciel (lavapipe).
  La fenêtre « cachée puis affichée » sous X11 et le rendu sur la carte graphique réelle restent à
  constater. En cas de souci : `--no-splash` rend le lancement d'avant.
- En rendu logiciel, l'animation prend tous les cœurs et ralentit le chargement (35 s au lieu de
  3 s) : c'est propre au rendu logiciel des tests ; sur une carte graphique le shader coûte de
  l'ordre de la milliseconde.
- Quelques étapes courtes restent sur le fil principal (interface, options, pose du décor) : une
  image est présentée entre elles, mais l'animation peut marquer un léger à-coup (mesuré : 102 ms
  au plus en Debug logiciel).
- Pas de son pendant l'écran de chargement.
