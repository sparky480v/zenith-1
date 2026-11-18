/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (AMB-1), Polystellarflow (ZENITH-1) */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voice leading, names) and the
 * render (sound, no NaN, bounded, the loops evolve, the chord drifts, freeze holds). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "zenith.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_count;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
    midi_count++;
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static uint32_t gain = 2048;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, gain);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

/* a world as the UI takes it up: its recipe, its scale, fresh loops */
static void world(int w)
{
    int16_t par[P_NPARAMS];
    uint16_t pool;
    int p;
    en_piece(w, par, &pool);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    en_regrow();
}

/* ZENITH-1's own: the worlds, the sounds, the speaker's ceiling, the space at its limits, Wander */
/* the lead: every sound and every way plays notes, bounded, in its register; Solo plays even with the Lead down;
 * left alone, Lo-fi's sax comes and goes */
static void lead_tests(void)
{
    int snd, st;
    en_init();
    en_play(1);
    for (snd = 0; snd < EN_NLEADSND; snd++)
        for (st = 0; st < EN_NLSTYLE; st++) {
            uint32_t n0;
            world(2);
            en_set(P_JOURNEY, 0);
            en_set(P_LEAD, 70);
            en_set(P_LOFTEN, 100);
            en_set(P_LEADSND, snd);
            en_set(P_LSTYLE, st);
            en_set(P_FXON, 0);
            run(1.0f);
            n0 = en_lead_notes;
            peak = sum2 = 0.0f;
            bad = 0;
            run(40.0f);
            CHECK(!bad && peak <= 1.0f, "lead %d style %d: NaN or over (%f)", snd, st, (double)peak);
            CHECK(en_lead_notes - n0 >= 3, "lead %d style %d: %u notes in 40 s", snd, st, en_lead_notes - n0);
            CHECK(en_lead_note >= 40 && en_lead_note <= 108, "lead %d style %d: note %d", snd, st, en_lead_note);
        }
    {   /* Style Keys: no phrases of its own; a key plays it, held; let go, it fades */
        uint32_t n0;
        world(2);
        en_set(P_JOURNEY, 2);
        en_set(P_FXON, 0);
        en_set(P_LEAD, 60);
        en_set(P_KEYS, EN_KEYS_PLAY);
        run(2.0f);
        n0 = en_lead_notes;
        run(10.0f);
        CHECK(en_lead_notes == n0, "Style Keys: the lead played by itself");
        en_lead_key(60, 1);
        run(1.0f);
        CHECK(en_lead_notes == n0 + 1 && en_lead_glow > 0.3f && en_lead_note == 60, "Style Keys: a key, not heard (%f)",
              (double)en_lead_glow);
        en_lead_key(64, 1);
        run(0.5f);
        CHECK(en_lead_note == 64, "Style Keys: the second key");
        en_lead_key(64, 0);
        run(0.5f);
        CHECK(en_lead_note == 60 && en_lead_glow > 0.3f, "Style Keys: back to the first, still held");
        en_lead_key(60, 0);
        run(3.0f);
        CHECK(en_lead_glow < 0.05f, "Style Keys: let go, still sounding (%f)", (double)en_lead_glow);
        en_set(P_KEYS, EN_KEYS_SCALE);
        en_set(P_JOURNEY, 1);
        printf("  lead: Keys Play plays from the keys, held, the last key sounding\n");
    }
    {   /* Auto FX: every effect comes round, shuffled, all 27 before any again */
        world(0);
        en_set(P_FXON, 1);
        en_set(P_FXAUTO, 100);
        en_fx_auto_seen = 0;
        run(900.0f);
        CHECK(en_fx_auto_seen == (1u << EN_NFX) - 1u, "Auto FX: only %d of the effects in 15 minutes",
              __builtin_popcount(en_fx_auto_seen));
        printf("  auto fx: all %d effects came round\n", __builtin_popcount(en_fx_auto_seen));
        en_set(P_FXON, 0);
    }
    {   /* Keys: Strum: a key strums six notes of the chord, from its note up, at once heard */
        uint32_t r0;
        world(0);
        en_set(P_FXON, 0);
        en_set(P_KEYS, EN_KEYS_STRUM);
        en_set(P_STRUM, 11);
        r0 = en_strum_notes;
        en_strum(64);
        en_strum(67);
        en_strum(71);
        peak = 0.0f;
        bad = 0;
        run(2.0f);
        CHECK(en_strum_notes - r0 == 18, "Strum: %u notes for three keys", en_strum_notes - r0);
        CHECK(!bad && peak > 0.01f && peak <= 1.0f, "Strum: NaN %d, peak %f", bad, (double)peak);
        en_set(P_KEYS, EN_KEYS_SCALE);
        printf("  strum: three keys, eighteen notes\n");
    }
    {   /* Sleep: 30 minutes, the last fading, then at rest; PLAY wakes it */
        world(0);
        en_set(P_SLEEP, 1);
        run(29.0f * 60.0f);
        CHECK(en_playing && !en_asleep, "Sleep: rested early");
        run(1.5f * 60.0f);
        CHECK(!en_playing && en_asleep, "Sleep: still playing after 30 min");
        en_play(1);
        run(2.0f);
        CHECK(en_playing && !en_asleep, "Sleep: PLAY did not wake it");
        en_set(P_SLEEP, 0);
        printf("  sleep: faded and rested after 30 minutes; PLAY woke it\n");
    }
    {
        uint32_t n0;
        world(2);
        en_set(P_LEAD, 0);
        en_set(P_FXON, 0);
        run(2.0f);
        n0 = en_lead_notes;
        run(20.0f);
        CHECK(en_lead_notes == n0, "the lead played with its level at 0");
        en_fx(26, 0);                            /* Solo */
        run(15.0f);
        CHECK(en_lead_notes > n0, "Solo: no lead notes");
        printf("  lead: 8 sounds x 6 ways play; Solo plays %u notes with the Lead down\n", en_lead_notes - n0);
    }
    {
        uint32_t n0;
        int on = 0, k;
        world(2);
        en_set(P_FXON, 0);
        n0 = en_lead_notes;
        for (k = 0; k < 60; k++) {
            uint32_t a = en_lead_notes;
            run(5.0f);
            on += en_lead_notes != a;
        }
        printf("  Lo-fi left alone 5 min: %u lead notes, playing in %d of 60 five-second spans\n", en_lead_notes - n0, on);
        CHECK(on >= 8 && on <= 57, "the lead never rests or never plays: %d of 60", on);
    }
}

static void zenith_tests(void)
{
    int w, k, p;
    static const int SNDP[5] = {P_PADTONE, P_BELLSND, P_BASSSND, P_KIT, P_AIRSND};
    static const int LVLP[5] = {P_PAD, P_BELLS, P_BASS, P_PULSE, P_AIR};
    en_init();
    en_play(1);
    for (w = 0; w < EN_NPIECES; w++) {          /* every world: sound, bounded, under the speaker's ceiling */
        world(w);
        en_out(0);
        gain = 4096;
        run(3.0f);
        peak = sum2 = 0.0f;
        bad = 0;
        run(30.0f);
        CHECK(!bad, "%s: NaN", en_piece_name(w));
        CHECK(peak <= 0.152f, "%s on the speaker at full volume: peak %.3f", en_piece_name(w), (double)peak);
        CHECK(peak > 0.02f, "%s: silent on the speaker", en_piece_name(w));
        en_out(1);
        peak = sum2 = 0.0f;
        run(30.0f);
        CHECK(!bad && peak <= 1.0f && peak > 0.05f, "%s on headphones: peak %.3f", en_piece_name(w), (double)peak);
        printf("  %-13s headphones at full: peak %.2f rms %.3f\n", en_piece_name(w), (double)peak,
               (double)sqrtf(sum2 / (30.0f * 44100.0f * 2.0f)));
    }
    gain = 2048;
    for (k = 0; k < 5; k++) {                   /* every sound of every layer, alone: it sounds */
        int lo = en_param_info(SNDP[k])->lo, hi = en_param_info(SNDP[k])->hi, v;
        for (v = lo; v <= hi; v++) {
            world(2);                           /* Lo-fi: it has every layer */
            for (p = 0; p < 5; p++)
                en_set(LVLP[p], p == k ? 80 : 0);
            en_set(P_HITS, 60);
            en_set(SNDP[k], v);
            en_set(P_WANDER, 0);
            run(8.0f);
            peak = 0.0f;
            bad = 0;
            run(12.0f);
            CHECK(!bad && peak > 0.002f, "%s %d: silent or NaN (peak %f)", en_param_info(SNDP[k])->name, v, (double)peak);
        }
    }
    /* the space at its limits: shimmer, size, echo, tape all the way up, for three minutes: it rings, it never runs away */
    world(3);
    en_set(P_SHIM, 100);
    en_set(P_SIZE, 100);
    en_set(P_REVERB, 100);
    en_set(P_ECHO, 100);
    en_set(P_TAPE, 100);
    en_set(P_PSPACE, 100);
    en_set(P_BSPACE, 100);
    {
        float r1, r2;
        run(60.0f);
        sum2 = 0.0f;
        run(30.0f);
        r1 = sum2;
        run(60.0f);
        sum2 = 0.0f;
        run(30.0f);
        r2 = sum2;
        CHECK(!bad && r2 < r1 * 2.0f, "the space at its limits grows: %f -> %f", (double)r1, (double)r2);
    }
    /* Wander and Key moves at full, the tides at their quickest: seasons pass, the key leaves home and comes back,
     * a sound turned by hand stays */
    world(0);
    en_set(P_JOURNEY, 0);                       /* Wander alone (an era would free what the hand set) */
    en_set(P_WANDER, 100);
    en_set(P_KEYMOVE, 100);
    en_set(P_TIDE, 0);
    en_set_hand(P_BELLSND, 3);
    {
        int moved = 0, home = 0, pad_swaps = 0, bell_kept = 1, last_pad = -1, t;
        for (t = 0; t < 40 * 60; t += 5) {
            run(5.0f);
            if (en_root_now != 2)
                moved = 1;
            else if (moved)
                home = 1;
            if (last_pad >= 0 && en_pad_snd != last_pad)
                pad_swaps++;
            last_pad = en_pad_snd;
            if (en_bell_snd != 3)
                bell_kept = 0;
        }
        printf("  wander 40 min: %u seasons, key moved %d, home again %d, pad sound changes %d\n", en_seasons, moved,
               home, pad_swaps);
        CHECK(en_seasons >= 4, "seasons: %u", en_seasons);
        CHECK(moved, "the key never moved");
        CHECK(bell_kept, "the bells' sound, set by hand, was changed");
        CHECK(!bad, "NaN while wandering");
        en_unpin();
        world(0);
        run(1.0f);
        CHECK(en_root_now == 2, "a new world: home again (root %d)", en_root_now);
    }
    /* the journey, Often, the tides quick: parts change, eras come, the loops take new lengths, and the world
     * asks for another; every effect, once and kept, stays bounded and stops when let go */
    {
        uint32_t asks = en_journey_n, parts = 0, lens = 0, eras0;
        int last = -1, t, l0 = en_loop_len[0] * 100 + en_loop_len[3];
        world(1);
        en_set(P_JOURNEY, 3);
        en_set(P_TIDE, 0);
        run(0.1f);
        eras0 = en_eras;
        bad = 0;
        for (t = 0; t < 30 * 60; t += 5) {
            run(5.0f);
            if (en_part != last) {
                parts++;
                last = en_part;
            }
            if (en_loop_len[0] * 100 + en_loop_len[3] != l0)
                lens = 1;
        }
        printf("  journey 30 min: %u parts, %u new eras, %u worlds asked for, new loop lengths %u\n", parts, en_eras - eras0,
               en_journey_n - asks, lens);
        CHECK(parts >= 8, "the parts: %u", parts);
        CHECK(en_eras - eras0 >= 1, "no new era in 30 minutes");
        CHECK(en_journey_n - asks >= 1, "the journey never asked for another world");
        CHECK(lens, "the loops kept their lengths");
        CHECK(!bad, "NaN on the journey");
    }
    {   /* every effect once: sounding, bounded, over by itself; two at most; the CPU running out stops them */
        int f;
        uint32_t sh;
        world(0);
        en_set(P_JOURNEY, 1);
        en_set(P_FXON, 0);
        for (f = 0; f < EN_NFX; f++) {
            en_fx(f, 1);                         /* (a keep asked for: played once all the same) */
            peak = 0.0f;
            bad = 0;
            run(0.5f);
            CHECK(en_fx_on >> f & 1u, "%s: not sounding", EN_FX_NAME[f]);
            run(5.5f);
            CHECK(!bad && peak <= 1.0f, "%s: NaN or past full scale", EN_FX_NAME[f]);
            CHECK(en_fx_kept == 0, "%s: kept", EN_FX_NAME[f]);
            run(18.0f);
            CHECK(!(en_fx_on >> f & 1u), "%s: still sounding after 24 s", EN_FX_NAME[f]);
        }
        en_fx(12, 0);
        en_fx(13, 0);
        en_fx(14, 0);
        run(1.0f);
        CHECK(__builtin_popcount(en_fx_on) <= EN_FX_VOICES, "%d effects at once", __builtin_popcount(en_fx_on));
        sh = en_sheds;
        en_fx(19, 0);                            /* an effect playing through one overload: it plays on */
        run(0.2f);
        en_overload();
        run(0.1f);
        CHECK((en_fx_on >> 19 & 1u) && en_sheds == sh + 1 && !en_busy, "one overload: effects %x, sheds %u, busy %d",
              en_fx_on, en_sheds, en_busy);
        en_overload();                           /* again, soon after: the effects stop, a moment's wait */
        run(0.1f);
        CHECK(en_fx_on == 0 && en_busy, "a second overload: effects %x, busy %d", en_fx_on, en_busy);
        en_fx(8, 0);                             /* Raindrops: bells of its own, refused */
        run(1.0f);
        CHECK(!(en_fx_on >> 8 & 1u), "a costly effect started while shedding");

        run(3.5f);
        CHECK(!en_busy, "still busy 4 s after the overload");
        en_fx(0, 0);
        run(0.5f);
        CHECK(en_fx_on & 1u, "no effect after the overload passed");
        en_cpu(95);                              /* busy, but no overload: effects still play (in the bells' voices) */
        en_fx(4, 0);
        run(0.5f);
        CHECK(en_fx_on >> 4 & 1u, "an effect refused at 95%% CPU with no overload");
        en_cpu(40);
        en_fx_off();
        run(10.0f);
        CHECK(en_fx_on == 0, "all let go: %x still on", en_fx_on);
    }
    {   /* Grains, every world, again and again: alive after (as the Whale was checked) */
        int w2, r;
        for (w2 = 0; w2 < EN_NPIECES; w2++) {
            world(w2);
            en_set(P_FXON, 0);
            bad = 0;
            for (r = 0; r < 4; r++) {
                en_fx(0, 0);
                run(3.0f);
            }
            peak = 0.0f;
            run(8.0f);
            CHECK(!bad && peak > 0.01f && peak <= 1.0f, "%s after Grains: NaN %d, peak %f", en_piece_name(w2), bad,
                  (double)peak);
        }
        CHECK(en_heals == 0, "the sound blew up %u times", en_heals);
    }
    {   /* a layer turned by hand is heard at once, whatever the part: the lead at dawn */
        uint32_t n0;
        world(2);
        en_set(P_JOURNEY, 2);
        en_part_next();
        run(0.1f);
        while (en_part != 0)
            en_part_next(), run(0.1f);
        run(20.0f);
        n0 = en_lead_notes;
        en_set_hand(P_LEADSND, 0);
        en_set_hand(P_LEAD, 60);
        run(4.0f);
        CHECK(en_lead_notes > n0, "the lead, turned up by hand at dawn: silent 4 s later");
        en_set(P_JOURNEY, 1);
    }
    gain = 2048;
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);

    /* voice leading: small moves, in range, no unisons */
    {
        int from[4] = {50, 57, 62, 66}, to[4], cm7[4] = {0, 4, 7, 11}, f7[4] = {5, 9, 0, 4}, tot = 0;
        en_voice_lead(from, cm7, 4, to, 48, 77);
        for (i = 0; i < 4; i++) {
            CHECK(to[i] >= 48 && to[i] <= 77, "range %d", to[i]);
            tot += abs(to[i] - from[i]);
        }
        CHECK(tot <= 8, "Cmaj7 from D-ish: moved %d", tot);
        memcpy(from, to, sizeof from);
        en_voice_lead(from, f7, 4, to, 48, 77);
        tot = 0;
        for (i = 0; i < 4; i++)
            tot += abs(to[i] - from[i]);
        CHECK(tot <= 6, "CM7 -> FM7 moved %d", tot);
        CHECK(to[0] != to[1] && to[1] != to[2] && to[2] != to[3] && to[0] != to[3], "no unisons");
    }

    /* texts */
    {
        char t[12];
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "20s"), "drift 0: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "5:00"), "drift 100: %s", t);
        en_param_text(P_ETIME, 3, t);
        CHECK(!strcmp(t, "1 bar"), "etime: %s", t);
        en_param_text(P_BOCT, 1, t);
        CHECK(!strcmp(t, "+1"), "boct: %s", t);
    }

    /* the render: silent at rest, sound when growing, never NaN, bounded */
    en_init();
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.05f && peak < 0.99f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u);
    CHECK(en_bars / 16u >= 5u, "the grid ran (%u sixteenths)", en_bars);
    CHECK(en_pad_level > 0.5f, "the pad swelled (%f)", (double)en_pad_level);

    /* the chord drifts: with Drift at its shortest, a few changes in two minutes */
    {
        int changes = 0, last = en_chord_root * 16 + en_chord[0] % 16, k;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 120; k++) {
            int now;
            run(1.0f);
            now = en_chord_root * 16 + en_chord[0] % 16;
            changes += now != last;
            last = now;
        }
        CHECK(changes >= 3, "chord changes in 2 min at Drift 0: %d", changes);
        printf("  chord changes in 2 min: %d\n", changes);
    }

    /* a chord change is heard: within 2 s no sounding voice sings a note outside the new chord */
    {
        int k, i, bad_s = 0, changes = 0, last = -1;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 90 * 4; k++) {
            int sig = en_chord[0] | en_chord[1] << 7 | en_chord[2] << 14 | en_chord[3] << 21;
            static int since = -1;
            run(0.25f);
            if (sig != last) {
                if (last >= 0) {
                    changes++;
                    since = 0;
                }
                last = sig;
            } else if (since >= 0 && ++since == 8) {   /* 2 s after a change */
                uint32_t pcs = 0;
                for (i = 0; i < EN_NPAD; i++)
                    pcs |= 1u << (en_chord[i] % 12);
                for (i = 0; i < EN_NPAD; i++)
                    if (en_pad_amp[i] > 0.05f && !(pcs >> (en_pad_note[i] % 12) & 1u))
                        bad_s++;
                since = -1;
            }
        }
        CHECK(changes >= 3, "chord changes in 90 s at Drift 0: %d", changes);
        CHECK(!bad_s, "%d voices still off the new chord 2 s after a change", bad_s);
    }

    /* the loops evolve: with Evolve at 100, the phrases change */
    {
        int8_t before[EN_NLOOP][EN_LOOP_MAX];
        int diff = 0, l;
        en_set(P_EVOLVE, 100);
        en_set(P_DENSITY, 80);
        memcpy(before, (const void *)en_loop_note, sizeof before);
        run(90.0f);
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                diff += before[l][i] != en_loop_note[l][i];
        CHECK(diff > 0, "the loops evolved");
        for (l = 0; l < EN_NLOOP; l++)
            for (i = EN_LOOP_LEN[l]; i < EN_LOOP_MAX; i++)
                CHECK(en_loop_note[l][i] == -1, "loop %d slot %d past its end", l, i);
    }

    /* freeze: the chord holds */
    {
        int held = en_chord[0] + 128 * en_chord[1], k;
        en_freeze(1);
        for (k = 0; k < 60; k++) {
            run(1.0f);
            CHECK(en_chord[0] + 128 * en_chord[1] == held, "the chord moved while frozen");
        }
        en_freeze(0);
    }

    /* a new key while playing (a piece, the root, the scale): within 3 s no voice still sings the old
     * chord (a voice only takes its note at its next breath otherwise, up to a minute later) */
    {
        int k, stale = 0;
        en_freeze(0);
        en_set(P_ROOT, 6);                       /* a tritone away: nothing in common to hide behind */
        en_pool(EN_MODE_POOL[MODE_LYDIAN]);
        en_regrow();
        run(3.0f);
        for (k = 0; k < EN_NPAD; k++)
            if (en_pad_amp[k] > 0.02f && en_pad_note[k] != en_chord[k]) {
                printf("  voice %d sings %d, the chord has %d (amp %.2f)\n", k, en_pad_note[k], en_chord[k],
                       (double)en_pad_amp[k]);
                stale++;
            }
        CHECK(!stale, "%d voices still in the old key after 3 s", stale);
        en_set(P_ROOT, 2);
    }

    /* a pool change: the chord's tones are in it */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_pool(pent);
        run(0.1f);
        for (i = 0; i < 4; i++)
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        en_set(P_TEMPO, 30);
        run(10.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* Bell tone and Bell oct are heard: brighter (zero crossings) and higher with them, bells alone */
    {
        float zc[3];
        int k;
        static const int TONE[3] = {0, 100, 0}, OCT[3] = {0, 0, 2};
        for (k = 0; k < 3; k++) {
            int b2, j;
            long z = 0;
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_LEAD, 0);
            en_set(P_FXON, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_DENSITY, 100);
            en_set(P_BTONE, TONE[k]);
            en_set(P_BOCT, OCT[k]);
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    z += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
            zc[k] = (float)z;
        }
        CHECK(zc[1] > 2.5f * zc[0], "Bell tone 100 not brighter: %.0f vs %.0f", (double)zc[1], (double)zc[0]);
        CHECK(zc[2] > 1.8f * zc[0], "Bell oct +2 not higher: %.0f vs %.0f", (double)zc[2], (double)zc[0]);
        en_init();
        en_play(1);
    }

    zenith_tests();
    lead_tests();

    /* stop: the tails die away */
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_play(0);
    run(40.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    printf(fails ? "zenith: %d FAILED\n" : "zenith: ok\n", fails);
    return fails != 0;
}
