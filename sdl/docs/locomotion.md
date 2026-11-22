# Déplacement relatif à la caméra (M4-07)

`engine/locomotion.h` : module pur (ni physique, ni SDL, ni GPU). Il transforme l'axe `MOVE` en une
**vitesse horizontale voulue** et en une **orientation** du personnage ; l'appelant donne la vitesse à
`eng_character_update` (physics.h), qui gère sol, pentes, marches et gravité.

```c
EngLocomotion loco;  eng_locomotion_init(&loco, 0.0F);
// à chaque pas fixe :
eng_orbit_camera_flat_forward(&cam, fwd);  eng_orbit_camera_flat_right(&cam, right);
eng_locomotion_update(&loco, &input, fwd, right, speed_scale, control, dt);
eng_locomotion_velocity(&loco, v);         eng_character_update(character, dt, v);
// au rendu : eng_locomotion_facing_lerp(prev_facing, facing, alpha)
```

## Modèle

- **Direction** : `forward * move.y + right * move.x`, la caméra étant aplatie sur le plan horizontal
  (regarder vers le haut ne ralentit pas). Longueur ramenée à 1 : pas de diagonale plus rapide ; le stick
  analogique règle l'allure.
- **Vitesse** : on rapproche la vitesse actuelle de la vitesse cible à taux borné (m/s²) :
  `accel` (30) pour accélérer, `decel` (40) pour ralentir ou s'arrêter, `reverse_decel` (60) quand la
  direction voulue s'oppose à la vitesse. Vitesse max 4,5 m/s (× `speed_scale`, sprint M4-09).
  `control` (]0 ; 1]) multiplie les taux (contrôle réduit en l'air, M4-08).
- **Rotation** : le lacet du personnage suit la direction **voulue** (pas la vitesse : réactif) par un
  filtre exponentiel (`turn_tau` 0,06 s) borné à 1080°/s, avec une zone morte (0,1) sous laquelle il ne
  se tourne pas. Convention des caméras : 0 = regarde vers -Z, positif vers la gauche.
  `eng_locomotion_facing_lerp` interpole par le plus court arc (le lacet est dans ]-π ; π]).

## Demi-tour : valeurs mesurées (tests `locomotion.*`)

| Cas | Glisse (distance encore parcourue vers l'ancienne direction) | Vitesse pleine dans le sens inverse |
|---|---|---|
| allure normale, 4,5 m/s | v²/(2·60) = 0,17 m | 0,225 s (freinage 0,075 s + accélération 0,15 s) |
| sprint x1,6, 7,2 m/s | 0,43 m | 0,36 s |

Le personnage tourne de 180° à ±5° en 0,15 à 0,30 s, sans dépassement, monotone.

## Limites

- Les chiffres (accélérations, 1080°/s) sont des **valeurs de départ raisonnables**, pas validées au
  ressenti : le critère « demi-tour fluide » est vérifié par la glisse et le temps mesurés, le ressenti
  réel est à juger au jalon M4-RV. Tous les réglages sont dans `EngLocomotionParams`.
- La vitesse voulue est horizontale ; la vitesse le long d'une pente est corrigée par le contrôleur
  (docs/physics.md), pas ici.
