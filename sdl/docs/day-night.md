# Cycle jour / nuit (M8-07)

Module : `engine/daynight.h` (fonctions pures, testées dans `tests/test_daynight.c`).

## Principe

- Une horloge de jeu `EngDayNight.hour` dans [0, 24[. Par défaut 24 h de jeu = 24 min réelles
  (1 h = 1 min, `day_seconds` = 1440), multipliée par `speed`.
- Le soleil se lève à l'est (+X) à 6 h, culmine au sud (-Z) à midi à 62°, se couche à l'ouest (-X)
  à 18 h ; la nuit il passe sous l'horizon (élévation -62° à minuit). Ce n'est pas un modèle
  astronomique : pas de latitude, pas de saisons, pas de lune visible ni de phases, pas d'étoiles.
- Le ciel atmosphérique, la couleur et la force du soleil et l'ambiante suivaient déjà la hauteur
  du soleil (M3-05, `engine/atmosphere.h`). Le cycle ajoute :
  - un **clair de lune** (`demo_scene.c`, `apply_moonlight`) : lumière faible (0,05) et bleutée,
    opposée au soleil et au moins 20° au-dessus de l'horizon, qui remplace le soleil quand il est
    plus faible qu'elle ; les ombres viennent alors de la lune. L'ambiante reçoit un plancher bleuté.
    Le ciel, lui, garde la direction du vrai soleil ;
  - un **biais d'exposition** : +1,5 EV en pleine nuit, 0 le jour.
  Les deux sont pondérés par `eng_daynight_night_weight` : 0 au-dessus de +2°, 1 sous -10°, lissé.

## Dans le jeu

- La partie (`GameFlow.daynight`) commence à 9 h. L'heure avance pendant le jeu, s'arrête dans les
  menus, s'affiche en haut à droite du HUD (« HH:MM ») et est **sauvegardée** dans la section
  facultative `TIME` de la sauvegarde (une ancienne sauvegarde sans cette section reprend à 9 h).
- Lanceur : `--day-night HEURE` (heure de départ, aussi hors partie : la démo tourne alors avec une
  horloge locale) et `--day-length S` (durée réelle d'une journée). Exemple pour voir la nuit :
  `build/release/bin/launcher --combat --new-game --day-night 22`.
- Panneau F1 : « jour/nuit : heure » (curseur) et « jour/nuit : vitesse » (0 = arrêt, jusqu'à 60).
  Quand le cycle est actif, il pilote les curseurs « soleil : azimut / hauteur ».

## Limites

- Lumière de l'environnement (IBL) cuite une fois au lancement pour le soleil de départ : la nuit,
  les reflets spéculaires de l'environnement restent ceux du ciel du départ, seulement assombris
  par l'ambiante. Une recuisson pendant le jeu coûterait environ 2,7 s sur SwiftShader.
- Les cubes texturés de la démonstration (pipeline `textured`, M1) ne sont pas éclairés du tout :
  ils restent clairs la nuit (et le biais d'exposition les éclaircit). Le terrain, le sol, les
  modèles PBR et l'eau suivent bien la nuit.
- Pas d'étoiles ni de lune dessinées dans le ciel ; ciel nocturne = plancher `night` de
  l'atmosphère, très sombre.
- Les valeurs (0,05 ; +1,5 EV ; plancher 0,03 ; premier essai à 0,12 / +2,5 EV : nuit trop claire) sont réglées à l'œil sur SwiftShader, jamais sur un
  vrai écran.
