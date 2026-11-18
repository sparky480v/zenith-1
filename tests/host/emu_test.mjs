// SPDX-License-Identifier: MPL-2.0
// ZENITH-1's browser emulator in Node, as the page's worklet drives it: boot, 40 s of sound, the screen, a world change,
// SEL for the picture, and saving. node tests/host/emu_test.mjs app/build/emu/app.wasm [OUT_DIR]
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "app/build/emu/app.wasm");
const outDir = process.argv[3];
let fails = 0;
const check = (c, m) => { if (!c) { console.log("FAIL " + m); fails++; } };
const env = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(wasm, { env, wasi_snapshot_preview1: env });
const ex = instance.exports, mem = ex.memory;
ex.web_master(2800);
ex.web_boot();
let peak = 0, sum = 0, n = 0, bad = 0;
function run(secs) {
  for (let i = 0; i < secs * 44100 / 128; i++) {
    ex.web_render(128);
    const l = new Float32Array(mem.buffer, ex.web_out_l(), 128), r = new Float32Array(mem.buffer, ex.web_out_r(), 128);
    for (let k = 0; k < 128; k++) {
      const a = Math.abs(l[k]), b = Math.abs(r[k]);
      if (!(a <= 1) || !(b <= 1)) bad++;
      peak = Math.max(peak, a, b); sum += l[k] * l[k] + r[k] * r[k]; n += 2;
    }
  }
}
function shot(name) {                     // the screen as a PPM (RGB565, byte-swapped as the panel takes it)
  if (!outDir) return;
  const fb = new Uint16Array(mem.buffer, ex.web_fb(), 240 * 240);
  const px = Buffer.alloc(240 * 240 * 3);
  for (let i = 0; i < 240 * 240; i++) {
    const c = ((fb[i] & 255) << 8) | (fb[i] >> 8);
    px[3 * i] = (c >> 11) << 3; px[3 * i + 1] = ((c >> 5) & 63) << 2; px[3 * i + 2] = (c & 31) << 3;
  }
  fs.writeFileSync(`${outDir}/${name}.ppm`, Buffer.concat([Buffer.from("P6 240 240 255\n"), px]));
}
const tap = (b) => { ex.web_buttons(1 << b); run(0.1); ex.web_buttons(0); run(0.1); };
run(40);
const rms = Math.sqrt(sum / n);
console.log(`  40 s: peak ${peak.toFixed(3)}, rms ${rms.toFixed(4)}, blits ${ex.web_blits()}, cpu ${ex.web_cpu()}%`);
check(bad === 0, `NaN or over full scale: ${bad}`);
check(peak > 0.1 && peak < 1, `peak ${peak} (the emulator plays as headphones)`);
check(ex.web_blits() > 100, "the screen was drawn");
check(ex.web_lit_buttons() & (1 << 10), "PLAY is lit (growing)");
shot("emu_garden");
ex.web_enc(2, 1); run(3);                 // PRESETS: the next world
shot("emu_cinematic");
tap(6); tap(6); run(3);                   // HOME, HOME: the Journey page, then the picture
shot("emu_picture");
check(ex.web_lit_buttons() & (1 << 6), "HOME lit with the picture up");
tap(0); run(0.5);                         // a button wakes the pages; FX: the menu
tap(0); run(0.3);
ex.web_keys(1 << 4); run(0.3); ex.web_keys(0); run(2);   // the key A: Comet, once
check(true, "");
tap(7); run(0.5);                         // SAVE
check(ex.web_store_writes() > 0, "saved");
console.log(fails ? `emu: ${fails} FAILED` : "emu: ok");
process.exit(fails ? 1 : 0);
