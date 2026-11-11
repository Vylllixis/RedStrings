#!/usr/bin/env bash
# Configure, compile puis lance les tests unitaires (CTest). Usage : tools/scripts/test.sh [preset]
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

preset="${1:-debug}"
cmake --preset "$preset" > /dev/null
cmake --build --preset "$preset"
ctest --preset "$preset"
