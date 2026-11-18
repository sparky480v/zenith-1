# Building ZENITH-1

ZENITH-1 is an app on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, included in this tree, with
felucca-bsp inside it). `./build.sh` runs fm1-base's build (`base/tools/build.py`) on `app/`, whose settings are in
`app/fm1.toml`. It makes, in `app/build/`:

| File | What |
| --- | --- |
| `zenith.bin` | the firmware app |
| `loader/ota.bin` | the update loader (felucca-bsp's) |
| `zenith.fwsc` | the installable package (app + loader); a release build: `zenith-X.Y.fwsc` |

## Prerequisites (Ubuntu, or Ubuntu in WSL on Windows)

The same as for any fm1-base firmware; if you have built one before, you have them.

- Python 3 with Pillow: `sudo apt install python3-pil` (or `pip3 install Pillow`).
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2) in `~/.jieli/toolchain`: `base/tools/get_toolchain.sh`, or see
  fm1-base's README.
- The JieLi AC79 SDK in `~/fw-AC79_AIoT_SDK` (three of its files go into the package; none are in this tree):

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- For the browser emulator and its tests (optional): `sudo apt install clang lld nodejs`.

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations, `PYTHON` the Python that runs the build.

## Build

```
./build.sh                    # app/build/zenith.fwsc (a development build)
./build.sh --release X.Y      # app/build/zenith-X.Y.fwsc, identity FM-1_97XXYYZZ
./build.sh --host             # the simulator: app/build/host/zenith_host
sh tools/build_emu.sh         # the browser emulator: app/build/emu/ (clang's wasm32, no Emscripten needed)
```

The build checks the image, the RAM and the pool (the reverb, the tape loop and the picture share it: the screen is
drawn through a 240 x 48 canvas), that register access stays in felucca-bsp, and that no double-precision code is
linked.

## Install

```
./make_install_page.sh X.Y    # the installer, the emulator and the manual, on http://localhost:8765/
```

Open <http://localhost:8765/install/> in Chrome or Edge, connect the FM-1 with a USB data cable and press Install.
Press Ctrl+C in the terminal when it has finished. On WSL, the page opens in Windows' own Chrome: WSL forwards
`localhost` to it, and Chrome reaches the FM-1 over Windows' USB.

From the command line instead (needs `pip3 install mido python-rtmidi`, and a USB MIDI path to the FM-1):
`python3 base/tools/fm1_install.py app/build/zenith-X.Y.fwsc`.

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for the update mode. The web installer can
put M-VAVE's own firmware back.

## Tests

```
sh tests/run_tests.sh             # ZENITH-1's
sh base/tests/run_tests.sh        # fm1-base's own
```

ZENITH-1's tests:

- **The instrument** (`tests/host/zenith_test.c`): AMB-1's tests (Euclid, scales, chords, voice leading, the render,
  freeze, the chord drift), and ZENITH-1's. Every world stays under the speaker's ceiling at full volume and plays on
  headphones. Every sound of every layer sounds. The space at its limits (shimmer, size, echo, tape all the way up)
  never runs away. Forty minutes of Wander: the seasons pass, the key leaves home and comes back, and a sound set by
  hand stays.
- **The update path** against `app/build/zenith.fwsc` (after `./build.sh`).
- **The browser emulator in Node** (`tests/host/emu_test.mjs`): it boots, plays, draws, shows the picture and saves.
- **The whole app in the simulator** (`tests/scenarios/*.zen`): the scale, the worlds and their persistence, the pages,
  the help cards, the picture, the jack (the speaker's ceiling, headphones), the screen reader. Screenshots and audio
  are in `app/build/scenarios/`.

`app/build/host/zenith_host SCRIPT OUTDIR` runs one script. The commands are listed at the top of
`base/host/plat_host.c`, and ZENITH-1's own checks and commands at the top of `app/host.c`.

## Sketches

`tools/sketch.c` renders a world straight from the engine to a WAV, with its peak, its loudness and how long it took:

```
cc -O2 -DEN_HOST -Iapp/dsp -Ibase/include -o sketch tools/sketch.c app/dsp/zenith.c -lm
./sketch WORLD SECONDS out.wav [MASTER 0..4096] [SPEAKER 0|1]
SET="22=4,44=80" ./sketch 0 60 out.wav    # with parameters set by number (zenith.h P_*)
```

The manual's panel drawing: `python3 tools/gen_manual_figures.py` (from `docs/img/screen-garden.png`).
