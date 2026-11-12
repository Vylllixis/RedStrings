# Comparaison avec un visualiseur de référence (M1-15)

Compare le rendu PBR du moteur à celui de **three.js** sur DamagedHelmet (acceptation de M1-15,
résultats dans `docs/pbr.md`). Hors du dépôt : Node, npm, Chromium + Playwright, Python avec numpy et
Pillow, accès à GitHub et au registre npm. Rien de tout cela n'est requis pour compiler ou tester le moteur.

```sh
W=/tmp/ref && mkdir -p $W/three $W/helmet $W/cmp
cp tools/reference/page.html tools/reference/shot.js $W/three/
(cd $W/three && npm init -y && npm install three@0.170.0 playwright)
B=https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/DamagedHelmet/glTF
for f in DamagedHelmet.gltf DamagedHelmet.bin Default_albedo.jpg Default_AO.jpg \
         Default_emissive.jpg Default_metalRoughness.jpg Default_normal.jpg; do
  curl -sSL -o $W/helmet/$f $B/$f; done

# 1. le moteur : quatre vues -> $W/cmp/engine_{0..3}.ppm (linéaire, 512 x 512)
OW_HELMET=$W/helmet/DamagedHelmet.gltf OW_DUMP=$W/cmp \
  build/debug/tests/bin/test_model_gpu --filter=eng_model_gpu.dump_a_model_for_the_reference_viewer

# 2. three.js, mêmes caméras et soleils (mode : voir plus bas)
(cd $W && python3 -m http.server 8765 --bind 127.0.0.1 &)
cd $W/three && for m in A B C D; do OUT_DIR=$W/cmp node shot.js $m; done

# 3. métriques
python3 -I tools/reference/metrics.py $W/cmp
```

Modes de `shot.js` : **A** three.js tel quel ; **C** diffus direct pondéré par `(1 − F)` (annexe B de
la spécification) ; **D** rugosité plancher 0,045 sans `geometryRoughness` ; **B** les deux. Le moteur
n'a ni l'un ni l'autre des comportements de three.js (voir `docs/pbr.md`), d'où B comme référence de
validation et A, C, D pour attribuer les écarts.

Réglages communs : caméra à 40° de champ vertical visant (0, 0, −0,187), soleil d'intensité 1,5
(three.js : `DirectionalLight` d'intensité π × 1,5), ambiante nulle, pas de tonemapping, sortie linéaire.

`pbr_ref.py` est la référence Python indépendante (double précision, formules de la spécification) qui
a fourni les valeurs attendues de `tests/test_pbr.c`.

## Éclairage basé image (M1-16)

Mêmes outils, deux pages de plus (`page_ibl.html`, `page_sky.html`) et deux scripts numpy
(`ibl_ref.py` : LUT BRDF et coefficient du préfiltrage ; `ibl_brute.py` : intégration par force brute
de l'irradiance et du préfiltré sur CHAQUE pixel d'une image équirectangulaire, à comparer au moteur).
`shot.js` prend `PAGE` (page à ouvrir), `PREFIX` (préfixe des fichiers) et `EXTRA` (paramètres d'URL, collés après `?mode=X`, donc commençant par `&`) :

```sh
# image HDR : royal_esplanade_1k.hdr (three.js r170, CC0), en $W/hdr
# moteur (ciel uniforme blanc, ou HDR réel)
OW_HELMET=$W/helmet/DamagedHelmet.gltf OW_DUMP=$W/cmp_ibl OW_ENV=$W/hdr/royal_esplanade_1k.hdr \
  build/debug/tests/bin/test_model_gpu --filter=eng_model_gpu.dump_a_model_for_the_reference_viewer
# three.js : mode A (tel quel) ou B (aligné sur le moteur : rugosité plancher 0,045, pas de
# diffusion multiple, pas d'atténuation du diffus) ; ?uniform=1 pour un ciel uniforme
(cd $W/three && for m in A B; do PAGE=page_ibl.html EXTRA="&hdr=royal_esplanade_1k.hdr" OUT_DIR=$W/cmp_ibl node shot.js $m; done)
python3 -I tools/reference/metrics.py $W/cmp_ibl
# validation de l'intégration du moteur contre la force brute
python3 -I tools/reference/ibl_brute.py $W/hdr/royal_esplanade_1k.hdr build/debug/tests/bin/test_ibl   # lance lui-même le moteur
```

Résultats et lecture : `docs/ibl.md`.
