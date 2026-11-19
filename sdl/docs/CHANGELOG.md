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

## v0.7 (M7, partiel) — monde ouvert (CPU)
- Cellules de 256 m (format RSCL), pool de threads, streaming asynchrone, origine flottante,
  persistance par cellule, uploads étalés, LOD du terrain (CDLOD) et des meshes, cascades
  d'ombre (calcul), benchmark de traversée. Pas encore affiché à l'écran.

## Raccords
- Combat jouable dans le lanceur (`--combat`), sur l'arène ou sur le terrain procédural
  (`--terrain`), démonstration automatique (`--combat-autoplay`), capture d'écran.

## v0.6 (M6) — ennemis et IA
- Perception, navigation (grille + A*), comportements, jetons d'attaque (2 au plus), archétypes en
  données, boss à phases, camps, débogueur d'IA.

## v0.5 (M5) — combat
- Machine à états, frame data en données rechargeables, hitbox/hurtbox, esquive, garde, parade,
  combos, game feel, verrouillage, replay déterministe, audio et effets de combat, mannequin.

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
