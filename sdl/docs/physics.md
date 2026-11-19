# Physique (M4)

Module `engine/physics.h` (M4-01) : enveloppe C de **Jolt Physics** (v5.7.0) à travers l'habillage C **joltc**
(commit `b670cef`). Décision : [ADR 0024](adr/0024-jolt-via-joltc-isole-derriere-physics-h.md).

## Ce que le jeu voit

Uniquement `engine/physics.h` : un monde opaque `EngPhysics`, des corps `EngBody` (entiers 32 bits,
`ENG_BODY_INVALID` = aucun), des vecteurs `float[3]`. **Aucun type de Jolt ni de joltc** n'apparaît dans `game/`, dans
les en-têtes du moteur ni dans les tests :

- `engine/src/physics.c` est le seul fichier qui inclut l'en-tête de joltc ;
- `joltc` est lié à `engine` en `PRIVATE` : ses dossiers d'en-têtes ne se propagent pas, `game/` ne pourrait pas
  l'inclure même en le voulant ;
- le test `physics.isolation` (`tests/physics_isolation.cmake`) parcourt `game/`, `launcher/`, `tools/`,
  `engine/include/`, `engine/src/` et `tests/` et échoue si `joltc.h`, `JPH_…` ou `JoltPhysics` apparaît ailleurs que
  dans `physics.c`.

## API (M4-01)

| Fonction | Rôle |
| --- | --- |
| `eng_physics_available()` | vrai si compilé avec Jolt (`OW_PHYSICS=ON`, défaut) |
| `eng_physics_create(desc)` / `_destroy` | monde (`max_bodies`, fils de travail, gravité) ; `desc` peut être `NULL` |
| `eng_physics_step(w, dt)` | un pas de `dt` s (0 < dt ≤ 0,1) ; **un seul pas de collision** ; le pas fixe 1/60 s est garanti par le jeu (M4-05) |
| `eng_physics_add_sphere / add_box / add_capsule` | corps de la couche demandée ; position = centre de masse ; capsule verticale |
| `eng_physics_remove_body` | retrait ; l'identifiant devient invalide (les appels suivants sont sans effet) |
| `eng_physics_body_position / rotation / velocity / set_body_velocity / is_active` | lecture / écriture ; `false` et sortie inchangée si le corps est inconnu |
| `eng_physics_body_count` | nombre de corps |
| `eng_physics_body_layer` | couche d'un corps (`ENG_LAYER_COUNT` si inconnu) |
| `eng_physics_move_kinematic` | déplace un capteur vers une cible en `dt` secondes (vitesse = écart / dt) |
| `eng_physics_overlaps` | corps en chevauchement avec un corps, filtrés par la matrice |
| `eng_physics_layer_interaction / is_sensor / name` | la matrice et les noms des couches |

Conventions du moteur : main droite, Y vers le haut, mètres, secondes ; gravité par défaut (0, −9,81, 0).

## Couches de collision (M4-02)

Chaque corps appartient à une couche, fixée à sa création (`EngPhysicsLayer`) :

| Couche | Rôle | Comportement |
| --- | --- | --- |
| `STATIC` | décor (terrain, rochers, murs) | immobile |
| `DYNAMIC` | objets simulés | dynamique |
| `CHARACTER` | personnages | dynamique, **rotation verrouillée** (reste debout) |
| `HITBOX` | volume d'attaque | **capteur** cinématique |
| `TRIGGER` | zone de déclenchement | **capteur** cinématique |

Matrice d'interaction (`eng_physics_layer_interaction`, symétrique) : *solide* = contact physique, *détect.* = vu par
`eng_physics_overlaps` seulement, aucune force.

| | STATIC | DYNAMIC | CHARACTER | HITBOX | TRIGGER |
| --- | --- | --- | --- | --- | --- |
| **STATIC** | – | solide | solide | – | – |
| **DYNAMIC** | solide | solide | solide | détect. | détect. |
| **CHARACTER** | solide | solide | solide | détect. | détect. |
| **HITBOX** | – | détect. | détect. | – | – |
| **TRIGGER** | – | détect. | détect. | – | – |

**Pourquoi une hitbox ne pousse jamais un personnage** (trois garde-fous indépendants) :

1. seules les paires *solides* sont déclarées au moteur de collision de Jolt : HITBOX et TRIGGER n'ont aucune paire, Jolt ne
   calcule donc aucun contact avec eux ;
2. leurs corps sont créés *capteurs* (`IsSensor`) dans Jolt ;
3. ils sont cinématiques : aucune gravité, aucune réponse aux forces ; on les déplace avec `eng_physics_move_kinematic`.

**Détection** : `eng_physics_overlaps(world, body, out, max)` teste le volume de `body` contre tous les autres corps
(requête de collision de forme de Jolt) et ne garde que ceux dont la couche interagit avec la sienne (matrice
ci-dessus). Résultat trié par identifiant croissant (déterministe), nombre total renvoyé même si `max` est plus petit,
au plus 256 corps examinés. La requête est faite au moment de l'appel (état courant des corps) : pas de rappel
asynchrone, pas de dépendance au nombre de fils. M5-07 (détection des coups) s'appuiera dessus.

## Réglages de compilation de Jolt

Fixés dans `third_party/CMakeLists.txt` (option `OW_PHYSICS`) :

- **SSE4.1/4.2 seulement** (pas d'AVX, AVX2, FMA, F16C, LZCNT/TZCNT) : le binaire tourne sur tout x86-64 raisonnable et les
  calculs en virgule flottante ne dépendent pas de la présence de FMA. À réexaminer si le profilage (M4-05/06) montre
  que la physique coûte cher.
- pas de compute shaders (corps mous GPU), pas d'exemples ni de tests de Jolt, pas d'interprocedural optimisation ;
- le moteur de rendu de debug de Jolt est présent (utilisé par M4-04, tracé des formes).
- Jolt est compilé avec les réglages de la configuration (Debug : `-O0`, sans ASan : les sanitizers ne s'appliquent qu'à
  notre code, comme pour SDL). La physique est donc lente en Debug (voir « Limites »).

`OW_PHYSICS=OFF` : rien n'est téléchargé ni compilé ; `eng_physics_available()` renvoie faux, `eng_physics_create`
renvoie `NULL` (journalisé), les autres fonctions sont sans effet.

## Comportements de Jolt à connaître

- **Amortissement par défaut** : chaque corps dynamique a un amortissement linéaire (0,05 /s) et angulaire (0,05 /s).
  Une chute libre d'une seconde atteint donc −9,56 m/s et non −9,81 m/s. Les tests comparent à la récurrence exacte du
  pas de Jolt (`v ← (v + g·dt)·(1 − 0,05·dt)`, `x ← x + v·dt`, précision 1e-3) et à la loi continue avec traînée
  (précision 0,03).
- **Endormissement** : un corps immobile s'endort (`eng_physics_body_is_active` renvoie faux) ; il se réveille au contact.
- **Fils** : le résultat d'un pas ne dépend pas du nombre de fils de travail (testé avec 0, 1 et 3 : positions identiques
  au bit près). `worker_threads = 0` : le fil appelant fait le travail ; joltc exige au moins un fil de pool, donc 1 fil
  est créé dans ce cas (le fil appelant participe aussi).
- **Mémoire** : Jolt alloue sur le tas ; tout est libéré par `eng_physics_destroy`. Jolt a un état global (fabrique de
  types, allocateur temporaire) initialisé par le premier monde et libéré avec le dernier (plusieurs mondes
  coexistent ; testé).

## Tests

- `engine.physics` (12) : création / destruction, `NULL` accepté, deux mondes ; arguments invalides rejetés (rayon,
  demi-côtés, `NaN`, couche, pas) ; corps inconnu sans effet et sans modifier la sortie ; retrait ; chute libre (récurrence
  exacte et loi continue) ; sphère qui se pose sur un sol et s'endort ; statique immobile ; gravité et vitesse initiale
  personnalisées ; même résultat à 0 / 1 / 3 fils ; capacité de paires ; monde plein signalé.
- `engine.physics_layers` (13) : la matrice égale une copie indépendante de la documentation et est symétrique ; un capteur
  n'est jamais solide ; noms ; **une hitbox de 1 × 2 × 1 m traversant un personnage à 10 m/s ne le déplace pas de 1e-4 m
  (et le détecte)**, tandis que, en contrôle, une sphère dynamique de même trajectoire le pousse de plus de 0,5 m ; une zone
  TRIGGER laisse tomber un corps à travers (et le voit passer) ; une hitbox n'est pas un sol ; un personnage frappé en
  haut reste droit (|w| = 1 à 1e-5) alors que le même poteau dynamique bascule ; filtrage de `eng_physics_overlaps`
  par la matrice sur une scène où tout se recouvre (tri, comptage seul, troncature, plafond de 256) ; corps éloignés,
  inconnus, `NULL` ; `eng_physics_move_kinematic` (cible atteinte, pas de dérive, entrées invalides, seuls les capteurs) ;
  couche d'un corps, validation des capsules.
- `physics.isolation` : aucun symbole de Jolt hors de `physics.c`.

## Collisions du monde (M4-03)

- **Terrain** : `eng_physics_shape_heightfield(world, &heightmap)` crée un HeightField Jolt à partir des mêmes
  échantillons 16 bits que le fichier `.hmap` (aucune quantification supplémentaire). Les sommets sont donc exacts ; testé
  en 65² (rampe) et en 1025² (taille par défaut, impaire : acceptée telle quelle par Jolt).
- **Rochers, murs** : `eng_physics_shape_mesh` (positions + indices) et `eng_physics_shape_model` (un `EngModel`).
  Les triangles dégénérés sont retirés. Jolt ignore les **faces arrière** des maillages pour une sphère qui les aborde :
  l'enroulement des triangles doit être sortant (c'est le cas de `eng_rock_build`).
- `eng_physics_add_static_shape(world, shape, pose)` place une forme (couche `STATIC`, `pose` NULL = identité).
  `eng_physics_shape_destroy` libère la forme ; les corps déjà créés restent valides (Jolt compte les références).
- **Rayon** : `eng_physics_raycast(world, origin, direction, max_distance, layer_mask, &hit)` renvoie la touche la plus
  proche (corps, distance, point, normale). Le masque est un champ de bits sur `EngPhysicsLayer` ; `ENG_LAYERS_SOLID`
  exclut les capteurs, `ENG_LAYERS_ALL` les inclut. Direction non normalisée acceptée ; entrées invalides rejetées.
- **Tailles impaires** : Jolt arrondit le nombre d'échantillons à un multiple de son bloc (65 -> 66, 33 -> 34) et marque les
  échantillons ajoutés « sans collision » : vérifié, il n'y a aucune surface au-delà du bord du terrain (rayon à 64,5 m sur
  un champ de 65). `JPH_HeightFieldShape_GetSampleCount` renvoie en revanche le nombre arrondi : le debug draw saute les
  échantillons sans collision.
- **Diagonales** : Jolt coupe chaque cellule selon la même diagonale, le rendu alterne les deux (damier). Au centre
  d'une cellule l'écart entre les deux surfaces vaut au plus |a + d − b − c| / 2 ; il est nul aux sommets. Cette borne
  n'a **pas** été mesurée sur le terrain réel du projet (le test vérifie seulement que le centre vaut (a+d)/2 ou
  (b+c)/2 sur un champ aléatoire 1025²).

Tests : `engine.physics_world` (15) — rampe (hauteur au rayon = surface du rendu), normale et distance, entrées invalides,
masque de couches (un capteur n'est vu qu'avec `ENG_LAYERS_ALL`), champ 1025² (sommets exacts, centres de cellules),
et le critère M4-03 : une sphère lâchée en haut d'une pente de 0,3 roule vers un rocher de rayon 2,5 m posé sur la pente ;
sans le rocher elle passe à moins de 1 m de son centre, avec lui elle reste à plus de 1,5 m, sans jamais s'enfoncer de
plus de 5 cm dans le terrain ; validation des paramètres (champ, maillage, pose), rayon oblique (point et distance exacts),
altitude minimale non nulle, champ 4097² (le plus grand accepté), et un test qui écrase la pile après la création du monde
(le pointeur de callbacks gardé par joltc ne doit pas viser une variable locale ; efficace hors ASan).

## Debug draw (M4-04)

`eng_physics_debug_draw(world, &lines, &options)` ajoute à une liste `EngDebugLines` (M1-11) les formes que Jolt simule.
La forme et la pose sont relues dans Jolt à chaque appel : il n'y a pas de copie parallèle qui pourrait se décaler du
monde simulé.

| couche    | couleur  | | forme    | dessin                                                           |
|-----------|----------|-|----------|------------------------------------------------------------------|
| STATIC    | gris     | | sphère   | 3 cercles                                                        |
| DYNAMIC   | vert     | | boîte    | 12 arêtes orientées                                              |
| CHARACTER | cyan     | | capsule  | 2 sphères aux bouts du cylindre + 4 génératrices                 |
| HITBOX    | rouge    | | terrain  | arêtes + diagonale a-d de chaque cellule, dans une fenêtre       |
| TRIGGER   | jaune    | | maillage | boîte englobante (Jolt l'élargit un peu pour une surface plate)  |

- Un corps DYNAMIC ou CHARACTER **endormi** est dessiné à 40 % d'intensité. Un décor ou un capteur cinématique est toujours
  « inactif » pour Jolt sans que cela signifie quelque chose : ils ne sont jamais atténués.
- `layer_mask` choisit les couches (`ENG_LAYERS_ALL`, ou `1U << couche`). `radius` > 0 ne garde que les corps dont la boîte
  englobante est à moins de `radius` de `focus` (hauteur ignorée pour un terrain, qui reste visible quand le foyer le
  survole). `radius` = 0 : tous les corps, le terrain en boîte englobante seule.
- Fenêtre du terrain : `radius` cellules de chaque côté du foyer, **au plus 64 cellules de côté** (`ENG_PHYSICS_DEBUG_MAX_HF_CELLS`),
  soit jusqu'à ~12 000 lignes ; au-delà des capacités de la liste, la forme n'est pas ajoutée (compteur `dropped`) plutôt que
  dessinée à moitié. La diagonale tracée est celle de Jolt : elle permet de **voir** l'écart avec la diagonale alternée du rendu.
- Le dessin est un coût CPU (relecture de chaque corps) à n'activer qu'en debug ; il n'alloue rien (le tampon
  d'identifiants est créé avec le monde : 4 octets par corps possible).

Tests : `engine.physics_debug` (20) — couleurs distinctes ; sphère (centre, rayon, couleur, segments transmis) ; boîte
(8 coins exacts) et **boîte inclinée sur une pente : chaque sommet dessiné est un coin `pos + R × demi-côtés` avec la
rotation lue dans la simulation** ; capsule (hauteur totale, rayon) ; masque de couches et couleur de chaque couche ;
atténuation des corps endormis (et pas des décors) ; rayon et foyer ; terrain (fenêtre exacte de 11 × 11, troncature au bord,
plafond de 64 cellules, sommets sur la surface de collision, boîte englobante sans rayon) ; **le milieu de chaque diagonale
tracée est à la hauteur que le lancer de rayon trouve au centre de la cellule** ; terrain incliné (le foyer est retrouvé
en coordonnées locales avec sa hauteur), pas de 2 m (fenêtre en cellules), plafond de 64 cellules, rayon inclusif et à trois
dimensions pour les corps ordinaires ; terrain tourné et déplacé ; maillage en
boîte englobante ; arguments invalides ; forme qui ne tient pas dans la liste ; corps retiré.

## Pas fixe (M4-05)

La physique avance par pas **identiques** : `eng_physics_advance(world, &loop, dt_image)` consomme la durée de l'image avec la
boucle `EngLoop` du moteur (60 Hz, temps accumulé en entiers) et exécute les `eng_physics_step(loop.tick_seconds)` dus. Après
la même durée réelle le même nombre de pas a été simulé, avec le même `dt` : l'état est le même quel que soit le rythme
d'affichage. Une image trop longue (débogueur, fenêtre déplacée) est plafonnée par la boucle (0,25 s = 15 pas) : la simulation
ralentit au lieu de rattraper.

- **Rendu** : `eng_physics_body_interpolated(world, body, loop.alpha, pos, rot)` donne la position (lerp) et la rotation (slerp)
  entre l'avant-dernier et le dernier pas. `alpha` = 0 et 1 rendent exactement l'état d'avant le pas et l'état courant.
  Un corps créé après le dernier pas, un emplacement réutilisé et un décor (STATIC) rendent leur état courant (jamais l'état
  d'un ancien corps). Coût : chaque pas relit les corps non statiques (`O(corps)`), à surveiller au-delà de quelques
  dizaines de milliers.
- **Déterminisme** : mesuré, pas supposé. Mêmes états bit à bit à 30, 60, 144, 240, 17 et 1000 images par seconde, avec des
  images de durées aléatoires, et avec 1 ou 3 fils de calcul (une scène : une rampe, 4 boîtes et 2 sphères qui roulent et se
  heurtent pendant 4 s = 240 pas). Il vaut pour **ce binaire sur cette machine** : Jolt ne garantit pas les mêmes bits d'un
  compilateur ou d'un processeur à l'autre (voir la limite sur la relecture de rejeu, M5-18).
- **Contrôle** : un pas variable (un `eng_physics_step` par image avec `dt` = durée de l'image) donne des états différents à
  30 et à 144 images par seconde : c'est ce que la boucle évite.
- **Branchement au jeu** : pas encore fait. Le jeu est une bibliothèque rechargeable qui ne lie pas le moteur (voir
  `game_api.h`) ; le monde physique y sera exposé comme un service avec le contrôleur de personnage (M4-06), qui en a besoin.
  Le lanceur a déjà la boucle 60 Hz : `eng_physics_advance` s'y branche en une ligne.

Tests : `engine.physics_fixed` (9) — états bit à bit identiques à six rythmes ; images irrégulières et 3 fils ; contrôle à pas
variable ; image trop longue plafonnée à 15 pas ; arguments invalides sans consommer de temps, pas de 0,1 s accepté ; interpolation
(positions, alpha invalides, sorties NULL) ; slerp des rotations ; corps neufs, emplacements réutilisés ; décors.

## Contrôleur de personnage (M4-06)

`EngCharacter` enveloppe le `CharacterVirtual` de Jolt : une capsule qui se déplace en **balayant**
son volume (pas de corps rigide poussé par la gravité). Le contrôleur gère sa vitesse verticale
(gravité du monde, saut). À chaque pas fixe, après `eng_physics_step` :
`eng_character_update(c, dt, move)` avec `move` = vitesse horizontale voulue (m/s, monde).

- Position = les **pieds**. Défauts : rayon 0,3 m, hauteur 1,8 m, pente max 45°, marche 0,3 m, 70 kg.
- `update` : vitesse du sol (plateformes) si debout, gravité, `CancelVelocityTowardsSteepSlopes`,
  puis `ExtendedUpdate` (glisse le long des obstacles, enjambe les marches, colle au sol en descente
  jusqu'à 0,5 m).
- Le corps intérieur (couche CHARACTER, `eng_character_body`) est vu par les hitbox/zones, compte dans
  `eng_physics_body_count`, et le debug draw le dessine en cyan.
- Le monde doit survivre à ses personnages (une erreur est journalisée sinon).

Comportements mesurés (tests `character.*`), à connaître pour M4-07 et suivants :

- **Marche franchissable réelle ≈ 0,39 m** avec `step_height` = 0,3 : le bas de la capsule est arrondi,
  un appui à moins de 45° de la verticale compte comme sol, soit `step_height + rayon x (1 - cos pente)`.
  Une marche de 0,45 m bloque, 0,30 m passe.
- **Vitesse le long du sol** : Jolt projette la vitesse voulue sur le plan de la pente, donc la vitesse
  horizontale baisserait de cos² (angle) en montée (-25 % à 30°). `eng_character_update` prend donc la
  composante tangente de la vitesse voulue et lui rend sa longueur : la vitesse **le long du sol** est
  celle demandée, en montée comme en descente. Debout seulement (pas pendant un saut, pas sur un sol
  trop raide). La gravité n'est pas ajoutée à la vitesse tant que le personnage marche au sol (sinon la
  descente allait ~9 % plus vite).
- Le corps intérieur est décalé de 2 cm vers le haut (`characterPadding` de Jolt) : normal.
- Pente à 40° franchie, 50° refusée (limite 45°) ; aucun passage à travers un mur de 5 cm à 30 m/s
  et 60 Hz (0,5 m par pas).
- Résultat bit à bit identique d'une exécution à l'autre et quel que soit le regroupement des pas
  (même réserve que M4-05 : même binaire, même machine).

## Limites

- Jolt est compilé en `-O0` dans le preset `debug` : les gros mondes (un terrain 1025²) y seront lents ; le preset
  `release` règle cela. Aucune mesure de temps sur un terrain 1025² n'a été faite : le test M4-03 n'y exécute pas de simulation.
- Un personnage est ici un corps rigide (capsule à rotation verrouillée) : le contrôleur « cinématique » de M4-06
  (`CharacterVirtual`) viendra le remplacer pour le jeu ; la couche `CHARACTER` et la matrice restent valables.
- `eng_physics_overlaps` teste la forme du corps interrogé contre le monde à l'instant de l'appel : un corps très rapide
  qui traverse un capteur entre deux appels n'est pas vu (balayage continu : M5-07, qui balaiera la hitbox entre deux
  images).

## Balayage de sphère (M4-11)

`eng_physics_spherecast(world, origin, direction, max_distance, radius, layer_mask, &hit)` : premier
corps touché par une sphère dont le centre va de `origin` vers `direction`. `hit->distance` est la
course du **centre** (un rayon `radius` avant la paroi), `point` le contact, `normal` unitaire et
orientée vers la sphère. Démarrer en contact : distance 0 ; démarrer dans un solide convexe fermé :
aucun contact (faces arrière ignorées). `radius` ∈ ]0 ; 10]. Sans Jolt : toujours `false`.

### Marches à vitesse de jeu (correctif découvert par le mode jouable)

À 3,5 m/s et plus, la capsule butait sur l'arête d'une marche : Jolt la faisait glisser le long du contact
et lui laissait une vitesse verticale de +2 à +4 m/s, le personnage décollait et décrivait un arc de 30 cm.
Les tests M4-06 marchaient à 1,5 m/s et ne le voyaient pas. `eng_character_update` supprime désormais
toute vitesse verticale montante qui ne vient pas d'un saut (`eng_character_jump`/`launch`) quand le
personnage n'est pas au sol après le pas. Vérifié de 1,5 à 7,2 m/s sur des marches de 0,15 et 0,30 m
(test `steps_do_not_launch_the_character_at_game_speeds`). Limite : à 12 m/s, rebond résiduel de 0,19 m.
