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

## v0.6 (M6) — ennemis et IA
- Perception, navigation (grille + A*), comportements, jetons d'attaque (2 au plus), archétypes en
  données, boss à phases, camps, débogueur d'IA.
- M6 (finition) : navigation sur le relief (ADR 0027) : pentes au-delà de 45° (même limite que le
  personnage Jolt) et îlots isolés bloqués, postes du camp déplacés hors pente, aucun ennemi ne
  remonte sur une pente (glissement le long du bord dans la simulation) ; autotest du camp sur un
  relief marqué (`launcher.camp_terrain`). Télégraphe : l'arme d'un ennemi s'allume en orange pendant
  l'anticipation puis en rouge pendant le coup (émissif par dessin). Clic dans la vue avec F1 :
  sélection d'un ennemi et son état dans le panneau. F3 : hitbox et hurtbox par-dessus les modèles.

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
