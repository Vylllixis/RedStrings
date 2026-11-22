#!/usr/bin/env bash
# Lance la tranche verticale (critère 3 de M8 : 30 min de jeu sans crash). Voir docs/tranche-verticale.md.
#
#   tools/scripts/tranche-verticale.sh [--build] [options du lanceur en plus...]
#
# Menu principal (Nouvelle partie / Continuer), puis le combat sur un terrain doux de 1 km : herbe,
# arbres et rochers (KayKit, CC0), clairière avec le mannequin et le camp de 3 ennemis, cycle jour /
# nuit, ambiance et musique. Le terrain est généré une fois dans build/release/bin
# (tranche-verticale.hmap, toujours le même : graine fixe de assetc). --build construit d'abord la
# configuration Release. Le journal est dans build/release/bin/logs/, un rapport de crash éventuel
# dans build/release/bin/logs/crash-*.txt.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

bin=build/release/bin
if [[ "${1:-}" == "--build" ]]; then
  shift
  cmake --build build/release
fi
if [[ ! -x "$bin/launcher" || ! -x "$bin/assetc" ]]; then
  echo "tranche-verticale : $bin/launcher ou assetc absent ; construire d'abord (--build)" >&2
  exit 1
fi
map=tranche-verticale.hmap
if [[ ! -s "$bin/$map" ]]; then
  "$bin/assetc" heightmap "$bin/$map" --size 513 --spacing 2 --alt-max 25 --wavelength 500
fi
cd "$bin"
exec ./launcher --combat --terrain "$map" --grass --placement "$@"
