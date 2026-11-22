# IK à deux os (M4-23)

`eng_ik_two_bone(pose, skeleton, upper, lower, end, target, pole, weight)` (`engine/anim_ik.h`) place le pied
(`end`) sur `target` en ne changeant que les rotations **locales** de la cuisse (`upper`) et du genou (`lower`).
Elle sert à poser les pieds sur un sol irrégulier (ou une main sur une poignée) après l'animation.

## Contrat

- Espace de la cible et du pôle : **espace du personnage** (celui de `eng_pose_world`), en mètres, même sous
  une racine à l'échelle 0,01 (les rotations sont extraites sans l'échelle).
- Chaîne : `upper` → `lower` → `end` (parent → enfant → enfant), sinon refus.
- Les longueurs des deux os, mesurées dans la pose courante, sont **conservées**.
- Hors de portée : jambe tendue vers la cible, arrêtée **1 mm avant** d'être droite (au-delà, le sens de
  pliage du genou est indéterminé). Trop près : arrêt à la portée minimale `|l1 − l2|` (au moins 1 mm).
- Sens du genou : du côté du `pole` (point de l'espace du personnage, typiquement devant le genou). Sans pôle,
  le genou garde le côté où il est déjà ; jambe droite à 0,1 mm près : vers l'avant (−Z), ou +X si la chaîne est
  alignée sur Z.
- `weight` ∈ [0 ; 1] : 0 ne fait rien, 1 résout, entre les deux mélange (nlerp, chemin le plus court) les
  rotations locales : l'effet s'estompe sans saut, par exemple quand le pied quitte le sol.
- Refus (pose **inchangée**) : NULL, tailles différentes, chaîne invalide, valeur non finie, poids hors de [0 ; 1],
  os de longueur nulle, matrice dégénérée.

## Utilisation prévue (pas encore branchée)

```
pose = machine.pose            // M4-19
pour chaque pied :
    cible = point du sol sous le pied (raycast physique / hauteur du terrain), + hauteur de cheville
    w = 1 si le pied est posé, fondu vers 0 pendant le pas
    eng_ik_two_bone(&pose, &sk, cuisse, genou, pied, cible, pôle, w)
```

Il reste à faire côté jeu : **abaisser le bassin** du plus petit des deux écarts (sinon une jambe est tendue
et l'autre pliée sur une marche), **recaler l'orientation du pied** sur la pente (le solveur ne touche pas à
l'orientation du pied dans le monde : elle suit le genou), et lisser les cibles dans le temps. Rien de cela n'est
fait ici (« Could » du milestone) ; le solveur est la brique.

## Vérifié / pas vérifié

Vérifié (`tests/test_anim_ik.c`, jambe de 0,6 m + 0,4 m, valeurs calculées à la main) : le pied atteint la cible
à 0,1 mm près, des deux côtés du pôle, genou à 0,6 m / 0,4 m des extrémités ; bords de portée (0,99 m pour
0,999 m ; 0,21 m pour 0,2 m) ; cible hors de portée, sur la cuisse ; jambe droite sans pôle ; côté conservé
sans pôle (genou en avant ou en arrière) ; mélange par poids (angle exactement moitié à 0,5) ; racine tournée
et décalée ; jambe en centimètres sous une racine à 0,01 ; deux résolutions successives ; refus avec pose
inchangée.

Pas vérifié : aucun vrai personnage ; aucune interaction avec un sol réel ; l'aspect (genou qui « claque » près
de la jambe tendue, torsion de la cheville) n'a pas été vu ; le coût CPU n'est pas mesuré (deux `eng_pose_world`
de 65 os par pied, soit environ 0,01 ms attendu, estimation non mesurée).
