# IA des ennemis (M6)

Décisions : `docs/adr/0025-ia-machine-a-etats-donnees-et-jetons.md`. Modules (tous en plan x, z, ticks
à 60 Hz, déterministes) :

| Module | Rôle |
|---|---|
| `ai_world` | obstacles (cercles, boîtes), distance signée, ligne de vue, repoussage |
| `navgrid` | grille de navigation, A\* 8 voisins sans coupe de coin, lissage par corde tendue |
| `ai_perception` | vue (portée + cône + ligne de vue), ouïe (traverse les murs), mémoire de la dernière position |
| `ai_tokens` | jetons d'attaque (max 2), cercle d'attente autour du joueur |
| `ai_archetype` | fichier texte d'archétypes (stats, sens, plans d'attaque, phases de boss) + validation ≥ 300 ms |
| `ai_group` | cerveau : états Patrol / Alert / Chase / Engage / Return / Flee / Dead, écrit des `EngFighterInput` |
| `ai_camp` | camps (création, repos du joueur, arène de boss) |
| `ai_debug` | état, chemin, cône, jeton, sélection par rayon |
| `ai_avoid` | évitement local des troncs et des rochers (carte du monde) : contournement à moins de 1 m, recul au contact |

## Archétypes (format)

Voir l'en-tête de `ai_archetype.h`. `light`/`heavy` nomment des attaques du jeu d'attaques de combat ;
`plan light|heavy portée_min portée_max recharge poids appuis` ; `plan pattern portée_min portée_max
recharge poids SUITE` (M6-09 : 2 à 4 lettres `L` légère / `H` lourde, par exemple `LLH`, jouées l'une
après l'autre : chacune part dès la fin de la précédente, le jeton est gardé, la recharge compte dès la
première) ; `phase fraction_PV légère lourde vitesse recharge` (boss : jusqu'à 3, PV décroissants). Une
erreur donne un message avec le numéro de ligne.

## Règles vérifiées par test (`test_ai_core`, `test_ai_camp`)

- Un ennemi ne voit pas à travers un obstacle, mais entend à travers.
- Jamais plus de 2 ennemis en état Attack ; séparation entre ennemis ≥ 0,5 m ; aucun ennemi dans un obstacle.
- Chaque attaque ennemie a ≥ 18 ticks entre le départ et le premier tick actif (mesuré sur un camp complet).
- Un ennemi contourne un mur pour rejoindre un joueur qu'il n'entend que (pas de vue).
- Cycle Patrol → Alert → Chase → Engage → Return → Patrol observé ; l'archer blessé fuit.
- Boss : 2 phases (changement d'attaques, vitesse, recharges) ; battable.
- Enchaînement scripté (`plan pattern ... LLH`) : léger, léger, lourd, chaque attaque partant au plus
  3 ticks après la fin de la précédente, puis la recharge du plan (mutations détectées : suite non
  poursuivie, lettres inversées).
- `eng_camp_cleared_range` : le camp est vaincu même si un boss du même groupe vit.
- Repos : camp remis à neuf (morts comprises), refusé pendant un combat.
- Même scénario → même hash (déterminisme, même binaire).

## Évitement des arbres (`ai_avoid`, 2026-11-22)

Idée de Théo, sans calcul de chemin : une règle locale appliquée à la direction de marche que
l'IA vient de choisir (`play_combat_tick`, après `eng_aig_tick`).

- À plus de 1 m (`ENG_AVOID_MARGIN`) de tout obstacle : rien ne change.
- À moins de 1 m : la part de la direction qui va VERS l'obstacle est réduite en proportion de la
  proximité (rien à 1 m, tout au contact) et remplacée par un pas de côté, du côté où l'ennemi
  allait déjà : il contourne. La vitesse est conservée.
- Au contact : il s'éloigne droit dehors jusqu'à être revenu à 1 m (`EngAvoidState.fleeing`),
  quoi que veuille son IA.
- `eng_avoid_push_out` garantit en plus qu'un corps n'est jamais DANS un obstacle (recul d'un
  coup, poussée d'un voisin).

Les obstacles sont des cercles (x, z, rayon) : sur la carte du monde, les troncs et les rochers à
moins de 48 m du camp (`world_trees_obstacles`, redonnés toutes les 30 images). Tests :
`engine.ai_avoid` (5), autotest `--boss-selftest` (un ennemi contourne un tronc posé sur son
chemin ; posé dans le tronc, il en sort et s'éloigne de 1 m), `launcher.world_trees_avoid`.

## Limites

Branché au jeu dans `--combat` (`docs/combat-play.md`). Navigation en plan (x, z) ; sur le terrain,
la grille bloque les pentes au-delà de 45° et les îlots de moins de 16 m² (ADR 0027 : un seul étage,
pas de Recast) ; les ennemis
tournent instantanément vers leur cible ; pas de projectile (l'archer attaque à longue portée) ;
éviter « l'empilement » = séparation simple des corps (pas d'évitement local prédictif) ;
perception : un seul joueur ; l'IA de groupe est testée jusqu'à 7 ennemis (limite du simulateur à 8).
