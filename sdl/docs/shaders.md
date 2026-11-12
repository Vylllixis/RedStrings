# Shaders

Les shaders sont écrits en **HLSL** (`shaders/*.hlsl`) et compilés **hors ligne**, à chaque build,
vers les trois formats que SDL_GPU sait charger. Rien n'est compilé au lancement du jeu.

```
shaders/triangle.vert.hlsl ──┐
                             ├─ shadercross ─→ bin/shaders/triangle.vert.spv    (Vulkan)
                             │   (DXC + SPIRV-Cross)  triangle.vert.dxil   (Direct3D 12)
                             │                        triangle.vert.msl    (Metal)
                             └─────────────────────── triangle.vert.json   (réflexion)
```

## Ajouter un shader

1. Crée `shaders/nom.vert.hlsl`, `nom.frag.hlsl` ou `nom.comp.hlsl` (l'étage est dans le nom).
2. Ajoute-le à `SOURCES` dans `shaders/CMakeLists.txt` (et les `.hlsli` partagés à `COMMON`).
3. `cmake --build --preset debug` : les quatre sorties apparaissent dans `build/<preset>/bin/shaders/`.
4. Côté moteur : `eng_shader_load(&gpu, &scratch, NULL, "nom.vert", ENG_SHADER_VERTEX)`.

Une erreur HLSL fait échouer la build avec le message de DXC (fichier, ligne, colonne).
Seul le shader modifié est recompilé ; un fichier `COMMON` modifié recompile tous les shaders
qui le déclarent.

## Conventions HLSL pour SDL_GPU

- Point d'entrée : `main`.
- Vertex : tampons uniformes en `register(b0, space1)`, etc.
- Fragment : textures et samplers en `register(t0, space2)` / `register(s0, space2)`, tampons
  uniformes en `register(b0, space3)` (octets passés à `eng_render_draw_uniforms2`).
- Les entrées de sommet utilisent la sémantique `TEXCOORDn` (c'est le `location` n côté GPU).

- Uniforms de vertex : un `cbuffer` en `register(b0, space1)`. Le renderer pousse sur le slot 0 les
  octets passés à `eng_render_draw_uniforms`. Le nombre de tampons uniformes vient de la réflexion
  (`.json`).
- Matrice : une `mat4` de cglm (colonne-majeur) se copie telle quelle et se lit
  `mul(mvp, float4(position, 1.0))` (vérifié par pixels, voir `tests/test_render3d.c`).

Les règles complètes sont dans la documentation de `SDL_CreateGPUShader` (SDL_gpu.h).

## Chargement

`eng_shader_load` choisit le format selon le périphérique : SPIR-V si Vulkan, DXIL si Direct3D 12,
MSL si Metal (ordre de préférence : SPIR-V, DXIL, MSL). Il lit aussi le `.json` pour renseigner
le nombre de samplers, de textures et tampons de stockage, et de tampons uniformes, qu'il faut
déclarer à la création du shader. Un même dossier `shaders/` sert donc les trois pilotes.

Entrée MSL : SPIRV-Cross renomme `main` en `main0` ; le chargeur le sait.

## Rechargement à chaud (Debug)

En Debug (option `OW_SHADER_HOT_RELOAD`, activée par défaut dans ce seul cas), un shader peut être
modifié pendant que le jeu tourne. `EngShaderHot` (`engine/shader_hot.h`) :

1. surveille la date et la taille du `.hlsl` (`EngFileWatch` : le fichier doit être stable entre
   deux contrôles, soit 0,25 à 0,5 s après l'enregistrement) ;
2. le recompile à l'exécution : DXC → SPIR-V, puis SPIR-V → format du pilote (SPIR-V, DXIL ou
   MSL) par la bibliothèque shadercross liée au moteur ;
3. remplace `hot->shader`, incrémente `hot->generation` et fait renvoyer `true` à
   `eng_shader_hot_poll`.

```c
EngShaderHot frag;
eng_shader_hot_init(&frag, &gpu, &scratch, SOURCE_DIR "/triangle.frag.hlsl", NULL,
                    "triangle.frag", ENG_SHADER_FRAGMENT);
...
if (eng_shader_hot_poll(&frag, &gpu, &scratch, now)) {
    /* frag.shader est un NOUVEAU shader : recréer le pipeline qui l'utilise */
}
```

- **Erreur de compilation** : le message de DXC (fichier, ligne, colonne) est journalisé, l'ancien
  shader reste en service, `failed_reloads` augmente. Corriger le fichier relance la compilation.
- **Pipeline** : SDL_GPU permet de libérer un shader dont un pipeline a déjà été créé ; l'ancien
  pipeline continue donc de fonctionner jusqu'à son remplacement (vérifié par un test).
- **Autres builds** (Release, RelWithDebInfo, profile) : `eng_shader_hot_init` charge le shader
  compilé hors ligne `name`, `eng_shader_hot_poll` renvoie toujours `false`. Le code d'appel est
  identique ; DXC et SPIRV-Cross ne sont pas liés au jeu.
- **Repli au démarrage** : si le source ne compile pas au lancement, le shader hors ligne sert de
  base (avec un avertissement).
- **Limite** : seul le `.hlsl` est surveillé. Modifier uniquement un fichier `#include`
  (`.hlsli`) ne déclenche rien : touchez le `.hlsl` qui l'inclut.
- **Windows** : `dxcompiler.dll`, `dxil.dll` et la DLL de SPIRV-Cross sont copiées à côté de
  l'exécutable (`ow_copy_shader_runtime`).
- **LeakSanitizer** : shadercross (commit `1ff05be`) ne libère pas tous les objets DXC qu'il crée
  (environ 0,5 Ko par recompilation). `engine/lsan.h` masque les allocations de `libdxcompiler`
  dans le lanceur et les tests ; les fuites de notre code restent signalées.

## Exemple complet : le triangle (M1-04, M1-05)

`launcher/src/demo_scene.c` dessine le triangle coloré à travers le renderer (`docs/renderer.md`) :

1. **tampon de sommets** : `eng_renderer_create_buffer` copie 3 sommets (position, couleur) dans un
   tampon GPU via un tampon de transfert ;
2. **pipeline graphique** : `eng_renderer_create_pipeline` avec les shaders `triangle.vert` /
   `triangle.frag` (compilés à chaud en Debug), deux attributs (emplacements 0 et 1 = sémantiques
   `TEXCOORD0` et `TEXCOORD1` du HLSL), face avant = sens antihoraire, faces arrière éliminées,
   format de la cible = celui de la swapchain ;
3. **commande de dessin** : `eng_render_draw` l'enregistre dans la file ; `eng_renderer_end_frame`
   l'exécute dans la passe ouverte sur la swapchain, soumet et présente.

Repère : coordonnées de découpage, x vers la droite, y vers le haut, comme dans tous les pilotes
SDL_GPU. Le test `test_renderer` dessine dans une texture 64 × 64 et vérifie les pixels (couleurs
interpolées calculées à la main, fond noir autour).

## Exemple : le cube (M1-07)

`shaders/cube.vert.hlsl` :

```hlsl
cbuffer Uniforms : register(b0, space1) { float4x4 mvp; };
// position (TEXCOORD0) et couleur (TEXCOORD1) -> SV_Position = mul(mvp, float4(position, 1))
```

Il réutilise `triangle.frag` : la couleur est interpolée, donc plate par face quand les 4 sommets
d'une face ont la même couleur.

## Exemple : le cube texturé (M1-08)

`shaders/textured.vert.hlsl` ajoute des coordonnées `uv` (TEXCOORD2) et une teinte au cube ;
`shaders/textured.frag.hlsl` lit la texture :

```hlsl
Texture2D<float4> albedo : register(t0, space2);
SamplerState albedo_sampler : register(s0, space2);
float4 main(float4 tint : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target0
{ return albedo.Sample(albedo_sampler, uv) * tint; }
```

Le nombre de samplers (1) vient de la réflexion (`.json`), comme les tampons uniformes. Un shader
qui échantillonne exige que chaque dessin lie une texture et un sampler : le renderer ne peut pas
le vérifier (il ne connaît pas le nombre de samplers d'un shader), c'est à l'appelant.

## Exemple : les modèles glTF (M1-10)

`model.vert.hlsl` reçoit position (TEXCOORD0), normale (TEXCOORD1), UV (TEXCOORD2) et tangente
(TEXCOORD3, `float4` : xyz + signe de la bitangente) — la disposition d'`EngModelVertex`, 48 octets
(M1-15) — et un cbuffer de 208 octets (`ModelUniforms` dans `model_gpu.c`, même ordre) : `mvp`,
`model` (modèle → monde), `normal_matrix` (inverse transposée du 3x3 du modèle, dans un 4x4) et
`color` (facteur de couleur du matériau). Il envoie au fragment la normale, la tangente (par la
matrice du modèle) et la position **en espace monde**, où l'éclairage est calculé.

`model.frag.hlsl` (M1-13, PBR depuis M1-15) lit un cbuffer `register(b0, space3)` de quatorze `float4` (les sept premiers ci-dessous, puis l'ombre, M1-17)
(`FragmentUniforms` dans `model_gpu.c`) : direction du soleil ; couleur × intensité du soleil ;
couleur × intensité ambiante (l'intensité de l'environnement) ; position de la caméra ; métal, rugosité, échelle de la normale, force
de l'occlusion ; facteur émissif et, en `w`, l'indicateur « normal map » ; et, en `x` du septième,
le nombre de niveaux du préfiltré moins 1. Il lit **neuf textures** (la neuvième est la carte d'ombre) : les cinq du matériau
(`t0..t4, space2`) plus l'irradiance (`TextureCube`, `t5`), le préfiltré (`TextureCube`, `t6`) et la LUT BRDF
(`Texture2D`, `t7`), chacune avec son sampler (`s0..s4, space2`) : SDL_GPU lie des COUPLES texture +
sampler, donc chaque `Texture2D` déclaré exige son `SamplerState` du même indice. Il applique le
modèle décrit dans `docs/pbr.md` et `docs/ibl.md`. Les rôles sans texture lient une texture blanche 1 × 1.
`multitex.frag.hlsl` est un shader de test (cinq textures lues séparément) pour la liaison.

`sky.vert.hlsl` / `sky.frag.hlsl` (M1-16) dessinent la skybox : un triangle plein écran fabriqué à partir de
`SV_VertexID` (sans attribut), la direction monde du pixel = `inverse(view_proj) × (ndc, 0,5, 1)` moins la
position de la caméra, `z = 0` (tout au fond, sans profondeur, dessiné avant la scène). Le fragment lit un
`TextureCube` (`t0`) et le multiplie par l'intensité (cbuffer `register(b0, space3)`). Voir `docs/ibl.md`.

`shadow.vert.hlsl` / `shadow.frag.hlsl` (M1-17) : la passe d'ombre. Le vertex shader ne lit que la
position (`TEXCOORD0`) et le premier membre de son cbuffer, `float4x4 light_mvp` : les blocs plus grands
(`ModelUniforms`) conviennent, leurs membres suivants sont ignorés. Le fragment shader est vide : seule
la profondeur est écrite. `model.frag.hlsl` lit en plus `shadow_map` (`Texture2D`, `t8, space2`) et son
sampler (`s8, space2`) et quatre `float4` + une matrice de plus (`shadow_matrix`, `shadow_a`, `shadow_b`,
`shadow_c`) : le bloc fait 14 `float4`, 224 octets. Voir `docs/shadows.md`.

## Exemple : la résolution HDR (M1-14)

`tonemap.vert.hlsl` n'a **pas de buffer de sommets** : un seul triangle plein écran fabriqué à
partir de `SV_VertexID` (les sommets 0, 1, 2 donnent les uv (0,0), (2,0), (0,2), le triangle déborde
de l'écran et le rasteriseur garde le carré [0,1]²), dessiné par `SDL_DrawGPUPrimitives(pass, 3,
1, 0, 0)`. `tonemap.frag.hlsl` lit la texture HDR (`t0/s0, space2`) et un cbuffer `register(b0,
space3)` d'un `float4` : x = multiplicateur d'exposition (2^EV), y = opérateur (0 aucun, 1 ACES,
2 AgX), z = 1 s'il faut encoder en sRGB. Même calcul que `eng_tonemap_apply`
(`docs/tonemapping.md`). Les matrices GLSL de three.js (colonnes) sont reproduites par
`mul_columns` ; la condition d'un `? :` ne vaut pas pour un vecteur en HLSL, d'où `lerp` et `step`
dans `srgb_encode`.

## Exemple : les lignes de debug (M1-11)

`lines.vert.hlsl` reçoit position (TEXCOORD0, float3) et couleur (TEXCOORD1, 4 octets lus comme
4 flottants : `ENG_VERTEX_UBYTE4_NORM`) — `EngDebugVertex`, 16 octets — et un cbuffer d'une seule
matrice (`view_proj`). `lines.frag.hlsl` affiche la couleur interpolée. Topologie `ENG_TOPOLOGY_LINES`
(liste de segments : 2 sommets par ligne). Voir `docs/debug-draw.md`.

## Outils (versions figées dans `third_party/CMakeLists.txt`)

| Outil | Version | Rôle |
|---|---|---|
| SDL_shadercross | commit `1ff05be` (aucune version publiée) | exécutable `shadercross` |
| DXC (Microsoft) | v1.9.2609, archives officielles Linux x86_64 et Windows, somme de contrôle SHA-256 | HLSL → SPIR-V et DXIL (signé) |
| SPIRV-Cross | tag `vulkan-sdk-1.4.363.0` | SPIR-V → MSL |

Désactiver : `-DOW_BUILD_SHADERS=OFF` (plus de compilateur C++ requis, plus de téléchargement DXC ;
les tests qui lisent les fichiers compilés disparaissent). Plateformes : Linux x86_64 et Windows ;
ailleurs la chaîne est désactivée avec un avertissement.

## Limites connues

- **Direct3D 12 et Metal ne sont pas testés** par l'auteur : seul le chemin Vulkan l'est de bout
  en bout (le pilote accepte réellement les shaders). Le DXIL est validé par `dxv` (signature
  correcte) mais n'a jamais été chargé par un vrai pilote D3D12. Le MSL n'a jamais été compilé
  par Metal.
- La réflexion ne remplit que les quatre compteurs ; la mise en page des sommets reste décrite à
  la main (M1-04).
- Windows : la copie des DLL de DXC à côté de `shadercross.exe` est prévue mais n'a pas été
  exécutée.
