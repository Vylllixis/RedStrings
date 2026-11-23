# Squelettes (M4-12)

Un **squelette** est une hiérarchie d'articulations (os) avec, pour chacune, sa pose de repos locale et la
matrice inverse de la pose de liaison. Il est cuit depuis le **skin** d'un glTF en un `.skel`
(format : `docs/asset-formats.md` §5), lu par `engine/skeleton.h`. Les animations (`.anim`, M4-13) et le
skinning GPU (M4-15) s'appuient dessus ; ce module ne fait ni l'un ni l'autre.

## Cuisson

```
build/debug/bin/assetc skel perso.glb perso.skel [--skin N] [--depfile perso.d]
```

- `--skin N` choisit le skin (0 par défaut) ; un glTF sans skin, ou un `N` trop grand, est refusé.
- Affiche : indice du skin, nombre d'articulations, racines, profondeur, taille. Avertit (stderr, sans
  échouer) si la pose de liaison s'écarte de la pose repos des nœuds de plus de 1 mm : voir « Alignement ».
- `--depfile` : mêmes dépendances que `assetc mesh` (le glTF et ses tampons externes).
- `assetc info perso.skel` liste les sections.

## Ce que fait l'import

| Cas dans le glTF | Résultat |
|---|---|
| Articulations dans un ordre quelconque (enfant avant parent) | rangées **parents d'abord** (passes successives, ordre du skin conservé à profondeur égale) ; la permutation `remap[indice_dans_le_skin] = indice_dans_le_squelette` est rendue à l'appelant : M4-15 s'en servira pour recoder `JOINTS_0` |
| Nœud intermédiaire qui n'est pas une articulation (ex. `Armature`) | sa transformation est **repliée** dans la pose locale de l'articulation enfant ; pour une racine, tous les ancêtres jusqu'à la racine de la scène sont repliés |
| Pas de `inverseBindMatrices` | identité (règle glTF) |
| Matrices en nombre différent des articulations, articulation listée deux fois | refus |
| Échelle nulle, négative ou miroir sur une articulation | refus (la décomposition T·R·S ne la représente pas) |
| Plus de 256 articulations, nom de plus de 63 octets | refus |

Les refus laissent les arènes dans leur état d'origine.

## Poses

`eng_skeleton_rest_world` compose `monde = monde_parent × (T·R·S)` ; `eng_skeleton_bind_world` donne
l'inverse des matrices de liaison. `eng_skeleton_bind_mismatch` renvoie l'écart maximal (mètres, sur les 12
coefficients affines) entre les deux : proche de 0 pour un export cohérent, grand si le fichier a été modifié
après coup ou si la pose repos des nœuds n'est pas la pose de liaison.

## Alignement au maillage (critère d'acceptation)

`launcher --skeleton perso.skel` dessine les os en **superposition** (visibles à travers la matière) avec la
matrice du modèle central de la démo : le squelette tourne avec le maillage. Il doit s'y superposer si :

1. le maillage cuit (`assetc mesh`) vient du **même** glTF (mêmes transformations de nœuds appliquées) ;
2. l'écart de liaison affiché par `assetc skel` / au lancement est ≈ 0.

**Limite honnête** : l'environnement de développement n'a pas d'écran. Les tests vérifient la géométrie par
le calcul (positions monde d'un squelette de référence, avec parents intermédiaires et ordre perturbé) et
que le lanceur lit le fichier et s'arrête proprement ; l'alignement visuel sur un vrai personnage Mixamo n'a
**pas** été vu, et aucun fichier Mixamo réel n'était disponible. Mixamo exporte en centimètres (échelle 0,01
sur `Armature`) : l'import replie cette échelle dans la racine, donc le maillage et le squelette doivent
rester cohérents, mais c'est à confirmer sur un vrai fichier (à signaler si ce n'est pas le cas).
