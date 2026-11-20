#!/usr/bin/env bash
# Prépare les arbres et rochers KayKit (Kay Lousberg, CC0) du décor (finition, B8).
#
#   tools/scripts/kaykit_nature_prepare.sh
#
# Télécharge depuis le dépôt GitHub officiel « KayKit : Medieval Hexagon Pack 1.0 » quelques
# modèles du dossier decoration/nature (glTF + .bin) et leur texture commune, dans
# assets/props/nature/ (versionnés : voir assets/LICENCES.md). Remplacent les sphères vertes et
# blanches qui tenaient lieu de végétation et de rochers (--placement, monde).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

mkdir -p assets/props/nature
hex=https://raw.githubusercontent.com/KayKit-Game-Assets/KayKit-Medieval-Hexagon-Pack-1.0/main
nat="$hex/addons/kaykit_medieval_hexagon_pack/Assets/gltf/decoration/nature"

fetch() { [[ -s "$2" ]] || curl -sSfL -o "$2" "$1"; }
for m in tree_single_A tree_single_B rock_single_A rock_single_B rock_single_C; do
  fetch "$nat/$m.gltf" "assets/props/nature/$m.gltf"
  fetch "$nat/$m.bin" "assets/props/nature/$m.bin"
done
fetch "$nat/hexagons_medieval.png" "assets/props/nature/hexagons_medieval.png"
fetch "$hex/LICENSE.txt" "assets/props/nature/LICENSE.txt"
grep -q "CC0" assets/props/nature/LICENSE.txt || { echo "licence inattendue" >&2; exit 1; }
echo "nature : $(ls assets/props/nature | wc -l) fichiers dans assets/props/nature/"
