# Revue de jalon M4 — Le personnage

Légende : **Prouvé** = commande ou test rejouable (machine de Théo : RTX 5070, Linux) ; **Partiel** ;
**À faire** = de ton côté (ressenti). Écrite pendant la finition (19/11/2026) : la première passe
n'avait pas de revue M4. Détail du personnage : `docs/characters.md`, décision : ADR 0026.

## Critères de sortie

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Un personnage animé contrôlable (physique Jolt, caméra 3e personne) | `launcher --combat` : chevalier KayKit skinné (casque, cape, épée, bouclier), ombres ; `launcher --avatar-gallery` : les 15 états de face (capture `docs/devlog/img/m4-galerie.png`) ; `launcher.play`, `launcher.combat`, `launcher.combat_rig` | **Prouvé** |
| 2 | Pieds sans glissement (M4-18) | `engine.character_motion` (vrais clips) : glissement net du pied d'appui 6 % (0,67 m/s), 10 % (1,5), 2 % (2,84), 1 % (4,5 et 7,2) ; seuil 15 % ; mutation (vitesse de course fausse) détectée | **Prouvé** (mesure) ; à l'œil : à confirmer |
| 3 | Transitions sans à-coup (M4-17) | `engine.character_motion` : 7,5 cm au plus pour une articulation sur une image de changement d'état, 17,9 cm sur une image ordinaire ; mutation (fondus coupés) détectée ; `engine.actor_anim` | **Prouvé** (mesure) |
| 4 | Roulade qui parcourt la distance de l'animation (M4-20) | roulade procédurale : rayon = distance / 2π, angle = 2π × la même courbe que le déplacement : roule sans glisser par construction (ADR 0026) ; `engine.actor_anim` (rotation et pivot exacts) | **Prouvé** (par construction) ; **écart** : pas de clip de roulade (KayKit n'en a pas) |
| 5 | Le déplacement est jugé agréable par un testeur extérieur | — | **À faire** (Théo : QUESTIONS-THEO Q5) |

## Tâches

| Tâche | État |
|---|---|
| M4-01 à M4-11 (Jolt, personnage, locomotion, saut, sprint, caméra et collision) | Prouvé par `launcher.play` (première passe) ; vu dans la vraie fenêtre (19/11) |
| M4-12 Squelette | Prouvé ; vrai rig de 41 articulations |
| M4-13 Clips | Prouvé ; 33 clips KayKit cuits par nom (`--anim-name`) |
| M4-14 Pose | Prouvé |
| M4-15 Skinning GPU | Prouvé ; **vu à l'écran** ; maillages rigides sous les os inclus (test `skin`) |
| M4-16 Débogueur d'animation | Prouvé (première passe) ; galerie `--avatar-gallery` en plus |
| M4-17 Fondus | Prouvé (mesure ci-dessus) |
| M4-18 Espace de mélange | Prouvé (mesure ci-dessus) ; vitesse lissée (0,1 s) |
| M4-19 Machine à états de données | Prouvé (tests) ; **non utilisée par le joueur** : animateur de combat dédié (ADR 0026) |
| M4-20 Root motion | Partiel : la roulade est procédurale ; root motion des clips non utilisé (clips KayKit en place) |
| M4-21 Pipeline d'import | Prouvé avec KayKit (`kaykit_prepare.sh`) ; Mixamo toujours non essayé |
| M4-22 Sockets | Prouvé : armes aux mains (décalages KayKit) |
| M4-23 IK à deux os | Partiel : module testé, **pas appliqué** aux pieds des personnages |

## Ce que les preuves ne disent pas

- Le ressenti (fluidité, réactivité, lisibilité) : critère 5, de ton côté.
- Les personnages n'ont pas encore été mesurés en performance (la scène de combat faisait 688 FPS
  avant eux) : à refaire.
- L'IK des pieds sur le terrain (pentes) n'est pas branchée.

## À cocher sur le tracker

M4-15 à M4-18, M4-21, M4-22 : terminées (vu à l'écran). M4-20, M4-23 : partielles (voir ci-dessus).
Critère « sensation » : en attente de ton test.
