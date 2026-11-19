# Revue de jalon M7 — Monde ouvert

Légende : **Prouvé** (commande rejouable, Linux, sans GPU réel) ; **Partiel** ; **À faire**.
Tests : `test_jobs` (4), `test_world_stream` (11), `test_world_ai` (3), `test_world_lod` (7),
`launcher.world_bench` ; `tools/scripts/tsan-jobs.sh`. Rapport : `docs/devlog/perf-world-m7.txt`.

## Critères de sortie

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Aucune frame > 25 ms pendant une traversée rapide | **Finition (vrai rendu, RTX 5070)** : `--world 7 --world-autopilot 100` : 6 km à 100 m/s, 9 869 images, pire 16,6 ms, p99 < 11 ms, **0 image > 25 ms** (ombre unique) ; avec 4 cascades : 1 image à 25,4 ms (présentation, sans envoi ni recentrage) ; sans vsync en 1080p : 3,1 km, moyenne 1,57 ms, pire 15,9 ms, 0 image > 25 ms. Avant : 10 km CPU seul (pire 6,7 ms) | **Prouvé sur le RTX 5070** (une image limite à 25,4 ms due à la présentation, notée) ; non mesuré sur un GPU modeste |
| 2 | Mémoire bornée | streaming : 1,1 Mo fixes (64 emplacements), 35 au plus utilisés ; rendu : 30 emplacements au plus sur 6 km, arènes fixes par emplacement (64 Kio de tables GPU) | Prouvé pour le streaming et le rendu ; 30 min de jeu réel non mesurées |
| 3 | Ombres stables | cascades à taille de texel indépendante de l'orientation, alignées sur les texels (test) ; **dans les shaders** (atlas 2 x 2, `shadow_gpu` : chaque quart = carte unique, 2 mutations détectées) ; vues dans le monde (ombre nette du chevalier) | **Prouvé** pour le calcul et l'affichage ; « pas de scintillement en mouvement » vu sur captures fixes seulement |
| 4 | Précision aux bords du monde | origine flottante : µm à 10 km (test) ; combat + IA décalés au recentrage ; **monde rendu** : 11 recentrages sur 6 km, personnage, caméra et corps Jolt décalés ensemble, joueur au sol à l'arrivée (0,05 m) | Prouvé ; « aucun tremblement visible » à regarder à l'œil (`launcher --world`, Q7) |

## Tâches

| Tâche | État |
|---|---|
| M7-01 Partition | Prouvé (format RSCL documenté, monde de 64 cellules aller-retour) |
| M7-02 Jobs | Prouvé (TSan propre) |
| M7-03 Streaming | Prouvé côté CPU (lecture de fichier en job au lieu de l'E/S asynchrone SDL3 : écart) |
| M7-04 Uploads étalés | Fait : branché au rendu du monde (une cellule par image, plus proches d'abord) ; `launcher.world_render` |
| M7-05 LOD terrain | Partiel : sélection CDLOD prouvée, pas de rendu |
| M7-06 LOD meshes | Partiel : simplification + hystérésis prouvées ; pas de meshoptimizer, pas dans le cooker |
| M7-07 Imposteurs | À faire (optionnel) |
| M7-08 Culling GPU | À faire (important, non indispensable) |
| M7-09 Ombres en cascade | Fait : atlas 2 x 2 dans les shaders, passe d'ombre par tuiles ; mode monde |
| M7-10 Origine flottante | Fait : aussi dans le monde rendu (personnage, caméra, corps Jolt) |
| M7-11 Physique/navmesh par cellule | Partiel : Jolt découpé par cellule (champ de hauteurs ajouté/retiré, `launcher.world_ground`) ; navigation de l'IA prouvée à travers les cellules mais pas d'ennemis dans le monde rendu |
| M7-12 Persistance | Prouvé |
| M7-13 Outil d'édition | À faire (important) |
| M7-14 Budgets | Prouvé (doc + benchmark en une commande + ctest) ; config de référence à fixer par toi |

## Ce qui manque pour « parcourir un monde de plusieurs km² »

Le rendu : créer/détruire les maillages de terrain par cellule à l'exécution (avec la file d'uploads),
le shader de morph CDLOD, les cascades dans les shaders, et le personnage physique sur les cellules
(corps Jolt par cellule). C'est le gros du travail restant de M7, et le seul qui se verra.
