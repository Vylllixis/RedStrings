# Revue de jalon M6 — Ennemis & IA

Légende : **Prouvé** = commande rejouable (Linux, sans GPU réel) ; **Partiel** ; **À faire** = de ton côté.
Tests : `test_ai_core` (16), `test_ai_camp` (11). Décisions : ADR 0025 ; modules : `docs/ai.md`.

## Critères de sortie

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Navigation sur navmesh sans blocage sur le camp de test | camp de 6 ennemis (3 archétypes, rochers + muret) : blocage maximal en poursuite = quelques ticks (< 120 exigés), aucun ennemi dans un obstacle ; contournement d'un mur vers un joueur seulement entendu. **Finition** : camp sur un relief marqué (`launcher.camp_terrain` : la moitié des cellules au-delà de 45°, les 3 postes dans une pente) : postes déplacés, îlots retirés, un ennemi atteint le joueur, 0 tick revenu sur une pente | Prouvé — **avec une grille + A\*, pas Recast** (ADR 0025, décision confirmée par l'ADR 0027) |
| 2 | Au plus 2 attaquants simultanés | `eng_aig_attackers` ≤ 2 à chaque tick d'un combat complet (11 000 ticks) | Prouvé |
| 3 | Télégraphie lisible de chaque attaque ennemie | validation des archétypes (anticipation ≥ 18 ticks = 300 ms sur toute la chaîne) + mesure départ → premier tick actif ≥ 18 sur le combat complet. **Finition** : animation calée sur la frame data (M4) et arme qui s'allume (orange en anticipation, rouge au coup), vues tick par tick à l'écran ; test GPU de l'émissif par dessin (mutation détectée) | Prouvé pour la durée et l'affichage ; « lisible » à juger par 2 testeurs (Q6) |

## Tâches

| Tâche | Livré | État |
|---|---|---|
| M6-01 ADR | `0025` | Prouvé |
| M6-02 Perception | vue (cône, portée, ligne de vue), ouïe, mémoire ; « pas à travers un rocher » testé | Prouvé |
| M6-03 Navmesh | `navgrid` (grille) ; dessin debug des cellules bloquées ; **finition** : pentes > 45° (limite du personnage Jolt), îlots < 16 m², postes déplacés, aucun ennemi ne remonte sur une pente (ADR 0027) | Fait (décision : grille, pas Recast) ; pas généré dans le cooker (calculé au chargement, < 0,1 s) |
| M6-04 Pathfinding | A\* + lissage + repath ; séparation des corps | Prouvé (évitement local = séparation simple) |
| M6-05 Comportements | Patrol, Alert, Chase, Engage, Return, Flee ; cycle observé | Prouvé en test (pas observé en jeu) |
| M6-06 Décisions de combat | plans pondérés (distance, recharge), se tourne avant de frapper | Prouvé |
| M6-07 Coordination | jetons (max 2), rotation, cercle d'attente | Prouvé |
| M6-08 Archétypes | fichier texte ; nouvel archétype sans recompiler | Prouvé ; « à distance » = poussée à longue portée, **sans projectile** |
| M6-09 Boss | 2 phases (attaques, vitesse, recharges), arène murée, battable en simulation | Partiel : pas de « patterns » scriptés, pas d'arène visuelle |
| M6-10 Débogueur d'IA | info, texte, sélection par rayon, dessin (cône, cible, jeton, chemin) ; **finition** : F1 + clic dans la vue sélectionne l'ennemi sous le curseur, section « IA » du panneau (état, perception, jeton, plan, recharges, distance, chemin) | Fait : vérifié dans la vraie fenêtre (balayage de 25 clics : la colonne de l'archer le sélectionne, les voisines non ; `docs/devlog/img/m5-m6-gel-panneau-selection.png`) |
| M6-11 Camps | création, repos (refusé pendant un combat), vaincu | Prouvé |

## Ce que les preuves ne disent pas

(Finition, 2026-11-19) Branché au jeu dans `--combat` avec les personnages animés de M4 ; vu sur le
RTX 5070. Reste : les patterns de boss (M6-09), le ressenti (2 testeurs, Q6).

Texte d'origine : rien de M6 n'est branché au jeu ni visible à l'écran. Les ennemis tournent instantanément ; le déplacement
est en plan (x, z) ; la sim de combat est limitée à 8 combattants (joueur + 7). Le tag v0.6 et le test
« camp affronté à la main » sont de ton côté.

## Mutations (modules critiques)

ai_tokens 37/63, ai_perception 17/31, ai_camp 32/61 (après renfort), ai_archetype 78/150, ai_world 70/140,
navgrid 90/224, ai_debug 25/81, training_dummy 10/24. Beaucoup de survivants sont des bornes équivalentes
ou des valeurs de dessin ; je ne les ai pas tous éliminés ; `ai_group`
(cerveau) n'a pas été muté (trop lent : chaque mutant rejoue un combat de 11 000 ticks).
