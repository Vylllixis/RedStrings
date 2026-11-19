#!/bin/sh
# M7-02 / M7-03 : le système de jobs et le streaming sous ThreadSanitizer (le reste du projet tourne sous ASan, incompatible).
# Usage : tools/scripts/tsan-jobs.sh [dossier_de_build_debug]  (défaut : build/debug)
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=${1:-$ROOT/build/debug}
SDL_INC=$ROOT/third_party/_deps/sdl3-src/include
SDL_LIB=$(find "$BUILD/../.." "$ROOT/third_party/_deps/sdl3-build" -name 'libSDL3.a' 2>/dev/null | head -n 1)
UTEST_INC=$(dirname "$(find "$ROOT/third_party/_deps/utest-src" -name utest.h | head -n 1)")
OUT=$(mktemp -d)
cc -std=gnu11 -g -O1 -fsanitize=thread -I"$ROOT/engine/include" -I"$SDL_INC" -I"$UTEST_INC" -I"$ROOT/tests" \
   "$ROOT/engine/src/jobs.c" "$ROOT/tests/test_jobs.c" "$ROOT/tests/utest_main.c" \
   "$SDL_LIB" -lm -lpthread -ldl -o "$OUT/test_jobs_tsan"
TSAN_OPTIONS="halt_on_error=1" "$OUT/test_jobs_tsan"
# streaming des cellules (M7-03) : jobs + génération + récolte sur le thread principal
FNL=$ROOT/third_party/_deps/fastnoiselite-src/C
cc -std=gnu11 -g -O1 -fsanitize=thread -I"$ROOT/engine/include" -I"$SDL_INC" -I"$UTEST_INC" -I"$ROOT/tests" \
   -isystem "$FNL" \
   "$ROOT/engine/src/jobs.c" "$ROOT/engine/src/world_cell.c" "$ROOT/engine/src/world_stream.c" \
   "$ROOT/engine/src/world_state.c" "$ROOT/engine/src/world_origin.c" "$ROOT/engine/src/third_party_fnl.c" \
   "$ROOT/tests/test_world_stream.c" "$ROOT/tests/utest_main.c" \
   "$SDL_LIB" -lm -lpthread -ldl -o "$OUT/test_stream_tsan"
(cd "$OUT" && TSAN_OPTIONS="halt_on_error=1" ./test_stream_tsan)
echo "TSAN JOBS : OK"
