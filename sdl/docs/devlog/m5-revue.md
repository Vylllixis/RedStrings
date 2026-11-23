# Revue de jalon M5 — critères de sortie

Légende : **Prouvé** = commande rejouable (Linux, SwiftShader, sans GPU réel) ; **Partiel** ; **À faire** = de ton côté.

## Critères de sortie

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Attaques définies en données, rechargeables à chaud | `attack_data` + `EngAttackFile` (open/poll), ticks entiers source de vérité ; `test_attack_data`, `test_fighter` | Prouvé (pas vu en jeu) |
| 2 | Visualisation hitbox/hurtbox + pas à pas | `combat_debug` : pas avant/arrière/saut, frise, dessin par zone ; `test_combat_debug`. **Finition** : F5 gèle le jeu, F6 avance d'un tick (vu à l'écran : tick 306 → 312 après 6 pressions, image identique au pixel près tant que rien n'est pressé) ; panneau « Combat » de F1 (état, attaque, phase, PV, posture, endurance par combattant) | Prouvé — pas à pas, panneau et capsules (F3, dessinées par-dessus les modèles) vus à l'écran |
| 3 | Replay déterministe | `combat_replay` : même entrées → même hash ; une entrée changée → première divergence ≥ ce tick ; fichier RSRP | Prouvé (même binaire / même machine) |
| 4 | 2 testeurs jugent le combat lisible | — | **À faire** (ton côté) |

## Tâches partielles à la livraison initiale, complétées ensuite

- **M5-05** : la simulation accepte des hurtboxes pilotées par un squelette (`eng_sim_set_rig`),
  prouvé sur un squelette synthétique animé (`test_combat_rig`). Pas de squelette Mixamo réel.
- **M5-17** : `training_dummy` (statique / garde / attaque, compteur de dégâts, remise à neuf,
  renaissance), `test_training_dummy`. Cible « affichant les dégâts » = `damage_numbers`.

## Écarts au plan

- Touches par capsules analytiques, pas par requêtes de chevauchement Jolt.
- Buffer d'entrée de 10 ticks (167 ms) au lieu de 150 ms.
- Audio SDL3 au lieu de miniaudio.
- Esquive sans direction = pas en arrière ; cône de verrouillage 60° au total.

## Ce que les preuves ne disent pas

(Finition, 2026-11-19) Branché au personnage jouable et vu sur le RTX 5070 : coups, chiffres de dégâts,
traînée de lame, étincelles ; son non écouté par Théo (Q2) ; 2 testeurs à faire (Q5/Q6).

Texte d'origine : le combat n'est pas branché au personnage jouable : aucun coup n'est visible à l'écran ni audible.
Aucun 60 FPS mesuré, aucune latence audio matérielle, aucun fichier Mixamo réel. Le tag v0.5
dépend du critère 4.

## Mutations (modules critiques)

combat_hit 32/48, combat_damage 13/25, input_buffer 26/31, fighter 101/159, combat_replay 95/136,
combat_sim 45/77, mixer 35/52, combat_audio 27/48, lockon 69/114 (56 avant ajout de tests à joueur décalé ;
alors). Beaucoup de survivants sont des bornes équivalentes ou des constantes de réglage ; je ne
les ai pas tous éliminés.
