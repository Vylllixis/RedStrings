# Modèles glTF (M1-10)

Deux modules, séparés pour que le premier se teste sans GPU :

- `engine/model.h` — **côté CPU** : lit un fichier glTF 2.0 (cgltf) et l'aplatit en un tableau de
  sommets (`EngModelVertex` : position, normale, UV, tangente = 48 octets), un tableau d'index `uint32_t`, une
  liste de **parties** (une par primitive : plage d'index + matériau), des matériaux et des images.
- `engine/model_gpu.h` — **côté GPU** : buffers, textures, sampler, une commande de dessin par
  partie (`eng_model_gpu_draw`), et le pipeline (`eng_model_pipeline_desc`).

```c
EngModel model;
if (eng_model_load(&level_arena, &scratch, "assets/models/demo.gltf", &model)) {
    EngModelGpu gpu;
    eng_model_gpu_create(&gpu, &renderer, &permanent_arena, &model, 16.0F);
    eng_model_free(&model);                    /* les images sont libérées : le GPU a tout */
    /* ... chaque image : */
    eng_model_gpu_draw(&gpu, queue, pipeline, model_matrix, view_proj, &lighting);
    /* ... à l'arrêt : */
    eng_model_gpu_destroy(&gpu, &renderer);
}
```

## Ce qui est géré

- `.gltf` (JSON ; tampons et images en fichiers externes ou en `data:` URI base64) et `.glb`.
- Primitives **triangles** : POSITION (obligatoire), NORMAL et TEXCOORD_0, TANGENT (facultatifs), index
  u8/u16/u32 ou absents, accessors normalisés ou « sparse » (lus par cgltf).
- **Hiérarchie de nœuds** : la matrice monde de chaque nœud portant un maillage est appliquée aux
  positions ; la matrice normale (inverse transposée du 3x3) aux normales. Un maillage partagé par
  deux nœuds est copié deux fois. Un nœud qui inverse l'orientation (déterminant négatif, par ex.
  échelle −1) inverse l'ordre des sommets de ses triangles pour qu'ils restent anti-horaires.
  Une matrice singulière (échelle 0) est refusée. Tous les nœuds du fichier sont chargés, même
  hors de la scène par défaut.
- **Normales absentes** : normales lissées (somme des normales de faces pondérées par l'aire).
  Attention : les sommets dupliqués (coutures d'UV) ne partagent pas leurs normales, d'où une
  couture visible sur un maillage lisse sans NORMAL ; fournir les normales dans le fichier.
- **UV absentes** : (0, 0). Convention glTF = convention du projet : (0, 0) en haut à gauche.
- **Matériaux PBR** (M1-15, `docs/pbr.md`) : couleur de base, métal / rugosité, normal map, occlusion,
  émissif (`KHR_materials_emissive_strength` compris), facteurs et textures (TEXCOORD_0). Le matériau
  0 de `EngModel` est un matériau par défaut (blanc) pour les primitives sans matériau ; le matériau
  glTF *n* est `materials[n + 1]`. Seules les images utilisées par un matériau sont décodées.

## Ce qui est ignoré ou refusé

| Cas | Comportement |
|---|---|
| modes autres que triangles (lignes, points, bandes, éventails) | primitive ignorée + avertissement ; si rien d'autre : échec |
| animations, skins, morph targets, caméras, lumières | ignorés (message d'information) |
| double face, `alphaMode` (masque, mélange), `KHR_texture_transform`, autres extensions de matériau | ignorés |
| TEXCOORD_1 et au-delà, sampler glTF (adressage, filtres) | ignorés : sampler par défaut (répétition, trilinéaire) |
| index hors limites, tampon trop court, nombre d'index non multiple de 3 | **refusé** |
| extensions obligatoires Draco, meshopt, basisu | **refusé** |
| plus de 4 M sommets, 16 M index ou 8192 parties | **refusé** |

Un échec est journalisé, rend `false`, met le modèle à zéro et restitue l'arène : rien à libérer.

## Mémoire

Les tableaux du modèle vont dans l'arène donnée (typiquement `level`, remise à zéro après l'envoi au
GPU) ; `scratch` sert aux calculs temporaires et à la lecture des fichiers (restituée). Les pixels des
images viennent de stb_image : `eng_model_free` les libère. cgltf alloue par `SDL_malloc` (compté par
le compteur d'allocations du moteur) ; le chargement n'a pas lieu dans la boucle de frame.

## Rendu

Shaders `model.vert` / `model.frag` (voir `docs/shaders.md`) : matériau PBR (`docs/pbr.md`) éclairé
par une lumière directionnelle et une ambiante (`docs/lighting.md`). `lighting`
(`EngLighting`) est copié dans la file à chaque appel ; `NULL` : éclairage par défaut. Le pipeline
élimine les faces arrière et utilise le depth buffer.

`eng_model_gpu_draw_shadowed(..., &EngModelShadow)` reçoit l'ombre du soleil (`docs/shadows.md`) et
`eng_model_gpu_draw_depth` met le modèle dans la passe d'ombre (une commande par partie, matrice
monde → découpage du soleil seulement). `eng_model_make_box` fabrique une boîte alignée sur les axes
(24 sommets, 36 index, une partie) sans fichier : le sol de la démo et les tests d'ombre s'en servent.

## Modèle de démonstration

`assets/models/demo.gltf` + `demo.bin` (tore texturé avec `checker.png` + sphère orange, hiérarchie
de deux nœuds) est généré par `tools/scripts/gen_demo_model.py` ; le lanceur le dessine au centre de
la grille de cubes.

## Limites connues

- Pas de rechargement à chaud des modèles, pas d'instanciation GPU (un maillage partagé est dupliqué).
- Une partie = une commande de dessin : pas de tri par matériau, pas de fusion.
- Les UV hors de [0, 1] répètent la texture (sampler unique) ; pas de sampler par texture.
- Le chargement se fait d'un bloc (pas de flux, pas de thread).
