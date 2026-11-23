#!/usr/bin/env bash
# Vérifie ce qui peut l'être mécaniquement des critères de sortie de M2 (docs/devlog/m2-revue.md).
# Usage : tools/scripts/m2-exit-check.sh [preset]   (défaut : debug ; le build doit exister)
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

preset="${1:-debug}"
build="build/$preset"
fail=0
ok() { printf '  OK   %s\n' "$1"; }
ko() { printf '  FAIL %s\n' "$1"; fail=1; }
run_tests() { # $1 = description, $2 = regex ctest
    if [ ! -d "$build" ]; then ko "$build absent : cmake --preset $preset && cmake --build --preset $preset"; return; fi
    if ctest --test-dir "$build" -R "$2" --output-on-failure >/tmp/m2_t.$$ 2>&1 && ! grep -q 'No tests were found' /tmp/m2_t.$$; then
        ok "$1"
    else
        ko "$1"; tail -20 /tmp/m2_t.$$
    fi
    rm -f /tmp/m2_t.$$
}

echo "Critère 1 : aucun parsing glTF au runtime"
# Le seul appelant de eng_model_load hors de model.c doit être la cuisson (tools/) ; ni game/, ni launcher/,
# ni le reste du moteur ne lisent du glTF.
if grep -rnE 'eng_model_load\b|cgltf_' game launcher --include='*.c' --include='*.h' >/tmp/m2_c1.$$ 2>&1 \
    || grep -rnE 'eng_model_load\b|cgltf_' engine/src --include='*.c' | grep -vE '^engine/src/(model|third_party_impl)\.c' >>/tmp/m2_c1.$$ 2>&1 && [ -s /tmp/m2_c1.$$ ]; then
    ko "lecture glTF hors du cooker :"; cat /tmp/m2_c1.$$
else
    ok "ni game/, ni launcher/, ni le runtime du moteur n'appellent le chargeur glTF (seulement model.c et la cuisson)"
fi
rm -f /tmp/m2_c1.$$

echo "Critère 2 : assets recuisinés automatiquement au build, sans recuisson inutile"
run_tests "assets.cook_incremental (build incrémental) + engine.asset_cook" 'assets\.cook_incremental|engine\.asset_cook'

echo "Critère 3 : scène sauvegardée puis rechargée à l'identique"
run_tests "engine.scene (aller-retour) + engine.editor + launcher.editor (selftest)" 'engine\.scene|engine\.editor$|launcher\.editor'

echo "Suite complète"
if [ -d "$build" ] && ctest --test-dir "$build" 2>&1 | tail -3 | tee /tmp/m2_all.$$ | grep -q '100% tests passed'; then
    ok "$(grep -E 'tests passed' /tmp/m2_all.$$)"
else
    ko "ctest"; cat /tmp/m2_all.$$ 2>/dev/null
fi
rm -f /tmp/m2_all.$$
exit $fail
