# Journal des versions

Une version par jalon (tag `v0.X` à la revue du jalon, voir le tracker). Dates : celles des
livraisons ; les tags sont posés par Théo après ses tests.

## v0.9 (M9, en cours) — production
- Rapport de crash (signal, pile d'appels, système, version) dans `logs/crash-*.txt`.
- Paquet autonome : `tools/scripts/package.sh` (build Release + LTO, dossier unique, licences).

## v0.8 (M8, en cours) — finition
- Menu principal, pause, options (volumes par bus, sensibilité, inversion Y, taille du texte,
  contraste élevé, FXAA, bloom), contrôles remappables ; navigation complète au clavier et à la manette.
- Sauvegarde binaire versionnée « RSSV » (somme de contrôle, migration v1 -> v2, écriture
  atomique) ; reprise de la partie (position, PV, endurance, ennemis du camp).
- Interface : barres de PV / endurance / posture, cible verrouillée, nombres de dégâts.
- Bus audio (effets, musique, ambiance, interface) et son spatialisé.
- Post-process dans la passe de tonemapping : FXAA simplifié et bloom en une passe.
- Cycle jour/nuit : une journée complète dure 20 min réelles par défaut (critère M8-07 ; 24 min avant).
- Cycle jour / nuit : soleil et ciel suivent l'heure, clair de lune, exposition de nuit, heure au
  HUD et dans la sauvegarde (section TIME) ; `--day-night HEURE`, `--day-length S`.
- Progression : XP par ennemi vaincu, niveaux 1 à 20 (PV, endurance, dégâts), « Niv. N » et barre
  d'XP au HUD, section PROG dans la sauvegarde.
- M8 (finition) : police TrueType Noto Sans (OFL, `assets/fonts/`) pour l'interface et le HUD,
  nette en 1080p comme en 2560 x 1600 ; fond sombre derrière l'heure, le niveau et l'aide (B4).
- M8 (finition) : options graphiques vsync, plein écran, qualité des ombres, distance de vue ;
  ciel de nuit (étoiles, lune) ; brouillard de nuit corrigé (il prenait un ciel de jour) ; cubes de
  la démonstration assombris la nuit ; image moins délavée (ambiante 0,5, −0,5 EV, brouillard
  0,0008 dans le lanceur) ; `--exposure EV` et `--ambient X` pour régler.
- M8 (finition) : l'éclairage par image suit le soleil : recuit en tâche de fond dès qu'il a bougé de
  2°, envoyé en place (`eng_renderer_update_texture_data`).
- M8 (finition) : équipement : épée et bouclier, espadon gagné en battant le camp (R pour changer,
  attaques et arme tenue différentes, section de sauvegarde EQUI, `--weapon greatsword` pour
  essayer) ; ambiance (vent, oiseaux le jour, grillons la nuit) et musique synthétisées, bouclées
  sans trou (ADR 0028 : SDL3 + mixeur maison, pas de miniaudio).
- Finition (B8) : arbres et rochers KayKit (CC0) au lieu des sphères vertes et blanches du placement ;
  forêt claire, clairière au centre en combat ; ombres en cascade aussi sur un terrain (personnages,
  arbres et rochers instanciés : `shadow_instanced.vert`) ; avant, ce mode n'avait aucune ombre.
  Les arbres et rochers arrêtent le joueur et la caméra (une capsule Jolt par instance).
- Finition (M8-06) : SSAO dans la passe de résolution (profondeur conservée et relue, 12 prises,
  moyenne 2 x 2 par les dérivées) ; option « Occlusion ambiante (SSAO) » au menu.
- Finition (M8-06) : échelle de résolution (50 à 100 %, agrandissement bilinéaire, menu) ; mesure du
  coût de chaque effet (`--post-bench N`) ; FXAA moins coûteux avec le bloom (voisins sans halo).
- Finition (B19) : le plein écran enregistré dans les options est appliqué au lancement.
- Mode créatif (touche « , ») : invincible, vol libre, dégâts inchangés.
- Finition (B5) : piège `ENG_ALLOC_TRAP=1` (pile d'appels des allocations faites pendant une image) ;
  aucune allocation récurrente en jeu ; file d'événements d'ImGui réservée à l'avance.
- Finition (B18) : la capture d'écran (`--screenshot`) passe par une texture à nous : plus d'erreur
  de validation Vulkan en Debug.

## v0.7 (M7, partiel) — monde ouvert
- Cellules de 256 m (format RSCL), pool de threads, streaming asynchrone, origine flottante,
  persistance par cellule, uploads étalés, LOD du terrain (CDLOD) et des meshes, cascades
  d'ombre (calcul), benchmark de traversée.
- M7 (finition) : le monde est rendu et jouable (`launcher --world [GRAINE]`) : cellules envoyées
  au GPU par la file d'uploads, sans couture d'éclairage aux frontières, un corps Jolt par cellule,
  origine flottante appliquée au personnage, à la caméra et aux corps ; ombres en cascade dans les
  shaders (atlas 2 x 2, passe d'ombre par tuiles) ; mesures de traversée sur le RTX 5070 (6 km à
  100 m/s sans image > 25 ms) ; tests `launcher.world_render`, `launcher.world_ground`. LOD du
  terrain au rendu : 3 niveaux par cellule et des jupes contre les fentes (M7-05).

## Raccords
- Combat jouable dans le lanceur (`--combat`), sur l'arène ou sur le terrain procédural
  (`--terrain`), démonstration automatique (`--combat-autoplay`), capture d'écran.

- Grande carte (demande du 2026-11-20) : quatre biomes en bandes dans le monde ouvert (plaine,
  montagne enneigée, désert, volcan) : relief et matières par biome, frontières fondues.
- Grande carte : 12 zones d'ennemis (farm facile, farm dur, boss dans chaque biome), difficulté
  croissante, camp de réveil sans ennemis ; `--at-zone N` pour commencer devant une zone.
- Écran de chargement animé (2026-11-22, `docs/splash.md`) : dès que le GPU est prêt, une scène
  animée (l'aube sur la carte du monde, le chevalier, le fil rouge, le titre) pendant que le
  chargement s'exécute dans un fil à part ; avant, la fenêtre restait une copie figée de l'écran
  pendant plusieurs secondes. `--no-splash` pour s'en passer. Texte en champ de distance
  (`engine/text_sdf`).
- Menu principal (2026-11-22) : trois emplacements de sauvegarde (Continuer la plus récente,
  Charger une partie, Nouvelle partie avec choix de l'emplacement et confirmation avant d'écraser),
  souris dans les menus, colonne à gauche sur le monde, fondu d'entrée. Corrigé : dans le monde
  ouvert, la sauvegarde enregistrait la position locale du joueur (fausse après un recentrage de
  l'origine) ; elle enregistre sa position monde et le relief est chargé autour avant de le poser.
- Carte du monde (2026-11-22) : eau des lacs et de la rivière (surface dessinée de près et de loin,
  traversée à gué, de l'eau jusqu'à la taille) ; la lave brûle (6 % des PV par tiers de seconde,
  retour au dernier endroit sûr) ; les ennemis évitent les arbres et les rochers
  (`engine/ai_avoid`, idée de Théo : contournement à moins de 1 m, recul d'au moins 1 m au
  contact) ; décor entre les forêts (chênes isolés par groupes de 1 à 3, buissons, quelques
  rochers teintés par biome) ; herbe de la plaine (touffes animées par le vent, toujours aux mêmes
  endroits) ; `--at-pos X Z`. Corrigé : le relief proche du joueur était écrêté à ±61 m
  (montagnes, volcan et falaises aplatis de près, chute au sommet du volcan).
- Carte du monde dans le jeu (`--world`) : relief de la carte, sol à six matières (sable et lave
  dans une seconde carte de poids : emplacements 9 à 11 du renderer), arbres (chênes, sapins
  enneigés, acacias ; versions simplifiées au loin, troncs solides), vue lointaine de toute la
  carte (plan lointain 6 km), château de départ sur la colline, zones placées par la carte.
- Carte du monde dessinée à la main (`assets/world/carte.txt`, `engine/world_map`, outil `carte`) :
  4 km × 4 km, plaine au centre et château de départ sur sa colline, montagne au nord-ouest, désert
  au sud-ouest, volcan au nord-est, rivière, lacs, chemins, coulées de lave ; aperçu vu de dessus.
  Pas encore branchée dans le jeu.
- Grande carte : décor des zones en boîtes posées sur le relief (`engine/zone_decor`) : château
  du chevalier (murs, tours, deux portes, donjon), piliers de glace, ruines du désert, pics et
  brasiers du volcan, campements, camp de réveil ; solides pour le joueur et les ennemis.
  Renderer du lanceur : 256 samplers.
- Grande carte : sol par biome : sable dans le désert, basalte sombre et coulées de lave dans le
  volcan (deux couches de texture réutilisées par cellule, shader inchangé).
- Grande carte : boss spéciaux : Chevalier noir à cheval (cheval, cinématique de la chute, combat
  à pied) ; Dragon de lave (griffes, morsure, souffle de flammes) ; cheval et dragon en boîtes
  (`engine/creature`), logique dans `engine/boss_special` ; `--boss-selftest`. L'arène de
  démonstration n'est plus dessinée dans le monde ouvert.
- Grande carte : butin des boss (arme, armure, bouclier : palier 0 à 4), sauvegardé ; nom du boss
  de chaque biome ; `--gear N`.
- M7 (finition) : un camp d'ennemis dans le monde ouvert (`launcher --world --combat`), ancré dans le
  monde à travers les recentrages de l'origine flottante.

## v0.6 (M6) — ennemis et IA
- Perception, navigation (grille + A*), comportements, jetons d'attaque (2 au plus), archétypes en
  données, boss à phases, camps, débogueur d'IA.
- M6 (finition) : navigation sur le relief (ADR 0027) : pentes au-delà de 45° (même limite que le
  personnage Jolt) et îlots isolés bloqués, postes du camp déplacés hors pente, aucun ennemi ne
  remonte sur une pente (glissement le long du bord dans la simulation) ; autotest du camp sur un
  relief marqué (`launcher.camp_terrain`). Télégraphe : l'arme d'un ennemi s'allume en orange pendant
  l'anticipation puis en rouge pendant le coup (émissif par dessin). Clic dans la vue avec F1 :
  sélection d'un ennemi et son état dans le panneau. F3 : hitbox et hurtbox par-dessus les modèles.
- M6-09 (finition) : le boss dans le jeu, sur un terrain : arène de pierre murée avec entrée (murs
  Jolt sur le relief, contournés par l'IA), Seigneur des os (guerrier squelette géant à l'espadon,
  2 phases), barre de vie en bas de l'écran, ne renaît pas une fois vaincu ; enchaînements scriptés
  `plan pattern ... LLH` dans les archétypes ; arène inaccessible signalée au journal.

## v0.5 (M5) — combat
- Machine à états, frame data en données rechargeables, hitbox/hurtbox, esquive, garde, parade,
  combos, game feel, verrouillage, replay déterministe, audio et effets de combat, mannequin.
- M5 (finition) : F5 gèle le combat, F6 avance d'un tick ; panneau « Combat » de F1 (état, tick,
  frame data des combattants) ; traînée de la lame pendant les attaques ; arène de M4 dessinée en
  volumes éclairés avec ombres (au lieu du fil de fer).

## v0.0 à v0.4 (M0 à M4)
- Fondations, rendu (SDL_GPU, PBR, ombres), assets et scènes, monde (terrain, ciel, végétation),
  personnage (Jolt, caméra, animation).
- M4 (finition) : personnages animés dans le jeu (chevalier KayKit CC0 pour le joueur, squelettes pour
  les ennemis) : maillage skinné, casque et cape rigides, armes aux sockets des mains, ombres ;
  animateur piloté par l'état de combat (attaques calées sur la frame data, roulade procédurale sans
  glissement, fondus, mélange lissé) ; hurtboxes sur les os et lame animée dans la simulation ;
  portées de l'IA bornées par les vraies lames ; galerie `--avatar-gallery`. ADR 0026.
  Correctif : entrées du shader d'ombre skinné (locations non contiguës, refusées par Vulkan).
- M0 (finition) : vérifié sur la machine de Théo (fenêtre, F11, redimensionnement, clavier, vsync,
  rechargement à chaud du code de jeu) ; portabilité MSVC (atomiques C11, avertissements, fonctions
  POSIX remplacées) ; la CI publie l'exécutable Windows ; sources d'assets versionnées.
