# Débogueur d'animation (M4-16)

Module : `engine/anim_debug.h`, `engine/src/anim_debug.c`. Intégration : `launcher --skeleton F.skel --anim F.anim`
(ou `--anim F.anim` seul : le squelette est celui que désigne le clip).

## Ce qu'il fait

Un lecteur de clip sur un squelette, sans maillage : à chaque image il évalue le clip
(`eng_pose_sample`, bouclage `CLAMP`), calcule la pose monde (`eng_pose_world`) et dessine les os en
lignes **superposées** (visibles à travers la matière) à l'emplacement du modèle central.

| Élément | Couleur | Condition |
|---|---|---|
| os (parent → enfant) | cyan | toujours |
| os de l'articulation suivie | jaune | `selected >= 0` |
| axes locaux (0,15 m) | RVB | articulation suivie |
| trajectoire (96 segments sur la durée du clip) | magenta | suivie + `show_trail` |
| pose de repos | gris | `show_rest` |

## Réglages (`EngAnimDebug`, tous ramenés dans leur plage à chaque mise à jour)

`time` (0..durée), `speed` (−4..4, négatif = lecture à l'envers), `frame_rate` (cadence du pas à
pas, 30 par défaut), `paused`, `loop`, `show_rest`, `show_trail`, `selected` (−1..n−1).

- Sans boucle, atteindre une butée **arrête** le lecteur (pause) : relancer la lecture à la fin
  ne repart pas dans le vide.
- `eng_anim_debug_step(dbg, ±n)` avance de n images à `frame_rate`, met en pause, boucle ou se
  bloque selon `loop`. Le nombre d'images affiché est `floor(durée × cadence) + 1`.
- `eng_anim_debug_init` refuse un clip qui cible une articulation absente du squelette (restitue
  l'arène). Rien n'est alloué après l'init.

## Dans le lanceur

Panneau F1 → « Réglages » : pause, instant (curseur), vitesse, boucle, pose de repos, trajectoire,
articulation suivie ; ligne d'état en tête (`t 0.50 / 1.00 s | image 15 / 31 | x1.00 | lecture |
boucle | suivi : 1 a`).

Raccourcis (ignorés quand l'interface réclame le clavier) : **P** pause, **,** / **.** image
précédente / suivante, **J** / **K** articulation précédente / suivante (retour à « aucune »),
**T** trajectoire. Pas de conflit avec ZQSD/WASD, Espace, Maj, F1–F3, F11, Échap.

## Limites

- Jamais vu sur un écran réel (rendu hors écran + SwiftShader seulement).
- Pas de maillage animé ici : c'est le débogueur de squelette (le maillage skinné est M4-15 ;
  les poids ne sont pas encore cuits par `assetc`, M4-21).
- Un clip qui anime un nœud intermédiaire non-articulation est ignoré (voir animation-clips.md).
