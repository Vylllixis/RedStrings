# Éclairage basé image (M1-16)

L'environnement (le ciel) éclaire le matériau PBR de M1-15 à la place de la lumière ambiante uniforme,
et se dessine en arrière-plan (skybox). Tout est calculé **sur CPU au chargement** (`engine/ibl.c`) puis
envoyé au GPU (`engine/environment.c`) ; les shaders ne font que lire les résultats.

## Modèle

Pour l'environnement `L(w)` (radiance linéaire HDR, cubemap), l'éclairage indirect d'un point est

```
indirect = ambiante × occlusion × ( c_diff × irradiance(N) + préfiltrée(R, r) × (F0 × A + B) )
```

avec `R = reflect(−V, N)` et :

| Donnée | Définition | Stockage |
|---|---|---|
| `irradiance(N)` | `(1/π) ∫ L(w) max(N·w, 0) dw` (donc un ciel uniforme de radiance 1 donne 1) | cube RGBA16F, 1 niveau |
| `préfiltrée(R, r)` | `L` filtrée par le lobe GGX (`alpha = r²`, `N = V = R`) | cube RGBA16F, niveau de mip `k` = rugosité `k / (n − 1)` |
| `(A, B)(N·V, r)` | LUT BRDF du « split sum » de Karis (2013), visibilité de Smith *height-correlated* (la même que le soleil) | RG16F 64 × 64 |

`ambiante = ambient_color × ambient_intensity` de `EngLighting` est l'**intensité de l'environnement**
(1 : le ciel tel quel). Le soleil de `EngLighting` reste une lumière directionnelle à part : il vient
**en plus** du disque de soleil que peut contenir l'environnement (voir limites).

## Calcul (`engine/ibl.h`)

- **Cubemaps** : faces `+X −X +Y −Y +Z −Z`, convention OpenGL / Vulkan / Direct3D (tableau 8.19 d'OpenGL,
  « vue depuis l'intérieur »), directions dans le monde (Y vers le haut). `eng_ibl_cube_direction` /
  `eng_ibl_cube_face_st` en sont l'implémentation, testée dans les deux sens et **de bout en bout sur
  GPU** (`test_environment_gpu` : chacune des six faces, d'une couleur unie, vue le long de son axe, avec
  une caméra loin de l'origine).
- **Sources** : ciel procédural (`eng_ibl_make_sky` : dégradé, sol, disque de soleil) ou image
  équirectangulaire `.hdr` (`eng_ibl_from_equirect`, convention de three.js : `u = atan2(z, x)/2π + 0,5`,
  `v = asin(y)/π + 0,5`, première ligne = haut). La conversion **sur-échantillonne** (2 à 8 points par
  axe et par texel) : avec un point par texel, un petit soleil très intense changeait d'énergie de plus
  de 30 % (voir « Un défaut trouvé par la comparaison »).
- **Irradiance** : quadrature exacte sur un niveau de mip de 32 × 32 au plus de la source (angle solide
  de chaque texel `(2/n)² / (1 + a² + b²)^{3/2}`).
- **Préfiltrée** : importance sampling GGX avec échantillons filtrés par les mips de la source
  (Křivánek & Colbert, GPU Gems 3). Le niveau `r = 0` est la source ; le niveau `r = 1` est l'irradiance
  (le lobe GGX d'`alpha = 1` avec `N = V` *est* le lobe en cosinus : la quadrature est plus juste que
  quelques centaines d'échantillons). La chaîne s'arrête à 16 × 16 par face.
- **LUT** : 1024 échantillons par texel côté test, 512 côté moteur ; texel `(i, j)` = `N·V = (i + ½)/64`,
  `r = (j + ½)/64`, donc la lecture bilinéaire aux centres est exacte ; le shader borne les
  coordonnées à un demi-texel du bord (son sampler répète les bords).
- **Formats** : `RGBA16F` / `RG16F` (`eng_float_to_half`, arrondi au plus proche pair, testé sur les
  65 536 valeurs), `eng_renderer_create_texture_data` (tous les mips fournis, 1 ou 6 couches).

## Réglages par défaut et mesures

Tailles : ciel 256, irradiance 32, préfiltré 128 (4 niveaux : 128, 64, 32, 16), 512 échantillons. Le calcul
prend **environ 3 s en release et 14 s en debug (ASan)** pour un `.hdr` (mesuré sur `royal_esplanade_1k.hdr`,
cube source + irradiance + préfiltré ; non mis en cache, donc payé à chaque lancement avec `--env`) ;
l'option `--env-samples N` du lanceur le réduit (les tests du lanceur utilisent 16). C'est un coût de
chargement, pas par image.

Erreur du moteur contre une **intégration par force brute sur chaque pixel de l'image** (script
`tools/reference/ibl_brute.py`, lecteur RGBE indépendant de stb), sur `royal_esplanade_1k.hdr` (soleil de
radiance jusqu'à 5·10⁴, 6 directions dont celle du soleil), pire cas relatif sur les trois canaux :

| Donnée | Erreur maximale |
|---|---|
| irradiance | 3,6 % |
| préfiltrée, r = 0,33 | 4,4 % |
| préfiltrée, r = 0,67 | 7,4 % |
| préfiltrée, r = 1 | 3,9 % |

L'erreur à r = 0,67 dépend du nombre d'échantillons (128 : 17 % ; 512 : 7 %) : c'est du bruit
d'échantillonnage face à un soleil 5 000 fois plus brillant que le reste du ciel. Ces valeurs ne
dépendent pas de la taille du cube source (64, 128 ou 256 donnent les mêmes irradiance et préfiltrée à
6 chiffres : le filtrage lit des niveaux de mip de 32 × 32 ou moins, identiques pour toutes ces tailles).

## Un défaut trouvé par la comparaison

Les tests synthétiques (ciel uniforme, dégradé `1 + y`, valeurs de numpy) passaient tous alors que le
casque éclairé par une image réelle était 30 % plus brillant que dans three.js. L'intégration par force
brute a montré que l'irradiance près du soleil était 33 % trop forte : la conversion équirectangulaire →
cube prenait un seul point par texel, et un soleil de quelques texels est alors sous- ou sur-échantillonné.
Corrigé par le sur-échantillonnage ci-dessus ; `ibl_equirect.a_small_very_bright_source_keeps_its_energy`
le verrouille (énergie du cube / énergie de l'image à 5 % près, soleil à 40° et près du pôle).

## Comparaison avec three.js (DamagedHelmet, `tools/reference/README.md`)

Même caméra, même casque, éclairage par image seul (pas de soleil), sortie linéaire sans tonemapping,
`royal_esplanade_1k.hdr` ; three.js 0.170 (`PMREMGenerator`). Mode A : three.js tel quel ; mode B :
rugosité plancher 0,045 sans `geometryRoughness`, sans compensation de diffusion multiple ni atténuation du
diffus (le modèle du moteur). Quatre vues du casque ; métriques sur les pixels où l'un des
deux rendus n'est pas noir (80 000 à 97 000 pixels par vue) ; « ratio » = somme moteur / somme three.js ;
p50 / p99 = percentiles de l'écart absolu par canal, en niveaux sur 255 (linéaire). Plages sur les quatre
vues (`tools/reference/metrics.py`).

| Ciel | Ratio A | Ratio B | PSNR A | PSNR B | p50 / p99 (A) |
|---|---|---|---|---|---|
| uniforme, radiance 1 (isole le BRDF) | 1,043 – 1,072 | 1,033 – 1,038 | 27,5 – 30,3 dB | 31,9 – 32,9 dB | 2 – 5 / 33 – 53 |
| HDR écrêté à 4 (sans pic de soleil) | 1,034 – 1,055 | 1,022 – 1,050 | 25,5 – 27,9 dB | 26,4 – 28,7 dB | 3 – 4 / 64 – 92 |
| HDR réel (soleil jusqu'à 5·10⁴) | 1,158 – 1,202 | 1,169 – 1,247 | 20,2 – 21,3 dB | 19,8 – 21,1 dB | 4 – 8 / 113 – 153 |

Lecture, en distinguant ce qui est établi de ce qui est supposé :

- **Établi** : le ciel dessiné par le moteur (skybox) et `scene.background` de three.js ont la même
  énergie à 0,5 % près (rapport ~1,005) : l'orientation et l'échelle de la source sont les mêmes.
- **Établi** : avec un ciel uniforme, le moteur est 3 à 4 % plus clair que three.js B (PSNR 32 dB) :
  la LUT du moteur est intégrée numériquement (validée contre numpy aux centres de texels), alors que
  l'approximation analytique de Karis s'écarte de cette intégrale jusqu'à 15 % à r = 0,6 (mesuré contre
  numpy). L'attribution précise à three.js n'est pas vérifiée (je n'ai pas lu son code de LUT) ; elle
  est cohérente avec le fait que l'écart disparaît presque en mode B, qui retire ses autres écarts.
- **Établi** : avec le soleil réel, le moteur est 16 à 25 % plus clair que three.js, alors que ses
  cartes sont à moins de 7,4 % de la force brute et qu'avec le même HDR écrêté à 4 (sans le pic de
  soleil) l'écart tombe à 2 – 5 %.
- **Hypothèse, non vérifiée** : la différence vient du filtrage de `PMREMGenerator` face à un soleil 5 000
  fois plus brillant que le ciel (perte d'énergie à rugosité moyenne). Je n'ai pas comparé les cartes de
  three.js à la force brute directement.

Ce n'est donc **pas** un accord au sens « mêmes images » : l'AC « reflets métalliques cohérents avec
l'environnement » est démontrée par les tests (orientation du cube de bout en bout, shader = référence
CPU, cartes = force brute à moins de 7,4 %, ciel et échelle = three.js), pas par un PSNR élevé sur
le HDR réel.

## Limites

- Le disque de soleil du ciel et le soleil directionnel de `EngLighting` sont indépendants : un ciel avec
  soleil + un soleil directionnel éclairent deux fois (le ciel procédural du lanceur place son disque
  dans la direction du soleil par défaut, mais ne suit pas les curseurs).
- Échantillonnage d'un cube **dans la face** (le GPU filtre au-dessus des arêtes, pas le CPU) ; le
  sur-échantillonnage et les mips ne lissent pas non plus les arêtes.
- Pas de compensation de diffusion multiple (les métaux rugueux perdent de l'énergie, comme avant).
- Une seule sonde globale : pas de sondes locales, pas de parallaxe, pas de mise à jour dynamique.
- Pas d'harmoniques sphériques : l'irradiance est un cube 32 × 32.
- Le calcul prend plusieurs secondes en debug (ASan) ; il n'est pas mis en cache sur disque.
- Les données HDR du ciel d'un `.hdr` sont lues par stb_image (RGBE uniquement).
