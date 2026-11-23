#!/usr/bin/env bash
# Analyse statique (clang-tidy) du code du projet. Prérequis : build/debug configuré.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

db="${1:-build/debug}"
[[ -f "$db/compile_commands.json" ]] || { echo "lint: lance d'abord 'cmake --preset debug'" >&2; exit 1; }

git ls-files -z --cached --others --exclude-standard -- \
    'engine/*.c' 'game/*.c' 'launcher/*.c' 'tools/*.c' 'tests/*.c' \
  | xargs -0 --no-run-if-empty clang-tidy -p "$db" --quiet --warnings-as-errors='*'

# Règle mémoire (M0-12) : le compteur d'allocations ne voit que SDL_malloc & co. Un malloc() de la
# libc appelé directement échapperait au contrôle « 0 allocation dans la boucle de frame ».
if git ls-files -z --cached --others --exclude-standard -- 'engine/*.[ch]' 'game/*.[ch]' \
     'launcher/*.[ch]' | xargs -0 --no-run-if-empty grep -nE '\b(malloc|calloc|realloc|free)[[:space:]]*\('; then
    echo "lint: malloc/calloc/realloc/free de la libc interdits, utiliser SDL_* ou une arène" >&2
    exit 1
fi
# Règle de rendu (M1-05) : le code de jeu ne touche jamais SDL_GPU (ni aucun autre en-tête SDL : la
# bibliothèque du jeu ne lie pas SDL). Il dessine par la file de commandes du renderer, et
# l'en-tête public du renderer n'expose aucun type SDL.
if git ls-files -z --cached --others --exclude-standard -- 'game/*.[ch]' \
     'engine/include/engine/renderer.h' 'engine/include/engine/ui.h' \
   | xargs -0 --no-run-if-empty grep -nE '#[[:space:]]*include[[:space:]]*[<"](SDL|SDL3/)'; then
    echo "lint: game/, renderer.h et ui.h ne doivent inclure aucun en-tête SDL (utiliser le renderer)" >&2
    exit 1
fi
echo "lint: OK"
