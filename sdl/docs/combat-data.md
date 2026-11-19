# Données de combat (M5-02, M5-03)

## Machine à états (`engine/combat_state.h`)

États : `idle, move, attack, dodge, block, parry, hitstun, stagger, dead`. La table des
transitions légales (une donnée unique : `eng_combat_legal`, `eng_combat_legal_mask`) suit
`docs/combat-design.md` §5 ; un test compare les 81 couples au document.

```c
EngCombatFsm f;  eng_fsm_init(&f);
if (!eng_fsm_request(&f, ENG_CS_ATTACK)) { /* refusé */ }
eng_fsm_tick(&f);                        /* f.ticks++ */
```

- Transition illégale : **assertion en Debug**, refus dans tous les cas, compteur `f.refused`.
- Redemander l'état courant n'est pas une transition (sauf `attack -> attack`, enchaînement de
  combo, qui remet `ticks` à 0).
- `dead` est définitif.
- État courant affiché en debug : `eng_combat_state_name(f.state)` + `f.ticks` (branché dans
  le débogueur de combat, M5-16).

## Fichier d'attaques (`engine/attack_data.h`)

```
attack light1
  startup 8              # ticks (60 Hz), >= 1
  active 4               # >= 1
  recovery 14            # >= 0
  damage 10  posture 8  stamina 10  hitstop 3
  cancel_from 6          # tick de la recovery où l'on peut enchaîner / esquiver (défaut = recovery)
  next light2            # coup suivant du combo (référence en avant permise)
  hitbox hand_r  0 0 0  0 0 0.6  0.12     # os, A, B (repère de l'os), rayon en m
end
```

- 32 attaques, 4 hitboxes par attaque, phases de 600 ticks au plus, `hitstop` 0 à 30.
- `anim CLIP [VITESSE [DÉCALAGE]]` (M4, facultatif) : clip du personnage joué pendant l'attaque,
  lu à l'instant `DÉCALAGE + tick / 60 × VITESSE` (vitesse ]0 ; 10], décalage [-60 ; 60] s : négatif =
  première image tenue). Calé pour que le passage de l'arme tombe dans les ticks actifs (test
  `engine.character_motion`, voir `docs/characters.md`).
- Une hitbox `hand_r` est remplacée par la **lame animée** quand le jeu la donne à la simulation
  (`eng_sim_set_weapon`, M5-06) ; les autres noms (ex. `bolt`) restent dans le repère du corps.
- Tout ou rien : la moindre erreur refuse le fichier entier (« ligne N : … »).
- Rechargement à chaud : `eng_attackfile_open` puis `eng_attackfile_poll` à chaque image ; une
  version invalide est journalisée et l'ancienne reste active.
- `eng_attack_phase`, `eng_attack_is_active`, `eng_attack_can_cancel` : requêtes pures à partir
  d'un numéro de tick (0 = premier tick de l'attaque).

Limite : la fenêtre d'annulation n'est qu'une plage de ticks de la recovery ; les « événements
d'animation » qui l'ouvrent depuis un clip viennent avec M5-04.

## Événements d'animation (`engine/anim_events.h`, M5-04)

Fichier texte à côté du clip (le format `.anim` ne change pas), relu à chaud :

```
event light1 0.133 hitbox_on
event light1 0.200 sound  swoosh_light
event light1 0.200 hitbox_off
event light1 0.300 cancel_open
event heavy  0.350 effect trail_heavy
```

Types : `hitbox_on`, `hitbox_off`, `sound NOM`, `effect NOM`, `cancel_open`, `cancel_close`. Temps en
secondes dans [0 ; 3600]. 128 événements au plus. Tout ou rien (« ligne N : … »).

`eng_animevents_collect(set, clip, durée, t0, t1, loop, out, max)` rend les événements du pas
`[t0 ; t1[` : **exactement une fois par passage**, y compris à travers le rebouclage ; sans `loop`, t1
est borné à la durée et un événement posé exactement à la durée est déclenché une fois à la fin.
Un pas de plus de 1024 cycles est refusé (retourne 0).

Décision : en v1, la **vérité du gameplay** pour les coups est `attack_data` (ticks entiers, déterministes) ;
les événements d'animation servent au son, aux effets et au contrôle de cohérence (le débogueur de
combat M5-16 pourra comparer `hitbox_on/off` aux ticks actifs). Les deux sources ne se corrigent pas
mutuellement.

Limite : le module n'est pas encore appelé par la machine d'animation (il faut lui fournir `t0`/`t1`
dépliés du clip) ; il sera branché avec le combat jouable.

## Détection des touches (M5-05 à M5-08)

- `combat_geom` : capsules (segment + rayon), recouvrement exact, balayage continu (la capsule de
  l'arme et celle de la cible sont interpolées entre deux ticks, pas ≤ somme des rayons min.)
  → pas de « tunnel » à grande vitesse. **Écart assumé** avec le libellé du tracker
  (« requêtes de chevauchement Jolt ») : tests analytiques, déterministes, sans monde physique.
- `hurtbox` : `hurtbox NOM ZONE OS ax ay az bx by bz R`, zones `head|torso|limb`, 16 max.
  Repère de l'os sans échelle ; rayon × échelle uniforme du modèle.
- `combat_hit` : une attaque ne touche jamais deux fois la même cible (`EngHitTracker`) ; plusieurs
  cibles par tick résolues par indice croissant ; contact le plus précoce d'abord.
- `combat_damage` : PV, multiplicateur de zone, 6 ticks d'invincibilité après un coup non mortel,
  `died_tick` posé au coup fatal.
- `input_buffer` : fenêtre 10 ticks (≈ 167 ms ; le design annonçait 150 ms = 9 ticks, à régler),
  8 entrées, priorité esquive > garde > lourde > légère.

## Combattant (M5-10 à M5-13) — `fighter`

Un tick = `eng_fighter_tick(f, attaques, entrées)` ; les touches du tick N sont résolues après le
tick de tous les combattants (`eng_combat_apply_hit`).
Les dégâts du coup sont multipliés par `params.damage_scale` de l'ATTAQUANT (0, négatif ou non fini :
x 1 ; sert à la progression M8-08, docs/progression.md) ; la posture infligée ne l'est pas.

- **Combos** : légère → `next` ; lourde seulement depuis une attaque qui a un `next` ; un
  finisseur (légère 3, lourde) ne s'annule que par une esquive. Une pression n'est jamais perdue :
  elle reste dans le buffer jusqu'à l'ouverture de la fenêtre d'annulation.
- **Esquive** : 24 ticks, invulnérable ticks 3 à 14, coût 15, distance 3 m en profil lissé
  (`root_delta`). Sans direction : pas en arrière (recul, opposé à l'avant du cap).
- **Garde / parade** : la pression ouvre une fenêtre de parade de `parry_frames` ticks (engagée
  même si le bouton est relâché), puis garde si maintenue. Parade réussie : 0 dégât, l'attaquant
  passe en Stagger. Garde : dégâts × 0,2, posture − posture du coup ; posture ≤ 0 → Stagger.
- **Réactions** : touche franche → HitStun de `hitstun_base + posture du coup` (≈ 18 ticks léger,
  40 lourd) ; interrompt l'attaque en cours ; recul `posture × knockback_per_posture` le long de la
  force. Direction de réaction (avant/arrière/gauche/droite) selon le cap du défenseur.
- `eng_fighter_hash` : empreinte FNV-1a de l'état (base du replay M5-18).

## Simulation, replay, verrouillage, feel, debug, audio, VFX (M5-14 à M5-20)

- `combat_sim` : jusqu'à 8 combattants. Résolution en **deux phases** par tick : toutes les touches
  sont calculées d'abord, puis appliquées (les échanges simultanés fonctionnent ; l'attaque d'un
  combattant touché au même tick est figée avant). Hitstop : pendant le gel, seuls les buffers
  d'entrée sont alimentés. Marche 4 m/s, cap = `atan2f(-dx, -dz)`. Corps simulé : hurtboxes
  tête / torse / jambes, hitbox d'arme dans le repère du corps (+Z avant, origine à 1,1 m).
- `combat_replay` : `EngPackedInput {flags, dir}` (dir 0 = aucune, 1..255 = angle quantifié) ;
  `eng_replay_run` produit une trace par tick, `eng_replay_first_divergence` compare deux traces.
  Format binaire « RSRP » v1, petit-boutiste, validé à la lecture. **Déterminisme garanti pour le
  même binaire sur la même machine** (flottants) ; pas entre plateformes.
- `lockon` : cône de **60° d'ouverture totale (±30°)**, acquisition ≤ 20 m, relâche > 25 m ;
  basculement gauche/droite par ordre angulaire (non limité au cône) ; cadrage joueur + cible ;
  strafe relatif à la direction de la cible.
- `game_feel` : 9 réglages `feel.*` (hitstop, secousse, ralenti de parade, FOV lourd) enregistrés
  comme tweaks. `damage_numbers` : 32 chiffres, 60 ticks de vie (« 15 », « 22! », « (2) », « PARRY »).
- `combat_debug` : pas en avant / en arrière / saut (re-simulation depuis le début), frise
  `. m s A r d D b P h S X #`, dessin des hurtboxes par zone (gris = intouchable), armes actives
  en rouge, contacts en blanc.
- Audio : `sfx` (6 sons procéduraux), `mixer` (16 voix stéréo, panoramique à puissance constante,
  démarrage différé en échantillons), `combat_audio` (`tick × rate / 60` : synchro exacte au
  échantillon, prouvée à 48 kHz), `audio_device` (flux SDL3 ; **miniaudio prévu au tracker est
  remplacé par SDL3**). `vfx` : 512 particules (xorshift graine), 4 préréglages, traînée d'arme
  24 échantillons, dessin debug.

**Limites** : rien de M5 n'est branché au personnage jouable ni visible à l'écran ; audio vérifié
avec le pilote « dummy » seulement (rien d'audible jugé, latence matérielle non mesurée) ; VFX =
lignes de debug, pas de billboards GPU ; aucun fichier Mixamo réel utilisé ; 60 FPS non mesuré ;
collisions analytiques (capsules) au lieu de requêtes Jolt.

## Mannequin d'entraînement et hurtboxes sur squelette (M5-17, M5-05)

- `training_dummy` : un combattant de la simulation piloté par un script (`Static`, `Block`,
  `Attack` toutes les `period` ticks). Il compte les dégâts reçus (`total_damage`, `hits_taken`,
  `last_damage`, lus dans les événements du tick), remet ses PV à fond après `reset_after` ticks
  sans coup (180) et renaît `respawn_after` ticks après sa mort (120). `eng_dummy_tick` s'appelle
  avant `eng_sim_step`. Les chiffres à l'impact viennent de `damage_numbers`.
- `eng_sim_set_rig(sim, i, capsules, zones, n)` : remplace le corps simplifié (3 capsules) d'un
  combattant par `n` ≤ 16 capsules calculées depuis un squelette animé (`eng_hurtboxset_world`).
  À appeler **avant chaque tick** ; le simulateur garde la pose précédente pour le balayage.
  `n = 0` revient au corps simplifié. `combat_debug` dessine ces capsules quand elles existent.
- **Preuve et limites** : `test_combat_rig` prouve, sur un squelette synthétique, que la touche
  suit la capsule posée par l'animation (touche si l'os est dans la zone, rate s'il en est
  écarté, entrée tardive au bon tick). **Aucun squelette Mixamo réel animé n'a été utilisé** et le
  lanceur ne branche pas encore ce chemin sur le personnage jouable.
