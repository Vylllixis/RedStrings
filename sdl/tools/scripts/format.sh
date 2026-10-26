#!/usr/bin/env bash
# Formate (ou vérifie avec --check) tout le code du projet.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

mode=(-i)
[[ "${1:-}" == "--check" ]] && mode=(--dry-run --Werror)

git ls-files -z -- 'engine/*.c' 'engine/*.h' 'game/*.c' 'game/*.h' \
                   'launcher/*.c' 'launcher/*.h' 'tools/*.c' 'tools/*.h' \
  | xargs -0 --no-run-if-empty clang-format "${mode[@]}"
echo "format: OK"
