# Personnages animés (M4)

Le joueur est un **chevalier** et les ennemis des **squelettes**, tous KayKit (Kay Lousberg, CC0 :
`assets/LICENCES.md`). Choix et alternatives : ADR 0026.

## Chaîne des fichiers

```
tools/scripts/kaykit_prepare.sh      # télécharge les packs, réduit les .glb (gltf_extract.py)
  -> assets/characters/knight.glb    # corps, casque et cape (rigides sous les os), 33 clips renommés
  -> assets/characters/skeleton_{minion,warrior,rogue,mage}.glb   # corps seuls (même rig)
  -> assets/props/*.gltf             # armes et boucliers, chacun dans son repère
  -> assets/characters/hurtboxes.txt # hurtboxes par os (M5-05)
build (tools/CMakeLists.txt : ow_cook_character, cible cooked_characters)
  -> bin/assets/cooked/characters/knight.{skel,mesh,skin,0.tex}, knight/<clip>.anim
  -> bin/assets/cooked/characters/skeleton_*.{skel,mesh,skin,0.tex}
  -> bin/assets/cooked/props/*.mesh
```

- Les maillages **rigides rangés sous une articulation** (casque, cape) sont inclus dans le maillage
  skinné avec un poids 1 sur cette articulation, placés en pose de liaison (`skin.h`,
  `eng_model_parse_skinned`) : c'est la règle glTF (un enfant d'un os suit l'os).
- Les clips sont choisis **par nom** (`assetc anim --anim-name idle`).
- Tous les personnages partagent le squelette et les clips du chevalier ; au chargement, le nombre
  d'articulations de chaque `.skin` est comparé au squelette partagé.

## Au jeu (`launcher/src/avatar.c`)

- Échelle de rendu 0,8 (le chevalier mesure 2,3 m dans son unité, 1,85 m en jeu) ; demi-tour autour
  de Y (KayKit regarde vers +Z, le jeu vers -Z).
- **Armes** attachées à `handslot.r` / `handslot.l` avec le décalage que KayKit leur donne dans le
  fichier du chevalier (droite : (0 ; 0,033 ; 0) puis demi-tour Y ; gauche : (0 ; 0,017 ; 0,156)).
- Qui porte quoi : chevalier = épée + bouclier ; grunt = squelette sbire + lame + petit bouclier ;
  brute = squelette guerrier (x 1,2) + hache ; archer = squelette voleur + arbalète ; mannequin =
  squelette sbire à mains nues.
- Deux poses par combattant : celle de l'**affichage** (une fois par image, interpolée) et celle de la
  **simulation** (une fois par tick, `dt` = 1/60) qui donne à la simulation de combat ses hurtboxes et
  sa lame (déterminisme).
- F3 : capsules (hurtboxes par os, lame active en rouge, anticipation en orange), cônes de l'IA,
  boîtes physiques ; sans F3, seulement les modèles, le cercle de verrouillage et les barres.

## Animateur (`engine/actor_anim.h`)

| État du jeu | Joué |
|---|---|
| IDLE / MOVE, au sol | espace de mélange repos (0) / marche (0,84 u/s) / course (3,55 u/s) ; au-delà, cadence accélérée |
| IDLE / MOVE, en l'air | `jump_start` puis `jump_air` en boucle ; retombée : `jump_land` 0,3 s |
| ATTACK | clip de l'attaque (`attacks.txt`, ligne `anim CLIP VITESSE DÉCALAGE`), instant = décalage + tick/60 x vitesse |
| DODGE | roulade procédurale (pose `jump_start` à 0,3 s + rotation 2π x avancée, rayon distance/2π) |
| BLOCK / PARRY | `block` (boucle) / `parry` |
| HITSTUN / STAGGER | `hit_a` / `hit_b` |
| DEAD | `death`, dernière image tenue |

Fondu de 0,12 s à chaque changement (0,2 s vers la mort) ; vitesse du mélange lissée (0,1 s).

## Mesures (tests sur les vrais fichiers : `engine.character_motion`, `launcher.combat_rig`)

- **Frame data et animation coïncident** : pour les 11 attaques (joueur et ennemis), le pic de vitesse
  de `handslot.r`, mesuré tick par tick avec le calage `anim`, tombe dans les ticks actifs.
- **Pieds sans glissement (M4-18)** : glissement net du pied d'appui = 6 % de la vitesse à 0,67 m/s,
  10 % à 1,5 m/s, 2 % à 2,84 m/s, 1 % à 4,5 et 7,2 m/s (seuil du test : 15 %).
- **Transitions sans à-coup (M4-17)** : au changement d'état, une articulation bouge au plus de 7,5 cm
  en une image, contre 17,9 cm pour une image ordinaire de course.
- **Roulade (M4-20)** : roule sans glisser par construction (ADR 0026) ; testée dans
  `engine.actor_anim` (quart de tour = 90° autour de l'axe latéral, pivot au rayon).
- **Portées réelles des lames** (journal au lancement) : épée du joueur 1,43 m ; grunt 1,26 / 1,58 m ;
  brute 1,56 / 0,85 m (le coup de hache vertical tombe près du corps). Les plans de l'IA sont bornés à
  portée + 0,2 m.
- Combat complet avec le rig animé : `launcher --combat-rig-selftest` (14 mesures, « rig animé »).

Galerie de tous les états, de face : `build/release/bin/launcher --avatar-gallery`.

## Limites

- Pas de roulade animée par un artiste (KayKit n'en a pas) : roulade procédurale, voir ADR 0026.
- Les ennemis partagent les clips du chevalier : même démarche pour tous.
- L'IK des pieds (M4-23) n'est pas encore appliquée aux personnages : sur une pente, les pieds suivent
  l'animation à plat.
- Le carreau de l'archer n'est pas dessiné (la touche est une capsule du repère du corps).
- Proportions « chibi » : la tête est grande, la portée des bras courte.
- Le ressenti (fluidité, lisibilité) reste à juger par Théo (QUESTIONS-THEO).
