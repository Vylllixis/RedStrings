# Caméra libre de debug (M1-09)

Module `engine/camera.h` : une caméra qui vole librement dans la scène, pour inspecter le rendu.
Elle est **pure** (ni SDL, ni GPU) : elle ne lit que `EngInputState`, donc elle se teste sans
fenêtre (`tests/test_camera.c`). Le lanceur l'utilise ; le futur mode jeu aura sa propre caméra.

## Commandes

| Action | Clavier / souris | Manette |
|---|---|---|
| Avancer, reculer, côtés | Z Q S D (touches physiques WASD) | stick gauche |
| Regarder | souris (capturée) | stick droit |
| Monter / descendre | Espace / Ctrl gauche (actions `jump` / `dodge`) | A / B |
| Sprint (vitesse × 4) | Maj gauche (`sprint`) | clic du stick gauche |
| Vitesse | molette : × 1,25 par cran (bornée à 0,5 – 500 m/s, défaut 6 m/s) | — |
| Libérer / reprendre la souris | F2 | — |
| Afficher / masquer le debug draw (grille, boîtes, repères) | F3 | — |
| Afficher / masquer l'interface de debug (statistiques, réglages ; libère la souris) | F1 | — |

Les actions suivent `config.ini` (`bind.jump`, `bind.sprint`…), la sensibilité de la souris aussi
(`mouse_sensitivity`, en radians par pixel : 0,002 → 100 px = 0,2 rad ≈ 11,5°).

## Conventions

Celles du projet (`docs/conventions-maths.md`) : main droite, Y vers le haut, mètres, radians.
`yaw = 0, pitch = 0` regarde vers −Z. **Yaw positif = tourne à gauche** ; **pitch positif = regarde
en haut**, borné à ±89° (la vue ne se retourne jamais). Le yaw n'est pas ramené dans ]−π, π] pour
que l'interpolation d'affichage reste continue.

- avant = (−sin yaw · cos pitch, sin pitch, −cos yaw · cos pitch)
- droite = (cos yaw, 0, −sin yaw), toujours horizontale.

Le déplacement avant/arrière suit le regard (vol libre : on monte en regardant vers le haut) ;
les côtés suivent la droite ; monter/descendre suit Y monde. La somme est ramenée à une longueur ≤ 1
(pas de diagonale plus rapide) puis multipliée par `vitesse × dt`.

## Regard : l'unité de regard

L'axe `ENG_AXIS_LOOK` mélange deux choses de natures différentes : la souris (déplacement cumulé en
pixels) et le stick (valeur de −1 à 1 *par pas*). La caméra convertit avec une seule constante,
`ENG_CAMERA_LOOK_UNIT` = 0,03 rad par unité. Le **lanceur** règle la sensibilité de la souris sur
`config.mouse_sensitivity / ENG_CAMERA_LOOK_UNIT`, de sorte que :
- la souris tourne de `pixels × mouse_sensitivity` radians (la valeur de `config.ini` garde son sens) ;
- un stick à fond tourne de 0,03 rad par pas, soit ≈ 103°/s à 60 Hz.
Les liaisons par défaut du moteur (0,002) ne sont pas modifiées : seul le lanceur convertit.

## Capture de la souris

`eng_platform_set_mouse_capture` active le mode relatif de SDL (curseur caché, déplacements sans
limite d'écran). Le lanceur capture la souris au démarrage ; F2 bascule ; la perte de focus la
libère. Quand la souris n'est pas capturée, le lanceur met la sensibilité de l'axe LOOK à 0 : le
curseur peut bouger sans faire tourner la caméra (le stick droit reste actif).

## Fluidité

La simulation tourne à pas fixe (60 Hz) ; l'affichage à la cadence de l'écran. Le lanceur garde les
deux derniers états de la caméra et affiche `eng_camera_lerp(précédent, courant, alpha)` : pas de
saccades quand l'écran est plus rapide que la simulation (coût : l'image affichée a au plus un pas de
retard, ≈ 17 ms).

## Limites connues

- Pas de collision, pas de lissage d'accélération (la vitesse est instantanée).
- La molette est la seule commande de vitesse : une manette seule ne peut pas la changer.
- Si SDL refuse le mode relatif (pilote `offscreen`, certains environnements distants), la souris
  reste libre et ne tourne pas la caméra ; un avertissement est journalisé.
- Le sens de la molette suit le réglage du système (défilement « naturel » inclus).
