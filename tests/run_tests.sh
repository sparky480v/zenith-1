#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal (AMB-1), Polystellarflow (ZENITH-1)
# ZENITH-1's tests: the instrument (tests/host/zenith_test.c: the worlds, every sound, the speaker's ceiling, the
# space at its limits, Wander), the whole app in fm1-base's simulator (tests/scenarios/*.zen: UI, chords, rhythm, worlds,
# persistence, the picture, the jack, the screen reader), the browser emulator (clang's wasm32, in Node), and the update
# path against ZENITH-1's package (./build.sh first). Screenshots and audio in app/build/scenarios/. fm1-base's own
# tests: base/tests/run_tests.sh.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"   # the package's SDK files, as ./build.sh
B=app/build
OUT=$B/tests
FAIL=0
mkdir -p "$OUT" $B/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run zenith sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/zenith_test tests/host/zenith_test.c app/dsp/zenith.c -lm && $OUT/zenith_test"
run vis sh -c "$CC -O2 -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include -c app/dsp/vis.c -o $OUT/vis.o"
if [ -f $B/zenith.fwsc ]; then
    run ota-entry sh -c "$CC -w -o $OUT/ota_test base/tests/ota_test.c && $OUT/ota_test $B/zenith.fwsc"
    run update-loader sh -c "head -c 100000 $B/zenith.bin > $OUT/old_app.bin && \
        $PY base/tools/fm1pkg_make.py $OUT/old_app.bin $B/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -w -o $OUT/ldr_test base/tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc $B/zenith.fwsc"
else
    echo "  skip update path (no $B/zenith.fwsc: ./build.sh)"
fi
run host-build sh build.sh --host
if command -v clang >/dev/null 2>&1 && command -v wasm-ld >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then
    mkdir -p $B/scenarios/emu
    run emu sh -c "sh tools/build_emu.sh >/dev/null && node tests/host/emu_test.mjs $B/emu/app.wasm $B/scenarios/emu"
else
    echo "  skip emu (no clang, wasm-ld or node)"
fi
for s in tests/scenarios/*.zen; do
    n=$(basename "$s" .zen)
    mkdir -p "$B/scenarios/$n"
    run "scenario-$n" $B/host/zenith_host "$s" "$B/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
