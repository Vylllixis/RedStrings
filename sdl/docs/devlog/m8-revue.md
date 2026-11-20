# Revue de jalon M8 — Contenu & finition (état partiel)

Légende : **Prouvé** (commande rejouable, sans GPU réel) ; **Partiel** ; **À faire**.
Tests : `test_game_systems` (8), `launcher.game` (« AUTOTEST PARTIE : OK »). Doc : `docs/game-flow.md`.

## Critères de sortie

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Partie démarrée, sauvegardée, quittée et reprise sans perte | `--game-selftest` : nouvelle partie, sauvegarde, tout détruit, relance, Continuer : position (écart 0), temps de jeu, PV, endurance, ennemis identiques | Prouvé (pour ce que contient la sauvegarde : voir docs/game-flow.md) |
| 2 | Options graphiques et remappage des contrôles | remappage de toutes les actions + options audio/caméra/texte/contraste, enregistrés et relus ; **finition** : options graphiques FXAA, bloom, vsync, plein écran, qualité des ombres, distance de vue (menu, `options.txt`, appliquées et journalisées ; `engine.game_systems`) | **Partiel** : manquent SSAO, échelle de résolution, bloom en chaîne, FXAA 3.11, coût par effet |
| 3 | Vertical slice jouée 30 min sans crash | — | **À faire** (de ton côté ; le rapport de crash est prêt) |

## Tâches

| Tâche | État |
|---|---|
| M8-01 Mixeur et son 3D | Partiel : bus + spatialisation prouvés ; pas de miniaudio (SDL3), pas de musique ni d'ambiance, rien d'écouté |
| M8-02 Musique dynamique | À faire (optionnel) |
| M8-03 Interface | Fait : police TrueType Noto Sans (OFL) via l'atlas dynamique d'ImGui (stb_truetype), nette à toutes les tailles ; fond sombre sous l'heure, le niveau et l'aide ; captures 1080p et 2560 x 1600 (`docs/devlog/img/m8-hud-police-1080p-2560.png`) |
| M8-04 Menus | Prouvé (navigation sans souris testée, rendu vu en capture) |
| M8-05 Sauvegarde | Prouvé (v1 relue en v2, corruption refusée, écriture atomique) |
| M8-06 Post-process | Partiel : FXAA simplifié + bloom une passe, prouvés par test GPU et réglables au menu ; pas de SSAO, coût non mesuré sur vrai GPU |
| M8-07 Jour/nuit | Partiel : journée de 20 min ; soleil/ciel/lumière pilotés, clair de lune, exposition de nuit, heure au HUD et sauvegardée ; **finition** : étoiles et lune, brouillard de nuit corrigé (B17), cubes assombris (B16), réglage de jour (B2, Q8) ; IBL recuite pendant le jeu (soleil + 2°, cuisson en tâche de fond, envoi 3 ms) ; réglage de la nuit à juger (Q8) |
| M8-08 Progression | Partiel : XP par ennemi, niveaux 1-20 (PV, endurance, dégâts), HUD, sauvegarde PROG ; pas de compétences ni d'équipement (docs/progression.md) |
| M8-09 Quêtes | À faire (optionnel) |
| M8-10 Accessibilité | Partiel : remappage complet, taille du texte, contraste élevé ; pas de sous-titres ni de mode daltonien |
