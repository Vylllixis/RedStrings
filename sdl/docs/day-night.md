# Cycle jour / nuit (M8-07)

Module : `engine/daynight.h` (fonctions pures, testées dans `tests/test_daynight.c`).

## Principe

- Une horloge de jeu `EngDayNight.hour` dans [0, 24[. Par défaut **une journée complète en 20 min
  réelles** (critère de M8-07 ; 1 h de jeu = 50 s, `day_seconds` = `ENG_DAYNIGHT_DAY_SECONDS` = 1200),
  multipliée par `speed`. Test : `daynight.a_full_day_lasts_twenty_real_minutes_by_default` (72 000
  pas de 1/60 s ramènent à la même heure).
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
- ~~Cubes de la démonstration non éclairés~~ : depuis la finition, leur teinte suit la lumière
  (soleil ou lune visibles, ambiante : `textured_lit.vert`) ; ils s'assombrissent la nuit (B16).
- ~~Pas d'étoiles ni de lune~~ : étoiles fixes (hachage de la direction, une case sur 400 environ,
  atténuées près de l'horizon) et disque lunaire (~1°, halo) dans `atmosphere.frag`, d'intensité =
  poids de nuit (`eng_atmosphere_draw_ex`, `EngNightSky`) ; la lune est là où `apply_moonlight` met
  sa lumière. Le ciel de jour est inchangé (le test qui compare le shader au calcul CPU passe).
- Brouillard de nuit corrigé (B17, voir `docs/fog.md`) : il prenait la couleur d'un ciel de jour.
- Les valeurs (0,05 ; +1,5 EV ; plancher 0,03 ; premier essai à 0,12 / +2,5 EV : nuit trop claire) sont réglées à l'œil sur SwiftShader, jamais sur un
  vrai écran.
