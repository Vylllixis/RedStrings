# Tranche verticale (critère 3 de M8)

Le critère : **30 min de jeu sans crash**, par un humain. Ce document dit comment lancer la tranche, ce
qu'elle contient, un déroulé de session et ce qu'il faut noter. Les preuves déjà obtenues de mon côté
(sans humain) sont en fin de page.

## Lancer

```sh
tools/scripts/tranche-verticale.sh --build    # construit la Release puis lance
tools/scripts/tranche-verticale.sh            # relance sans reconstruire
```

Le script génère une fois le terrain (`build/release/bin/tranche-verticale.hmap` : 1 km de côté,
513², 2 m, 25 m de relief, toujours le même) puis lance
`launcher --combat --terrain tranche-verticale.hmap --grass --placement`. Les options du lanceur
passées en plus sont transmises (par exemple `--day-night 21` pour commencer de nuit,
`--weapon greatsword` pour avoir l'espadon tout de suite). Journal : `build/release/bin/logs/` ;
rapport de crash éventuel : `build/release/bin/logs/crash-*.txt`.

## Ce qu'elle contient

- **Menu principal** : Nouvelle partie, Continuer (s'il y a une sauvegarde), Options (audio, caméra,
  texte, contraste, graphiques : FXAA, bloom, vsync, plein écran, ombres, distance de vue ;
  contrôles remappables), Quitter. Pause (P, ou Start à la manette) : reprendre, sauvegarder, options,
  menu principal.
- **Monde** : terrain doux avec herbe animée, forêt claire et rochers (KayKit, CC0), ombres en
  cascade (personnages, arbres), ciel analytique, brouillard, cycle jour / nuit de 20 min (étoiles,
  lune), éclairage par image qui suit le soleil.
- **Clairière** au centre (45 m) : le mannequin d'entraînement et un camp de 3 squelettes (soldat,
  brute, archer) qui patrouillent, donnent l'alerte et attaquent à tour de rôle ; le camp réapparaît
  10 s après avoir été battu.
- **Combat** : attaque légère (clic), lourde (clic droit), garde (E), parade (garde au bon moment),
  esquive (Ctrl), endurance, posture, verrouillage de cible (A / Q), arrêt sur impact, télégraphes
  des ennemis (lueur orange puis rouge).
- **Progression** : XP par ennemi, niveaux (PV, endurance, dégâts) ; **équipement** : l'espadon est
  gagné en battant le camp la première fois, R (croix droite à la manette) change d'arme.
- **Son** : effets, ambiance (vent, oiseaux le jour, grillons la nuit) et musique, volumes par bus.
- **Sauvegarde** : position, PV, endurance, ennemis, heure, progression, équipement.

## Déroulé proposé pour la session de 30 min

| Minutes | À faire |
|---|---|
| 0 – 3 | Menu principal ; Options : essayer vsync, plein écran, qualité des ombres ; revenir ; Nouvelle partie |
| 3 – 10 | Mannequin (attaques, garde, parade, esquive) puis le camp ; le battre pour gagner l'espadon |
| 10 – 15 | R : espadon ; rebattre le camp quand il réapparaît ; revenir à l'épée |
| 15 – 20 | Sortir de la clairière, marcher dans la forêt, attendre ou forcer la nuit (`--day-night 21` au lancement suivant) |
| 20 – 25 | Pause → Sauvegarder ; Menu principal → Quitter ; relancer ; Continuer : tout doit être là |
| 25 – 30 | Jouer librement ; mourir au moins une fois (réapparition) |

## Ce qu'il faut noter

- **Crash** : l'heure, ce qu'on faisait, et le fichier `logs/crash-*.txt`.
- **Blocage** (le jeu ne répond plus, personnage coincé), **saccades** (F3 affiche les temps), son
  coupé ou en boucle audible.
- **Ressenti** : lisibilité des attaques ennemies, réactivité, caméra, lisibilité de l'image de
  jour et de nuit, son (trop fort, trop répétitif ?).

## Limites connues (pas des bugs à signaler)

- Les arbres et rochers **n'ont pas de collision** : on les traverse (la clairière évite d'y combattre).
- Un seul modèle d'arbre et un seul de rocher ; le relief ne projette pas d'ombre.
- Pas de boss (M6-09), pas de quête (M8-09), pas d'inventaire d'objets.

## Preuves obtenues sans humain

- `launcher.combat_terrain_render` (ctest) : combat en pilote automatique sur un terrain doux avec
  placement : arbres KayKit, 4 cascades, 60 passes d'ombre, aucun dessin perdu ni rejeté.
- Mesure RTX 5070, 1280 × 720 sans vsync : 1,4 ms par image en moyenne (herbe, 5 424 arbres, ombres).
- Essai d'endurance en pilote automatique : voir `docs/devlog/m8-revue.md` (critère 3).
