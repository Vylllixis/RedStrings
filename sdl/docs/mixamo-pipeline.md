# Pipeline Mixamo → moteur (M4-21)

> **Personnages du jeu** : ce ne sont pas des fichiers Mixamo (licence : pas de redistribution des
> fichiers bruts) mais KayKit (CC0), préparés par `tools/scripts/kaykit_prepare.sh` puis cuits par la
> même chaîne (`assetc skel`, `mesh --skin 0`, `anim --anim-name`) : voir `docs/characters.md` et
> l'ADR 0026. La procédure ci-dessous reste valable pour un personnage Mixamo.

De la sortie de Mixamo aux fichiers que le moteur charge. **Aucun fichier Mixamo réel n'a été utilisé pour
écrire ou tester ceci** : toutes les étapes côté moteur sont testées sur des glTF construits en mémoire ;
les étapes côté Mixamo et Blender décrivent la procédure usuelle et sont **à confirmer sur ton fichier**.

## 1. Télécharger

- Le **personnage** : format FBX, pose en T, **avec la peau** (*With Skin*). Il fournit le maillage, le
  squelette (`mixamorig:Hips` …, 65 articulations) et les textures.
- Chaque **animation** : FBX, **sans la peau** (*Without Skin*) pour alléger ; 30 images/s. Pour le root
  motion (M4-20), **ne pas cocher « In Place »** : le déplacement de la racine doit rester dans le clip.
  Pour une boucle de marche/course destinée à un espace de mélange, choisir des clips qui démarrent au même
  point du cycle (pied gauche devant, par exemple) : le moteur ne le vérifie pas.

## 2. Convertir en glTF (Blender)

Importer le FBX, puis `Fichier > Exporter > glTF 2.0` (`.glb`) avec les réglages de `docs/blender-export.md`
(Y haut, appliquer les modifieurs), sauf l'onglet *Animation* qui y est décoché pour un modèle statique :
pour le personnage, activer *Skinning* ; pour un fichier d'animation, activer *Animation*. Un `.glb` par animation. Mixamo exporte en centimètres : Blender place alors l'échelle 0,01 sur
`Armature` ; l'import du moteur la replie (voir `docs/animation-clips.md`).

## 3. Cuire

```
assetc skel   perso.glb   perso.skel                       # squelette (skin 0)
assetc mesh   perso.glb   perso.mesh --skin 0              # perso.mesh + perso.*.tex + perso.skin
assetc anim   walk.glb    walk.anim  --skeleton perso.skel # une commande par animation
assetc anim   run.glb     run.anim   --skeleton perso.skel
assetc anim   idle.glb    idle.anim  --skeleton perso.skel
assetc info   perso.skin                                   # vérifie le fichier
```

`--skeleton` inscrit dans le clip le chemin du `.skel` auquel il se rapporte (voir `docs/animation-clips.md`) ;
les clips doivent venir du même rig que le personnage. `skel` et `mesh --skin` doivent recevoir le **même**
numéro de skin.
Chaque commande accepte `--depfile` pour le suivi des dépendances (voir §9 de `asset-formats.md`).

## 4. Charger

```c
EngSkeleton skeleton;  eng_skeleton_load(&arena, &scratch, "perso.skel", &skeleton);
EngModel model;        eng_mesh_load(&arena, &scratch, "perso.mesh", &model);
EngSkinVertex *skin;   eng_skin_load(&arena, &scratch, "perso.skin", model.vertex_count, &skin, NULL);
```

`eng_model_gpu_create_skinned` prend `model` et `skin` ; l'animation passe par la machine à états
(`docs/animation-state-machine.md`) ; la palette est `eng_pose_skin_matrices`. Un `.skin` dont le nombre de
sommets diffère de celui du `.mesh` est refusé.

## 5. Ce qui est vérifié, et ce qui ne l'est pas

Vérifié (tests `skin_file`) : aller-retour des poids ; refus de chaque corruption listée en
`asset-formats.md` §10 ; cuisson de bout en bout avec `assetc` sur un glTF dont le skin liste les os dans un
ordre différent du squelette : les indices cuits désignent les bons os (par nom) et sont **identiques** au
chargement direct du glTF (`eng_model_parse_skinned`) ; un `.skin` périmé est supprimé.

**Pas vérifié** : un vrai fichier Mixamo (65 os, plusieurs milliers de sommets, plus de 4 influences par
sommet : les influences au-delà de 4 sont ignorées avec un avertissement) ; le temps de cuisson d'un vrai
personnage ; l'affichage du personnage animé (le jeu jouable n'affiche pas encore de maillage animé : prochaine
étape) ; la licence d'usage des assets Mixamo pour ton projet, à lire sur le site.
