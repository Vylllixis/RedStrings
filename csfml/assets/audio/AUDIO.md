# Le système audio de RedStrings

Guide pratique pour changer un son, en ajouter un ou créer une nouvelle identité sonore,
**sans lire tout le moteur**. Il concerne la version graphique (`./redstrings_csfml`) :
la version terminal n'a pas de son, pour rester sans aucune dépendance externe.

## 1. Comment ça marche

```
écran (menu, arène…)             moteur audio (csfml/src/audio/)                 fichiers
─────────────────────            ──────────────────────────────────              ─────────────────────────
son(app, SON_CARTE_SELECTION) →  1. pack du thème actif : défini ?  ─ oui ─→      yugioh/pose_carte_1.ogg
                                 2. sinon pack standard : défini ?  ─ oui ─→      standard/carte_selection_1.ogg
                                 3. sinon : silence (aucune erreur)
                                 puis : rythme, variante, voix, volume
```

- Les écrans ne parlent qu'en **événements** (`SON_…`, déclarés dans `csfml/include/gui_sons.h`).
  Ils ne connaissent aucun nom de fichier.
- Chaque **pack** (un dossier par thème) a un **manifeste** `sons.txt` qui dit quel fichier
  jouer pour chaque événement. On change un son en éditant ce texte, sans recompiler.
- La plupart des sons ne sont même pas appelés par les écrans : `audio_suivi.c` compare l'état
  des écrans d'une image à l'autre (une carte ajoutée, une fiche ouverte, le combat qui se
  termine…) et en déduit l'événement. Un son ne peut donc pas partir deux fois par accident.

| Fichier | Rôle |
|---|---|
| `csfml/include/gui_sons.h` | événements, packs, réglages : tout ce qui se teste sans carte son |
| `csfml/src/audio/audio_evenements.c` | la table des événements : nom, priorité, délai, lectures simultanées |
| `csfml/src/audio/audio_manifeste.c` | lecture et validation des `sons.txt` |
| `csfml/src/audio/audio_regles.c` | repli entre thèmes, variantes, volumes, rythme, choix des voix |
| `csfml/src/audio/audio_reglages.c` | le fichier `reglages_audio.txt` du joueur |
| `csfml/src/audio/audio_moteur.c` | création, chargement des packs, sons décodés en mémoire, changement de thème |
| `csfml/src/audio/audio_lecture.c` | `son()`, `son_secondaire()`, `son_regle()` : la seule porte d'entrée |
| `csfml/src/audio/audio_musique.c` | musiques en boucle, fondus enchaînés, musique baissée sous les sons importants |
| `csfml/src/audio/audio_suivi.c` | les changements d'état des écrans deviennent des événements |
| `csfml/src/audio/audio_panneau.c` | le panneau F2 et la coupure F3 |

## 2. Où sont les ressources

```
csfml/assets/audio/
├── standard/          pack « Standard » : sons.txt + bruitages + musique_menu / musique_combat
├── yugioh/            pack « Yu-Gi-Oh! » : même organisation
├── outils/            générateur des sons (Python pur) et convertisseur WAV → OGG
├── LICENCES.md        provenance et licence de chaque fichier
└── AUDIO.md           ce guide
```

Formats acceptés : **OGG Vorbis** (recommandé), WAV, FLAC. Les bruitages sont décodés une fois
au lancement (moins de 2 Mo en mémoire) ; les musiques sont lues en continu depuis le disque.

## 3. Associer un son à un événement

Une ligne du `sons.txt` du pack :

```
# evenement          options…                               fichiers (1 à 4)
carte.selection      volume=0.65                            pose_carte_1.ogg pose_carte_2.ogg
combat.victoire      volume=0.95  rare=victoire_rare.ogg@0.05  victoire.ogg
ui.saisie            volume=0.35  hauteur=0.10              saisie_1.ogg saisie_2.ogg
musique.combat       volume=0.70                            musique_combat.ogg
```

| Option | Effet |
|---|---|
| plusieurs fichiers | l'un d'eux au hasard, **jamais deux fois de suite** |
| `volume=0..1` | gain de ce son dans le mélange |
| `hauteur=0..0.5` | variation aléatoire de hauteur à chaque lecture (remplace celle du moteur) |
| `rare=fichier.ogg@0.05` | variante jouée avec cette probabilité (ici 1 fois sur 20) |

Les noms d'événements sont dans `csfml/src/audio/audio_evenements.c` (tableau `EVENEMENTS`).
Une ligne fausse (événement inconnu, fichier refusé, volume hors bornes) est **ignorée et
signalée une fois** sur la sortie d'erreur ; le reste du pack fonctionne.

## 4. Remplacer un son existant

Deux façons, sans recompiler :

1. **même nom** : remplacer le fichier (`csfml/assets/audio/standard/victoire.ogg`) ;
2. **autre fichier** : le copier dans le dossier du pack, puis changer le nom dans `sons.txt`.

Relancer le jeu suffit. `make csfml-tests` vérifie ensuite que chaque fichier cité existe, et
qu'aucun `.ogg` du dossier n'est oublié.

## 5. Ajouter un nouveau bruitage

Pour un **nouvel événement** (par exemple « une classe rare est choisie ») :

1. `csfml/include/gui_sons.h` : ajouter `SON_CLASSE_RARE` dans `son_t` (avant `NB_SONS`) ;
2. `csfml/src/audio/audio_evenements.c` : ajouter sa ligne dans `EVENEMENTS`, par exemple
   `[SON_CLASSE_RARE] = {"classe.rare", 2, 0.2f, 1, 0.f}` (nom, priorité 1-3, délai minimum
   en secondes, lectures simultanées, variation de hauteur) ;
3. le déclencher : `son(app, SON_CLASSE_RARE);` à l'endroit où cela arrive, ou dans
   `audio_suivi.c` si c'est un changement d'état d'un écran ;
4. `standard/sons.txt` : `classe.rare  volume=0.6  classe_rare.ogg` (le pack standard doit
   couvrir **tous** les événements : c'est le repli, et un test le vérifie) ;
5. facultatif : une ligne dans les autres packs, pour leur version du son.

Pour la **position** gauche-droite ou une **hauteur** imposée (un tic qui monte avec une
valeur, un coup du côté de la cible) : `son_regle(app, SON_UI_VALEUR, 1.2f, 0.f)` (hauteur ×1,2,
centré) ou `son_regle(app, SON_COMBAT_IMPACT, 1.f, -0.4f)` (à gauche).

Pour un son de **repli** qui ne doit pas s'empiler sur un son plus parlant de la même image
(c'est le cas des notifications) : `son_secondaire(app, SON_UI_ERREUR)`.

## 6. Créer un nouveau pack audio

**Remplacer l'identité d'un pack existant** : il suffit de remplacer les fichiers et le
`sons.txt` du dossier (`standard/` ou `yugioh/`).

**Ajouter un troisième thème** (par exemple « rétro ») :

1. créer `csfml/assets/audio/retro/` avec son `sons.txt` ; il peut ne définir que quelques
   événements : les autres viendront du pack standard ;
2. `gui_sons.h` : ajouter `THEME_RETRO` dans `theme_son_t` et `CHOIX_THEME_RETRO` dans
   `choix_theme_t` ;
3. `audio_evenements.c` : ajouter `[THEME_RETRO] = {"Rétro", "retro"}` dans `THEMES` ;
4. `audio_regles.c` (`theme_effectif`) et `audio_reglages.c` (`CHOIX`) : une ligne chacun ;
5. `audio_panneau.c` (`ligne_theme`) : le libellé du nouveau bouton dans `libelles[]` ; le
   panneau affiche alors un bouton de plus.

Aucun écran n'est à modifier.

## 7. Activer le thème Yu-Gi-Oh!

| Comment | Effet |
|---|---|
| `./redstrings_csfml "Yu-Gi-Oh!"` | mode visuel Yu-Gi-Oh! ; le réglage **Auto** choisit alors le thème sonore Yu-Gi-Oh! |
| **F2** dans le jeu, ligne *Thème sonore* | Auto, Standard ou Yu-Gi-Oh!, à chaud : les sons et la musique changent tout de suite |
| `reglages_audio.txt` : `theme=yugioh` | même chose, conservé entre deux parties |

Les sons et le visuel sont donc indépendants : on peut jouer en mode visuel normal avec les
sons Yu-Gi-Oh!, ou l'inverse. Trois sons très fréquents et neutres (`ui.saisie`,
`ui.effacement`, `ui.valeur`) viennent volontairement du pack standard.

## 8. Régler les volumes

- **F2** : panneau des réglages (souris ou clavier : ↑↓ choisir, ←→ régler, Maj pour un pas
  de 1, Échap pour fermer).
- **F3** : couper ou rétablir tout le son, partout, immédiatement.
- Trois volumes indépendants, de 0 à 100 : **général**, **musique**, **effets**. Le volume
  joué vaut général × (musique ou effets) × volume du manifeste, chaque curseur passant par
  une courbe au carré (50 % ≈ -12 dB, ce que l'oreille entend comme « moitié moins fort »).
- Les réglages sont enregistrés à la fermeture du panneau dans `reglages_audio.txt`, dans le
  dossier où le jeu est lancé (comme `personnages.txt`) :

```
general=80
musique=55
effets=80
coupe=0
theme=auto
```

Un fichier absent ou abîmé ne bloque jamais le jeu : chaque valeur illisible garde sa valeur
par défaut, et les nombres sont ramenés entre 0 et 100.

## 9. Sons manquants et diagnostic

| Situation | Comportement |
|---|---|
| événement absent du thème actif | son du pack standard |
| absent du standard aussi | silence, sans message |
| fichier cité mais illisible | message **une seule fois** sur stderr, puis silence pour ce fichier |
| pack introuvable | message au lancement, le pack standard le remplace |
| pas de carte son | l'audio se désactive, le jeu fonctionne normalement |

Pour voir chaque décision du moteur :

```bash
REDSTRINGS_AUDIO_TRACE=1 ./redstrings_csfml
# audio    3.27  chargement.ok        pioche.ogg
# audio    6.27  ui.saisie            saisie_4.ogg  (repli standard)
# audio    7.10  ui.saisie            refusé (rythme)
```

Règles de lecture qui évitent la cacophonie : chaque événement a un **délai minimum** entre
deux lectures et un **nombre de lectures simultanées** ; 16 voix au plus ; un nouveau son ne
coupe jamais un son plus important (un K.O. n'est jamais masqué par des survols) ; la
**musique baisse** pendant les sons de priorité 3 (fanfares, K.O.) puis remonte doucement.

## 10. Licences

Tous les fichiers livrés sont des créations originales, générées par `outils/` et distribuées
sous la licence MIT du dépôt (détails et références de style dans [`LICENCES.md`](LICENCES.md)).
Avant d'ajouter un son venu d'ailleurs : licence CC0, CC BY ou compatible MIT, **jamais** un son
extrait d'un jeu commercial, et une ligne dans le tableau de `LICENCES.md`.

## Régénérer les sons

```bash
make sons                                                  # tout (≈ 40 s)
python3 csfml/assets/audio/outils/generer_sons.py yugioh   # un seul pack
```

| Fichier de `outils/` | Contenu |
|---|---|
| `synthe.py` | oscillateurs, enveloppes, filtres, bruit, réverbération (Python pur) |
| `motifs.py` | gestes sonores partagés : souffle, clic, cloches, coup, cuivre, roulement de LP… |
| `sons_standard.py`, `sons_yugioh.py` | un son = une fonction, un nom de fichier |
| `musiques.py`, `compositions.py` | instruments et partitions des quatre boucles |
| `generer_sons.py` | synthèse, normalisation (crête -1 dB, sonie ≤ -16 dB RMS), encodage OGG |
| `wav_vers_ogg.c` | encodeur OGG intégré à SFML : rien d'autre à installer |
