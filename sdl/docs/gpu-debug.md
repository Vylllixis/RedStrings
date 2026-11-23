# Déboguer le GPU : passes nommées et ressources nommées (M1-19)

Une capture RenderDoc brute montre des centaines d'appels `vkCmd*` sans contexte. Le moteur y ajoute
deux choses, via SDL_GPU et `engine/gpu_debug.h` :

- des **groupes** autour de chaque passe (« Frame 12 » > « Shadow map », « Scene », « Tonemap »...) ;
- des **noms** sur les textures, buffers, échantillonneurs et pipelines.

## Ce que la capture doit montrer

Hiérarchie d'une image du lanceur (ombres, HDR et interface actifs) :

```
Frame 42
├─ Upload streams          copie des buffers de streaming (debug draw...)
├─ Shadow map              passe d'ombre du soleil (profondeur seule)
├─ Overlay upload          copie des données de l'interface (Dear ImGui)
├─ Scene                   modèle, sol, ciel, debug draw : dans la texture HDR
├─ Tonemap                 résolution HDR → swapchain (exposition, tonemapping, sRGB)
└─ Overlay                 interface par-dessus, dans la swapchain
```

Un groupe est absent quand sa passe n'a pas lieu : pas d'ombre → pas de « Shadow map » ; interface
masquée (F1) → pas de « Overlay » (« Overlay upload » reste, vide : le moteur appelle quand même la
préparation de l'interface) ; HDR coupé → pas de « Tonemap » ; aucun flux écrit → pas d'« Upload
streams ». « Scene » est toujours là (une image vide l'efface).

Hors image, à la création des ressources : « Upload buffer <nom> » et « Upload texture <nom> », un
groupe par envoi. Chacun a son propre tampon de commandes : ils ne sont pas dans « Frame N » et
n'apparaissent pas dans la capture d'une image du jeu en cours.

Noms de ressources, tels qu'ils apparaissent dans l'inspecteur de ressources :

| Type | Noms |
|---|---|
| Texture | `Depth`, `HDR scene color`, `Shadow map depth`, `environment-*` (ciel, irradiance, préfiltrée, LUT BRDF), `model-texture`, `model-white`, les textures de la scène |
| Buffer | `Model vertices/indices`, `Demo cube vertices/indices`, `Sky dummy vertices`, `Stream vertex buffer`, `Stream index buffer` |
| Échantillonneur | `HDR scene sampler`, `model`, `model-shadow`, `environment`, `demo` |
| Pipeline | `model`, `model-shadow`, `cube`, `cube-shadow`, `sky`, `tonemap`, `debug-lines`, `debug-lines-overlay` |

## Capturer avec RenderDoc (Linux)

> Non vérifié ici : je n'ai pas RenderDoc dans mon environnement. Ce qui suit est la procédure
> habituelle ; voir « Ce qui a été vérifié » plus bas pour ce que j'ai réellement pu tester.

1. Lancer RenderDoc (`qrenderdoc`), « Launch Application ».
2. Exécutable : `build/debug/bin/launcher`, dossier de travail : `build/debug/bin`.
3. Variable d'environnement : `SDL_VIDEODRIVER=x11`. RenderDoc ne gère pas Wayland nativement : sur une
   session Wayland, le lanceur passe alors par XWayland.
4. « Launch », puis F12 (ou le bouton « Capture Frame(s) Immediately ») dans la fenêtre du jeu.
5. Ouvrir la capture : l'« Event Browser » affiche la hiérarchie ci-dessus ; « Texture Viewer » et
   « Resource Inspector » affichent les noms.

Les noms de **ressources** ne sont transmis au pilote que si SDL a été créé en mode débogage, c'est-à-dire
dans les builds sans `NDEBUG` (`debug`, `relwithdebinfo`, `profile`...) : le lanceur demande alors
`EngGpuDesc.debug`. Dans un build `release`, les groupes sont là mais les ressources ont leurs noms
internes (`VkImage 0x...`). C'est une limite de SDL (ses fonctions de nommage ne font rien hors du mode
débogage), pas un choix du moteur.

## API

```c
#include "engine/gpu_debug.h"

eng_gpu_debug_push(commands, "Particules");      // ouvre un groupe
/* ... passes, copies ... */
eng_gpu_debug_pop(commands);                      // ferme le dernier groupe ouvert

eng_gpu_debug_push_fmt(commands, "Upload texture %s", name);   // nom formaté, tronqué à 63 octets
eng_gpu_debug_label(commands, "début du ciel");                // repère ponctuel
```

Noms de ressources : `eng_gpu_debug_name_texture(device, texture, "nom")` et
`eng_gpu_debug_name_buffer`. Pour les échantillonneurs, pipelines et shaders, le nom se donne **à la
création** (propriété SDL) :

```c
uint32_t props = eng_gpu_debug_props("sampler", SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, "mon sampler");
info.props = props;
SDL_GPUSampler *s = SDL_CreateGPUSampler(device, &info);
eng_gpu_debug_props_free(props);                  // après la création
```

Les noms des passes de l'image sont des constantes (`ENG_GPU_PASS_SCENE`, `ENG_GPU_PASS_SHADOW`...) :
anglais et ASCII, parce que certains outils affichent mal les accents.

### Règles

- **Un push, un pop**, sur le même tampon de commandes, avant la soumission. Un groupe ouvert à la
  soumission est une erreur de validation Vulkan : `eng_gpu_end_frame` ferme « Frame N » juste avant
  de soumettre, et les passes ferment leur groupe même quand `SDL_BeginGPURenderPass` échoue.
- **Groupe autour de la passe, jamais dedans** : sur Metal, un groupe ouvert pendant une passe appartient
  à la passe native (doc SDL). Ici chaque groupe est ouvert avant `SDL_Begin*Pass` et fermé après
  `SDL_End*Pass`.
- Un `pop` sans groupe ouvert est **ignoré** (jamais transmis au pilote) et compté
  (`unbalanced_pops` dans `eng_gpu_debug_stats`) : c'est un bug de l'appelant, pas un plantage.
- Au-delà de 32 niveaux imbriqués, les groupes restent équilibrés mais leur nom n'est plus mémorisé
  (« ? » pour l'enregistreur).
- État global, **fil de rendu seulement**. `eng_gpu_debug_set_enabled(false)` coupe push, label et noms ;
  à appeler entre deux images (un groupe déjà ouvert est quand même fermé).

### Observer sans RenderDoc

`eng_gpu_debug_set_recorder(fn, user)` reçoit chaque push, pop, repère et nom, dans l'ordre, avec la
profondeur. Les tests l'utilisent : SDL ne permet pas de relire ce qui part vers le pilote.
`eng_gpu_debug_stats()` donne les compteurs (`pushes`, `pops`, `labels`, `names`, `unbalanced_pops`,
`depth`, `max_depth`).

## Ce qui a été vérifié

- **Tests** (`test_gpu_debug`, 29 tests, GPU requis) :
  - *ce que le module décide* (enregistreur) : ordre exact des groupes pour une image vide, avec
    overlay, avec HDR, avec flux + ombre + interface ; numérotation « Frame N » ; équilibre sur 90
    images avec redimensionnements, HDR et overlay en alternance (validation SDL active) ; troncature ;
    pop sans groupe ; imbrication au-delà de 32 niveaux ; coupure ; compteurs ; noms des cibles
    recréées au redimensionnement ;
  - *ce que SDL reçoit* : l'éditeur de liens remplace les fonctions SDL concernées par des espions qui
    notent l'appel puis le transmettent (`-Wl,--wrap=...`, `tests/CMakeLists.txt`). On vérifie ainsi
    que chaque groupe arrive sur le tampon de commandes de l'image, que les noms de textures et de
    buffers arrivent avec le bon objet, et que les créations d'échantillonneurs, de pipelines et de
    shaders portent bien leur nom. Ces tests ne sont compilés que sous Linux avec GCC ou Clang
    (`--wrap` n'existe ni sous macOS ni avec MSVC) ; les autres tests restent.
- **Au niveau du pilote**, avec une couche Vulkan de trace écrite pour la vérification (elle se place
  au même endroit que RenderDoc : entre SDL et le pilote). Lanceur sur SwiftShader, 6 images, interface
  affichée : 66 `vkCmdBeginDebugUtilsLabelEXT` pour 66 `vkCmdEndDebugUtilsLabelEXT`, les passes ci-dessus
  dans l'ordre, et 47 noms d'objets (images, buffers, échantillonneurs, pipelines) reçus par le pilote.
  Cette couche n'est pas dans le dépôt : c'est un contrôle ponctuel, pas un test.

## Mutants

79 mutants appliqués à `gpu_debug.c`, `gpu.c`, `renderer.c` et `shader.c` (compteurs, gardes, ordre et
présence de chaque push/pop, appel SDL supprimé, nom ou objet erroné, propriétés non transmises...) :
74 tués. **5 survivants**, tous du même type : le `pop` qui suit l'échec de `SDL_BeginGPURenderPass` ou
`SDL_BeginGPUCopyPass` (`scene_fail_pop`, `tonemap_fail_pop`, `overlay_fail_pop`, `shadow_fail_pop`,
`streams_fail_pop`). Ces échecs ne se provoquent pas avec un pilote qui fonctionne ; le `pop` est
nécessaire (sans lui, un groupe resterait ouvert à la soumission) mais non testé.

## Limites

- **RenderDoc lui-même n'a pas été lancé** (ni sur SwiftShader, ni sur la RTX 5070). Les groupes et les
  noms atteignent bien le pilote ; la façon dont RenderDoc les affiche est son comportement documenté
  (`VK_EXT_debug_utils`), non testé ici.
- Sur Direct3D 12, SDL exige `WinPixEventRuntime.dll` pour les groupes et repères (doc SDL) ; sans
  elle, ils sont sans effet. Non testé (pas de Windows ici).
- Un pilote sans `VK_EXT_debug_utils` ignore groupes et noms : le moteur continue sans erreur.
- Les shaders compilés **à chaud** (`ENG_SHADER_HOT_RELOAD`, builds de développement) ne sont pas nommés :
  seuls les shaders chargés hors ligne (`eng_shader_load`) le sont. Les modules de shader apparaissent
  rarement utiles dans une capture ; les pipelines, eux, sont nommés dans les deux cas.
- Le coût d'un groupe (un appel SDL, environ neuf par image) n'est pas mesuré.
- Seuls les chemins du moteur (`EngGpu`, `EngRenderer`) posent des groupes : les passes que les tests
  ouvrent eux-mêmes (`test_render_util.h`, rendu vers une texture) n'en ont pas.
