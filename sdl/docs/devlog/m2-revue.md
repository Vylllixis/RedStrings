# Revue de jalon M2 — critères de sortie

Légende : **Prouvé** = vérifié par une commande rejouable (Linux, SwiftShader, pas de GPU réel) ;
**Partiel** = prouvé pour une partie du critère, la partie manquante est dite ; **À faire** = à vérifier
sur ta machine.

Vérification mécanique : `tools/scripts/m2-exit-check.sh [preset]` (critères 1 à 3 + suite complète).
Le script échoue si l'on plante un appel au chargeur glTF dans `launcher/` ou dans `engine/src/` (testé
pendant la revue avec deux fichiers sentinelles, retirés ensuite).

## Critères de sortie du jalon

| # | Critère | Preuve | État |
|---|---|---|---|
| 1 | Aucun parsing glTF au runtime | `m2-exit-check.sh` : ni `game/`, ni `launcher/` n'appellent `eng_model_load` ni `cgltf_*` ; dans le moteur seuls `model.c` (le chargeur, utilisé par le cooker et ses tests) et `third_party_impl.c` y touchent. Le lanceur et le gestionnaire d'assets lisent `.mesh`/`.tex` (`eng_mesh_load_visit`) | Prouvé — portée ci-dessous |
| 2 | Assets recuisinés automatiquement au build | `assets.cook_incremental` (cmake -P rejoue des builds : rien n'est recuit sans changement ; une source ou le cooker modifié recuit exactement ce qui dépend d'eux) ; `engine.asset_cook` (rechargement à chaud côté lanceur) ; ADR 0008 | Prouvé (Linux) |
| 3 | Scène sauvegardée puis rechargée à l'identique | `engine.scene` (aller-retour texte → monde → texte, comparaison au bit près des flottants, hiérarchie, composants personnalisés) ; `engine.editor` ; `launcher.editor` (`--editor-selftest` : le lanceur charge, modifie, sauve et recharge une scène) | Prouvé (Linux) ; geste interactif **À faire** |

### Ce que disent (et ne disent pas) les preuves

**1. Portée.** Le code du chargeur glTF reste **dans la bibliothèque du moteur** (lié au lanceur) : le
critère est « aucun appel au runtime », pas « aucun octet de glTF dans l'exécutable ». Le contrôle est un
`grep` sur les noms `eng_model_load` et `cgltf_` : il ne verrait pas un appel indirect par pointeur de
fonction, ni un nouveau chargeur d'un autre nom. Les sources glTF du dépôt ne sont lues que par `assetc`
(étape de build).

**2. Ce qui est automatique.** Le build cuit `assets/models/*.gltf` listés dans `ow_cook_assets`
(`tools/CMakeLists.txt`) : un nouveau modèle n'est cuit que s'il y est **déclaré** ; il n'y a pas de
découverte automatique des sources. Le dossier de sortie est `assets/cooked/`.

**3. Ce que couvre l'aller-retour.** Texte → monde → texte à l'identique, pour la version 1 du format. Le
rechargement d'une scène dans le gestionnaire d'assets (le `model` est un chemin, non résolu par la scène)
est du ressort du lanceur. Non couvert : la souris réelle (sélection, glisser du gizmo) — les tests
simulent la souris par `eng_editor_pointer` ; seul le geste dans une vraie fenêtre est **À faire**.

## Livrable : « une scène décrite dans un fichier, chargée depuis des assets cuisinés, éditable en jeu »

| Élément | Preuve | État |
|---|---|---|
| Format binaire `.mesh`/`.tex` versionné | ADR 0006, `docs/asset-formats.md`, `engine.mesh_file`, `engine.tex_file` | Prouvé |
| Cooker `assetc` (meshes, textures BC7, 4× moins de mémoire GPU) | ADR 0007, `engine.asset_cook`, `engine.image` | Prouvé ; affichage BC7 sur GPU NVIDIA **À faire** |
| Gestionnaire d'assets (une seule charge par chemin, comptage, rechargement à chaud) | ADR 0009/0010, `engine.assets`, `engine.assets_gpu`, `launcher.watch_assets` | Prouvé (Linux) |
| Entités (ECS maison), hiérarchie, composants réflexifs | ADR 0011, `engine.world` | Prouvé ; **ADR 0011 encore « Proposé »** |
| Format de scène texte | ADR 0012, `engine.scene`, `assets/scenes/demo.scene` | Prouvé |
| Éditeur : picking, gizmo, inspecteur, sauvegarde | ADR 0013, `docs/editor.md`, `engine.editor`, `engine.editor_ui`, `launcher.editor` | Prouvé (simulation) ; interactif **À faire** |
| Procédure Blender → glTF | `docs/blender-export.md`, test `engine.model` | **Non validé avec un vrai Blender** (voir le document) |

## Écarts entre le plan du suivi et ce qui a été fait

- **M2-07 / M2-08** : système d'entités maison (ADR 0011) au lieu de flecs ; décision à confirmer.
- **M2-09** : format de scène **texte maison** piloté par la réflexion (ADR 0012), pas du JSON via cJSON.
- **M2-10** : gizmo de translation et inspecteur écrits dans le moteur (ADR 0013), **sans ImGuizmo** :
  pas de rotation ni d'échelle par poignées (on édite le quaternion via des angles d'Euler dans
  l'inspecteur).
- **M2-03** : BC7 uniquement ; BC5 pour les normales n'est pas fait (les normales sont en BC7).
- **M2-11** : le critère « un modèle exporté se charge sans retouche » repose sur un fichier écrit à la
  main dans la forme de l'exporteur, pas sur une exportation Blender réelle.

## À faire sur ta machine

1. `launcher --editor --scene <chemin du dépôt>/assets/scenes/demo.scene` : cliquer un objet, tirer une
   flèche du gizmo, modifier un champ, « Enregistrer », quitter, relancer : la modification est là.
2. Exécuter la procédure de `docs/blender-export.md` (étapes 1 à 3) avec ton Blender ; corriger le
   document si un nom de réglage diffère.
3. Vérifier l'affichage BC7 (couleurs, normales) sur la RTX 5070.
4. Confirmer ou changer l'ADR 0011 (ECS maison vs flecs), puis passer son statut à « Accepté ».
5. `ctest --test-dir build/debug` → 67 entrées ; `tools/scripts/m2-exit-check.sh debug`.
6. La marge de la mesure de temps dans `engine.assets_gpu` peut être trop juste sur ta machine : à
   élargir si elle échoue seule.

## Dette connue à l'entrée de M3

- Pas d'annulation dans l'éditeur ; pas de duplication ni de re-parentage à la souris.
- Chargement d'assets synchrone (un modèle lourd = une image longue) ; un seul type d'asset (modèle).
- Un `.gltf` n'est cuit que s'il est listé dans `ow_cook_assets`.
- Scène : pas de références entre entités autres que le parent ; pas de migration de version.
- Dette de M1 non résolue : deux allocations inexpliquées en boucle de frame (ADR 0003), un SEGV observé
  avec le pilote NVIDIA sans cause établie, avertissements « dessin ignoré » au démarrage.

## Clôture

```bash
tools/scripts/m2-exit-check.sh debug
git switch main && git pull
git tag -a v0.2 -m "M2 : assets et scène"
git push origin v0.2
```

Le tag est ton choix : les lignes « À faire » ne bloquent pas la revue, mais note leur résultat dans
`docs/devlog/v0.2.md`.
