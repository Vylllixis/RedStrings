# Partie : menus, sauvegarde, options, interface (M8)

`launcher --combat` (ou `--combat --terrain fichier.hmap`) démarre sur le **menu principal**.
`--new-game` le saute ; `--combat-autoplay` aussi (démonstration).

## Menus (M8-04) — `game_menu` (logique) + `ui_game` (rendu)

| Écran | Lignes |
|---|---|
| Principal | Continuer (grisé sans sauvegarde), Nouvelle partie, Options, Quitter |
| Pause (P / Start) | Reprendre, Sauvegarder, Options, Menu principal |
| Options | Volume général, effets, musique, ambiance, interface ; sensibilité ; inverser Y ; taille du texte ; contraste élevé ; anticrénelage (FXAA) ; halo lumineux (bloom) ; synchronisation verticale ; plein écran ; qualité des ombres (bas : sans filtre, moyen, haut) ; distance de vue (plan lointain x 0,5 / 1 / 1,5) ; Contrôles... ; Retour |
| Contrôles | une ligne par action (attaque, lourde, esquive, saut, garde, sprint, verrouillage, pause), Par défaut, Retour |

Navigation **sans souris** : haut/bas (ZQSD, stick) choisit, gauche/droite règle une valeur,
Espace / A ou clic / X valide, Ctrl / B revient, P / Start ouvre ou ferme la pause. Le jeu est figé
quand un menu est ouvert (la souris est alors libérée). Remapper : valider une action, puis
presser la nouvelle touche (clavier, souris ou manette) ; elle remplace les anciennes.

## Sauvegarde (M8-05) — `save_game`

Format « RSSV » : en-tête (signature, version, nombre de sections, FNV-1a), sections étiquetées
(PLYR, WRLD, ENMY, et facultatives TIME : heure du cycle jour / nuit, PROG : niveau et XP). Section inconnue ignorée ; somme fausse, section tronquée ou valeur non finie :
fichier refusé en entier. **Migration** : un fichier v1 (sans posture, PV max, temps de jeu ni
ennemis) est relu en v2 avec des valeurs par défaut, puis réécrit en v2. Écriture atomique
(fichier temporaire + renommage). Emplacement : dossier de préférences (`~/.local/share/RedStrings/game/`
sous Linux) : `savegame.rssv`, `options.txt`.

Contenu sauvegardé : position (double), cap, PV, PV max, endurance, posture, temps de jeu, heure du jour (docs/day-night.md), niveau et XP (docs/progression.md), et pour
chaque ennemi du camp : vivant, PV, position, cap. **Pas sauvegardé** : le mannequin, les cellules du
monde ouvert (la section WRLD existe mais le mode combat ne l'utilise pas encore).

## Interface (M8-03) — `ui_game`

Barres PV / endurance / posture en haut à gauche, cible verrouillée (nom de l'archétype, PV,
posture) en haut au centre, nombres de dégâts projetés au-dessus des têtes, message central (mort,
camp vaincu, niveau atteint), niveau et barre d'XP sous les barres du joueur, heure du jour en haut à droite, rappel des commandes en bas. Taille proportionnelle à la hauteur de l'écran
(référence 1080 p) × « taille du texte ». **Limite** : police bitmap d'ImGui agrandie (floue en 4K) ;
une police vectorielle (stb_truetype) reste à brancher.

## Audio (M8-01) — `mixer`

4 bus (effets, musique, ambiance, interface), volumes appliqués en direct ; `eng_audio_spatialize`
(atténuation en 1/d, panoramique, léger affaiblissement derrière). **Limites** : pas de musique ni
d'ambiance dans le jeu (aucun fichier son) ; seuls les coups utilisent le mixeur ; rien n'a été écouté.

## Autotest

`launcher --game-selftest` (ctest `launcher.game`) : menu principal, nouvelle partie, marche,
pause, sauvegarde, menu principal, destruction complète, relance, Continuer : position, temps de
jeu, PV, endurance et ennemis identiques ; options relues après relance.

## Options graphiques (finition M8)

Clés de `options.txt` : `graphics.fxaa`, `graphics.bloom`, `graphics.vsync`, `graphics.fullscreen`,
`graphics.shadows` (0, 1, 2), `graphics.view_distance` (0, 1, 2). Pendant le jeu, ces options
**priment sur `config.ini`** (vsync, plein écran) ; F11 met l'option « Plein écran » à jour. Chaque
application est journalisée (« options graphiques : vsync non (présentation immédiate), plein écran
non, ombres bas, plan lointain 1500 m, FXAA oui, bloom 50 % »). Vérifié avec un `options.txt` isolé
(`XDG_DATA_HOME`) : vsync coupée, ombres basses, plan lointain 1 500 m appliqués au lancement.
Manquent encore : SSAO, échelle de résolution, bloom en chaîne, FXAA 3.11 / SMAA, coût mesuré par
effet.
