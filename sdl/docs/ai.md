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

## Archétypes (format)

Voir l'en-tête de `ai_archetype.h`. `light`/`heavy` nomment des attaques du jeu d'attaques de combat ;
`plan light|heavy portée_min portée_max recharge poids appuis` ; `phase fraction_PV légère lourde vitesse
recharge` (boss : jusqu'à 3, PV décroissants). Une erreur donne un message avec le numéro de ligne.

## Règles vérifiées par test (`test_ai_core`, `test_ai_camp`)

- Un ennemi ne voit pas à travers un obstacle, mais entend à travers.
- Jamais plus de 2 ennemis en état Attack ; séparation entre ennemis ≥ 0,5 m ; aucun ennemi dans un obstacle.
- Chaque attaque ennemie a ≥ 18 ticks entre le départ et le premier tick actif (mesuré sur un camp complet).
- Un ennemi contourne un mur pour rejoindre un joueur qu'il n'entend que (pas de vue).
- Cycle Patrol → Alert → Chase → Engage → Return → Patrol observé ; l'archer blessé fuit.
- Boss : 2 phases (changement d'attaques, vitesse, recharges) ; battable.
- Repos : camp remis à neuf (morts comprises), refusé pendant un combat.
- Même scénario → même hash (déterminisme, même binaire).

## Limites

Branché au jeu dans `--combat` (`docs/combat-play.md`). Navigation en plan (x, z) ; sur le terrain,
la grille bloque les pentes au-delà de 45° et les îlots de moins de 16 m² (ADR 0027 : un seul étage,
pas de Recast) ; les ennemis
tournent instantanément vers leur cible ; pas de projectile (l'archer attaque à longue portée) ;
éviter « l'empilement » = séparation simple des corps (pas d'évitement local prédictif) ;
perception : un seul joueur ; l'IA de groupe est testée jusqu'à 7 ennemis (limite du simulateur à 8).
