# Formats binaires des assets (M2-01)

Les assets du jeu sont **cuisinés** hors ligne (`tools/assetc`, M2-02) depuis les sources (glTF, PNG...)
vers quatre formats maison, lus en place par le moteur : `.mesh`, `.tex`, `.skel`, `.anim`.
Décision et alternatives : [ADR 0006](adr/0006-format-binaire-des-assets.md).
Code de référence du conteneur : `engine/include/engine/asset_format.h` + `engine/src/asset_format.c`
(tests : `tests/test_asset_format.c`).

Statut des formats : **`.mesh` et `.tex` : stables en v1** (alignés sur `EngModel` et `EngImage`, M2-02 les
produit). **`.skel` : produit et lu depuis M4-12** (`assetc skel`, `eng_skeleton_load`), version 1, validé à la
lecture (§5). **`.anim` : produit et lu depuis M4-13** (`assetc anim`, `eng_anim_load`), version 1, validé à la lecture (§6).

## 1. Règles communes

| Sujet | Règle |
|---|---|
| Boutisme | **Petit-boutiste**, partout. Un `#error` interdit de compiler sur une machine grand-boutiste. Toutes les valeurs sont lues en place, sans conversion. |
| Flottants | IEEE 754 binaire32, petit-boutiste. |
| Alignement | Chaque section commence à un **multiple de 16 octets** depuis le début du fichier (le tampon de `eng_file_read_all` est aligné sur 16 : les sections le sont donc en mémoire). Remplissage : octets à zéro, **non vérifié** au chargement. |
| Chemins | Chaînes UTF-8 terminées par `\0`, séparateur `/`, **relatifs au dossier du fichier qui les contient** (un `.mesh` désigne ses `.tex` à côté de lui). Rangées dans la section `STRS`, désignées par leur offset. |
| Références entre assets | Par chemin (chaîne), jamais par indice global : le gestionnaire d'assets (M2-05) décide des identifiants. |
| Déterminisme | Mêmes entrées → mêmes octets (remplissage à zéro, pas d'horodatage, pas de chemin absolu). Le cuiseur s'en sert pour ne pas recuire inutilement. |
| Limites | Fichier < 4 Gio (`file_size` et offsets sur 32 bits), 64 sections au plus. |

## 2. Conteneur (commun aux quatre formats)

```
offset 0    En-tête           32 octets
offset 32   Table des sections 16 octets × section_count
...         Sections          chacune alignée sur 16, dans l'ordre de la table
```

### En-tête (32 octets)

| Offset | Taille | Champ | Valeur / règle |
|---|---|---|---|
| 0 | u32 | `magic` | `OWMH` (.mesh), `OWTX` (.tex), `OWSK` (.skel), `OWAN` (.anim) : quatre caractères ASCII, 1er caractère à l'offset 0 |
| 4 | u32 | `endian_tag` | `0x01020304` (octets `04 03 02 01` dans le fichier) |
| 8 | u16 | `version` | doit être **égale** à la version que le moteur lit pour ce type |
| 10 | u16 | `header_size` | 32 |
| 12 | u32 | `flags` | 0 (réservé) |
| 16 | u32 | `file_size` | taille exacte du fichier, remplissage final **non** compté (la dernière section n'est pas complétée) |
| 20 | u32 | `section_count` | 0 à 64 |
| 24 | u32 ×2 | `reserved` | 0 |

### Entrée de la table (16 octets)

| Offset | Taille | Champ | Règle |
|---|---|---|---|
| 0 | u32 | `tag` | quatre caractères ASCII, non nul, **unique** dans le fichier |
| 4 | u32 | `offset` | depuis le début du fichier ; multiple de 16 |
| 8 | u32 | `size` | octets, sans remplissage ; peut valoir 0 |
| 12 | u32 | `count` | nombre d'éléments ; sens propre à chaque section ; **non vérifié** par le conteneur |

### Vérifications au chargement (`eng_asset_open`)

Dans cet ordre ; la première qui échoue donne le statut renvoyé et un message dans le journal.

| # | Vérification | Statut |
|---|---|---|
| 1 | pointeur non nul et taille ≥ 32 | `TOO_SMALL` |
| 2 | `magic` = celui du type demandé | `BAD_MAGIC` |
| 3 | `endian_tag` = `0x01020304` | `BAD_ENDIAN` |
| 4 | **`version` = version lue par le moteur** | `VERSION_OLDER` (fichier plus ancien : « recuire l'asset ») ou `VERSION_NEWER` (« moteur trop ancien ») |
| 5 | `header_size` = 32, `flags` = 0, `reserved` = 0 | `BAD_HEADER` |
| 6 | `file_size` = taille reçue (détecte troncature et octets en trop) | `SIZE_MISMATCH` |
| 7 | `section_count` ≤ 64 et table entièrement dans le fichier | `BAD_TABLE` |
| 8 | pour chaque section, dans l'ordre : `tag` ≠ 0 | `BAD_TAG` |
| 9 | `tag` absent des sections précédentes | `DUPLICATE_TAG` |
| 10 | `offset` multiple de 16 | `SECTION_ALIGN` |
| 11 | `offset` ≥ fin alignée de la table ou de la section précédente (offsets croissants, sans chevauchement) | `SECTION_OVERLAP` |
| 12 | `offset + size` ≤ taille du fichier (calculé en 64 bits) | `SECTION_RANGE` |

**Pourquoi la version est jugée avant le reste.** Un format futur peut changer l'en-tête lui-même : on doit
dire « moteur trop ancien », pas « en-tête invalide ». Seuls `magic` et `endian_tag` (stables pour toujours)
passent avant.

**Politique de version.** Un seul entier par type de fichier, **égalité exacte** exigée, pas de
compatibilité ascendante ni descendante : changer la disposition d'une section ou en ajouter une
obligatoire = incrémenter la version et recuire (`tools/assetc` régénère tout, c'est automatique). On
n'écrit pas de code de migration : les assets sont dérivés des sources, jamais l'inverse. Une section
**facultative** ajoutée sans changer le sens des autres n'impose pas d'incrémenter (les lecteurs ignorent
les étiquettes inconnues).

**Ce que la vérification garantit.** Un fichier accepté peut être parcouru sans lecture hors du tampon.
Elle ne garantit **pas** que le contenu des sections est cohérent (nombres de sommets, indices dans les
bornes...) : c'est le rôle du lecteur de chaque type (M2-02 / M2-05), qui doit vérifier `size` contre
`count × taille d'élément` avant de lire.

## 3. `.mesh` — version 1 — magic `OWMH`

Géométrie + matériaux d'un modèle, dans la même disposition que `EngModel` (`engine/model.h`) pour un
chargement sans conversion.

| Tag | Obligatoire | Contenu | `count` |
|---|---|---|---|
| `INFO` | oui | 56 octets, voir ci-dessous | 1 |
| `VERT` | oui | `EngModelVertex[]`, 48 octets chacun | nombre de sommets |
| `INDX` | oui | indices : `u16[]` si `index_size` = 2, `u32[]` si 4 | nombre d'indices |
| `PART` | oui | parties dessinables, 40 octets chacune | nombre de parties |
| `MATL` | oui | matériaux, 64 octets chacun ; le matériau 0 est le matériau par défaut | nombre de matériaux |
| `TREF` | non | références de textures, 8 octets chacune | nombre de références |
| `STRS` | non | chaînes (chemins des textures) | nombre d'octets |

**`INFO`** (8 × u32, puis 6 × f32) :
`vertex_count`, `index_count`, `index_size` (2 si `vertex_count` ≤ 65 536, sinon 4), `part_count`,
`material_count` (≥ 1), `texture_ref_count`, `flags` (0), `reserved` (0), `bounds_min[3]`, `bounds_max[3]`
(boîte englobante de tout le modèle, repère du modèle).

**`VERT`** — `EngModelVertex`, 48 octets : `position` f32[3] @0, `normal` f32[3] @12, `uv` f32[2] @24,
`tangent` f32[4] @32 (xyz + signe de la bitangente dans w). Aucun remplissage.

**`PART`** — 40 octets : `first_index` u32, `index_count` u32 (multiple de 3), `material` u32 (indice dans
`MATL`), `flags` u32 (0), `bounds_min` f32[3], `bounds_max` f32[3] (par partie : sert au culling dès M3).

**`MATL`** — 64 octets (matériau glTF métallique-rugueux) :

| Offset | Type | Champ |
|---|---|---|
| 0 | f32[4] | `base_color` (facteur, linéaire) |
| 16 | f32 | `metallic` |
| 20 | f32 | `roughness` |
| 24 | f32 | `normal_scale` |
| 28 | f32 | `occlusion_strength` |
| 32 | f32[3] | `emissive` |
| 44 | i32 | `base_color_tex` |
| 48 | i32 | `metallic_roughness_tex` |
| 52 | i32 | `normal_tex` |
| 56 | i32 | `occlusion_tex` |
| 60 | i32 | `emissive_tex` |

Les `*_tex` sont des **indices dans `TREF`**, ou −1 pour « pas de texture ».

**`TREF`** — 8 octets : `path_offset` u32 (offset d'une chaîne `\0`-terminée dans `STRS`), `flags` u32
(bit 0 : la texture est lue en **sRGB**). Le cuiseur le fixe d'après le rôle : couleur de base et émissive en
sRGB, normale, occlusion et métal-rugosité en linéaire. Le chemin désigne un `.tex`.

**Chemin sûr** (vérifié à l'écriture comme à la lecture) : relatif, séparateur `/` uniquement, aucun composant
vide, `.` ou `..`, aucun `\`, `:` ni caractère de contrôle, 255 caractères au plus. Un `.mesh` ne peut donc pas
sortir de son dossier.

**Contrôles du lecteur** (`eng_mesh_decode`) : `count` et `size` de chaque section cohérents avec `INFO` ;
indices < `vertex_count` ; chaque partie dans `INDX`, `index_count` multiple de 3 ; indices de matériau et de
texture existants ; facteurs et boîtes finis ; `TREF`/`STRS` absents si aucune texture. Non vérifié : que
chaque coordonnée de sommet soit finie (coût proportionnel au modèle, sans bénéfice hors fichier corrompu).
`TREF` et `STRS` ne sont écrits que s'il y a des textures. Au plus 4096 textures.

**Une image, deux rôles.** Un `.mesh` désigne des *textures* (image + espace de couleur), pas des images :
une image glTF servant à la fois en couleur et en normale donne deux `.tex`, chacun avec ses mips calculés
dans le bon espace. `eng_mesh_plan` établit cette liste (ordre de première utilisation : couleur de base,
métal-rugosité, normale, occlusion, émissif, matériau par matériau) et nomme les fichiers `<stem>.<k>.tex`.

## 4. `.tex` — version 1 — magic `OWTX`

Image prête pour le GPU : texels déjà dans leur format final, mips déjà calculés (plus de génération au
chargement).

| Tag | Obligatoire | Contenu | `count` |
|---|---|---|---|
| `INFO` | oui | 32 octets, voir ci-dessous | 1 |
| `MIPS` | oui | `{offset u32, size u32}` par sous-ressource, 8 octets | `layer_count × mip_count` |
| `DATA` | oui | octets des texels de toutes les sous-ressources | nombre d'octets |

**`INFO`** (8 × u32) : `format`, `width`, `height`, `mip_count` (≥ 1), `layer_count` (≥ 1 ; 6 pour un cube),
`flags` (bit 0 : cube map), `reserved` ×2 (0). `layer_count` ≤ 256 ; 6 si cube. La chaîne de mips est complète (jusqu'à 1×1).

**`format`** — énumération **propre au fichier**, stable pour toujours (ne pas la déduire de `SDL_GPUTextureFormat`,
dont les valeurs peuvent changer d'une version de SDL) :

| Valeur | Format | Octets par bloc (taille de bloc) |
|---|---|---|
| 1 | `RGBA8_UNORM` | 4 (1×1) |
| 2 | `RGBA8_SRGB` | 4 (1×1) |
| 3 | `BC7_UNORM` | 16 (4×4) |
| 4 | `BC7_SRGB` | 16 (4×4) |
| 5 | `BC5_UNORM` | 16 (4×4) |
| 6 | `BC4_UNORM` | 8 (4×4) |
| 7 | `RGBA16F` | 8 (1×1) |
| 8 | `R8_UNORM` | 1 (1×1) |

**État d'usage (M2-03).** `assetc` produit **`BC7_SRGB`** (couleurs) ou **`BC7_UNORM`** (données : normales,
métal/rugosité, occlusion) par défaut ; `--raw` conserve `RGBA8_*`. Les formats `RGBA8_*` et `BC7_*` se
convertissent en `EngImage` (`eng_tex_to_image`) ; `BC5`, `BC4`, `RGBA16F` et `R8` sont validés (tailles de
blocs) mais refusés à la conversion (message « RGBA8 et BC7 seulement »). Une texture BC7 doit avoir une seule
couche et ne pas être un cube.

- **Mips.** Pour BC7, les mips cuits sont **consommés tels quels** : l'`EngImage` porte toute la chaîne
  compressée (`encoding`, `mip_count`) et `model_gpu` la téléverse sans rien regénérer. Les mips sont calculés
  avant compression, en lumière linéaire pour les textures sRGB. Pour `RGBA8_*`, le chargeur ne garde que le
  niveau 0 et le renderer regénère les mips (comportement M2-02).
- **Rôle et espace de couleur.** Le format GPU d'un BC7 est fixé par le fichier. Un `BC7_SRGB` utilisé comme
  donnée, ou un `BC7_UNORM` utilisé comme couleur, est **refusé** au chargement du modèle (erreur, pas de
  conversion silencieuse).
- **Encodeur.** bc7enc (richgel999, commit `f66c2e4`, récupéré par `FetchContent` à la configuration, donc
  réseau nécessaire au premier `cmake --preset`). Il n'emploie que les modes 1 et 6 pour les blocs opaques et
  5, 6, 7 avec transparence : moins fin qu'un encodeur à 8 modes. Erreur perceptuelle (poids 128/64/16/32)
  pour le sRGB, RGB uniforme pour les données. Sortie déterministe.
- **Réduction mesurée.** Chaîne BC7 contre chaîne RGBA8 : **3,9998×** au minimum sur une texture 64×64 (les
  petits niveaux sont arrondis au bloc de 16 octets : 5 488 octets contre 21 844), 4,00× arrondi sur Sponza
  (95 087 040 contre 380 283 556 octets pour 69 textures). L'objectif « ≥ 4× » est donc atteint à l'arrondi,
  pas strictement.
- **Qualité mesurée (Sponza, niveau 0, RGB).** PSNR moyen 43,3 dB sur 69 textures ; plus mauvaise 25,1 dB
  (`sponza.68`), puis 31,4 dB. La cause de la valeur la plus basse n'a pas été analysée (hypothèse : contenu à
fort contraste local, limité aux modes 1 et 6 de bc7enc).
- **Coût.** Cuisson de Sponza : environ 73 s sur un seul fil (release). Chargement cuit contre glTF :
  87 ms contre 1 207 ms (13,8×).
- **BC5 reporté.** Les normales restent en BC7 linéaire : BC5 occupe aussi 16 octets par bloc (aucun gain de
  mémoire) et exigerait de reconstruire `z` dans le shader.

**`MIPS`** — une entrée par sous-ressource, rangées **couche par couche, niveau par niveau** (indice =
`layer × mip_count + level`, niveau 0 = pleine taille). `offset` est relatif au début de `DATA`, multiple de 16 ;
`size` = nombre de blocs en largeur × en hauteur × octets par bloc du niveau (dimensions du niveau :
`max(1, width >> level)`, arrondies au bloc supérieur pour les formats compressés).

## 5. `.skel` — version 1 — magic `OWSK`

Squelette : hiérarchie d'articulations et matrices inverses de la pose de liaison.

| Tag | Obligatoire | Contenu | `count` |
|---|---|---|---|
| `INFO` | oui | `joint_count` u32, `reserved` ×3 (0) | 1 |
| `JNTS` | oui | articulations, 48 octets chacune | nombre d'articulations |
| `IBMS` | oui | matrices inverses de liaison, 64 octets chacune (f32[16], colonnes d'abord) | nombre d'articulations |
| `STRS` | non | noms des articulations | nombre d'octets |

**`JNTS`** — 48 octets : `parent` i32 (−1 pour une racine ; un parent précède **toujours** ses enfants,
donc une seule passe suffit pour composer les poses), `name_offset` u32 (dans `STRS`, ou 0xFFFFFFFF si
absent), `translation` f32[3], `rotation` f32[4] (quaternion x,y,z,w), `scale` f32[3] — pose de repos locale.

Validation à la lecture (`eng_skeleton_decode`, tests dans `tests/test_skeleton.c`) : 1 à 256 articulations ;
`INFO`, `JNTS` et `IBMS` de tailles et de comptes cohérents ; champs réservés nuls ; parent = −1 ou indice
strictement inférieur à celui de l'articulation ; valeurs finies ; quaternion unitaire (|q|² à 2·10⁻³ près) ;
échelle non nulle ; noms ≤ 63 octets, dans `STRS`, qui se termine par un zéro. Le fichier produit est
déterministe (mêmes entrées, mêmes octets). Voir `docs/skeleton.md`.

## 6. `.anim` — version 1 — magic `OWAN`

Un clip d'animation : des canaux, chacun animant la translation, la rotation ou l'échelle d'une articulation
d'un `.skel` (mêmes indices, parents d'abord). Code : `engine/include/engine/anim_clip.h`, tests :
`tests/test_anim.c`, guide : `docs/animation-clips.md`.

| Tag | Obligatoire | Contenu | `count` |
|---|---|---|---|
| `INFO` | oui | `duration` f32 (secondes), `channel_count` u32, `skeleton_path_offset` u32 (dans `STRS`), `reserved` u32 (0) | 1 |
| `CHAN` | oui | canaux, 16 octets chacun | nombre de canaux |
| `TIME` | oui | instants des clés, `f32[]`, strictement croissants dans chaque canal | nombre total de clés |
| `VALS` | oui | valeurs des clés : f32[3] (T, S) ou f32[4] (R) par clé | nombre total de clés |
| `STRS` | oui | chemin du `.skel` visé, terminé par zéro | nombre d'octets |

**`CHAN`** — 16 octets : `joint` u32 (indice dans le `.skel`, 0 à 255), `path` u32 (0 translation, 1 rotation,
2 échelle), `key_count` u32 (1 à 1 048 576), `first_key` u32 (indice de la première clé du canal dans `TIME` :
somme des `key_count` des canaux précédents). Les canaux sont rangés par `(joint, path)` strictement croissants
(pas de doublon). La première valeur d'un canal dans `VALS` est la somme, sur les canaux précédents, de
`key_count × (3 ou 4)` flottants : chaque clé occupe 3 flottants (translation, échelle) ou 4 (rotation :
quaternion x, y, z, w), dans l'ordre des canaux. (La première version de ce paragraphe disait « se déduit de
`first_key` » : c'est faux dès que les canaux mélangent des valeurs de 3 et de 4 flottants ; aucun fichier
v1 n'avait été produit.)

Validation à la lecture (`eng_anim_decode`) : 1 à 768 canaux ; comptes et tailles de `CHAN`, `TIME`, `VALS`
cohérents entre eux et avec `INFO` (calculs en 64 bits : un nombre de clés absurde est refusé sans
débordement) ; au plus 4 194 304 clés au total ; champ réservé nul ; propriété 0 à 2 ; articulation < 256 ;
canaux rangés et uniques ; instants finis, ≥ 0, strictement croissants et ≤ `duration` ; valeurs finies ;
quaternions unitaires (|q|² à 2·10⁻³ près) ; `STRS` terminée par zéro, chemin non vide de 255 octets au plus.
Le fichier produit est déterministe.

Interpolation : linéaire (translation, échelle) ; sphérique par le plus court chemin (rotation) ; valeur tenue
avant la première clé et après la dernière. Les interpolations glTF STEP et CUBICSPLINE ne sont pas
représentées : l'import les **refuse** (au lieu de les approcher en silence) ; elles viendront avec une
version 2 si le besoin existe.

## 7. Ce que ce document ne couvre pas

- Le **gestionnaire d'assets** et ses identifiants (M2-05), les **bundles** : ils utilisent ce format sans le
  modifier.
- La validation du contenu de `.skel` et `.anim` : décrite aux §5 et §6.
- La compression des fichiers (zstd...) et la signature : non retenues pour l'instant (ADR 0006).
- Le grand-boutiste : non pris en charge (aucune cible du projet).

## 8. Cuiseur `assetc` (M2-02)

Outil en ligne de commande (`build/<preset>/bin/assetc`). Les chemins sont relatifs au **dossier courant**.

| Commande | Effet |
|---|---|
| `assetc mesh <in.gltf\|in.glb> <out.mesh> [--raw] [--bench N] [--skin N] [--depfile f.d]` | écrit `<stem>.<k>.tex` à côté (BC7 par défaut, RGBA8 avec `--raw`) PUIS `<out.mesh>` ; supprime les `<stem>.<k>.tex` périmés d'une cuisson précédente ; affiche les statistiques ; `--bench` compare N chargements glTF à N chargements cuits ; `--skin N` (M4-21) cuit un personnage animé et écrit `<stem>.skin` (§10), incompatible avec `--bench` |
| `assetc tex <in> <out.tex> [--linear] [--raw] [--depfile f.d]` | image → `.tex` BC7 (sRGB par défaut ; `--linear` pour des données) avec chaîne de mips ; `--raw` pour RGBA8 |
| `assetc info <fichier>` | en-tête, sections, contenu validé |

Codes de sortie : 0 succès, 1 échec (fichier ou contenu), 2 usage. La sortie est déterministe. Le chargement
d'un modèle cuit (`eng_mesh_load`) remplit le même `EngModel` que `eng_model_load` : le rendu est identique
par construction, et vérifié pixel à pixel par les tests GPU.

`--depfile f.d` écrit la liste des fichiers lus au format Makefile (`<sortie>: <entrée> <.bin> <images>...`,
espaces, `#` et `$` protégés), que Ninja et make savent lire. Pour un glTF, la liste vient de
`eng_model_dependencies` (tampons et images externes, URI décodées, `data:` ignorées).

## 9. Cuisson au build (M2-04)

`cmake/Assets.cmake` fournit `ow_cook_assets`, utilisée dans `tools/CMakeLists.txt` pour cuire les assets du
jeu dans `build/<preset>/bin/assets/cooked/` (sources dans `assets/`, qui reste copié tel quel) :

```cmake
ow_cook_assets(cooked_assets
  TOOL assetc  OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/assets/cooked  BASE_DIR ${CMAKE_SOURCE_DIR}/assets
  MODELS models/demo.gltf            # -> models/demo.mesh + models/demo.<k>.tex
  TEXTURES textures/checker.png      # -> textures/checker.tex (sRGB) ; LINEAR_TEXTURES pour des données
  [RAW])                             # RGBA8 au lieu de BC7
```

Pour ajouter un asset : l'ajouter à la liste, c'est tout. Un asset est recuit si, et seulement si :

| Changement | Recuit ? | Mécanisme |
|---|---|---|
| source principale (`.gltf`, `.png`) | oui | `DEPENDS` |
| `.bin`, image référencée par le glTF | oui | fichier de dépendances écrit par `assetc --depfile` |
| `assetc` (recompilé, ou ses bibliothèques) | oui | `DEPENDS assetc` |
| sortie supprimée | oui | `OUTPUT` manquant |
| tout autre fichier | non | |

Une seconde build affiche `ninja: no work to do` : aucune cuisson. Garanties et limites :

- **Écriture du `.mesh` en dernier.** Il n'y a pas d'`OUTPUT` pour les `<stem>.<k>.tex` (leur nombre n'est
  connu qu'après lecture du glTF). Le `.mesh` est donc la sortie qui représente la cuisson : tant qu'il
  est absent ou plus ancien qu'une source, tout est refait ; il n'existe jamais sans ses textures.
- **Un `.tex` supprimé seul n'est pas détecté** (supprimer le `.mesh` force la recuisson). Les `.tex`
  périmés, eux, sont supprimés par la cuisson suivante.
- **Fichier de dépendances : Ninja et Makefiles seulement.** Avec un autre générateur (Visual Studio),
  `ow_cook_assets` signale un avertissement et ne suit que la source principale.
- **Chemins absolus** passés à `assetc` : CMake retraduit lui-même le fichier de dépendances pour Ninja ;
  une cible relative ne correspondrait pas à la sortie déclarée et ferait recuire à chaque build.
- **Coût.** La cuisson de la démo prend environ 0,5 s en Debug. Celle de Sponza prend environ 73 s en
  Release (BC7, un seul fil) : elle n'est pas dans les assets du jeu, et ne serait payée qu'une fois
  par modification.

Test : `assets.cook_incremental` (`tests/cook_incremental.cmake`) configure et construit un mini-projet
(`tests/cook_fixture/`, avec une copie d'`assetc` et des sources dans un dossier à espaces) et compte les
cuissons après chaque modification.

## 10. `.skin` — poids de skinning — version 1 — magic `OWSN` (M4-21)

Le `.mesh` v1 ne porte pas de poids. Plutôt que de le faire évoluer (format « stable en v1 »), `assetc mesh
--skin N` écrit un fichier **à côté** : `<stem>.skin`, avec **un `EngSkinVertex` par sommet, dans le même
ordre que les sommets du `.mesh`**. Il est écrit **avant** le `.mesh` (un `.mesh` présent est complet) ; une
cuisson **sans** `--skin` supprime un `.skin` périmé.

| Section | Contenu |
|---|---|
| `INFO` (16 octets) | `u32 vertex_count`, `u32 joint_count` (1 à 256), `u32 reserved[2]` = 0 |
| `WGHT` (`count` = sommets) | 8 octets par sommet : `u8 joints[4]`, `u8 weights[4]` |

Contrôles du lecteur (`eng_skin_decode`) : conteneur (§1), `INFO` et `WGHT` cohérents, `vertex_count` égal à
celui du `.mesh` associé (passé par l'appelant), somme des poids de **chaque sommet = 255**, indice
d'articulation `< joint_count` pour toute influence de poids non nul, indice 0 pour une influence de poids nul.

Les indices sont ceux du **`.skel` cuit du même skin** (la permutation « parents d'abord » est déjà
appliquée) : cuire le squelette et le maillage avec le même `--skin`. Le lien entre `.mesh`, `.skin` et
`.skel` repose sur le **nombre** de sommets et d'articulations ; il n'y a pas de somme de contrôle croisée :
un `.skin` d'un autre personnage de même nombre de sommets serait accepté.
