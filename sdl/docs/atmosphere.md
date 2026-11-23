# Ciel atmosphérique analytique (M3-05)

`engine/atmosphere.h`, `shaders/atmosphere.frag.hlsl`. Décision et alternatives :
[ADR 0018](adr/0018-ciel-analytique-a-diffusion-simple-et-masses-d-air.md).

## Ce que c'est

Un ciel calculé par pixel à partir de la **direction du soleil**, sans texture : à chaque image, le soleil de
`EngLighting` (azimut et hauteur réglables dans le panneau F1, ou `--sun-azimuth` / `--sun-elevation` au
lancement) pilote à la fois le ciel dessiné, la couleur et l'intensité de la lumière directe, et la lumière
ambiante. Sans image HDR (`--env`), le lanceur l'utilise par défaut, avec la démonstration comme avec `--terrain`.

## Modèle

Diffusion **simple** (un rebond) de Rayleigh et de Mie dans une atmosphère plane, par masses d'air :

```
m(mu)  = 1 / (mu + 0,025 exp(-11 mu))          mu = cos de l'angle au zénith ; m(1) = 1, m(0) = 40
ext    = rayleigh + mie                         épaisseurs optiques au zénith, par canal RVB
S(cos) = rayleigh P_R(cos) + mie P_M(cos)       P_R = 3/(16 pi)(1 + cos^2), P_M = Henyey-Greenstein (g)
L      = E S (1 - exp(-ext m_vue)) / ext exp(-ext m_soleil)  [x crépuscule] + nuit
```

| Paramètre | Défaut | Rôle |
|---|---|---|
| `rayleigh` | (0,040 ; 0,094 ; 0,229) | épaisseurs optiques de Rayleigh à 680, 550, 440 nm : le bleu est diffusé 6 fois plus que le rouge |
| `mie`, `mie_g` | 0,03 ; 0,70 | brume : diffusion grise, vers l'avant (halo autour du soleil) |
| `irradiance` (E) | 50 | échelle de la radiance du ciel |
| `path_limit` | 8 | masse d'air maximale de la ligne de vue (sinon l'horizon est trop clair) |
| `sun_path_limit` | 15 | masse d'air maximale du soleil jusqu'au point diffusant (sinon un soleil bas éteint tout le ciel : les points en altitude voient le soleil à travers moins d'air que le sol) |
| `sun_radiance`, `sun_radius_deg` | 60 ; 0,8° | disque du soleil, atténué par la transmittance (rouge au coucher), bord adouci sur les 20 % extérieurs du rayon |
| `ground_albedo` | (0,25 ; 0,22 ; 0,18) | sous l'horizon : albédo × ciel à l'horizon, raccord sur 0,05 en y |
| `night` | (0,002 ; 0,003 ; 0,006) | radiance ajoutée en permanence |

Crépuscule : le ciel reste éclairé quand le soleil passe sous l'horizon, jusqu'à −12° (facteur
`clamp((sin(h) + 0,21) / 0,21, 0, 1)²`), puis il ne reste que `night`.

**Lumière directe** (`eng_atmosphere_sun_light`) : couleur = transmittance du soleil à travers toute l'atmosphère,
divisée par celle d'un soleil au zénith (donc (1, 1, 1) à midi, jamais plus de 1) ; visibilité = fondu de 1 à 0
entre +1,2° et −1,2° (le soleil passe derrière l'horizon). Le lanceur multiplie `sun_color` par la couleur et
`sun_intensity` par la visibilité.

**Lumière ambiante** (`eng_atmosphere_ambient`) : radiance moyenne du ciel (zénith + 8 directions à 45°) divisée,
par canal, par celle d'un soleil au zénith : environ 0,9 à 45°, 0,3 à 10°, 0,15 à l'horizon, 0,003 la nuit, de plus
en plus rouge au coucher. Le lanceur multiplie `ambient_color` par ce vecteur.

## Cohérence avec la hauteur du soleil (critère d'acceptation)

Valeurs mesurées par les tests (`engine.atmosphere`, lanceur `launcher.sky`) :

| Hauteur du soleil | Lumière directe (r g b) | Ambiante (r g b) | Ciel à l'opposé du soleil |
|---|---|---|---|
| 90° | 1,00 1,00 1,00 | 1,00 1,00 1,00 | bleu |
| 45° | 0,97 0,95 0,90 | 0,94 0,90 0,83 | bleu > vert > rouge |
| 10° | 0,72 0,56 0,30 | 0,32 0,31 0,20 | plus sombre, bleu plus faible |
| 5° | 0,52 0,31 0,09 | — | |
| 0° | 0,07 0,01 0,00 | 0,15 0,09 0,02 | rouge-orangé près du soleil |
| −6° | 0 | 0,04 0,03 0,01 | crépuscule |
| −20° | 0 | ≈ 0 | plancher `night` |

Au coucher, près du soleil, le ciel est rouge (rouge/bleu ≥ 20 fois celui de midi) ; le zénith est plus saturé que
l'horizon, qui est plus pâle et plus clair ; la luminance du ciel croît strictement de −10° à +20° puis reste dans
une bande étroite jusqu'à 80°.

## Un seul calcul, deux implémentations

HLSL n'a pas d'`#include` dans cette chaîne d'outils : `atmosphere.frag.hlsl` recopie les formules de
`atmosphere.c`. Le test `engine.atmosphere_gpu` rend le ciel pour 6 hauteurs du soleil × 3 orientations de
caméra (fov 70°) et compare **chaque pixel** à la version C évaluée dans la direction de ce pixel (écart ≤ 2/255) :
modifier une constante, une formule ou l'exposition d'un seul côté le fait échouer (vérifié par 6 mutations du
shader, toutes détectées).

## Éclairage par image : cuit une fois

Le reflet des métaux et la partie « environnement » de l'ambiante viennent de la cubemap d'`EngEnvironment`
(docs/ibl.md), calculée **une fois au chargement**, à partir d'un dégradé équivalent
(`eng_atmosphere_to_sky_desc` : zénith, horizon à 90° du soleil, sol, disque) pour le soleil initial. Changer la
hauteur du soleil ensuite change le ciel dessiné, la lumière directe et l'intensité/couleur de l'ambiante, **pas**
les reflets : les reflets restent ceux du soleil initial.

## Fichiers

| Fichier | Rôle |
|---|---|
| `engine/include/engine/atmosphere.h`, `engine/src/atmosphere.c` | modèle (ciel, disque, lumière, ambiante, dégradé IBL), pipeline et dessin |
| `shaders/atmosphere.frag.hlsl` | fragment shader (vertex : `sky.vert`) |
| `tests/test_atmosphere.c`, `tests/test_atmosphere_gpu.c`, `tests/launcher_sky.cmake` | preuves |

## Preuves

- `engine.atmosphere` (12 cas) : validité (chaque champ, chaque borne), masse d'air (1, 40, monotone), **5 valeurs
  épinglées recalculées par un script indépendant** (±0,2 %), couleur selon la hauteur du soleil, invariance par
  rotation autour de la verticale, sol et continuité à l'horizon, disque (présent, absent hors du rayon, décroissant
  vers le bord, rouge au coucher, caché sous l'horizon), lumière directe (valeurs épinglées, ≤ 1, de plus en plus
  rouge, fondu strictement monotone), ambiante, entrées invalides (noir et `false`), 60 000 tirages aléatoires
  (finis, positifs, bornés, y compris verticales et vue = soleil), dégradé IBL.
- `engine.atmosphere_gpu` (4 cas) : comparaison pixel par pixel shader / C, disque au bon endroit, échelle
  d'exposition, atmosphère invalide refusée, export PPM relu à l'œil (`OW_TERRAIN_DUMP`).
- `launcher.sky` : `--sun-elevation 60 / 3 / −20` → lumière directe presque blanche / rouge-orangé / éteinte,
  ambiante forte / faible / nulle ; options hors plage refusées (code 2).

## Limites

- **Approximation, pas un modèle de référence** : rien n'est comparé à Hillaire, Hosek-Wilkie ou à des mesures. Les
  ordres de couleur et de luminosité sont plausibles, pas les valeurs absolues. Les plafonds de masse d'air et le
  facteur de crépuscule sont des réglages choisis à l'œil et par cohérence, pas des grandeurs physiques.
- Pas de diffusion multiple, pas d'ozone, pas de nuages, pas d'étoiles ni de lune, pas de perspective aérienne (le
  brouillard de distance vient avec M3-06).
- Lumière ambiante : cuite une fois pour la couleur de l'environnement (voir ci-dessus) ; l'ambiante relative
  dépend un peu de l'azimut du soleil (±3 %, échantillonnage à 8 azimuts).
- Un seul soleil ; pas de lune. Sous l'horizon le ciel est un sol uniforme d'albédo fixe.
- Exposition : le ciel est en radiance HDR avec disque à ~40 ; il est tonemappé comme le reste de la scène.
- Rendu vérifié uniquement sur SwiftShader.
- Brouillard (M3-06) : la couleur d'horizon `eng_atmosphere_horizon` sert de couleur de brouillard au terrain (docs/fog.md).
