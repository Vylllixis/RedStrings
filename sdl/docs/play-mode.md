# Mode jouable (`--play`)

Assemble les briques de M4 dans le lanceur : personnage (M4-06), saut (M4-08), locomotion (M4-07),
sprint et endurance (M4-09), caméra orbitale (M4-10) avec collision (M4-11). Code : `launcher/src/play_mode.c`.

```
build/debug/bin/launcher --play --ui        # --ui : panneau F1 ouvert (endurance, réglages)
build/debug/bin/launcher --play-selftest    # sans fenêtre ; journalise « AUTOTEST JEU : OK »
```

## Commandes

| Action | Touche |
|---|---|
| Déplacement (relatif à la caméra) | ZQSD / WASD, stick gauche |
| Regard | souris (capturée ; F2 libère), stick droit |
| Distance de la caméra | molette (1 à 12 m) |
| Saut (relâcher tôt = saut court) | Espace |
| Sprint (consomme l'endurance) | Maj gauche |
| Panneau de debug | F1 ; F3 : grille et repères de la démo |

## Arène (boîtes grises, visibles en fil de fer)

Sol de la démo (±28 m, dessus à y = −0,5), les 48 cubes de la démo rendus **solides** (le cube central
est le modèle : non solide), puis dans le couloir z = 4 (entre deux rangées de cubes) :

- vers **+X** : marches de 0,15 / 0,30 / 0,45 m (x = 10, 13, 16), puis un mur de 3 m (x = 24) ;
- vers **−X** : plateforme de 1,2 m (x de −8 à −5), rampe de 18° (x de −12 à −18, 2 m de haut).

Départ en (4, 4), regard vers +X. Le personnage est dessiné en fil de fer (capsule du debug draw
physique, flèche jaune = direction) : pas de modèle animé avant les tâches d'animation de M4.

## Réglages en direct (panneau F1)

endurance, vitesse de course (m/s), vitesse de saut (m/s), contrôle en l'air, distance caméra (m).
Valeurs de départ (à juger à la sensation) : marche 4,5 m/s ; sprint ×1,6 ; saut 6 m/s (apex 1,8 m) ;
chute ×1,6 ; coyote 0,10 s ; buffer 0,12 s ; contrôle en l'air 0,35.

## Ce que l'autotest vérifie (26 mesures, entrées scriptées à 60 Hz)

Pose au sol ; marche 4,5 m/s et arrêt net ; sprint 7,2 m/s et endurance 100 → 70 en 1,5 s ;
épuisement, refus de courir, reprise ; saut (apex 1,79 m, 64 pas en l'air) et saut court ;
marches de 0,15 et 0,30 m franchies, 0,45 m bloquante ; caméra dos au mur (1,55 m au lieu de 4,
l'œil reste devant) ; rampe suivie sans décoller ; saut sur la plateforme de 1,2 m.

## Limites

- Aucune de ces vérifications ne voit l'écran : le rendu (fil de fer, caméra, fluidité) est à juger
  sur ta machine. Le flux d'entrées est scripté, pas celui de SDL (clavier, souris, manette).
- Marche de 0,15 à 0,30 m franchie à 4,5 m/s avec un léger à-coup de 2 à 3 pas (la capsule bute sur
  l'arête avant que le pas d'escalier de Jolt ne la relève).
- Au-delà de 7,2 m/s (curseur « vitesse de course » à fond), une marche de 0,30 m fait encore
  rebondir le personnage (0,19 m mesuré à 12 m/s).
- Pas de ciel/terrain/rochers de la démo en collision : seuls le sol, les cubes et l'arène existent
  pour la physique.
