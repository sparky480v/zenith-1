#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# ZENITH-1's browser emulator without Emscripten (as HADAL-1's): clang's wasm32 target and wasm-ld, the few libc pieces
# the simulator needs in tools/emu_shim. The real app on fm1-base's simulator (web/emu/emu_web.c), and fm1-base's page.
#   tools/build_emu.sh        ->  app/build/emu/ (index.html, worklet.js, app.wasm); serve it and open index.html
# Needs: clang and wasm-ld (Ubuntu: sudo apt install clang lld), Python 3 with Pillow (the generated headers).
set -e
cd "$(dirname "$0")/.."
[ -f app/build/gen/felucca_font.h ] || sh build.sh --host >/dev/null      # the fonts and the words' header
G=app/build/gen; O=app/build/emu; mkdir -p $O
CC="${CLANG:-clang}"
FL="--target=wasm32 -O2 -ffp-contract=off -std=gnu99 -nostdlib -w -DPLAT_HOST -DPLAT_WEB -DFM1B_BLE=0 -DFM1B_HEADPHONES=1 -Itools/emu_shim -Ibase/host -Ibase/device -Ibase/include -Ibase/lib -Iapp -Iapp/dsp -I$G"
$CC $FL -c app/dsp/zenith.c -o $O/zenith.o
$CC $FL -c app/dsp/vis.c -o $O/vis.o
$CC $FL -fno-builtin -O1 -c tools/emu_shim/libc.c -o $O/libc.o
$CC $FL -Wl,--no-entry -Wl,--export-dynamic -Wl,--initial-memory=33554432 -Wl,-z,stack-size=1048576 \
    -o $O/app.wasm base/web/emu/emu_web.c $O/zenith.o $O/vis.o $O/libc.o
rm -f $O/*.o
python3 - <<'PY'
import re, shutil
from pathlib import Path
src, out = Path("base/web"), Path("app/build/emu")
page = (src / "emu" / "index.html").read_text()
page = page.replace("{{NAME}}", "ZENITH").replace("{{ID}}", "zenith")
page = page.replace("{{PANEL_JS}}", re.sub(r"^export\s+", "", (src / "fm1panel.js").read_text(), flags=re.M))
(out / "index.html").write_text(page)
shutil.copy(src / "emu" / "worklet.js", out / "worklet.js")
PY
ls -la $O
echo "the emulator: python3 -m http.server 8766 --directory $O, then http://localhost:8766/"
