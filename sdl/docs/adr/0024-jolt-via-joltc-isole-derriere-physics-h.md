# ADR 0024 : Jolt Physics via joltc, isolé derrière `physics.h`

Statut : accepté (M4-01)

## Contexte

M4 demande une physique rigide, des requêtes spatiales (rayons, balayages), un contrôleur de personnage « cinématique »
(`CharacterVirtual`) et des collisions de monde (champ de hauteurs, maillages). Le projet est en C11 : il faut une
bibliothèque que l'on sait utiliser depuis le C, sans laisser son modèle objet (C++) se répandre dans le jeu.

## Décision

- **Jolt Physics v5.7.0** (commit `cf16769`), via l'habillage C **joltc** (commit `b670cef`, qui expose la quasi-totalité
  de Jolt, `CharacterVirtual`, `HeightFieldShape` et `MeshShape` compris), téléchargés par `FetchContent` derrière
  l'option `OW_PHYSICS` (défaut ON). Les deux commits sont figés ; Jolt est déclaré avant joltc pour que sa version soit
  la nôtre.
- **Isolation** : `engine/physics.h` n'expose que nos types ; seul `engine/src/physics.c` inclut l'en-tête de joltc ;
  joltc est lié en `PRIVATE` ; un test de CTest (`physics.isolation`) échoue si un symbole de Jolt apparaît ailleurs.
- **Réglages** : SSE4.1/4.2 sans AVX/FMA (portabilité et reproductibilité), pas de compute shaders, pas de
  précision double, pas de déterminisme inter-plateformes forcé (`CROSS_PLATFORM_DETERMINISTIC` est coûteux ; les rejeux
  de M5-18 ne demandent que la même machine et le même binaire).

## Alternatives

- **Écrire un petit habillage C nous-mêmes** au-dessus de Jolt : ~1 000 lignes de C++ à maintenir pour ce que joltc
  fournit déjà ; reste possible si joltc se révèle un jour insuffisant (l'isolation derrière `physics.h` rend ce
  remplacement local).
- **Bullet / PhysX** : Bullet est plus lourd, PhysX est lourd et à la licence/au système de build moins simples ; Jolt est
  en C++17 compact, MIT, avec un contrôleur de personnage et un déterminisme à pas fixe soignés.
- **Physique maison** : un contrôleur de personnage robuste (pentes, marches, sans traverser les murs à vitesse
  maximale) est précisément le travail de M4-06 ; l'écrire de zéro coûterait plus qu'il n'apporte.

## Conséquences

- Premier « vrai » code C++ tiers volumineux du dépôt : ~145 fichiers, ~1 min de compilation sur 2 cœurs en Debug (mesuré),
  ~5 min en Release (mesuré, 2 cœurs) ; téléchargé une fois (clone superficiel).
- joltc est un projet jeune dont la version est figée par commit : une mise à jour est une décision explicite (relire
  les changements de l'API C), pas un effet de bord.
- `joltc.h` utilise des constantes binaires (extension C23) : son dossier d'en-têtes est déclaré « système » pour rester
  compatible avec `-Wpedantic -Werror`.
- Sans `OW_PHYSICS`, le moteur se compile sans Jolt ; `eng_physics_*` échoue proprement.
