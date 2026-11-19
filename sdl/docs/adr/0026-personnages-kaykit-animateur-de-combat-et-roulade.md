# ADR 0026 : personnages KayKit (CC0), animateur piloté par l'état de combat, roulade procédurale

Statut : accepté (finition de M4, 19/11/2026)

## Contexte

Tout le code d'animation de M4 existait (squelette, clips, pose, skinning GPU, fondus, espace de
mélange, machine à états de données, root motion, sockets, IK) mais aucun modèle animé n'était
branché : le joueur et les ennemis étaient des capsules en fil de fer. Il fallait un personnage
humanoïde animé, sous une licence qui permette de le versionner et de le distribuer, avec au moins
repos, marche, course, saut, esquive, trois attaques légères, une lourde, garde, touché et mort.

## Options pour les fichiers

- **Mixamo** : très complet, mais sa licence interdit de redistribuer les fichiers bruts (le dépôt et
  le paquet les contiendraient).
- **Quaternius** (Universal Animation Library, CC0) : bon catalogue, mais téléchargement par itch.io
  (page JavaScript) : pas de récupération scriptée fiable.
- **KayKit** (Kay Lousberg, CC0) : « Character Pack : Adventurers 1.0 » et « Skeletons 1.0 » sont sur
  GitHub (dépôts officiels), en glTF. Le chevalier porte 76 clips dont tous ceux de la liste sauf une
  **roulade** ; les quatre squelettes ont **exactement le même rig** (41 articulations, mêmes noms,
  mêmes translations) : ils partagent les clips du chevalier. Armes et boucliers sont des maillages
  séparés.

## Décision

1. **KayKit**, réduit par `tools/scripts/kaykit_prepare.sh` + `gltf_extract.py` (33 clips gardés et
   renommés, armes retirées du corps) et **versionné** dans `assets/characters/` et `assets/props/`
   (3,2 Mo) ; cuit au build (`ow_cook_character`). Licences : `assets/LICENCES.md`.
2. **Animateur de combat dédié** (`engine/actor_anim.h`) plutôt que la machine à états de données de
   M4-19 : l'instant d'un clip d'attaque doit être une **fonction du tick de combat** (frame data,
   gel d'impact, enchaînement de combo), pas d'un temps libre. Chaque attaque nomme son clip, sa
   vitesse et son décalage dans `attacks.txt` (ligne `anim`) ; un test vérifie sur les vrais clips
   que le passage de l'arme tombe dans les ticks actifs (`engine.character_motion`). La machine de
   M4-19 reste disponible pour des personnages non combattants.
3. **Roulade procédurale** : KayKit n'en a pas (ses « Dodge » sont des pas de 0,65 m en 0,4 s, alors
   que l'esquive du jeu parcourt 3 m). La pose est recroquevillée (`jump_start` à 0,3 s) et la racine
   tourne de `2π × avancée` autour de l'axe `haut × direction`, autour d'un pivot de rayon
   `distance / 2π` : un tour complet d'une boule de ce rayon parcourt exactement la distance de
   l'esquive, avec la même courbe (smoothstep) que la simulation. **La roulade parcourt donc la
   distance de son animation sans glisser (M4-20) par construction.**
4. **Hurtboxes et lame suivent l'animation** (M5-05, M5-06) grâce à une pose avancée **au rythme des
   ticks** (déterministe, indépendante de l'affichage) : hurtboxes par os (`assets/characters/
   hurtboxes.txt`), lame en main (`eng_sim_set_weapon`, qui remplace les hitboxes `hand_r` ; un
   carreau `bolt` reste dans le repère du corps). Les portées des plans de l'IA sont bornées par la
   portée réelle mesurée de chaque lame.

## Conséquences

- Le jeu affiche de vrais personnages ; le replay (M5-18) et les autotests sans squelette gardent le
  corps simplifié (rien ne change sans appel à `eng_sim_set_rig` / `eng_sim_set_weapon`).
- Proportions « chibi » (grosse tête) : style assumé du jeu ; mise à l'échelle 0,8 (1,85 m).
- La roulade procédurale est moins riche qu'une roulade animée par un artiste ; si un clip CC0 de
  roulade apparaît sur ce rig, il suffit de remplacer le rôle `ENG_ACTOR_ROLL_POSE` par un clip à root
  motion et d'ajuster `dodge_distance` à sa distance.
- Les ennemis partagent les clips du chevalier (même rig) : pas de démarche propre à chaque archétype.
