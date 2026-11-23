#!/usr/bin/env bash
# Vérifie ce qui peut l'être mécaniquement des critères de sortie de M1 (docs/devlog/m1-revue.md).
# Usage : tools/scripts/m1-exit-check.sh [preset]   (défaut : debug ; le build doit exister)
# Les critères 2 (casque vs visualiseur) et 3 (60 FPS avec ombres) demandent une vraie machine : voir la revue.
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

preset="${1:-debug}"
build="build/$preset"
fail=0
ok() { printf '  OK   %s\n' "$1"; }
ko() { printf '  FAIL %s\n' "$1"; fail=1; }

echo "Critère 1 : le code de jeu n'appelle aucune fonction SDL_GPU"
# game/ : ni include SDL_gpu, ni symbole SDL_GPU*/SDL_*GPU* ; launcher/ : idem (il passe par engine/gpu.h).
if grep -rnE 'SDL_gpu|SDL_GPU|SDL_[A-Za-z]*GPU[A-Za-z]*\(' game launcher --include='*.c' --include='*.h' --include='*.cpp' >/tmp/m1_c1.$$ 2>&1; then
    ko "appel ou include SDL_GPU hors du moteur :"; cat /tmp/m1_c1.$$
else
    ok "aucun symbole SDL_GPU dans game/ ni launcher/"
fi
rm -f /tmp/m1_c1.$$
# game/ n'inclut que les en-têtes partagés avec le jeu (ADR 0001) : jamais gpu, renderer, shader...
if grep -rnE '#include *["<]engine/' game --include='*.c' --include='*.h' \
    | grep -vE 'engine/(assert|log|game_api)\.h'; then
    ko "game/ inclut un en-tête du moteur hors assert.h, log.h, game_api.h"
else
    ok "game/ n'inclut que engine/{assert,log,game_api}.h (ADR 0001)"
fi

echo "Critère 4 : rechargement à chaud des shaders"
if [ ! -d "$build" ]; then
    ko "$build absent : cmake --preset $preset && cmake --build --preset $preset"
elif ctest --test-dir "$build" -R 'engine\.shader_hot' --output-on-failure >/tmp/m1_c4.$$ 2>&1; then
    ok "engine.shader_hot (modification de la source -> le dessin change ; erreur de compilation -> ancien shader conservé)"
else
    ko "engine.shader_hot"; tail -20 /tmp/m1_c4.$$
fi
rm -f /tmp/m1_c4.$$

echo "Suite complète"
if [ -d "$build" ] && ctest --test-dir "$build" 2>&1 | tail -3 | tee /tmp/m1_all.$$ | grep -q '100% tests passed'; then
    ok "$(grep -E 'tests passed' /tmp/m1_all.$$)"
else
    ko "ctest"; cat /tmp/m1_all.$$ 2>/dev/null
fi
rm -f /tmp/m1_all.$$

exit $fail
