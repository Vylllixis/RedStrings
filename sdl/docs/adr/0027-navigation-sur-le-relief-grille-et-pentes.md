# ADR 0027 : navigation sur le relief — la grille garde la main (pentes, îlots), pas de Recast

Date : 2026-11-19 (finition, tâche M6-03). Complète l'ADR 0025.

## Contexte

L'ADR 0025 a choisi une grille régulière + A\* (`navgrid`) au lieu de Recast/Detour, en notant
qu'elle « ne gère ni dénivelé ni plusieurs étages ». Le tracker demande de trancher pour M6-03 :
passer à Recast, ou garder la grille et la tester sur le terrain avec un camp.

Mesure de départ (avant cette décision), sur un relief marqué : avec la grille plane, les ennemis
traversaient les pentes que le personnage Jolt ne peut pas gravir (1 949 ticks d'ennemi sur des
cellules à plus de 35° en 30 s), et 2 des 3 postes du camp tombaient dans une pente.

## Décision

On **garde la grille** et on lui donne le relief, en quatre pièces testées séparément :

1. `eng_nav_block_steep` : chaque cellule dont la pente (différences centrées sur le champ de
   hauteurs, à l'échelle d'une cellule) dépasse la limite est bloquée. La limite est **la même que
   celle du personnage Jolt** (`PLAY_MAX_SLOPE_DEG`, 45°) : les ennemis vont partout où le joueur
   peut aller, et nulle part ailleurs.
2. `eng_nav_remove_islands` : les régions praticables de moins de 64 cellules (16 m²) sont
   bloquées. C'est la « surface minimale de région » de Recast : sans elle, un ennemi déplacé ou
   poussé dans une poche entre deux pentes y reste coincé.
3. `eng_nav_nearest_walkable` : les postes et rondes du camp tombés dans une pente sont déplacés
   sur la cellule praticable la plus proche, au chargement.
4. Dans la simulation de combat (`EngCombatSim.nav`) : un combattant qui était sur une cellule
   praticable au début du tick n'entre jamais dans une cellule bloquée ; il glisse le long du bord.
   C'est ce qui manquait : A\* évitait les pentes, mais la séparation des corps, le recul des coups
   et les déplacements en cercle d'attente y poussaient les ennemis.

## Pourquoi pas Recast maintenant

- Recast/Detour est en C++ sans API C : il faudrait un shim C (ADR 0024 pour Jolt : même travail),
  une dépendance de plus à construire sous MSVC et sous Linux, et des builds plus longues.
- Ce que Recast apporterait de plus, plusieurs étages (ponts, grottes), n'existe pas dans ce jeu : le
  terrain est un champ de hauteurs, donc un seul étage par (x, z).
- Pour un camp (au plus 128 x 128 cellules de 0,5 m), la grille est déterministe (le replay de M5
  reste valable) et tient en ~430 lignes testées. Le coût d'un chemin n'est pas mesuré à part ;
  l'autotest du camp (1 800 ticks, 3 ennemis qui recalculent leur chemin) tourne en 0,1 s.

## Preuves

- `engine.ai_core` : `navgrid.steep_terrain_is_blocked_and_the_path_takes_the_gentle_ramp` (falaise
  de 76° percée d'une rampe de 15° : le chemin passe par la rampe),
  `navgrid.small_isolated_pockets_are_blocked_and_the_large_region_is_kept`, point praticable le plus
  proche.
- `engine.combat_sim` : `a_fighter_never_walks_into_a_steep_cell_and_slides_along_its_edge`.
- `launcher.camp_terrain` : le camp sur un relief de 60 m (longueur d'onde 100 m : la moitié des
  cellules dépasse 45°, les 3 postes tombent dans une pente) ; postes déplacés, 309 cellules d'îlots
  retirées, un ennemi atteint le joueur (1,8 m), 0 tick revenu sur une pente, au plus 2 attaquants,
  15 coups reçus.
- Mutations (chacune détectée) : glissement retiré (2 tests échouent), îlots gardés (2 tests),
  point le plus proche = point d'origine (autotest).

## Conséquences et limites

- La pente est mesurée au centre de la cellule, à l'échelle de 0,5 m : Jolt, lui, regarde la
  normale du triangle sous le pied. À ±quelques degrés près, il existe des points où l'un dit oui et
  l'autre non. Un joueur qui glisse dans une cuvette entourée de pentes est hors d'atteinte des
  ennemis de mêlée : ils s'arrêtent au bord (l'archer tire toujours).
- La grille couvre le camp (128 x 128 cellules au plus) ; le monde ouvert (M7) navigue cellule par
  cellule (`ai_area`), avec le même blocage des pentes à brancher sur les cellules streamées.
- Si un jour le jeu a des étages (ponts, intérieurs), Recast redevient la bonne réponse : l'interface
  de `ai_group` (`eng_nav_path`, `eng_nav_walkable`) est celle qu'un shim Detour exposerait.
