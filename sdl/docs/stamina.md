# Sprint et endurance (M4-09)

Module pur `engine/stamina.h` : aucune dépendance à SDL, à la physique ou au rendu.
Il décide si le joueur **court** ce pas-ci ; le résultat se branche sur
`eng_locomotion_update(..., speed_scale, ...)` avec `ENG_STAMINA_SPRINT_SCALE` (1,6).

## Règles

| Élément | Valeur par défaut |
|---|---|
| Réserve | 0 … 100 (`ENG_STAMINA_MAX`), pleine au départ |
| Consommation en course | 20 /s (soit 5 s depuis le plein) |
| Régénération | 25 /s, après un délai de 1 s sans courir |
| Reprise après épuisement | dès 25 points |

- On court si : touche demandée **et** déplacement en cours **et** non épuisé **et** réserve > 0.
- À 0 : état `exhausted`. Tenir la touche ne sert à rien ; la course reprend quand la réserve
  atteint `resume_at` (hystérésis, pour éviter le clignotement course/marche à 0).
- Chaque pas de course remet le délai de régénération à zéro.
- Indépendant du framerate (testé à 30 et 144 FPS). `dt` ∈ ]0 ; 0,1].
- `value` est assainie à chaque pas (NaN → 0, bornée à [0 ; 100]).

## Debug UI

`eng_stamina_register_tweaks` expose la réserve sous le nom **« endurance »** (curseur 0…100 dans
le panneau F1). Le curseur modifie la valeur vivante ; la remonter à la main lève l'épuisement.

## Limites

Valeurs de départ à juger à la sensation. Rien n'est branché au lanceur par ce module seul.
