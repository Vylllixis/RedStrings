# Saut et chute (M4-08)

`engine/jump.h` (module pur) décide **quand** sauter ; `eng_character_*` (physics.h) applique le saut.

```c
EngJump jump;  eng_jump_init(&jump);
// à chaque pas fixe, avant eng_character_update :
bool grounded = eng_character_ground(c) == ENG_GROUND_SUPPORTED;
EngJumpEvents ev;
eng_jump_update(&jump, grounded, eng_action_pressed(&in, ENG_ACTION_JUMP), dt, &ev);
if (ev.jump) eng_character_launch(c, 6.0F);          // lance même juste après un rebord
if (eng_action_released(&in, ENG_ACTION_JUMP)) eng_character_cut_jump(c, 0.5F); // saut court
```

- **Délai de grâce (coyote)** 0,10 s : le saut reste permis après avoir quitté un rebord.
- **Mémorisation de la touche (buffer)** 0,12 s : une touche pressée juste avant d'atterrir saute à
  l'atterrissage et la pression est consommée.
- **Pas de double saut** : après un saut, plus de saut possible avant le prochain atterrissage, même si
  le sol est encore signalé un pas.
- Les durées sont des **sommes de dt** (temps de simulation) : même comportement à 30 ou 144 FPS.
  Tolérance de 0,1 ms pour que 6 pas de 1/60 s comptent bien 0,1 s.
- `ev.landed` / `ev.left_ground` : événements pour l'animation et les sons (M4-13 et suivants).
- `eng_character_launch` donne la vitesse verticale sans exiger le sol ; `eng_character_jump` (sol exigé)
  reste pour les cas simples.
- **Gravité ajustée** : `eng_character_set_gravity_scale(c, rise, fall)` (défaut 1, 1). Un saut de
  vitesse v monte de v²/(2·g·rise) ; une chute plus forte (fall > rise) rend le saut plus sec.
  Valeurs de départ conseillées : rise 1, fall 1,6 (à régler au ressenti).

## Limites
- Les valeurs (0,10 s, 0,12 s, 6 m/s, fall 1,6) sont des points de départ, pas validés au ressenti.
- La détection « au sol » vient de Jolt : le sol est considéré perdu dès que la capsule n'est plus
  supportée, ce qui peut précéder de quelques pas le moment visuel où le pied quitte le bord.
