# ADR 0025 : IA des ennemis — machine à états à données, jetons d'attaque, navigation en grille

Statut : accepté (M6-01)

## Contexte

M6 demande des ennemis qui perçoivent, naviguent, attaquent de façon lisible et se coordonnent
(« au plus 2 attaquants simultanés », « anticipation ≥ 300 ms »). Le combat (M5) est déterministe, à pas
fixe, et rejouable : l'IA ne doit pas le casser.

## Options

- **Behavior tree** : arbre de nœuds (séquence, sélecteur, décorateurs). Puissant quand les comportements
  sont nombreux et recombinables ; coût : un interpréteur d'arbre, un format d'arbre, un éditeur ou une
  discipline de nommage, et des états implicites (« nœud en cours ») plus difficiles à inspecter.
- **Machine à états finis (hiérarchique)** : peu d'états (Patrol, Alert, Chase, Engage, Return, Flee) +
  sous-décisions dans l'état Engage (choix d'attaque pondéré). Un état courant lisible à l'écran.
- **GOAP / utility AI** : adapté à beaucoup d'actions concurrentes ; disproportionné pour 3 archétypes.

## Décision

Machine à états + **données** : l'état courant est une énumération affichable (débogueur M6-10) ; ce qui
varie entre ennemis (PV, vitesse, sens, plans d'attaque, phases de boss) est un fichier texte d'archétypes
(`ai_archetype`), pas du code. L'IA ne touche jamais à la simulation : elle **écrit des `EngFighterInput`**,
les mêmes entrées qu'un joueur — le replay M5 (entrées → hash) reste valable, un ennemi n'a aucun privilège.
La coordination est un module à part (`ai_tokens`) : au plus 2 jetons, rendus après chaque attaque avec un
délai de rotation, et un cercle d'attente pour les autres.

Navigation : **grille régulière + A\*** (`navgrid`) au lieu de Recast/Detour, pour deux raisons mesurées :
Recast/Detour n'a pas d'API C et n'est pas disponible hors ligne dans l'environnement de développement ;
pour un camp (≤ 128 × 128 cellules) la grille fait le même travail (contournement, lissage par corde
tendue) avec ~300 lignes déterministes. **Écart assumé avec le tracker** (« Recast + shim C »).

## Conséquences

- Un behavior tree reste possible plus tard : l'état courant deviendrait « le nœud actif » sans changer
  l'interface (entrées écrites, débogueur).
- Les archétypes sont rechargeables à chaud (texte) ; la validation refuse toute attaque ennemie dont
  l'anticipation est < 18 ticks (300 ms à 60 Hz).
- La grille ne gère ni dénivelé ni plusieurs étages (plan x, z) ; sur le vrai terrain il faudra projeter
  la hauteur et bloquer les pentes (tâche de raccord).
- Les attaques à distance sont des poussées à longue portée (pas de projectile) : écart assumé.
