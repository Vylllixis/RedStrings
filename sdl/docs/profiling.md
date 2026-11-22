# Profilage (Tracy)

Le profilage est une **option de build** : `OW_PROFILE=ON`. Désactivée (par défaut), rien n'est
téléchargé, aucun compilateur C++ n'est requis, et les macros `ENG_PROF_*` se réduisent à rien.

- Version : Tracy **v0.15.0** (commit `f2a4f25`). Le visualiseur doit avoir **exactement** la même
  version que le client compilé dans le jeu (sinon il refuse la connexion).
- Le client Tracy est en C++ ; on n'en voit que l'API C (`TracyC.h`), derrière `engine/profile.h`.

## Utilisation

```bash
cmake --preset profile            # RelWithDebInfo + OW_PROFILE=ON
cmake --build --preset profile
```

1. Télécharge le visualiseur Tracy **v0.15.0** (page *Releases* de `wolfpld/tracy`) et lance-le.
2. Lance le jeu : `build/profile/bin/launcher` (le visualiseur le propose dans sa liste, ou
   saisis `127.0.0.1` puis *Connect*).
3. Une frame complète s'affiche avec ses zones : `frame` contient `game_reload_check`,
   `events`, `simulation` (→ `tick` → `game.update`), `game.render`, `present`.

Notes :
- `TRACY_ON_DEMAND` : rien n'est collecté tant qu'un visualiseur n'est pas connecté. Les images
  jouées avant la connexion n'apparaissent donc pas.
- Pour une exécution courte (`--frames N`), définis `TRACY_NO_EXIT=1` : le programme attend alors
  que le visualiseur ait reçu toutes les données avant de quitter.
- Le client n'écoute que `127.0.0.1` (`TRACY_ONLY_LOCALHOST`).
- Mesure toujours en `RelWithDebInfo`/`Release`, pas en Debug (ASan/UBSan faussent les durées).

## Écrire des zones

```c
#include "engine/profile.h"

ENG_PROF_ZONE(z, "simulation");   // ouvre la zone ; `z` : nom de variable local
...
ENG_PROF_ZONE_END(z);             // la ferme
ENG_PROF_FRAME();                 // fin d'image, une fois par tour de boucle principale
ENG_PROF_PLOT("pas de simulation", steps);   // courbe d'une valeur
```

Règles :
- le nom est un **littéral** de chaîne (Tracy garde le pointeur) ;
- toute zone ouverte est fermée sur **tous** les chemins de sortie (`return`, `break`, `goto`) ;
- deux zones ouvertes dans la même portée ont des noms de variable différents ;
- les arguments des macros ne doivent pas avoir d'effet de bord : hors profilage ils ne sont pas
  évalués ;
- le code du jeu (`game/`) ne lie pas le moteur : il ne peut pas encore ouvrir de zones. Ses
  appels (`game.update`, `game.render`) sont mesurés de l'extérieur par le lanceur. Des zones
  côté jeu passeront par la table `EngServices` (à décider quand le jeu aura du code à profiler).

## Limites connues

- **Sous Linux, Tracy neutralise `dlclose`** (comportement documenté, non désactivable) : en
  build de profilage, une bibliothèque de jeu rechargée à chaud n'est jamais déchargée. L'hôte
  de rechargement en tient compte (`ENG_HOST_DLCLOSE_IS_NOOP`) : noms de copie uniques dans le
  processus, copies supprimées à l'arrêt seulement. Le rechargement à chaud fonctionne, avec
  quelques dizaines de Ko perdus par rechargement.
- Sous Windows, ce problème n'existe pas (non testé ici : MSVC n'est pas disponible dans mon
  environnement de développement).
- Pas de zones GPU avant M1 (il n'y a pas encore de rendu GPU).
