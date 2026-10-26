# Jeu d'action 3D en monde ouvert (C11, SDL3)

Version 3D du projet, séparée de la version terminal/CSFML.

## Structure
- engine/   moteur : plateforme, rendu, assets, physique (ne connaît pas le gameplay)
- game/     gameplay (futur code rechargeable à chaud)
- launcher/ exécutable qui charge le jeu
- tools/    outils hors ligne (cuisson d'assets)
- assets_src/ sources des assets ; assets/ = assets cuisinés (générés, ignorés par git)
- third_party/ dépendances ; docs/ documentation et ADR ; tests/

## Build
À compléter en M0-04 (CMake). Pour l'instant : `make -C sdl`.
