#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# ZENITH-1's browser installer, from a release build (./build.sh --release X.Y), served on this computer, with the
# browser emulator and the manual beside it.
#   ./make_install_page.sh X.Y      then open http://localhost:8765/install/ in Chrome or Edge
set -e
cd "$(dirname "$0")"
V="${1:?usage: ./make_install_page.sh X.Y (the version you built with ./build.sh --release X.Y)}"
[ -f "app/build/zenith-$V.fwsc" ] || { echo "no app/build/zenith-$V.fwsc: run ./build.sh --release $V first"; exit 1; }
if [ ! -s app/build/emu/app.wasm ]; then             # the emulator: clang's wasm32 (tools/build_emu.sh)
    sh tools/build_emu.sh >/dev/null 2>&1 || { echo "(no emulator: sudo apt install clang lld, for the emu/ page)";
                                               mkdir -p app/build/emu; : > app/build/emu/app.wasm; }
fi
rm -rf app/docs && cp -r docs app/docs             # the manual, where the site builder looks for it
python3 base/tools/make_site.py app "app/build/zenith-$V.fwsc" "$V" app/build/site
rm -rf app/docs
echo
echo "Open this in Chrome or Edge on Windows:  http://localhost:8765/install/"
echo "The emulator: http://localhost:8765/emu/    The manual: http://localhost:8765/manual/"
echo "Press Ctrl+C here when the install has finished."
exec python3 -m http.server 8765 --bind 127.0.0.1 --directory app/build/site
