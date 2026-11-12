# Renderer (M1-05)

Couche au-dessus de `gpu` : le jeu et la scène décrivent *quoi* dessiner, le renderer sait *comment*
(SDL_GPU). Aucun en-tête SDL dans `game/` ni dans `engine/renderer.h` (règle vérifiée par
`tools/scripts/lint.sh`).

## Ressources : handles typés

- `EngBufferHandle`, `EngPipelineHandle` : index + génération (pool, `eng_pool_*`). Un handle détruit
  devient périmé pour toujours, même si l'emplacement est réutilisé.
- `eng_renderer_create_buffer(r, &(EngBufferDesc){usage, size, data})` : tampon **immuable**
  (sommets ou index), envoyé au GPU à la création.
- `eng_renderer_create_pipeline(r, &desc)` : shaders (noms ou chemins), stride, attributs
  (`location`, `format`, `offset`), topologie, culling, sens de la face avant, format de couleur
  (0 = swapchain). Description invalide → handle nul, aucun emplacement consommé.
- Hot reload (Debug) : `eng_renderer_update(r, now)` recompile les shaders modifiés et reconstruit le
  pipeline **derrière le même handle** ; en cas d'erreur, l'ancien pipeline est conservé.

## Cycle d'une image

```c
EngRenderQueue *q = eng_renderer_begin_frame(&r);   // file vidée
eng_render_draw(q, &(EngDrawCmd){ .pipeline = p, .vertex_buffer = vb, .count = 3 });
eng_renderer_end_frame(&r);                          // exécute, soumet, présente
```

- La file est de la donnée pure (`commands`, `count`, `capacity`, `dropped`) : `eng_render_draw` est
  inline, sans appel au GPU, sans allocation. File pleine → commande comptée dans `dropped`.
- À l'exécution, chaque commande est validée : handle périmé ou absent → **cette** commande est
  rejetée (compteur `rejected_commands`, avertissement une seule fois), les autres sont dessinées.
- Les liaisons de pipeline / tampons identiques à la précédente commande sont omises
  (`stats.pipeline_binds`).
- `eng_renderer_execute(r, queue, commands, pass)` exécute une file dans n'importe quelle passe
  (tests, rendu hors écran). Il prend aussi le command buffer : les uniforms se poussent sur lui.

## Uniforms (M1-07)

```c
mat4 mvp;  /* proj * view * model, voir conventions-maths.md */
EngDrawCmd cmd = { .pipeline = p, .vertex_buffer = vb, .index_buffer = ib, .count = 36 };
eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp);   // copie 64 octets dans la file
```

- Les octets sont **copiés** dans une zone de la file (`EngRendererDesc.max_uniform_bytes`, défaut
  raisonnable) : la source peut être une variable locale. Chaque dessin a son emplacement, aligné
  sur 16 octets, 4096 octets au plus (`ENG_MAX_UNIFORM_BYTES`).
- Refus (`dropped` augmente) : file ou zone pleine, pointeur NULL, taille 0 ou trop grande.
- À l'exécution, `offset + size` hors de la zone utilisée → commande rejetée comme un handle périmé.
- Vertex uniforms : slot 0 (`register(b0, space1)`).

### Uniforms de fragment (M1-13)

```c
eng_render_draw_uniforms2(q, &cmd, &vertex_block, sizeof vertex_block,
                                   &light_block,  sizeof light_block);  // les deux blocs, copiés
eng_render_draw_uniforms2(q, &cmd, NULL, 0, &light_block, sizeof light_block); // fragment seul
```

- Le fragment shader les lit dans `register(b0, space3)`, slot 0. Chaque bloc a son emplacement
  (aligné sur 16 octets, 4096 octets au plus) ; `EngDrawCmd.fragment_uniform_offset/size` les
  désignent. Un bloc absent s'écrit `(NULL, 0)`.
- Refus (`dropped` augmente, **rien** n'est écrit, pas même le premier bloc) : deux blocs absents,
  NULL avec une taille ou des données avec une taille nulle, bloc trop grand, file ou zone pleine.
- `stats.uniform_pushes` compte chaque envoi : un dessin avec les deux blocs en compte 2.
- `eng_render_draw_uniforms` reste le cas courant (vertex seul).

## Profondeur (M1-07)

- `EngRendererDesc.depth_buffer = true` active une texture de profondeur (D32_FLOAT, sinon D24, sinon
  D16) recréée à chaque changement de taille de la fenêtre, effacée à 1,0 à chaque image.
- Par pipeline : `depth_test` (comparaison LESS) et `depth_write`. `depth_write` sans `depth_test`
  est refusé, ainsi que toute profondeur demandée sans `depth_buffer` : le pipeline n'est pas créé et
  aucun emplacement du pool n'est consommé.
- Découpage (`enable_depth_clip`) actif : ce qui est avant le plan proche ou après le plan lointain
  n'est pas dessiné. Sans cela, SDL_GPU *écrête* la profondeur et dessine quand même ces surfaces
  (défaut trouvé par `the_clip_volume_depth_range_is_zero_to_one`).

## Passe d'ombre (M1-17)

- `eng_renderer_create_shadow_map(r, size)` : texture D32F carrée (8 à 8192), à la fois cible de profondeur
  et texture lue ; `ENG_TEXTURE_NULL` (journalisé) si la taille est hors limites ou si le GPU ne gère
  pas ce format dans les deux usages (`SDL_GPUTextureSupportsFormat`).
- Un pipeline `EngPipelineDesc.depth_only` n'a **aucune cible couleur** et dessine dans la profondeur
  de la carte (D32F) : il exige `depth_test` **et** `depth_write`, refuse `no_depth_target`, et n'a pas
  besoin du depth buffer de l'image.
- `eng_renderer_run_shadow_pass(r, commandes, carte, file)` ouvre une passe (profondeur effacée à 1,
  rien d'autre), exécute la file, la ferme ; `0` et aucune passe si `carte` n'est pas une carte
  d'ombre (`stats.shadow_passes` compte les passes ouvertes).
- File d'ombre du renderer : `EngRendererDesc.max_shadow_commands` (0 : aucune ; `max_shadow_uniform_bytes`,
  64 Kio par défaut), `eng_renderer_begin_shadow_queue` la vide et la rend, `eng_renderer_set_shadow_map`
  désigne la carte que `eng_renderer_end_frame` rend avant la passe de l'image. Carte nulle : pas de
  passe d'ombre (coût nul).
- Lecture : `EngDrawCmd.shadow_map` + `shadow_sampler` sont liés à l'emplacement
  `ENG_SHADOW_SLOT` (8) du fragment shader, hors des huit emplacements de `texture` /
  `extra_textures`. Les deux ou aucun, et les emplacements 0 à 7 doivent être tous occupés, sinon le
  dessin est rejeté et compté.

## Textures et samplers (M1-08)

```c
EngImage img;  eng_image_load(&scratch, "assets/textures/checker.png", &img);   // stb_image
EngTextureHandle tex = eng_renderer_create_texture(&r, &(EngTextureDesc){
    .name = "damier", .width = img.width, .height = img.height,
    .format = ENG_TEXTURE_RGBA8_SRGB, .pixels = img.pixels});     // mip_levels = 0 : chaîne complète
eng_image_free(&img);                                             // le GPU a sa copie
EngSamplerHandle smp = eng_renderer_create_sampler(&r, &(EngSamplerDesc){
    .name = "sol", .max_anisotropy = 16.0F});                     // trilinéaire, répété par défaut
EngDrawCmd cmd = { /* ... */ .texture = tex, .sampler = smp };
```

- **Image** (`engine/image.h`) : `eng_image_decode` / `eng_image_load` décodent PNG, JPEG, TGA, BMP...
  en RGBA 8 bits (alpha 255 si absent), première ligne = haut. Côté maximal : 16384.
- **Mipmaps** : calculés sur le CPU à la création (filtre boîte), puis envoyés avec le niveau 0.
  Pour `ENG_TEXTURE_RGBA8_SRGB`, la moyenne se fait en lumière linéaire (un damier noir/blanc donne
  le gris sRGB 188, pas 128). Tailles non puissances de 2 : chaque pixel source compte une fois,
  avec des poids légèrement inégaux. L'alpha est moyenné tel quel (non prémultiplié).
- **Formats** : `RGBA8_SRGB` pour les images de couleur (le GPU décode à la lecture),
  `RGBA8` pour les données (normales, rugosité).
- **Sampler** : filtrage min/mag (`LINEAR`, `NEAREST`), filtre de mipmaps (`LINEAR` = trilinéaire,
  `NEAREST`, `OFF` = niveau 0 seul), adressage par axe (`REPEAT`, `CLAMP`, `MIRROR`),
  anisotropie jusqu'à 16 (au-delà : plafonné). Une texture et un sampler sont indépendants.
- **Dessin** : `EngDrawCmd.texture` + `.sampler`, emplacement 0 du fragment shader
  (`register(t0, space2)` / `register(s0, space2)`). Un seul des deux, ou un handle périmé : dessin
  rejeté. `extra_textures[]` (M1-15) : emplacements 1 à 7 (`t1..t7`, `s1..s7`), **même sampler**,
  sans trou (un trou ou un handle périmé rejette aussi le dessin). Les liaisons identiques à la précédente commande sont omises (`stats.sampler_binds`).
- **Cubes et formats HDR** (M1-16) : `eng_renderer_create_texture_data` crée une texture à partir de
  données FOURNIES par l'appelant, en `RGBA16F` ou `RG16F`, avec 1 couche (2D) ou 6 (cube : faces
  `+X −X +Y −Y +Z −Z`), tous les niveaux de mip donnés (`mip_levels` entre 1 et la chaîne complète) ;
  `eng_renderer_texture_layers` donne le nombre de couches. Descriptions invalides : refus sans fuite.
- **UV** : (0, 0) = coin haut-gauche de l'image, v vers le bas.
- Handles générationnels, pools bornés (256 textures, 32 samplers par défaut), fuites rendues et
  comptées à l'arrêt comme pour les buffers et pipelines.

## Buffers de streaming (M1-11)

Les buffers de `eng_renderer_create_buffer` sont **immuables** (contenu donné à la création). Pour ce
qui change à chaque image (lignes de debug, plus tard particules, interface) :

```c
EngBufferHandle sb = eng_renderer_create_stream_buffer(&r, ENG_BUFFER_VERTEX, capacite_octets);
chaque image :   eng_renderer_stream_write(&r, sb, offset, donnees, taille);   /* avant end_frame */
```

- `create_stream_buffer` crée le buffer GPU **et** un tampon de transfert permanent : rien n'est créé
  ou détruit par image (0 allocation, mesuré).
- `stream_write` copie dans le tampon de transfert (le premier appel après un envoi prend un tampon
  libre : le GPU peut encore lire celui de l'image précédente ; les suivants complètent le même).
  Plusieurs écritures par image à des offsets différents sont permises ; la zone envoyée est leur
  **enveloppe**. Refus (`false`) : handle invalide, périmé, buffer immuable, `data` nul, ou écriture hors
  du buffer (le calcul `offset + size` ne déborde pas).
- `eng_renderer_end_frame` appelle `eng_renderer_flush_streams` **avant** la passe de rendu (une passe de
  copie ne peut pas s'ouvrir dans une passe de rendu). Avec une passe ouverte à la main (tests,
  captures) : appeler `eng_renderer_flush_streams(r, commands)` avant d'ouvrir la passe.
- Un buffer de streaming se dessine comme un autre (`EngDrawCmd.vertex_buffer`, `first`). Son contenu
  hors de ce qui a été écrit depuis l'image précédente est **indéfini** : ne dessiner que ce qu'on a
  écrit cette image.

## Debug draw (M1-11)

Lignes, boîtes, sphères, repères, grille : voir `docs/debug-draw.md`.

## Overlay (M1-12)

`eng_renderer_set_overlay(r, prepare, draw, user)` installe un dessin **par-dessus** la scène, dans
une seconde passe : `prepare` (avant toute passe, c'est là qu'on envoie des données au GPU) retourne
`false` s'il n'y a rien à dessiner (la passe n'est alors pas ouverte), `draw` reçoit le tampon de
commandes et la passe. La passe overlay **conserve** la couleur de la scène (`LOAD`) et n'a **pas de
profondeur** (`eng_gpu_begin_overlay_pass`, `eng_gpu_fill_overlay_target`). Utilisé par l'interface de
debug : `docs/debug-ui.md`. Compteur : `stats.overlay_passes`.

## HDR et tonemapping (M1-14)

`EngRendererDesc.hdr = true` : la scène est dessinée en RGBA16F, puis une passe plein écran
(exposition, tonemapping ACES/AgX, sRGB) la ramène à l'écran ; l'overlay est dessiné après, sans
tonemapping. Détails, formules et limites : `docs/tonemapping.md`. Côté API :

- `eng_renderer_hdr_enabled`, `eng_renderer_scene_format` (format par défaut des pipelines :
  RGBA16F en HDR, sinon la swapchain), `eng_renderer_set_tonemap` / `eng_renderer_tonemap`
  (réglages copiés et remis dans le domaine valide), `eng_renderer_resolve_hdr` (dessine une
  texture HDR dans une passe : c'est ce qu'appelle `EngGpu`, et ce que les tests appellent à la main).
- `EngPipelineDesc.no_depth_target` : pipeline d'une passe **sans** profondeur (résolution,
  post-traitement) même si le renderer a un depth buffer ; incompatible avec `depth_test` /
  `depth_write`. `eng_renderer_pipeline_depth_format` dit quelle profondeur un pipeline déclare.
- `EngRendererDesc.resolve_format` : format de la cible de la résolution (0 : la swapchain), pour
  résoudre dans une texture de test.
- Si le HDR ne peut pas être mis en place, le renderer continue sans (avertissement journalisé).
- Statistique : `stats.resolve_draws`. Le pipeline et le sampler de la résolution sont libérés par
  `eng_renderer_shutdown` sans compter comme fuites.

## Modèles glTF (M1-10)

`engine/model.h` charge un `.gltf` / `.glb` côté CPU, `engine/model_gpu.h` l'envoie au renderer et
met en file un dessin par partie : voir `docs/model.md`.

## Fermeture

`eng_renderer_shutdown` libère tout ce qui reste et renvoie le nombre de ressources non détruites
(le lanceur avertit si ce nombre n'est pas 0 ; 0 attendu pour la scène de démo).

## Limites connues

Tampons et textures immuables (pas de mise à jour ni de flux), pas de texture compressée (M2-03), une
au plus 8 textures par dessin (un seul sampler) plus la carte d'ombre (emplacement 8, son propre sampler), un seul slot d'uniforms par étage (vertex, fragment), file non triée, `instance_count` non couvert par les tests de pixels. Le jeu
(`game.render`) ne dessine pas encore via le renderer. La scène de démo est vue par la caméra libre
(`docs/camera.md`) : une grille de 7 × 7 cubes.

Images en vol, barrières et redimensionnement des cibles : voir `docs/frames-in-flight.md`.

Passes et ressources nommées pour RenderDoc : voir `docs/gpu-debug.md`.
