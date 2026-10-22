#!/bin/sh
# Télécharge les illustrations du mode Yu-Gi-Oh! (easter egg de la version CSFML).
#
#   sh csfml/assets/yugioh/telecharger.sh
#
# Les codes des cartes sont lus dans csfml/src/yugioh/cartes.c : c'est la seule
# liste à tenir à jour. Les images viennent de la base YGOPRODeck (illustrations
# recadrées, sans le cadre de la carte). Elles restent la propriété de leurs
# ayants droit (Kazuki Takahashi / Konami) : elles ne sont pas versionnées
# (.gitignore) et ne servent qu'en local. Sans elles, le mode fonctionne avec
# l'emblème de chaque classe à la place de l'illustration.

set -eu
DOSSIER=$(cd "$(dirname "$0")" && pwd)
SOURCE="$DOSSIER/../../src/yugioh/cartes.c"
URL="https://images.ygoprodeck.com/images/cards_cropped"

[ -f "$SOURCE" ] || { echo "cartes.c introuvable : $SOURCE" >&2; exit 1; }
command -v curl >/dev/null || { echo "curl est nécessaire" >&2; exit 1; }

total=0
ok=0
for code in $(grep -o '", [0-9]\{3,9\}, CADRE_' "$SOURCE" | grep -o '[0-9]\{3,9\}'); do
    total=$((total + 1))
    fichier="$DOSSIER/$code.jpg"
    if [ -s "$fichier" ]; then
        ok=$((ok + 1))
        continue
    fi
    if curl -fsS -m 30 -o "$fichier.tmp" "$URL/$code.jpg" && head -c 3 "$fichier.tmp" \
        | od -An -tx1 | grep -q 'ff d8 ff'; then
        mv "$fichier.tmp" "$fichier"
        ok=$((ok + 1))
    else
        rm -f "$fichier.tmp"
        echo "échec : carte $code" >&2
    fi
    sleep 0.1
done
echo "$ok illustration(s) sur $total dans $DOSSIER"
