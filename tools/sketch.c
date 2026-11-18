/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Polystellarflow (ZENITH-1) */
/* ZENITH-1's sketches: one world, rendered by the engine on a computer.
 *   sketch WORLD SECONDS OUT.wav [master 0..4096] [speaker 0|1] [skip seconds]
 * Prints: peak, RMS (dBFS), the limiter's deepest gain, and how long the render took per second of sound. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "zenith.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

static void wr32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void wr16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv)
{
    int world = argc > 1 ? atoi(argv[1]) : 0, p;
    double secs = argc > 2 ? atof(argv[2]) : 60.0;
    const char *out = argc > 3 ? argv[3] : NULL;
    uint32_t gain = argc > 4 ? (uint32_t)atoi(argv[4]) : 3400u;
    int speaker = argc > 5 ? atoi(argv[5]) : 0;
    double skip = argc > 6 ? atof(argv[6]) : 8.0;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    static int32_t buf[512];
    long total = (long)((secs + skip) * 44100.0), done = 0, wrote = 0, nskip = (long)(skip * 44100.0),
         nout = (long)(secs * 44100.0);
    double pk = 0, ss = 0, lim_min = 1.0, render_s = 0;
    FILE *f = NULL;
    en_init();
    en_piece(world, par, &pool);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    {   /* SET="param=value,param=value": overrides, by index */
        const char *e = getenv("SET");
        while (e && *e) {
            int k = atoi(e), v;
            const char *q = strchr(e, '=');
            if (!q)
                break;
            v = atoi(q + 1);
            en_set(k, v);
            e = strchr(q, ',');
            if (e)
                e++;
        }
    }
    {   /* SET's overrides, kept in par (the journey keeps the global ones across worlds) */
        const char *e = getenv("SET");
        while (e && *e) {
            int k = atoi(e);
            const char *q2 = strchr(e, '=');
            if (!q2)
                break;
            if (k >= 0 && k < P_NPARAMS)
                par[k] = (int16_t)atoi(q2 + 1);
            e = strchr(q2, ',');
            if (e)
                e++;
        }
    }
    en_pool(pool);
    en_regrow();
    en_out(!speaker);
    en_seed((uint32_t)(world * 7919 + 1));
    en_play(1);
    if (out) {
        f = fopen(out, "wb");
        fwrite("RIFF", 1, 4, f);
        wr32(f, 36 + (uint32_t)nout * 4);
        fwrite("WAVEfmt ", 1, 8, f);
        wr32(f, 16);
        wr16(f, 1);
        wr16(f, 2);
        wr32(f, 44100);
        wr32(f, 44100 * 4);
        wr16(f, 4);
        wr16(f, 16);
        fwrite("data", 1, 4, f);
        wr32(f, (uint32_t)nout * 4);
    }
    uint32_t asked = en_journey_n;
    int last_part = -1;
    while (done < total) {
        int n = 256, i;
        clock_t t0 = clock();
        if (en_journey_n != asked) {             /* the journey: another world, as the FM-1 takes it up */
            int16_t keep[P_NPARAMS];
            static const int GLOBAL[] = {P_TUNE, P_MIDI, P_JOURNEY, P_WANDER, P_TIDE, P_KEYMOVE, P_FXLVL, P_FXAUTO,
                                         P_FXSPACE};
            int g;
            asked = en_journey_n;
            for (g = 0; g < (int)(sizeof GLOBAL / sizeof GLOBAL[0]); g++)
                keep[GLOBAL[g]] = par[GLOBAL[g]];
            en_piece(en_journey_to, par, &pool);
            for (g = 0; g < (int)(sizeof GLOBAL / sizeof GLOBAL[0]); g++)
                par[GLOBAL[g]] = keep[GLOBAL[g]];
            for (p = 0; p < P_NPARAMS; p++)
                en_set(p, par[p]);
            en_pool(pool);
            en_regrow();
            fprintf(stderr, "%6.1f s  -> %s\n", (done - nskip) / 44100.0, en_piece_name(en_journey_to));
        }
        if (en_part != last_part) {
            last_part = en_part;
            fprintf(stderr, "%6.1f s  part %s, era %u\n", (done - nskip) / 44100.0, EN_PART_NAME[en_part], en_eras);
        }
        if (done + n > total)
            n = (int)(total - done);
        if (getenv("NEXT") && done < (long)(45 * 44100) && done + n >= (long)(45 * 44100) + nskip)
            en_next();                           /* a chord move, to hear one */
        en_render(buf, (uint32_t)n, gain);
        render_s += (double)(clock() - t0) / CLOCKS_PER_SEC;
        if (en_lim_gr < lim_min && done >= nskip)
            lim_min = en_lim_gr;
        for (i = 0; i < n; i++, done++) {
            double l = buf[2 * i] / 8388608.0, r = buf[2 * i + 1] / 8388608.0, fade = 1.0;
            long k = done - nskip;
            if (k < 0)
                continue;
            if (fabs(l) > pk)
                pk = fabs(l);
            if (fabs(r) > pk)
                pk = fabs(r);
            ss += l * l + r * r;
            if (k < 66150)
                fade = k / 66150.0;              /* 1.5 s in */
            if (k > nout - 352800)
                fade = (nout - k) / 352800.0;    /* 8 s out */
            if (f) {
                wr16(f, (uint16_t)(int16_t)lrint(l * fade * 32767.0));
                wr16(f, (uint16_t)(int16_t)lrint(r * fade * 32767.0));
            }
            wrote++;
        }
    }
    if (f)
        fclose(f);
    printf("%-14s peak %6.1f dBFS  rms %6.1f dBFS  limiter %5.1f dB  render %.4f s/s\n", en_piece_name(world),
           20 * log10(pk + 1e-12), 10 * log10(ss / (2.0 * wrote) + 1e-20), 20 * log10(lim_min), render_s / (secs + skip));
    return 0;
}
