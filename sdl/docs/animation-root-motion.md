# Root motion (M4-20)

Le root motion sépare **ce que fait l'os racine** en deux parties :

- un **déplacement** (`EngRootMotion` : translation x/z en mètres + lacet en radians) rendu à
  l'appelant, qui le donne au contrôleur de personnage (physique, collisions) ;
- une **pose « en place »** : x/z et lacet de la racine sont figés à la valeur du clip à t = 0.
  La hauteur (y), le tangage et le roulis restent dans la pose (rebond du bassin, inclinaison).

Sans cela, le mesh avancerait deux fois : une fois par l'animation, une fois par le contrôleur.

## Module `anim_rootmotion.h`

| Fonction | Rôle |
|---|---|
| `eng_rootmotion_delta(sk, clip, joint, t0, t1, loop, &out)` | déplacement de la racine entre t0 et t1 |
| `eng_rootmotion_strip(pose, sk, clip, joint)` | fige x/z + lacet de la pose à t = 0 |
| `eng_rootmotion_compose / lerp / yaw` | outils |

Conventions : Y haut, −Z avant, mètres. Le delta est exprimé dans le **repère du personnage au
début du pas** (lacet de départ), donc `delta.z < 0` = avancer, quel que soit le cap du clip.
Avec `loop`, les temps sont « dépliés » : un pas de plusieurs cycles enchaîne `fin − début` de
chaque cycle, sans saut au rebouclage. Sans `loop`, t1 est borné à la durée (un clip `once` ne
déplace plus rien après sa fin).

## Dans le fichier de graphe

```
root_motion Hips
```

Une seule ligne, à un endroit quelconque. L'articulation doit exister dans le squelette **et**
être une racine (parent = −1) ; sinon `eng_animmachine_set_def` refuse la définition (message
dans le log) et **l'ancienne définition reste active** (rechargement à chaud sûr).
Sans la ligne : comportement inchangé (la pose garde sa racine, `motion` = 0).

## Machine à états

`machine.motion` est recalculé à chaque `eng_animmachine_update` :

- état clip : delta du clip entre l'ancien et le nouveau temps de couche (× vitesse) ;
- état blend : delta de chaque clip sur son propre cycle, puis mélange avec les mêmes poids que
  la pose → marche (1,5 m/s) et course (4,5 m/s) donnent bien une vitesse interpolée ;
- pendant un fondu : mélange des deux `motion` avec le même smoothstep que la pose ;
- source gelée (transition pendant un fondu) : contribue 0 déplacement.

Utilisation côté jeu : `character_move(motion.translation)` après rotation par le cap du
personnage, puis `yaw += motion.yaw`.

## Limites connues

- Les clips d'un espace de mélange doivent démarrer au même point du cycle (non vérifié).
- Un seul os racine ; pas de root motion sur y.
- Non branché au personnage jouable : il n'y a pas encore de mesh animé (M4-21).
- Testé sur des clips synthétiques, pas sur un vrai fichier Mixamo.
