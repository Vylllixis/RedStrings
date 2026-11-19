# Exporter un modèle de Blender vers le moteur (M2-11)

> **Statut de ce document.** Rédigé à partir du code du moteur (ce qu'il lit, ignore ou refuse :
> [model.md](model.md), [asset-formats.md](asset-formats.md)) et de ma connaissance de l'exporteur glTF de
> Blender 4.x. **Je n'ai pas pu lancer Blender** pour le vérifier (voir « Ce qui est vérifié » en bas) :
> les noms de réglages peuvent différer légèrement selon la version. La section « Valider une première
> fois » est faite pour que la vérification se fasse en cinq minutes sur votre machine.

## Conventions du moteur à respecter

| Sujet | Convention du moteur | Conséquence dans Blender |
|---|---|---|
| Unité | 1 unité = 1 mètre | `Propriétés de scène > Unités` : Métrique, **Échelle d'unité 1,0**. À ma connaissance l'exporteur écrit les unités Blender telles quelles, sans appliquer l'échelle d'unité : une échelle ≠ 1 donne un modèle de mauvaise taille (non vérifié). |
| Haut | **+Y** (glTF) | Blender est Z vers le haut : cocher **« +Y Up »** à l'export (coché par défaut). Décoché, le modèle arrive couché. |
| Devant | **−Z** (caméra, `ENG_AXIS_FORWARD`) | L'exporteur convertit (x, y, z)<sub>Blender</sub> → (x, z, −y)<sub>glTF</sub> : le **+Y de Blender devient −Z, le « devant » du moteur**. Modélisez les personnages et véhicules face à +Y dans Blender. |
| Origine | l'origine de l'objet Blender est le point d'accroche dans le monde | Placez l'origine aux pieds (objets posés au sol) ou au centre de rotation. |
| Triangles | seuls les triangles sont lus | L'exporteur triangule ; rien à faire. Lignes et points : ignorés. |
| UV | (0, 0) en haut à gauche, **TEXCOORD_0 seulement** | L'exporteur retourne V comme il faut ; une seule carte UV utilisée (les suivantes sont ignorées). |
| Normales | utilisées si présentes ; sinon lissées par le moteur | Cocher **Normals** ; faces à l'envers = invisibles (le moteur élimine les faces arrière) : `Maj+N` (recalculer vers l'extérieur) dans Blender. |
| Tangentes | utilisées si présentes ; sinon calculées | Facultatif. À cocher seulement si une normal map est exportée depuis Blender avec ses tangentes ; sinon laisser le moteur les calculer. |
| Matériaux | PBR métal-rugosité : couleur de base, métal, rugosité, normal map, occlusion, émissif | Un seul matériau par primitive (l'exporteur crée une primitive par matériau). Les shaders Blender non reconnus par l'exporteur sortent en matériau par défaut. |
| Double face, alpha | **ignorés** (`doubleSided`, `alphaMode`) | Pas de transparence ni de feuillage découpé pour l'instant. |
| Animations, squelettes, shape keys, caméras, lumières | **ignorés** (message d'information) | Les décocher réduit le fichier ; ils n'empêchent pas le chargement. |
| Compression | **Draco, meshopt, basisu : refusés** | `Compression` : **décoché**. |
| Images | PNG ou JPEG | `Images : Automatique` (garde le format d'origine) ou PNG. **Pas WebP** (le moteur ne le décode pas). |
| Limites | 4 M sommets, 16 M index, 8192 parties par modèle | Découper les très gros décors. |

Un modèle partagé par deux nœuds du fichier est copié deux fois ; un nœud à échelle négative est géré (les
triangles sont retournés pour rester anti-horaires) ; une échelle nulle est refusée.

## Réglages d'export (`Fichier > Exporter > glTF 2.0`)

| Panneau | Réglage | Valeur |
|---|---|---|
| Format | | **glTF séparé (.gltf + .bin + textures)** (diffs lisibles, cuisson incrémentale par `.bin`/image) ou glTF binaire (.glb) |
| Inclure | Limiter à | « Objets sélectionnés » (ou « Objets visibles ») : n'exporter que le modèle |
| Inclure | Propriétés personnalisées, Caméras, Lumières ponctuelles | décoché |
| Transformation | **+Y Up** | **coché** |
| Données > Maillage | **Appliquer les modificateurs** | coché (sinon le maillage exporté est celui d'avant les modificateurs) |
| Données > Maillage | **UV**, **Normales** | cochés |
| Données > Maillage | Tangentes | facultatif (voir ci-dessus) |
| Données > Maillage | Couleurs de sommets, Attributs, Arêtes/points libres | décochés (ignorés par le moteur) |
| Données > Matériaux | Matériaux | **Exporter** |
| Données > Matériaux | Images | **Automatique** (ou PNG) |
| Données > Compression | Compression Draco | **décoché** |
| Animation | tout | décoché |

## Nommage et rangement

- Noms de fichiers : minuscules ASCII, sans espace ni accent (`caisse.gltf`, `arbre_chene.gltf`). Les chemins
  sont comparés tels quels par le gestionnaire d'assets.
- Noms d'objets Blender : uniques dans le fichier (Blender ajoute `.001` sinon). Ils deviennent des noms de
  nœuds glTF ; le moteur ne les lit pas aujourd'hui, mais c'est ce que verront les outils.
- Sources dans `sdl/assets/models/`, textures à côté ou dans `assets/textures/` (chemins relatifs dans le
  `.gltf`). Ajouter le modèle à la liste `MODELS` de `ow_cook_assets` (`tools/CMakeLists.txt`) : la cuisson
  produit `assets/cooked/models/<nom>.mesh` + `<nom>.<k>.tex` ([asset-formats.md](asset-formats.md), §9).

## Valider une première fois

1. **Générer un export de référence** (facultatif mais recommandé : il applique exactement les réglages
   ci-dessus à une caisse et une sphère) :
   ```
   blender -b -P tools/blender/make_reference_export.py -- /tmp/reference
   ```
   (script non exécuté par l'assistant : s'il échoue, c'est un bug du script, pas de la procédure.)
2. **Cuire** : `build/debug/bin/assetc mesh /tmp/reference/reference.gltf /tmp/reference/reference.mesh` doit
   rendre 0 et afficher « N sommets, M index, P parties, Q matériaux » (format observé sur
   `assets/models/demo.gltf` : `1786 sommets, 9984 index, 2 parties, 3 matériaux, 1 textures`). Attendu pour la
   référence : 2 parties (caisse, balle) ; une caisse non lissée donne 24 sommets et 36 index (à vérifier,
   je ne peux pas le contrôler sans Blender).
3. **Voir** : ajouter `reference.gltf` à `MODELS`, `cmake --build`, lancer
   `launcher --editor`, créer une entité (« Nouvelle »), mettre `MeshRenderer.model` à
   `assets/cooked/models/reference.mesh`. Contrôles : la caisse **repose sur le sol** (pas couchée), mesure
   **2 m** de côté, sa face avant regarde −Z, la texture n'est pas retournée, aucune face n'est
   manquante.

## Symptômes courants

| Symptôme | Cause probable |
|---|---|
| Modèle couché sur le côté | « +Y Up » décoché |
| Modèle minuscule ou énorme | échelle d'unité de la scène ≠ 1,0, ou objet mis à l'échelle sans l'appliquer à la géométrie (`Ctrl+A > Échelle`) |
| Faces manquantes, modèle « creux » | normales à l'envers (`Maj+N`) : les faces arrière sont éliminées |
| Texture noire ou blanche | image introuvable à côté du `.gltf` (chemins) ou en WebP ; matériau non exporté (« Matériaux : Exporter ») |
| Texture inversée verticalement | UV non exportées depuis la bonne carte (seule `TEXCOORD_0` est lue) |
| Refus au chargement (« extension obligatoire ») | compression Draco / meshopt activée |
| Éclairage plat, détail de normal map absent | normal map non connectée au nœud « Normal Map » du Principled BSDF, ou image marquée sRGB au lieu de « Non-color » |

## Ce qui est vérifié, et ce qui ne l'est pas

- **Vérifié** (`ctest --test-dir build/debug -R engine.model`, test `blender_style_metadata_and_optional_extensions_are_accepted`) :
  le chargeur accepte, sans retouche, un `.gltf` portant ce que l'exporteur de Blender 4.x ajoute
  d'optionnel (noms de nœuds, `extras`, `doubleSided`, extensions de matériau `KHR_materials_ior` et
  `KHR_materials_specular`, `KHR_texture_transform` déclarée). Ce fichier est **écrit à la main** dans la
  forme de l'exporteur, il n'a pas été produit par Blender.
- **Vérifié avant** (M1-10, M2-02) : rotations/échelles de nœuds, hiérarchie, échelle négative, normales,
  tangentes, extensions refusées (Draco, meshopt, basisu), fichiers externes et `.glb`.
- **Non vérifié** : un export réel d'un vrai Blender. L'AC « un modèle exporté selon la procédure se charge
  sans retouche » n'est donc **pas démontré** tant que l'étape 1–3 ci-dessus n'a pas été faite une fois.
  J'ai essayé d'installer `bpy` (Blender en module Python) pour le faire moi-même ; l'exécution m'a été
  refusée par l'environnement, je ne l'ai pas contournée.
