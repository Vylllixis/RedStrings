#!/usr/bin/env bash
# Arbres réalistes (2026-11-22, docs/arbres-realistes.md) : refait assets/props/nature/real/ à
# partir de trois arbres scannés de Poly Haven (CC0, https://polyhaven.com).
#
#   tools/scripts/arbres_realistes.sh [dossier des sources] [treebake]
#
#   1. télécharge les sources en glTF 1k (environ 720 Mo, vérifiées par leur empreinte md5) dans
#      le dossier des sources : build/arbres-sources par défaut, hors du dépôt git ;
#   2. les convertit avec l'outil treebake (cible CMake « treebake », build/release par défaut).
#
# Le résultat ne dépend que des sources et de ces réglages : relancer le script redonne les mêmes
# fichiers, à l'octet près. Les sources ne sont PAS dans le dépôt (trop lourdes) ; ce qu'il garde,
# ce sont les arbres convertis (environ 23 Mo).
set -euo pipefail
cd "$(dirname "$0")/../.."
SOURCES="${1:-build/arbres-sources}"
TREEBAKE="${2:-build/release/bin/treebake}"
OUT=assets/props/nature/real

if [ ! -x "$TREEBAKE" ]; then
  echo "arbres_realistes : $TREEBAKE introuvable : cmake --build build/release --target treebake" >&2
  exit 1
fi
mkdir -p "$SOURCES" "$OUT"
for tree in jacaranda_tree fir_tree_01 quiver_tree_01; do
  python3 -I tools/scripts/arbres_fetch.py "$tree" 1k "$SOURCES"
done

# Feuillu de la plaine : jacaranda, 9 m ; le tronc et ses grosses branches en maillage, feuilles et
# rameaux en cartes.
"$TREEBAKE" "$SOURCES/jacaranda_tree/jacaranda_tree_1k.gltf" "$OUT" oak --trunk trunk --height 9
# Sapin de la montagne : le premier des trois sapins du fichier, 11 m, enneigé.
"$TREEBAKE" "$SOURCES/fir_tree_01/fir_tree_01_1k.gltf" "$OUT" pine --node _a_ \
  --trunk bark,trunk,dead --height 11 --snow 0.6
# Arbre du désert : arbre à carquois (Aloidendron dichotomum), 6,5 m ; ses feuilles épaisses restent
# en maillage (pas de cartes).
"$TREEBAKE" "$SOURCES/quiver_tree_01/quiver_tree_01_1k.gltf" "$OUT" acacia --trunk trunk,leaf \
  --height 6.5 --trunk-tris 14000 --trunk-lod 3000
du -sh "$OUT"
