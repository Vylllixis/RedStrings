#!/usr/bin/env bash
# Analyse statique (clang-tidy) du code du projet. Prérequis : build/debug configuré.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

db="${1:-build/debug}"
[[ -f "$db/compile_commands.json" ]] || { echo "lint: lance d'abord 'cmake --preset debug'" >&2; exit 1; }

git ls-files -z -- 'engine/*.c' 'game/*.c' 'launcher/*.c' 'tools/*.c' \
  | xargs -0 --no-run-if-empty clang-tidy -p "$db" --quiet --warnings-as-errors='*'
echo "lint: OK"
