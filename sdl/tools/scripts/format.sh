#!/usr/bin/env bash
# Formate (ou vérifie avec --check) tout le code du projet.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

mode=(-i)
[[ "${1:-}" == "--check" ]] && mode=(--dry-run --Werror)

# --cached --others : les fichiers suivis ET les nouveaux (pas encore ajoutés à git)
git ls-files -z --cached --others --exclude-standard -- \
    'engine/*.c' 'engine/*.h' 'engine/*.cpp' 'game/*.c' 'game/*.h' 'launcher/*.c' 'launcher/*.h' \
    'tools/*.c' 'tools/*.h' 'tests/*.c' 'tests/*.h' \
  | xargs -0 --no-run-if-empty clang-format "${mode[@]}"
echo "format: OK"
