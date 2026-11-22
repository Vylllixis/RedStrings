# Gestionnaire d'assets (M2-05)

`engine/assets.h` charge un asset **une seule fois** par (type, chemin), le garde tant que quelqu'un s'en
sert, et le décharge au dernier `release`. Il ne connaît ni fichier ni GPU : ce sont les **types** qui
savent charger (`engine/asset_types.h` fournit le type « modèle »).

```c
EngAssets assets;
eng_assets_init(&assets, 64);                       /* 64 assets présents au plus */
EngModelAssetContext ctx = {.renderer = &renderer, .anisotropy = 8.0F, .environment = &env};
uint32_t model = eng_model_assets_register(&assets, &ctx);   /* ctx vit aussi longtemps que assets */

EngAssetHandle h = eng_assets_acquire(&assets, model, "assets/cooked/models/demo.mesh");
const EngModelGpu *gpu = eng_model_asset_gpu(&assets, model, h);   /* NULL si invalide */
/* ... dessiner avec eng_model_gpu_draw(gpu, ...) ... */
eng_assets_release(&assets, h);                     /* dernier release : déchargé */
eng_assets_shutdown(&assets);                       /* nombre d'assets jamais rendus (fuites) */
```

## Règles

- Même (type, chemin) → **même handle**, un seul chargement (`stats.loads`, `stats.hits`). Chaque
  `acquire` prend une référence, chaque `release` en rend une ; `retain` copie un handle.
- Au dernier `release`, l'asset est déchargé et son handle devient **périmé** : `eng_assets_get` rend
  `NULL`, `release` refuse (journalisé). Jamais de pointeur qui pend via un handle.
- **Un échec n'est pas mémorisé** : rien n'est gardé, le prochain `acquire` réessaie (fichier réparé).
- Un chargeur peut demander d'autres assets (ré-entrant) mais pas **le sien** : refusé, donc le
  chargement échoue proprement au lieu de boucler.
- Capacité fixe (`eng_assets_init`) ; trop d'assets → refus journalisé. Chemins : au plus 255 octets,
  comparés tels quels (`a/b` et `a//b` sont deux assets).
- Le payload (taille fixe par type) est alloué à zéro puis libéré par le gestionnaire ; `unload` libère
  ce que `load` a pris. `eng_assets_shutdown` décharge tout et **compte** les assets encore référencés.
- Une seule file d'exécution, pas de verrou (comme le reste du moteur).

## Type « modèle »

Charge un `.mesh` cuit (`docs/asset-formats.md`) et ses `.tex` via `eng_mesh_load_visit`, sans parsing glTF
ni décodage d'image, puis crée le modèle GPU. Mémoire de travail d'un chargement : deux arènes de
`cpu_bytes` (64 Mio par défaut), réservées puis rendues ; l'arène du modèle GPU est dimensionnée d'après
le nombre de parties, de matériaux et de textures. Les dépendances déclarées (pour le rechargement à
chaud, M2-06) sont le `.mesh` et chacune de ses textures, en chemins résolus.

## Rechargement à chaud (M2-06)

```c
eng_assets_set_hot_reload(&assets, true, 0.0);   /* période par défaut : 0,25 s */
/* à chaque image, AVANT de construire l'image : */
eng_assets_poll(&assets, now);                   /* nombre d'assets rechargés */
```

- `eng_assets_poll` surveille les fichiers que chaque chargeur a déclarés (`eng_asset_load_dependency` :
  pour un modèle, le `.mesh` et chacun de ses `.tex`) avec `EngFileWatch` : un fichier n'est signalé
  qu'une fois **stable** (inchangé entre deux contrôles), jamais pendant qu'on l'écrit. Détection : au
  plus deux périodes (0,5 s).
- `eng_assets_reload` recharge **en place** : le nouveau payload est chargé à côté de l'ancien ; s'il
  réussit, l'ancien est déchargé et le nouveau prend sa place (même handle, mêmes références, **même adresse
  de payload** : un pointeur obtenu par `eng_model_asset_gpu` reste valide). S'il échoue, l'ancien reste en
  service (`stats.reload_failures`) ; pas de nouvelle tentative avant la prochaine modification du fichier.
- Conséquence pour un type : son payload doit pouvoir être **recopié octet par octet** (pas de pointeur vers
  lui-même). Le type modèle (arène heap + handles GPU) le respecte.
- Pendant le rechargement, deux versions du modèle existent un instant côté GPU.

### De la source au jeu : `engine/asset_cook.h`

Un fichier cuit ne change que si on le recuit. `EngAssetCook` surveille les fichiers **sources** (PNG, glTF…)
et lance la commande de cuisson (`assetc`) quand l'un d'eux change : un processus à la fois, sans bloquer
l'image, une source modifiée pendant une cuisson est recuite ensuite. Le lanceur l'active avec
`--watch-assets` (sources de la démo : `assets/models/demo.gltf`, `demo.bin`, `assets/textures/checker.png`
du dépôt).

Chaîne complète : sauvegarde de l'image → source vue stable (≤ 0,5 s) → `assetc` (≈ 0,2 s en `--raw`,
≈ 1 s en BC7 sur la machine de développement du moteur : Debug + ASan) → `.mesh`/`.tex` vus stables → modèle
rechargé. **Mesuré** : 1,03 s de l'écriture de la source à l'image différente (test GPU, texture RGBA8) ;
1,6 s dans le lanceur réel avec la cuisson BC7 par défaut.

## Limites connues

- Rechargement : la nouvelle version n'est pas vue par les dessins déjà mis en file dans l'image en cours
  (d'où « avant de construire l'image »). Un fichier réécrit avec la même taille et la même date de
  modification n'est pas détecté (limite de `EngFileWatch`).
- `--watch-assets` ne couvre que le modèle de démonstration ; le cube du lanceur lit toujours sa texture
  directement (hors gestionnaire) : elle n'est pas rechargée.
- Un seul type fourni (modèle). Les textures seules, sons, etc. viendront avec leurs besoins.
- Le chargement est **synchrone** : il bloque l'image qui le demande (un modèle lourd = une image longue).
  Le chargement asynchrone n'est pas dans M2.
- 64 Mio × 2 de mémoire virtuelle sont réservés à chaque chargement de modèle ; un modèle dont les
  textures dépassent cette taille est refusé (augmenter `cpu_bytes`).
