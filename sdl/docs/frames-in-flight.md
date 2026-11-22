# Images en vol, barrières et redimensionnement (M1-18)

Le CPU prépare l'image N + 1 pendant que le GPU dessine l'image N. Ce document décrit combien d'images
peuvent être en préparation en même temps, comment le moteur le sait (barrières) et ce qui se passe
quand la fenêtre change de taille.

## Images en vol

`EngGpuDesc.frames_in_flight` : 1 à 3, **2 par défaut** (0 ou une valeur hors de 1..3 : la valeur par
défaut). Le choix est aussi annoncé à SDL (`SDL_SetGPUAllowedFramesInFlight`). Réglable ensuite avec
`eng_gpu_set_frames_in_flight`.

| Valeur | Effet |
|---|---|
| 1 | le CPU attend la fin de chaque image : aucun recouvrement, latence minimale |
| 2 | une image se prépare pendant que l'autre se dessine : compromis par défaut |
| 3 | plus de débit si le CPU est irrégulier, une image de latence de plus |

## Barrières (fences)

`eng_gpu_end_frame` soumet avec `SDL_SubmitGPUCommandBufferAndAcquireFence` et range la barrière dans un
anneau de 3 emplacements (la plus ancienne d'abord). `eng_gpu_begin_frame` :

1. rend, sans attendre, les barrières des images que le GPU a finies (dans l'ordre) ;
2. tant que `fence_count >= frames_in_flight`, **attend la plus ancienne** puis la rend ;
3. seulement ensuite, prend un tampon de commandes et l'image de la swapchain.

La règle tient quel que soit le mode de présentation : en `IMMEDIATE` ou sur une fenêtre sans écran,
la swapchain ne cadence pas toujours le CPU. Une attente qui échoue est journalisée et la barrière est
rendue quand même (pas de blocage sur une barrière que le pilote ne signale plus).

Autres fonctions : `eng_gpu_frames_pending` (interroge, n'attend pas), `eng_gpu_wait_idle` (attend
tout, avant de détruire une ressource que ces images utilisent), `eng_gpu_resources_balanced`
(barrières obtenues = rendues + en vol ; cibles créées = libérées + vivantes). `eng_gpu_shutdown`
retourne `false` (et journalise une erreur) si une barrière ou une cible n'a pas été rendue.

Les données dynamiques (uniforms, sommets du debug draw, interface) passent par des tampons de transfert
**cyclés** (`cycle = true`, M1-05) : SDL en prend un libre à chaque image, pas besoin d'anneau de
tampons côté moteur pour N images en vol.

## Redimensionnement

La profondeur et la texture HDR ont la taille de la swapchain. `begin_frame` les recrée quand la taille
change ; l'ancienne texture est rendue à SDL (`SDL_ReleaseGPUTexture` : libérée quand les images qui
l'utilisent sont finies, donc sans attente ni arrêt du GPU). Rien n'est recréé si la taille n'a pas
changé.

`eng_gpu_set_resize_callback` : appelé après la recréation, et à la toute première image, avec la
nouvelle taille en pixels (largeur **ou** hauteur qui change). Les futures cibles qui dépendent de
l'écran (SSAO, bloom) s'y recréent.

Compteurs : `resizes`, `targets_created`, `targets_released`, `fence_waits`, `fences_acquired`,
`fences_released`, `max_pending`. Le lanceur les journalise à l'arrêt.

## Lanceur

- `--frames-in-flight N` (1 à 3).
- `--stress-resize` : change la taille de la fenêtre à **chaque image** ; à l'arrêt, le lanceur attend le
  GPU, vérifie le bilan des ressources et quitte avec le code 1 s'il y a une fuite ou une image ignorée.
  Tests `launcher.resize_1`, `_2`, `_3` : 120 images, 120 changements de taille chacun.

## Vérification

- `tests/test_gpu.c` (SwiftShader, 42 tests dont 17 pour cette étape) : limites 1 à 3 respectées ;
  **GPU simulé** (`eng_gpu_set_fence_hooks`) : à la limite, `begin_frame` attend exactement une fois
  par image (frames − N attentes, jamais plus de N en vol), un GPU rapide n'attend jamais, retrait
  dans l'ordre et sans attente, réduction de la limite, échec d'attente sans blocage ni fuite ;
  redimensionnements rapides (150 changements, 1 à 3 images en vol, profondeur et HDR) avec bilan
  des cibles à chaque image ; rappel de redimensionnement (une fois par changement, largeur seule,
  hauteur seule) ; l'arrêt signale une ressource non rendue.
- Le simulateur existe parce que, sur le pilote logiciel, le GPU a toujours fini avant le retour du
  CPU : la limite n'est jamais atteinte par elle-même (mesuré : au plus 1 image en vol même à 3 de limite).

## Limites

- **Le recouvrement CPU/GPU n'a pas été mesuré sur un vrai GPU.** Les tests prouvent la règle
  (attendre au plus N images), pas le gain de débit ni la latence. À regarder chez toi avec
  `--frames-in-flight 1` puis `2` (FPS, panneau F1).
- SDL applique aussi sa propre limite (`SDL_WaitAndAcquireGPUSwapchainTexture`) : avec la même valeur,
  les deux sont redondantes en `VSYNC`. L'appel `SDL_SetGPUAllowedFramesInFlight` n'a pas d'effet
  observable dans mon environnement (non testé).
- Pendant un redimensionnement en direct, l'**aspect de la caméra et l'interface** utilisent la taille de
  la fenêtre lue aux événements ; la swapchain peut avoir un retard d'une image : une image peut être
  légèrement étirée. Non corrigé.
- La garde « anneau plein » de `end_frame` est défensive : `begin_frame` garantit déjà de la place, aucun
  chemin de l'API ne l'atteint (mutant `end_cap` survivant, assumé).
- Seuls la profondeur et le HDR dépendent de la taille de l'écran pour l'instant ; la carte d'ombre est
  indépendante.
