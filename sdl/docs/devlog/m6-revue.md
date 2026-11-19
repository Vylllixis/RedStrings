# Revue de jalon M6 — Ennemis & IA

Légende : **Prouvé** = commande rejouable (Linux, sans GPU réel) ; **Partiel** ; **À faire** = de ton côté.
Tests : `test_ai_core` (16), `test_ai_camp` (11). Décisions : ADR 0025 ; modules : `docs/ai.md`.

## Critères de sortie

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Navigation sur navmesh sans blocage sur le camp de test | camp de 6 ennemis (3 archétypes, rochers + muret) : blocage maximal en poursuite = quelques ticks (< 120 exigés), aucun ennemi dans un obstacle ; contournement d'un mur vers un joueur seulement entendu | Prouvé — **avec une grille + A\*, pas Recast** (ADR 0025) ; plan x, z sans terrain |
| 2 | Au plus 2 attaquants simultanés | `eng_aig_attackers` ≤ 2 à chaque tick d'un combat complet (11 000 ticks) | Prouvé |
| 3 | Télégraphie lisible de chaque attaque ennemie | validation des archétypes (anticipation ≥ 18 ticks = 300 ms sur toute la chaîne) + mesure départ → premier tick actif ≥ 18 sur le combat complet | Prouvé (la lisibilité *visuelle* n'est pas jugée : animation absente) |

## Tâches

| Tâche | Livré | État |
|---|---|---|
| M6-01 ADR | `0025` | Prouvé |
| M6-02 Perception | vue (cône, portée, ligne de vue), ouïe, mémoire ; « pas à travers un rocher » testé | Prouvé |
| M6-03 Navmesh | `navgrid` (grille) ; dessin debug des cellules bloquées | Partiel : pas Recast, pas généré dans le cooker, pas de terrain |
| M6-04 Pathfinding | A\* + lissage + repath ; séparation des corps | Prouvé (évitement local = séparation simple) |
| M6-05 Comportements | Patrol, Alert, Chase, Engage, Return, Flee ; cycle observé | Prouvé en test (pas observé en jeu) |
| M6-06 Décisions de combat | plans pondérés (distance, recharge), se tourne avant de frapper | Prouvé |
| M6-07 Coordination | jetons (max 2), rotation, cercle d'attente | Prouvé |
| M6-08 Archétypes | fichier texte ; nouvel archétype sans recompiler | Prouvé ; « à distance » = poussée à longue portée, **sans projectile** |
| M6-09 Boss | 2 phases (attaques, vitesse, recharges), arène murée, battable en simulation | Partiel : pas de « patterns » scriptés, pas d'arène visuelle |
| M6-10 Débogueur d'IA | info, texte, sélection par rayon, dessin (cône, cible, jeton, chemin) | Partiel : **pas branché au clic de la caméra** ni à l'UI |
| M6-11 Camps | création, repos (refusé pendant un combat), vaincu | Prouvé |

## Ce que les preuves ne disent pas

Rien de M6 n'est branché au jeu ni visible à l'écran. Les ennemis tournent instantanément ; le déplacement
est en plan (x, z) ; la sim de combat est limitée à 8 combattants (joueur + 7). Le tag v0.6 et le test
« camp affronté à la main » sont de ton côté.

## Mutations (modules critiques)

ai_tokens 37/63, ai_perception 17/31, ai_camp 32/61 (après renfort), ai_archetype 78/150, ai_world 70/140,
navgrid 90/224, ai_debug 25/81, training_dummy 10/24. Beaucoup de survivants sont des bornes équivalentes
ou des valeurs de dessin ; je ne les ai pas tous éliminés ; `ai_group`
(cerveau) n'a pas été muté (trop lent : chaque mutant rejoue un combat de 11 000 ticks).
