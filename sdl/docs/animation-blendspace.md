# Espace de mélange 1D (M4-18)

Module : `engine/anim_blendspace.h`, `engine/src/anim_blendspace.c`. Tests : `tests/test_anim_blendspace.c`.

Un paramètre (la vitesse au sol, m/s) choisit deux clips voisins et leur poids. Exemple typique :
repos à 0, marche à 1,5, course à 4,5. Jusqu'à 8 échantillons, positions **strictement croissantes**.

```c
eng_blendspace_init(&bs, &arena, &skeleton);
eng_blendspace_add(&bs, &idle, 0.0F);
eng_blendspace_add(&bs, &walk, 1.5F);
eng_blendspace_add(&bs, &run,  4.5F);
/* chaque image : */
eng_blendspace_update(&bs, dt, speed);   /* résultat : bs.pose (pose locale) */
```

## Règles

- Paramètre entre deux échantillons a < b : `t = (p − a) / (b − a)`, pose = `eng_pose_blend(a, b, t)`.
  Hors de [premier ; dernier] : **borné**, pas d'extrapolation (une vitesse de 8 m/s joue la course
  telle quelle). Paramètre NaN : on garde le précédent.
- **Phase synchronisée** : tous les clips sont lus sur la même phase [0 ; 1[ (temps = phase × durée
  du clip). Marche et course restent donc au même point du pas : pas de glissement des pieds au
  passage de l'une à l'autre.
- **Durée du cycle** = interpolation des durées avec le même `t` (marche 1,2 s, course 0,6 s : 0,9 s
  à mi-chemin). La phase avance de `dt / cycle` et est **conservée** quand le paramètre change
  (pas de redémarrage du pas). `eng_blendspace_set_phase` permet de reprendre un cycle déjà en cours
  (transition depuis un autre état).
- Un clip de durée nulle ou non finie, visant une articulation absente, ou une position non
  croissante : `eng_blendspace_add` renvoie false, état inchangé.
- Rien n'est alloué après l'init.

## Hypothèse non vérifiée par le moteur

Tous les clips d'un espace doivent **commencer au même point du cycle** (par exemple pied gauche
posé) et être bouclables. Un clip décalé se verra comme un glissement ; le moteur ne le détecte pas
(à régler à l'export, voir le pipeline Mixamo, M4-21).

## Limites

- Pas de lissage du paramètre : c'est à l'appelant (machine à états M4-19 / locomotion) de le lisser.
- Pas de root motion ici (M4-20) : la translation de la racine du clip est mélangée comme les autres.
- 1D seulement (pas de strafe / direction). Pas vu à l'écran : mesuré numériquement (mêmes angles pour
  tout paramètre, racines pondérées linéairement, déplacement par image borné pendant une
  accélération marche → course).
