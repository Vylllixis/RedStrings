# Progression du joueur (M8-08)

Module : `engine/progression.h` (fonctions pures, `tests/test_progression.c`).

## Règles

| Quoi | Formule | Exemples |
|---|---|---|
| XP pour quitter le niveau L | 100 + 50 (L - 1) | 100, 150, 200... 1000 au niveau 19 |
| XP d'un ennemi tué | max(10, arrondi(PV max / 2)) | PV max 60 -> 30 XP |
| PV max au niveau L | 100 + 10 (L - 1) | 290 au niveau 20 |
| Endurance max | 100 + 5 (L - 1) | 195 au niveau 20 |
| Dégâts infligés | x (1 + 0,05 (L - 1)) | x 1,95 au niveau 20 |

- Niveau maximal 20 : l'XP au-delà est comptée (`total_xp`, saturée à 2^32 - 1) mais ne fait plus
  monter.
- Une montée de niveau soigne complètement (PV et endurance) et affiche « Niveau N ! » au centre
  de l'écran pendant 3 s. Le HUD montre « Niv. N » et une barre d'XP sous les barres du joueur.
- La mort garde le niveau (renaissance au maximum de PV du niveau). Le camp réapparaît après sa
  défaite : on peut donc gagner de l'XP en boucle (pas de plafond par camp, choix assumé pour la
  démo).
- Le multiplicateur de dégâts est `EngFighterParams.damage_scale` (0 = 1, donc les données et
  replays existants ne changent pas), appliqué dans `eng_combat_apply_hit` aux coups francs et
  gardés. La posture infligée n'est PAS multipliée.

## Sauvegarde

Section facultative `PROG` (niveau, XP du niveau, XP totale, en u32). Absente (ancienne
sauvegarde) : niveau 1. Incohérente (`eng_prog_valid` : niveau hors bornes, XP >= besoin, total <
XP) : fichier refusé en entier, comme pour les autres sections.

## Limites

- Pas d'arbre de compétences, pas d'équipement, pas de répartition de points : les statistiques
  sont fixées par le niveau.
- Valeurs d'équilibrage choisies à la main, jamais testées par des joueurs.
