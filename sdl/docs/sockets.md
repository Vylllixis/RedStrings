# Sockets : points d'attache sur les os (M4-22)

Un **socket** est un point nommé sur une articulation : l'arme dans la main, le bouclier dans le dos, la
lampe sur la tête. Il se compose d'une articulation et d'un décalage local (translation, rotation,
échelle propre). Chaque image, on lui demande sa matrice dans le monde de jeu, et l'objet attaché la suit.

## Fichier texte

```
# <nom> <articulation> tx ty tz [rx ry rz [échelle]]
socket main_droite  mixamorig:RightHand  0 0.08 0
socket dos          mixamorig:Spine2     0 0.1 -0.15   0 180 0   0.8
```

- `nom` : 1 à 31 caractères parmi `A-Z a-z 0-9 _`, unique ; au plus 16 sockets.
- `articulation` : nom exact d'une articulation du `.skel` (casse comprise, préfixe `mixamorig:` inclus).
- `tx ty tz` en **mètres**, dans le repère de l'articulation ; `rx ry rz` en **degrés**, appliqués X puis Y puis
  Z autour des axes fixes de l'articulation (R = Rz·Ry·Rx, comme « XYZ » de Blender) ; `échelle` > 0, propre au
  socket (1 par défaut). Lignes vides et `#` (commentaire jusqu'à la fin de la ligne) acceptés.
- Erreur : tout le fichier est refusé avec `ligne N : …` ; l'ensemble précédent reste en place.

## API (`engine/socket.h`)

```c
EngSocketSet set;  eng_socketset_init(&set);
eng_socketset_parse(texte, &skeleton, &set, err, sizeof err);   // ou eng_socketset_add(...)
uint32_t i = eng_socketset_find(&set, "main_droite");
eng_pose_world(&pose, &skeleton, world);                        // pose animée (M4-14 / machine)
mat4 m;  eng_socket_world(&set.sockets[i], world, joint_count, model_du_personnage, m);
```

`m` place l'objet attaché : on l'utilise comme matrice modèle de l'arme.

## Pourquoi l'échelle de l'os est retirée

Un import Mixamo / Blender replie l'échelle 0,01 d'`Armature` dans la racine : **toutes** les matrices monde du
squelette portent un facteur 0,01 (les translations en centimètres deviennent des mètres). Attacher un objet avec
la matrice brute de la main le rendrait cent fois trop petit. Le socket prend donc la **position** de
l'articulation et son **orientation** (colonnes normalisées), sans échelle ; seule l'échelle propre du socket
s'applique. Conséquence : une échelle non uniforme ou un cisaillement dans la hiérarchie n'est pas reporté sur
l'objet attaché (voulu : une arme ne doit pas s'écraser avec l'os).

## Voir les sockets

`launcher --skeleton perso.skel --anim marche.anim --sockets perso.sockets` : un repère (X rouge, Y vert,
Z bleu, 12 cm) est dessiné par socket, superposé, et suit l'animation. Test : `launcher.sockets`.

## Vérifié / pas vérifié

Vérifié (`tests/test_socket.c`, valeurs calculées à la main sur une hiérarchie Armature 0,01 → Hips → Hand
tournée de 90°) : taille conservée malgré l'échelle 0,01 ; position et axes ; placement par la matrice du
personnage (aussi avec `out == model`) ; ordre des angles d'Euler ; échelle propre ; suivi de la pose animée ;
refus (NULL, indice hors limites, valeurs non finies, axe de longueur nulle, nom invalide ou en double,
17e socket, fichier mal formé avec numéro de ligne).

Pas vérifié : un vrai personnage Mixamo (les décalages de main / dos de ton arme sont à régler à l'œil) ;
l'aspect des repères à l'écran ; aucun objet n'est encore réellement attaché dans le jeu (pas encore de
personnage animé affiché).
