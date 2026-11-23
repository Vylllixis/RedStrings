# Mélange de poses et fondu enchaîné (M4-17)

Module : `engine/anim_blend.h`, `engine/src/anim_blend.c`. Tests : `tests/test_anim_blend.c`.

## eng_pose_blend(out, a, b, w)

`out = a (1 - w) + b w`, articulation par articulation, sur des poses **locales** (`EngPose`).

- translation et échelle : interpolation linéaire ;
- rotation : **nlerp par le chemin le plus court** (si `a . b < 0`, `b` est retourné ; résultat
  normalisé). Deux quaternions opposés représentent la même rotation : le mélange ne passe pas par
  « l'autre côté » et ne s'annule pas ;
- `w` borné à [0 ; 1], NaN → 0 ; `out` peut être `a` ou `b` ; quaternion dégénéré (norme nulle) :
  repli sur `a`, jamais de NaN.

Pourquoi nlerp et pas slerp : la vitesse angulaire n'est pas parfaitement uniforme (écart de
l'ordre de 20 % à mi-chemin pour 180°), invisible sur un fondu de 0,1 à 0,3 s, et c'est ~3× moins
cher. Un slerp reste possible plus tard si un cas visible apparaît.

Ce que ça ne fait pas : mélanger des **matrices** (on mélange les poses locales, jamais les
matrices monde ou de skinning, ce qui déformerait le maillage).

## EngCrossfade

Lecteur à deux sources, sortie dans `cf->pose` (pose locale, à passer à `eng_pose_world` puis
`eng_pose_skin_matrices`).

| Appel | Effet |
|---|---|
| `eng_crossfade_init(cf, arena, sk)` | 3 poses allouées une fois (sortie, instantané, brouillon), repos |
| `eng_crossfade_play(cf, clip, fade, loop, speed, start)` | nouveau clip ; l'ancien continue pendant `fade` s ; `fade = 0` : immédiat |
| `eng_crossfade_update(cf, dt)` | avance les temps, évalue, mélange |
| `eng_crossfade_active / weight` | fondu en cours ? poids du nouveau clip (1 hors fondu) |

Courbe : poids = smoothstep(`elapsed / fade`) = `3t² − 2t³` : pente nulle au début et à la fin,
donc **pas d'à-coup** quand le fondu commence ou se termine (mesuré dans le test : à 60 Hz sur 0,3 s,
le déplacement du bout de la chaîne par image reste ≤ 0,2 m sur ~1,8 m d'écart entre les deux poses,
et la première / dernière image ne bougent que du mouvement propre du clip).

Cas particuliers :
- **Premier clip** : rien dont se fondre, il démarre immédiatement.
- **Relance pendant un fondu** : la pose affichée à cet instant est figée comme source du nouveau
  fondu (aucun saut visible) ; contrepartie : l'ancien mouvement ne continue pas pendant ce
  second fondu.
- **Sans boucle** : le temps se bloque à la fin (ou à 0 en lecture arrière), `cf->finished` passe à
  `true`.
- Clip qui vise une articulation absente, durée non finie, `fade` négatif / NaN, vitesse infinie :
  `play` renvoie false et l'état reste inchangé.
- Rien n'est alloué après l'init.

## Limites

- Pas de synchronisation de phase entre les deux clips (marche → course : les pas peuvent être
  décalés) : c'est le rôle du blend space (M4-18).
- Jamais vu à l'écran (aucune fenêtre réelle ici) ; le test mesure la continuité numériquement.
- Pas encore branché au personnage jouable (machine à états : M4-19).
