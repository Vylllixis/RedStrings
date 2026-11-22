# Partie : menus, sauvegarde, options, interface (M8)

`make sdl` à la racine du dépôt compile le jeu en Release et le lance (`make sdl ARGS="--windowed"`
pour ajouter des options du lanceur). C'est `launcher --world 7 --combat`.

`launcher --combat` (ou `--world 7 --combat`) démarre sur l'**écran-titre**, dans la continuité de
l'écran de chargement animé (`docs/splash.md`, `docs/title-menu.md`). `--new-game` le saute ;
`--combat-autoplay` aussi (démonstration). Lancé à la main, le jeu s'ouvre **en plein écran** tant
que les options ne disent pas le contraire (`--windowed` : en fenêtre pour ce lancement ; F11 ou
l'option « Plein écran » ensuite). Les lancements automatiques (`--frames`, autotests) restent en
fenêtre.

## Touches des menus (2026-11-22)

| Geste | Clavier | Manette | Souris |
|---|---|---|---|
| ouvrir le menu en jeu | **Échap**, P | Start | |
| choisir | flèches, ZQSD | croix, stick gauche | survol (quand elle bouge) |
| valider | **Entrée**, Espace | A, X | clic sur la ligne |
| régler une valeur | gauche / droite | gauche / droite | clic à gauche ou à droite de la valeur |
| revenir d'un niveau | **Échap**, Ctrl, P | B, Start | « Retour » |

- **Échap ne ferme plus le jeu** : en jeu il ouvre la pause ; dans un menu il revient d'un niveau,
  sur la ligne d'où l'on venait ; à la racine de la pause il reprend la partie ; à l'écran-titre il
  ne fait rien. Le jeu se ferme par « Quitter », la croix de la fenêtre ou Alt+F4. (Les outils sans
  menu, démonstration et éditeur, gardent Échap = fermer : `EngPlatform.escape_quits`.)
- Flèches, Entrée, Échap et croix de la manette sont des **touches de menu fixes**
  (`ENG_MENU_KEY_*` dans `EngInputState`) : elles ne dépendent pas des liaisons, on ne peut pas les
  perdre en remappant.
- Un clic ne valide que la ligne sous la souris (avant : il validait la ligne choisie, où que soit
  la souris).
- Le jeu est figé quand un menu est ouvert (la souris est alors libérée).
- Remapper : valider une action, puis presser la nouvelle entrée (touche du clavier, **flèche
  comprise**, bouton de souris ou de manette, gâchette, ou **cran de molette**) ; elle remplace les
  anciennes ; Échap annule. Les liaisons s'affichent par leur nom (« Espace, manette A », « molette
  haut », « Flèche bas »), dans la disposition du clavier (AZERTY : « Z » pour avancer).
- L'entrée choisie n'agit pas aussitôt sur le menu : une flèche (touche de menu, encore enfoncée)
  ne déplace pas la sélection tant qu'elle n'est pas relâchée, et le cran de molette qui a servi à
  choisir ne presse pas l'action.

## Écrans — `game_menu` (logique), `menu_motion` (animation), `ui_menu` (dessin)

| Écran | Lignes |
|---|---|
| Principal | Continuer (la partie la plus récente, nommée) — ou « Nouvelle partie » s'il n'y en a aucune —, Parties, Options, Quitter |
| Parties | les trois emplacements (cartes), Retour |
| Partie (emplacement occupé) | Jouer, Renommer, Dupliquer (grisé sans emplacement libre), Supprimer, Retour |
| Nom de la partie | le nom, saisi au clavier ; à la création : Mode de jeu (Aventure / Créatif) ; Créer la partie (ou Renommer), Annuler |
| Supprimer cette partie ? | Non, garder la partie (choisi par défaut) ; Oui, supprimer |
| Pause | Reprendre, Sauvegarder, Options, Sauvegarder et quitter |
| Options | Volume général, effets, musique, ambiance, interface ; sensibilité ; inverser Y ; taille du texte ; contraste élevé ; anticrénelage (FXAA) ; halo lumineux (bloom) ; synchronisation verticale ; plein écran ; qualité des ombres ; distance de vue ; SSAO ; échelle de résolution ; Contrôles... ; Retour |
| Contrôles | une ligne par action (onze : les neuf du combat, puis « Vol créatif : accélérer » et « Vol créatif : ralentir »), Par défaut, Retour |

## Gestion des parties (2026-11-22) : comme la liste des mondes de Minecraft

Trois emplacements : `partie1.rssv`, `partie2.rssv`, `partie3.rssv` dans le dossier de préférences
(`~/.local/share/RedStrings/game/`). Chaque partie a un **nom**, et sa carte montre son niveau, son
temps de jeu, le lieu et la date de sa dernière sauvegarde, et les boss vaincus.

- **Créer** : choisir un emplacement libre (ou « Nouvelle partie » à l'écran-titre quand il n'y en a
  aucune), taper un nom (22 caractères au plus ; « Partie N » est proposé, la première frappe le
  remplace ; un nom vide est refusé), Entrée. La partie est enregistrée aussitôt.
- **Mode de jeu** (2026-11-22) : il se choisit à la création, sur la première ligne (flèche haut,
  puis gauche / droite : Aventure ou Créatif ; le curseur part de « Créer la partie », donc Entrée
  crée une partie d'aventure sans y passer). Créatif : invincible et vol libre
  (`docs/combat-play.md`). Le mode est enregistré avec la partie (section META), repris avec elle,
  affiché sur sa carte, et **ne se change pas en jeu** ni en la renommant. `--new-game --creative`
  lance directement une partie créative.
- **Jouer** : « Continuer » reprend la partie enregistrée le plus récemment ; « Parties » permet de
  choisir.
- **Renommer** : le nom actuel est proposé ; le fichier garde tout le reste, sa date comprise
  (renommer ne fait pas d'une partie « la plus récente »).
- **Dupliquer** : copie dans le premier emplacement libre, nommée « <nom> (copie) », datée une
  seconde avant l'original (« Continuer » reste sur l'original).
- **Supprimer** : après confirmation (« Non » par défaut ; Échap annule). Le fichier n'est pas
  détruit : il est renommé `partieN.rssv.supprimee` (une seule partie supprimée gardée par
  emplacement), pour qu'une suppression par erreur reste rattrapable à la main.
- **En jeu** : « Sauvegarder » écrit dans l'emplacement de la partie en cours ; **« Sauvegarder et
  quitter »** enregistre puis revient à l'écran-titre (si l'écriture échoue, on reste en pause avec
  le message d'échec). Fermer la fenêtre n'enregistre pas.
- Pour recommencer dans un emplacement occupé : le supprimer, puis créer (il n'y a plus d'écran
  « écraser »).
- Une ancienne sauvegarde unique (`savegame.rssv`) est **copiée** dans l'emplacement 1 s'il est
  libre ; l'ancien fichier reste en place. Une sauvegarde d'avant les noms s'appelle « Partie N »
  et porte la date de son fichier.
- Un fichier illisible compte comme occupé : il n'est jamais proposé à « Continuer » ni comme
  emplacement libre ; il ne peut qu'être supprimé.
- Pendant la saisie du nom, le clavier **écrit** : Espace, ZQSD, P, Ctrl ne commandent rien
  (`eng_menu_text_input`, `EngPlatform.text`). Entrée valide, Échap annule ; à la manette, A valide
  le nom proposé.
- `--new-game` (développement) n'écrit rien au lancement ; « Sauvegarder » vise alors le premier
  emplacement libre (le troisième s'ils sont tous pris).
- `--save-dir DOSSIER` range les parties et les options ailleurs que dans le dossier de
  préférences (tests, essais sans toucher aux vraies parties).

Fichiers : `launcher/src/game_slots.{h,c}` (décrire, renommer, copier, supprimer),
`launcher/src/game_flow.{h,c}` (déroulé, fondus), section META de la sauvegarde (plus bas).

### Fondus entre le titre et le jeu

Avec l'écran de chargement (lancement à la main) : choisir une partie éteint la scène du titre
(éclat du soleil puis noir, 0,7 s), la partie est chargée dans le noir (le relief du monde aussi),
puis le jeu apparaît en fondu ; « Sauvegarder et quitter » assombrit le jeu puis fait apparaître le
titre. L'action attend la fin de son fondu de sortie. Sans écran de chargement (tests) : tout est
immédiat.

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

Section facultative **META** (2026-11-22) : nom de la partie et lieu de la sauvegarde (longueur
puis octets UTF-8), date (secondes depuis 1970, 64 bits), puis des drapeaux (32 bits, eux-mêmes
facultatifs : bit 0 = partie en mode créatif). Sans changer de version : un jeu plus
ancien ignore la section. Un texte plus long que la borne fait refuser le fichier ; les octets de
contrôle d'un nom deviennent des espaces.

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

`launcher --game-selftest` (ctest `launcher.game`, 74 mesures) pilote le menu comme un joueur
(touches de menu, texte tapé), dans un dossier à part : création d'une partie nommée, les touches
du jeu ignorées pendant la saisie, marche, Échap et P (pause), « Sauvegarder », « Sauvegarder et
quitter », destruction complète, relance, « Continuer » : position, temps de jeu, heure, PV,
endurance, niveau et ennemis identiques ; options relues ; puis les emplacements : nom vide
refusé, enregistrement immédiat, « Sauvegarder » sans toucher aux autres parties, renommer (date et
état intacts), dupliquer, supprimer (« Non » puis « Oui », fichier mis de côté), jouer depuis la
liste, fondus, fichier illisible (jamais proposé à « Continuer »), ancienne sauvegarde unique,
carte d'une partie (résumé, date, boss, nom d'une copie), plein écran par défaut.

De bout en bout avec le rendu (un tick par image, `--menu-script`) : `launcher.title_menu` (avec la
scène du titre : créer, jouer, « Sauvegarder et quitter », renommer, dupliquer, supprimer),
`launcher.menu_plain` (sans scène, de vrais événements Échap : la fenêtre ne se ferme pas),
`launcher.menu_mouse` (survol, clics, de vrais événements de texte) ; `launcher.world_save` vérifie
aussi le lieu (« Désert », « Château ») et les boss de la carte de la partie.

## Options graphiques (finition M8)

Clés de `options.txt` : `graphics.fxaa`, `graphics.bloom`, `graphics.vsync`, `graphics.fullscreen`,
`graphics.shadows` (0, 1, 2), `graphics.view_distance` (0, 1, 2). Pendant le jeu, ces options
**priment sur `config.ini`** (vsync, plein écran) ; F11 met l'option « Plein écran » à jour. Chaque
application est journalisée (« options graphiques : vsync non (présentation immédiate), plein écran
non, ombres bas, plan lointain 1500 m, FXAA oui, bloom 50 % »). Vérifié avec un `options.txt` isolé
(`XDG_DATA_HOME`) : vsync coupée, ombres basses, plan lointain 1 500 m appliqués au lancement.
Manquent encore : SSAO, échelle de résolution, bloom en chaîne, FXAA 3.11 / SMAA, coût mesuré par
effet.
