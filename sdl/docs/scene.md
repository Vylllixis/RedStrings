# Scène (M2-09)

`engine/scene.h` — un monde d'entités ([docs/world.md](world.md)) écrit en texte lisible. Décision :
[ADR 0012](adr/0012-format-de-scene-texte-pilote-par-la-reflexion.md).

```
openworld-scene 1

entity 0 parent -
[Name]
text = "arbre"
[Transform]
position = 0 1 0
rotation = 0 0 0 1
scale = 1 1 1

entity 1 parent 0
[MeshRenderer]
model = "assets/cooked/models/demo.mesh"
visible = true
cast_shadow = true
```

## Règles du format
- Première ligne utile : `openworld-scene 1` (version). Une version inconnue est refusée.
- Lignes vides et lignes commençant par `#` ignorées ; indentation libre ; fins de ligne `\n` ou `\r\n` ;
  1023 caractères au plus par ligne.
- `entity N parent P` : `N` consécutifs à partir de 0, `P` = numéro d'une autre entité (avant ou après) ou
  `-` pour une racine.
- `[Composant]` ouvre un composant de l'entité courante ; `champ = valeur` le remplit. Un champ omis garde
  la valeur par défaut du composant ; un composant ou un champ en double est une erreur.
- Valeurs : `true`/`false`, entiers décimaux (32 bits), flottants (un float fini : `1`, `-0.25`, `1e-3`),
  vecteurs de 3 et quaternions de 4 nombres séparés par des espaces, chaînes entre guillemets avec les
  échappements `\"` `\\` `\n` `\r` `\t` `\xHH`. UTF-8 tel quel.

## Ce qui est écrit
Tous les composants enregistrés **qui ont des champs**, avec tous leurs champs, dans l'ordre de leur
description : le format suit donc automatiquement un nouveau composant (`eng_world_register`), sans
code de sérialisation. Les composants sans champ (état d'exécution) et les octets hors description
(par exemple `MeshRenderer.asset`, le handle du gestionnaire) ne sont jamais écrits.

Ordre : racines dans l'ordre du monde, chacune suivie de son sous-arbre en profondeur d'abord (un parent
précède toujours ses enfants). Écrire → relire → réécrire redonne **les mêmes octets** : les flottants sont
écrits avec 9 chiffres significatifs, qui restituent chaque `float` au bit près (zéro négatif compris).
Valeurs non finies (NaN, ±∞) : refusées à l'écriture (l'écriture échoue plutôt que de produire un fichier
que l'on ne pourrait pas relire).

## API
| Fonction | Rôle |
|---|---|
| `eng_scene_write(world, out, cap, &needed)` | texte en mémoire ; `out == NULL` pour mesurer |
| `eng_scene_read(world, text, len, &error)` | ajoute les entités au monde (ne le vide pas) |
| `eng_scene_save(world, path)` | fichier temporaire puis renommage : jamais de scène à moitié écrite |
| `eng_scene_load(world, path, &error)` | `path` relatif à l'exécutable |

La lecture est **atomique** : au premier défaut (entête, composant ou champ inconnu, valeur invalide,
parent introuvable, boucle, profondeur > 32, monde plein) tout ce qu'elle avait créé est détruit et
`EngSceneError{line, message}` désigne la ligne fautive (celle de l'entité pour une erreur de parent).

## Limites connues
- Pas de références entre entités autres que le parent, ni de références d'assets résolues : `model` est
  un chemin ; c'est celui qui charge la scène qui le confie au gestionnaire d'assets.
- Pas de migration entre versions : la version 1 est la seule. Un changement incompatible incrémentera
  `ENG_SCENE_VERSION` et ajoutera une conversion.
- Chaînes limitées par la taille du champ (`Name` : 47 octets, `model` : 191) ; plus long = erreur, jamais
  de troncature silencieuse.
- La conversion texte ↔ flottant dépend de la locale C si le programme en change (`setlocale`) ; le moteur
  ne le fait pas.
- Pas de flux : la scène entière est lue en mémoire (limite de lecture de fichier : celle de SDL_LoadFile).
