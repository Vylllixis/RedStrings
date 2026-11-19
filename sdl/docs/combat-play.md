# Combat jouable (`--combat`) — raccord M4 + M5 + M6

Branche la simulation de combat (M5) et l'IA (M6) sur le personnage du mode jouable (M4), dans
l'arène de M4. Code : `launcher/src/play_combat.c`.

```
build/debug/bin/launcher --combat              # jouer (son activé ; --no-audio pour le couper)
build/debug/bin/launcher --combat-autoplay     # un pilote automatique joue (démonstration)
build/debug/bin/launcher --combat-selftest     # sans fenêtre : journalise « AUTOTEST COMBAT : OK »
build/debug/bin/launcher --combat-autoplay --screenshot capture.ppm 120   # image n° 120 en PPM
```

## Sur le terrain procédural (`--terrain`)

```
build/release/bin/assetc heightmap doux.hmap --size 513 --spacing 2 --alt-max 25 --wavelength 500
build/release/bin/launcher --combat --terrain doux.hmap [--grass --placement]
build/debug/bin/launcher --play-terrain-selftest doux.hmap   # « AUTOTEST TERRAIN : OK »
```

Le sol devient le champ de hauteurs (collision Jolt alignée sur le rendu, origine −côté/2) ; le
personnage, le mannequin et le camp sont posés sur le relief (le dessin suit la hauteur sous
chaque combattant). Les ennemis n'ont pas d'obstacles (pas de cubes sur le terrain). Un relief
**doux** est conseillé : sur un terrain raide (120 m de relief sur 1 km, comme celui de M3) le départ
peut tomber sur une pente non praticable et la caméra se colle au versant. Les dessins de l'IA
(cônes) suivent la hauteur sous le joueur seulement.

## Commandes (en plus de celles de `--play`)

| Action | Clavier / souris | Manette |
|---|---|---|
| Attaque légère (chaîne de 3) | clic gauche | X |
| Attaque lourde | clic droit | gâchette |
| Esquive (sans direction : pas en arrière) | Ctrl gauche | B |
| Garde (maintenir) ; parade = presser au bon moment | E | Y |
| Verrouillage (de nouveau : relâcher) | touche A en AZERTY (Q en QWERTY : même touche physique) | clic du stick droit |

Pendant une attaque, une esquive ou une réaction, le personnage ne marche pas (le combat décide).
Verrouillé, une attaque se tourne automatiquement vers la cible.

## Ce qui est à l'écran (fil de fer)

- Mannequin en (4, −1), devant le départ : poteau fixe, 1000 PV, se remet à neuf après 3 s sans coup.
- Camp vers (−14, −11) : grunt (rapide, ronde), brute (lent, lourd), archer (portée longue, fuit blessé).
- Hurtboxes par zone (tête / torse / membres ; gris = intouchable), arme **orange** pendant
  l'anticipation d'un ennemi (≥ 300 ms), **rouge** quand elle peut toucher.
- Barres au-dessus des têtes : PV (vert), posture (jaune). Cercle magenta : cible verrouillée.
- Cônes de vision des ennemis (couleur = état : vert ronde, jaune alerte, magenta poursuite,
  rouge engagé, bleu retour, cyan fuite), ligne vers la cible, anneau jaune = jeton d'attaque.
- Étincelles d'impact (particules) ; son des coups (synthétisé).
- Mort du joueur : renaissance au bout de 3 s et camp remis à neuf ; camp vaincu : il réapparaît
  au bout de 10 s.

## Données rechargeables

`assets/combat/attacks.txt` (frame data du joueur et des ennemis) est relu à chaud : enregistre,
l'effet est immédiat (refusé et journalisé si invalide ou si une attaque ennemie descend sous
300 ms ; différé si une attaque est en cours). `assets/combat/enemies.txt` (archétypes) est lu au
lancement. Sans ces fichiers, des valeurs intégrées identiques sont utilisées.

## Ce que l'autotest vérifie (12 mesures)

Dégâts sur le mannequin (10 coups légers = 100), verrouillage puis relâche, esquive (état Dodge,
recul de 3 m), camp : ennemis engagés, au plus 2 attaquants, coups reçus ; puis combat verrouillé
jusqu'à une victoire, toujours au plus 2 attaquants.

## Limites

- ~~Fil de fer seulement~~ : depuis la finition de M4, chevalier et squelettes animés (`docs/characters.md`) ; F3 montre les capsules. L'arène est celle de M4,
  pas la carte procédurale.
- Le personnage physique (Jolt) reste l'autorité de position du joueur : la simulation lui renvoie
  esquive, recul et séparation, bornés à 0,6 m par tick. Les ennemis se déplacent dans la
  simulation (plan), sans physique ni marches.
- Les lignes de l'IA (cônes) sont dessinées à hauteur fixe.
- Pas de texte à l'écran (nombres de dégâts calculés, non affichés) ; pas de HUD.
- Son : vérifié avec le pilote « dummy » chez moi, **jamais écouté** ; latence matérielle non mesurée.
- Ressenti (lisibilité, réactivité) : à juger sur ta machine ; c'est le critère « 2 testeurs » de M5.
- Captures faites chez moi en rendu logiciel (SwiftShader), pas sur un vrai GPU.
