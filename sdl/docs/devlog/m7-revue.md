# Revue de jalon M7 — Monde ouvert (état partiel)

Légende : **Prouvé** (commande rejouable, Linux, sans GPU réel) ; **Partiel** ; **À faire**.
Tests : `test_jobs` (4), `test_world_stream` (11), `test_world_ai` (3), `test_world_lod` (7),
`launcher.world_bench` ; `tools/scripts/tsan-jobs.sh`. Rapport : `docs/devlog/perf-world-m7.txt`.

## Critères de sortie

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Aucune frame > 25 ms pendant une traversée rapide | 10 km à 30 m/s (release) : pire 6,7 ms, p99 0,2 ms CPU du thread principal ; 5 km à 50 m/s en test : 0 trou | **Partiel** : CPU seulement, le rendu ne streame pas encore |
| 2 | Mémoire bornée | streaming : 1,1 Mo fixes (64 emplacements), 35 au plus utilisés | Prouvé pour le streaming ; 30 min de jeu réel non mesurées |
| 3 | Ombres stables | cascades à taille de texel indépendante de l'orientation, alignées sur les texels (test) | **Partiel** : calcul seulement, pas dans les shaders |
| 4 | Précision aux bords du monde | origine flottante : µm à 10 km (test) ; combat + IA décalés au recentrage | Prouvé (CPU) ; « aucun tremblement visible » non vu |

## Tâches

| Tâche | État |
|---|---|
| M7-01 Partition | Prouvé (format RSCL documenté, monde de 64 cellules aller-retour) |
| M7-02 Jobs | Prouvé (TSan propre) |
| M7-03 Streaming | Prouvé côté CPU (lecture de fichier en job au lieu de l'E/S asynchrone SDL3 : écart) |
| M7-04 Uploads étalés | Partiel : ordonnanceur prouvé, upload GPU réel non branché |
| M7-05 LOD terrain | Partiel : sélection CDLOD prouvée, pas de rendu |
| M7-06 LOD meshes | Partiel : simplification + hystérésis prouvées ; pas de meshoptimizer, pas dans le cooker |
| M7-07 Imposteurs | À faire (optionnel) |
| M7-08 Culling GPU | À faire (important, non indispensable) |
| M7-09 Ombres en cascade | Partiel : calcul prouvé, pas dans les shaders |
| M7-10 Origine flottante | Prouvé (CPU) |
| M7-11 Physique/navmesh par cellule | Partiel : navigation de l'IA prouvée à travers les cellules ; Jolt non découpé |
| M7-12 Persistance | Prouvé |
| M7-13 Outil d'édition | À faire (important) |
| M7-14 Budgets | Prouvé (doc + benchmark en une commande + ctest) ; config de référence à fixer par toi |

## Ce qui manque pour « parcourir un monde de plusieurs km² »

Le rendu : créer/détruire les maillages de terrain par cellule à l'exécution (avec la file d'uploads),
le shader de morph CDLOD, les cascades dans les shaders, et le personnage physique sur les cellules
(corps Jolt par cellule). C'est le gros du travail restant de M7, et le seul qui se verra.
