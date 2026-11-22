# Écran-titre et menus animés (2026-11-22)

## Le problème

Le menu principal était dessiné par-dessus le jeu déjà lancé : une colonne de texte sur un
dégradé, sans animation, et le monde figé derrière. Théo : « le menu est dégueulasse », il le
voulait « ultra bien animé en motion design », comme l'écran de chargement.

## Ce qui est fait

- **La scène de l'écran de chargement continue derrière le menu** (`shaders/splash.frag.hlsl`,
  `launcher/src/title.{h,c}`) : l'aube, le chevalier, le fil rouge, le titre. Il n'y a pas de
  coupure entre le chargement et le menu : la corde de chargement devient la barre du menu, sa
  perle glisse jusqu'à la ligne choisie, le titre se range. Le jeu n'est pas dessiné tant que
  l'écran-titre est affiché.
- **Le fil rouge est la colonne vertébrale du menu** (`engine/src/ui_menu.c`,
  `ui_menu_cards.c`) :
  - menu principal : les lignes sont posées sur le fil, en bas de l'écran ; le fil vibre quand on
    change de ligne ; sous le fil, une phrase dit ce que fait la ligne choisie ;
  - parties : trois **cartes suspendues au fil** par un nœud et deux brins ; chacune montre une
    vignette du lieu de la sauvegarde (château, plaine, montagnes, désert, volcan), le nom, le
    niveau, le temps de jeu, la date, les boss vaincus ; la carte choisie est cousue de rouge ; la
    carte visée vient à gauche et ses actions s'ouvrent à droite ; une carte supprimée tombe (son
    fil est coupé), une carte créée descend le long du fil ; les cartes se balancent quand on les
    déplace ;
  - pause, options, contrôles : une colonne de lignes ; les curseurs sont des perles sur un fil ;
    la liste défile en douceur quand elle déborde.
- **Rien n'apparaît d'un coup** (`engine/menu_motion`) : entrées en cascade, ressorts amortis
  (curseur, cartes, cadrage), impulsions (validation, refus : la ligne tremble, réglage).
- **La scène se recadre** selon l'écran : zoom, léger déplacement en parallaxe (les plans lointains
  bougent moins), assombrissement derrière les cartes et les listes ; le titre monte et rétrécit.
- **Fondus** entre le titre et le jeu (`docs/game-flow.md`).
- Contraste élevé et taille du texte (options) sont respectés : pavés blancs, fonds opaques.

## Comment c'est fait

| Module | Rôle |
|---|---|
| `engine/game_menu` | la logique : écrans, lignes, actions (aucun rendu) |
| `engine/menu_motion` | l'animation : état pur (ressorts, cascades, impulsions, cibles des cartes, cadrage de la scène), avancé une fois par image ; testable sans GPU |
| `engine/ui_menu` | le dessin, avec les listes de dessin de Dear ImGui : texte aux lettres espacées, fil (trait net + halo), perle, groupes transformés après coup (rotation, échelle, opacité des sommets déjà émis : c'est ce qui fait tourner une carte entière autour de son nœud) |
| `launcher/title` | la scène derrière le menu : elle réutilise le pipeline et le titre de l'écran de chargement, dans la passe de l'interface, juste avant elle |
| `launcher/game_flow` | le déroulé : actions, fondus, fichiers des parties |

Le shader reçoit trois vecteurs de plus (`menu`, `view`, `title`) : poids du menu (la corde de
chargement s'efface), assombrissement, cadrage (déplacement et zoom ; chaque plan de la scène se
déplace selon sa profondeur, et les attaches du fil suivent chacune leur plan), place et taille du
titre. Pendant le chargement ces valeurs sont neutres : l'image est celle d'avant.

Sans écran de chargement (`--no-splash`, lancements automatiques), il n'y a pas de scène : le menu
assombrit le jeu et écrit le titre lui-même.

## Essayer, capturer

```
./launcher --world 7 --combat              # l'écran-titre, en plein écran
./launcher --world 7 --combat --windowed   # en fenêtre
```

Développement : `--menu-script "40:ok,50:text=Aube,60:ok"` joue des touches dans le menu (un tick
par image : même déroulé sur toute machine) ; `mouse=X/Y` et `click` pour la souris ;
`--shots DOSSIER 30,60,90` écrit ces images en PPM ; `--splash` force la scène du titre avec
`--frames`.

## Tests

- `engine.menu_motion` (9 tests) : courbes, ressorts (stables avec une image d'une demi-seconde),
  cascade, entrée et sortie des écrans, cartes (rangée, carte visée, fenêtre étroite, chute d'une
  partie supprimée), défilement, cadrage de la scène.
- `engine.game_systems` : navigation (flèches, Entrée, Échap, répétition), gestion des parties,
  saisie du nom, souris, noms des touches.
- `launcher.title_menu`, `launcher.menu_plain`, `launcher.menu_mouse` : le menu rendu, piloté de
  bout en bout (voir `docs/game-flow.md`).

## Limites (dites clairement)

- **Rien n'a été vu sur un vrai écran** : tout est rendu hors écran, par logiciel. Le rythme des
  animations sur une vraie carte graphique, à 144 ou 240 Hz, n'a pas été regardé.
- En plein écran, la fenêtre est affichée avant sa première image (une fraction de seconde) : la
  fenêtre cachée jusqu'à sa première image n'existe qu'en fenêtre.
- Pas de son dans les menus.
- La vignette d'une carte est un dessin du lieu, pas une capture de la partie.
- Les options sont une seule longue liste (pas de catégories).
