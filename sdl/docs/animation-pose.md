# Évaluation de pose (M4-14)

`engine/pose.h` transforme **(squelette, clip, temps)** en matrices prêtes pour le skinning. Trois étapes,
chacune testable seule :

| Étape | Fonction | Résultat |
|---|---|---|
| 1 | `eng_pose_sample(pose, skeleton, clip, time, wrap)` | pose **locale** : translation / rotation / échelle par articulation |
| 2 | `eng_pose_world(pose, skeleton, world)` | matrices **monde** (espace du personnage) |
| 3 | `eng_pose_skin_matrices(skeleton, world, skin)` | `monde × inverse_bind` : ce que lira le vertex shader (M4-15) |

## Règles

- **Repos par défaut** : `eng_pose_sample` remet tout au repos du squelette, puis écrase les articulations
  qui ont un canal (par propriété : une articulation peut n'avoir qu'une rotation animée, sa translation reste
  celle du repos).
- **Temps** (`eng_anim_wrap_time`) : `ENG_ANIM_WRAP_CLAMP` borne à `[0 ; durée]` ; `ENG_ANIM_WRAP_LOOP` tourne
  dans `[0 ; durée[` (négatif accepté : `-0,5` sur 2 s → `1,5` ; durée nulle → 0). Temps ou durée non fini : refus.
- **Clip d'un autre squelette** : un canal dont l'articulation dépasse `joint_count` (ou dont la propriété est
  inconnue) fait échouer l'appel, journalisé, **pose inchangée** (validation avant toute écriture). Ce contrôle
  ne détecte pas un clip dont les indices sont valides mais qui vise un autre squelette de même taille : le chemin
  `skeleton_path` du clip est là pour que l'appelant le vérifie.
- **Pas d'allocation par image** : `eng_pose_init` alloue une fois dans l'arène (3 tableaux alignés 16 octets) ;
  `sample` / `world` / `skin_matrices` n'allouent rien (test : `arena.used` inchangé).
- **Déterminisme** : mêmes entrées, mêmes octets.

## Vérification

Chaîne de 3 articulations (racine, +1 m en Y, +1 m en Y), quart de tour autour de Z à l'articulation 1 :
`monde[2]` = (−1, 1, 0) ; un sommet posé en liaison sur l'articulation 2 se retrouve là, et un sommet décalé
de +1 en X se retrouve à (−1, 2, 0) — calculé à la main, pas par le code testé. Au repos (repos = liaison), toutes
les matrices de skinning valent l'identité.

## Ce qui n'a pas été vu

Rien à l'écran : l'évaluation n'est reliée ni au rendu ni au debug draw (M4-15 : skinning GPU, M4-16 :
débogueur d'animation). Les tests utilisent des squelettes construits à la main, pas un personnage réel.

## Coût

`pose.ten_characters_of_65_joints_cost_little_cpu_per_frame` : 10 personnages de 65 os, un clip de 65 canaux de
rotation à 60 clés chacun, échantillonnage + matrices monde + matrices de skinning = **0,056 ms par image** en
release (un cœur), 0,52 ms sous ASan/UBSan. La borne exigée par le test (20 ms) est volontairement large ; le
chiffre utile est écrit sur stderr.
