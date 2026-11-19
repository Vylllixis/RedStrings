# Monde d'entités (M2-08)

`engine/world.h` — implémente l'[ADR 0011](adr/0011-modele-d-entites-ecs-maison.md) (statut « Proposé » :
à confirmer avant de fusionner).

## Modèle
- **Entité** : un `EngEntity` (= `EngHandle`) d'un pool à capacité fixe (1 à 1 048 576, donnée à
  `eng_world_init`). Un handle détruit reste détecté comme périmé même quand sa case est réutilisée.
- **Composant** : une table dense par type (ensemble creux : `sparse[entité] → indice`, `entities[]`,
  `data[]`). Ajout et retrait en O(1) ; le retrait déplace le dernier élément à la place libérée, donc
  **un pointeur de composant n'est valable que jusqu'au prochain ajout/retrait/destruction**.
- **Description** (`EngComponentDesc`) : nom, taille, champs (`EngFieldDesc` : nom, type, décalage, longueur
  pour les chaînes), valeur initiale. La même description servira à la scène (M2-09) et à l'inspecteur
  (M2-10). Validée à l'enregistrement : nom unique, champs dans la structure, alignés, pas de doublon.
- **Natifs** (identifiants fixes) : `Name` (1), `Transform` (2), `MeshRenderer` (3). Limites : 32
  composants, 32 champs par composant, 4096 octets par composant, profondeur de hiérarchie 32.

## Hiérarchie
`eng_world_set_parent(enfant, parent)` (parent nul = racine). Les enfants sont chaînés dans l'ordre
d'ajout. Refusés : soi-même, un descendant (boucle), un handle invalide, une profondeur qui dépasserait 32
**pour tout le sous-arbre déplacé**. `eng_world_destroy` détruit l'entité et son sous-arbre et la détache
de son parent. Itérer avec `eng_world_next_entity` reste correct si l'on détruit l'entité courante.

## Matrices
`eng_world_world_matrix` multiplie, de la racine vers l'entité, les matrices locales T·R·S (quaternion
normalisé avant usage). Une entité sans `Transform` compte pour l'identité. Rien n'est mis en cache : le
calcul est en O(profondeur) ; on ajoutera un cache sale/propre quand le profilage le demandera.

## Limites connues
- Pas de requêtes multi-composants : on parcourt la table d'un composant (`eng_world_table_*`) et on
  interroge `eng_world_get` pour les autres.
- Le `MeshRenderer` garde le chemin du modèle et un `EngAssetHandle` ; c'est M2-09/M2-10 qui s'occupent
  de résoudre le chemin via le gestionnaire d'assets.
- `EngTransform` n'est pas aligné sur 16 octets : `eng_transform_matrix` copie dans des variables locales
  avant d'appeler cglm (bug trouvé par UBSan pendant M2-08).
- Pas de thread-safety.
