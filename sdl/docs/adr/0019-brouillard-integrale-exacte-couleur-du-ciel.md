# ADR 0019 : brouillard à épaisseur optique exacte, couleur du ciel à l'horizon

Statut : accepté (M3-06)

## Contexte

M3-06 demande un brouillard de distance et de hauteur dont les chunks lointains « se fondent dans le ciel ».

## Décision

- Densité exponentielle en hauteur et **intégrale analytique** de l'épaisseur optique le long du rayon caméra -> point
  (docs/fog.md), appliquée par pixel dans `terrain.frag` avec la position monde du fragment.
- Couleur du brouillard = radiance du ciel de M3-05 à l'horizon dans l'azimut du point (`eng_atmosphere_horizon`),
  multipliée par l'échelle d'exposition du ciel : le raccord avec le ciel est exact par construction.
- Référence CPU (`engine/fog.c`) recopiée dans le shader (pas d'`#include` en HLSL ici, comme pour l'atmosphère) ;
  un test GPU compare le rendu à la référence pixel par pixel.
- Les uniforms du brouillard sont une **queue** ajoutée au bloc de fragment d'éclairage
  (`EngModelDrawOptions.fragment_tail`), pour ne pas dupliquer la file de dessin ni toucher `model.frag`.

## Alternatives

- **Brouillard exponentiel classique** `exp(-density x d)` : ignore la hauteur ; une vallée et un sommet à la même
  distance auraient la même brume.
- **Intégration par pas (ray marching)** : plus général (densité arbitraire), coûteux par pixel et inutile avec une
  densité exponentielle dont l'intégrale est connue.
- **Couleur fixe réglable** : se désaccorde du ciel dès que le soleil bouge ; la couleur liée au ciel suit le
  coucher et la nuit sans paramètre.
- **Brouillard par sommet** : artefacts sur de grands triangles de terrain et sur le raccord entre chunks.
- **Passe plein écran à partir du tampon de profondeur** : s'appliquerait à tout (modèles, herbe...), mais demande de
  lire la profondeur et de reconstruire la position ; reportable quand plusieurs types d'objets devront être
  brouillardés (M3-07+).

## Conséquences

- Le brouillard ne touche que le terrain (limite documentée) ; chaque futur fragment shader d'objet dans le décor
  devra recopier `apply_fog`, ou on passera à la passe plein écran.
- Le bloc de fragment du terrain grandit de 64 octets (288 au lieu de 224) ; la limite du renderer est 4096.
- Qualité : le brouillard est homogène ; pas de diffusion propre ni de volumétrique.
