# Machine à états d'animation (M4-19)

Modules : `engine/anim_graph.h` (données : fichier texte, analyse, rechargement à chaud) et
`engine/anim_machine.h` (exécution). Tests : `test_anim_graph.c`, `test_anim_machine.c`.

## Le fichier

```
# locomotion.animgraph
param speed                         # paramètre flottant (0/1 pour un booléen)
param jump 0                        # valeur initiale facultative
clip idle idle.anim
clip walk walk.anim loop speed=1.5
clip run  run.anim
clip jump jump.anim once            # « once » : joué une fois (loop par défaut)

state idle clip idle
state move blend speed idle:0 walk:1.5 run:4.5   # espace de mélange 1D (M4-18)
state jump clip jump

transition idle -> move when speed > 0.1  fade 0.2
transition move -> idle when speed <= 0.1 fade 0.3
transition any  -> jump when jump >= 0.5  fade 0.1
transition jump -> idle when ended fade 0.2
start idle
```

- Une instruction par ligne, `#` commence un commentaire. Les clips et paramètres sont déclarés
  **avant** les états qui les utilisent, les états avant les transitions.
- Opérateurs : `> < >= <=`, ou `ended` (clip `once` arrivé à sa fin ; jamais vrai pour une boucle ni
  pour un espace de mélange). `any` : depuis n'importe quel état sauf la cible.
- Les transitions sont essayées **dans l'ordre du fichier**, la première vraie gagne.
- Une transition sans condition est refusée (elle bouclerait indéfiniment).
- Limites : 8 paramètres, 32 clips, 16 états, 64 transitions, 8 échantillons par espace de mélange,
  noms de 31 caractères (`A-Z a-z 0-9 _ - .`), fondu 0 à 10 s. Les clips d'un espace de mélange doivent
  être `loop`.
- **Tout ou rien** : la moindre erreur (référence inconnue, doublon, nombre invalide, limite...) refuse
  le fichier entier avec `ligne N : message` ; jamais de définition à moitié lue.

## Exécution (`EngAnimMachine`)

```c
eng_animmachine_init(&m, &arena, &skeleton);
eng_animmachine_set_def(&m, &file.def, finder, user);  /* finder(user, nom, chemin) -> EngAnimClip* */
/* chaque image : */
eng_animmachine_set_param(&m, "speed", v);
eng_animmachine_update(&m, dt);                         /* résultat : m.pose (pose locale) */
```

- Fondu : poids smoothstep (`eng_crossfade_curve`), l'ancien état continue de jouer pendant le fondu.
- Changement d'état **pendant** un fondu : la pose affichée est figée comme source du nouveau fondu
  (aucun saut, au prix de figer l'ancien mouvement).
- Rien n'est alloué après `init`. Les clips sont fournis par l'appelant (`finder`) : le moteur
  n'ouvre aucun fichier d'animation lui-même ici.

## Rechargement à chaud

`EngAnimGraphFile` surveille le fichier (`eng_animgraph_file_poll`, ~0,5 s de délai, après stabilité).
Une version **invalide** est journalisée et mémorisée dans `file->error` ; **l'ancienne définition
reste active**. À chaque nouvelle version valide, rappeler `eng_animmachine_set_def` :

- un clip introuvable ou incompatible avec le squelette, ou de durée nulle dans un espace de
  mélange : tout est refusé, la machine continue avec l'ancienne définition ;
- si l'état courant existe encore (même nom), il garde son instant / sa phase ; sinon retour à
  l'état initial ; un éventuel fondu en cours est abandonné ;
- les paramètres gardent leur valeur **par nom** (un paramètre ajouté prend sa valeur initiale).

## Limites

- Pas branché au personnage jouable ni au lanceur (le jouable n'a pas encore de maillage animé :
  poids de skinning non cuits, M4-21). Validé uniquement par tests numériques.
- Une seule condition par transition (pas de `and`/`or`), pas de triggers ni de « temps de sortie »
  en fraction du clip.
- Un changement d'état n'emporte pas la phase d'un espace de mélange vers un autre (départ à 0).
- Fichier lu par chemin tel quel (pas de recherche dans les dossiers d'assets).
