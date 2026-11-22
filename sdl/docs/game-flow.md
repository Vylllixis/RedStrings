# Partie : menus, sauvegarde, options, interface (M8)

`launcher --combat` (ou `--combat --terrain fichier.hmap`) démarre sur le **menu principal**,
après l'écran de chargement animé (`docs/splash.md`). `--new-game` le saute ; `--combat-autoplay`
aussi (démonstration).

## Menus (M8-04) — `game_menu` (logique) + `ui_game` (rendu)

| Écran | Lignes |
|---|---|
| Principal | Continuer (la partie la plus récente ; grisé sans sauvegarde), Nouvelle partie, Charger une partie (grisé sans sauvegarde), Options, Quitter |
| Reprendre une partie | Partie 1, Partie 2, Partie 3 (résumé « Niv. 3 · 12 min » ; les emplacements vides sont grisés), Retour |
| Nouvelle partie | Partie 1, 2, 3 (tous choisissables ; le curseur est sur le premier libre), Retour |
| Écraser cette partie ? | Non, revenir (choisi par défaut) ; Oui, écraser la partie N |
| Pause (P / Start) | Reprendre, Sauvegarder, Options, Menu principal |
| Options | Volume général, effets, musique, ambiance, interface ; sensibilité ; inverser Y ; taille du texte ; contraste élevé ; anticrénelage (FXAA) ; halo lumineux (bloom) ; synchronisation verticale ; plein écran ; qualité des ombres (bas : sans filtre, moyen, haut) ; distance de vue (plan lointain x 0,5 / 1 / 1,5) ; Contrôles... ; Retour |
| Contrôles | une ligne par action (attaque, lourde, esquive, saut, garde, sprint, verrouillage, pause), Par défaut, Retour |

Navigation possible **sans souris** : haut/bas (ZQSD, stick) choisit, gauche/droite règle une
valeur, Espace / A ou clic / X valide, Ctrl / B revient, P / Start ouvre ou ferme la pause. **Avec
la souris** (2026-11-22) : la ligne survolée devient la ligne choisie quand la souris bouge, et le
clic la valide. Le jeu est figé quand un menu est ouvert (la souris est alors libérée). Remapper :
valider une action, puis presser la nouvelle touche (clavier, souris ou manette) ; elle remplace
les anciennes.

Le menu principal et les écrans des emplacements sont dessinés en colonne à gauche, sous le titre
du jeu, sur un dégradé sombre qui laisse voir le monde à droite ; pause, options et contrôles
restent au centre sur un voile.

## Trois emplacements de sauvegarde (2026-11-22)

- Fichiers `partie1.rssv`, `partie2.rssv`, `partie3.rssv` dans le dossier de préférences.
- **Continuer** reprend la partie enregistrée le plus récemment (date du fichier).
- **Nouvelle partie** : sans aucune sauvegarde, elle commence tout de suite dans l'emplacement 1 ;
  sinon elle fait choisir l'emplacement, et demande confirmation avant d'écraser une partie
  (« Non » est choisi par défaut). Elle est enregistrée aussitôt dans son emplacement.
- **Sauvegarder** (pause) écrit dans l'emplacement de la partie en cours.
- Une ancienne sauvegarde unique (`savegame.rssv`) est **copiée** dans l'emplacement 1 s'il est
  libre ; l'ancien fichier reste en place.
- Un fichier illisible compte comme occupé (« illisible ») : il n'est jamais écrasé sans
  confirmation.
- `--new-game` (développement) n'écrit rien au lancement ; « Sauvegarder » vise alors le premier
  emplacement libre (le troisième s'ils sont tous pris).
- `--save-dir DOSSIER` range les parties et les options ailleurs que dans le dossier de
  préférences (tests, essais sans toucher aux vraies parties).

### Dans le monde ouvert (`--world`)

Le monde ouvert a une origine flottante : les coordonnées locales du joueur changent à chaque
recentrage (tous les 512 m) et d'un lancement à l'autre. Avant le 2026-11-22 la sauvegarde
enregistrait ces coordonnées locales : reprendre une partie enregistrée loin du départ replaçait
le joueur au mauvais endroit. Maintenant (`GameFlowWorld`, `flow_place` dans le lanceur) :

- la sauvegarde enregistre la position **monde** du joueur ;
- reprendre une partie (ou en recommencer une depuis la pause) **charge le relief autour** de la
  position voulue (chargement bloquant des 9 cellules), répercute le recentrage de l'origine sur le
  jeu et les caméras, puis pose le joueur, au sol s'il était dessous ;
- les ennemis ne sont pas replacés un par un (leurs positions étaient locales) : chaque zone renaît
  quand le joueur s'en approche. Les boss vaincus, eux, le restent (section GEAR).

Test : `launcher.world_save` (`--save-selftest`) : enregistré devant la zone 7 (à 1,5 km du
château), envoyé au château, repris : revenu à sa position monde à 0,5 m près, au sol, après deux
recentrages de l'origine ; le point de départ du jeu n'a pas bougé dans le monde.

## Sauvegarde (M8-05) — `save_game`

Format « RSSV » : en-tête (signature, version, nombre de sections, FNV-1a), sections étiquetées
(PLYR, WRLD, ENMY, et facultatives TIME : heure du cycle jour / nuit, PROG : niveau et XP). Section inconnue ignorée ; somme fausse, section tronquée ou valeur non finie :
fichier refusé en entier. **Migration** : un fichier v1 (sans posture, PV max, temps de jeu ni
ennemis) est relu en v2 avec des valeurs par défaut, puis réécrit en v2. Écriture atomique
(fichier temporaire + renommage). Emplacement : dossier de préférences (`~/.local/share/RedStrings/game/`
sous Linux) : `partie1.rssv` à `partie3.rssv`, `options.txt`. Sections facultatives TIME (heure), PROG (niveau),
EQUI (armes possédées et arme en main, M8-08 ; absente : épée et bouclier ; incohérente : fichier
refusé).

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
jeu, PV, endurance et ennemis identiques ; options relues après relance ; puis les trois
emplacements : choix de l'emplacement d'une nouvelle partie, enregistrement immédiat,
« Sauvegarder » dans l'emplacement en cours sans toucher aux autres, « Charger une partie »,
confirmation avant d'écraser (« Non » ne change rien, « Oui » remplace), copie de l'ancienne
sauvegarde unique.

## Options graphiques (finition M8)

Clés de `options.txt` : `graphics.fxaa`, `graphics.bloom`, `graphics.vsync`, `graphics.fullscreen`,
`graphics.shadows` (0, 1, 2), `graphics.view_distance` (0, 1, 2). Pendant le jeu, ces options
**priment sur `config.ini`** (vsync, plein écran) ; F11 met l'option « Plein écran » à jour. Chaque
application est journalisée (« options graphiques : vsync non (présentation immédiate), plein écran
non, ombres bas, plan lointain 1500 m, FXAA oui, bloom 50 % »). Vérifié avec un `options.txt` isolé
(`XDG_DATA_HOME`) : vsync coupée, ombres basses, plan lointain 1 500 m appliqués au lancement.
Manquent encore : SSAO, échelle de résolution, bloom en chaîne, FXAA 3.11 / SMAA, coût mesuré par
effet.
