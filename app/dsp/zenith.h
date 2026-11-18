/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (AMB-1), Polystellarflow (ZENITH-1) */
/* The instrument: a generative ambient garden. Four layers draw on one pool of notes (a scale:
 * the root plus up to eleven more pitch classes) and one chord that drifts slowly through it. ZENITH-1 is AMB-1's
 * engine, grown: more sounds a layer, a texture of air, a shimmering plate, worlds, and Wander:
 *
 *   PAD     four breathing voices: each swells in, hangs and fades on its own cycle (unequal lengths,
 *           as the loops of Music for Airports), taking its note of the chord as it starts, so the
 *           chord forms and dissolves. Six sounds: Warm, Strings, Choir, Rhodes, Glass, Breath; a filter that
 *           blooms with each breath; then Warmth's low-pass and a slow four-tap ensemble. The chord moves every
 *           DRIFT (20 s .. 5 min), voiced as the world likes: voices on shared notes hold, the others breathe anew.
 *   BELLS   FM bells (seven sounds: Soft, Felt, Celesta, Marimba, Vibes, Gamelan, Crystal) played by four tape
 *           loops of unequal length (13, 17, 23, 29 quarter notes), each holding a short phrase that shifts against
 *           the others, each its own shade of bell; SHIMMER gives each a twin a little apart.
 *   BASS    on the chord's root (now and then its fifth), gliding: Sine, Warm, Round, Drone.
 *   PULSE   three voices, each on its own Euclidean pattern (16, 12 and 7 sixteenths): polyrhythms. Four kits:
 *           Soft (AMB-1's thump and hats), Brush, Wood, Gong; all tuned to the chord.
 *   AIR     a quiet texture: hiss, vinyl, wind, sea, rain.
 *
 * Over it all, the ARC: a slow tide, minutes long (TIDE), between sparse (fewer bells, a darker vowel, a quieter
 * pulse, more reverb) and full; MOTION sets how far it goes. Rests: with EVOLVE, the pulse sits out a few bars,
 * the bass a chord, a bell loop a pass. WANDER: slow offsets under every value set (not those set by hand), and
 * seasons, when a sound may change a while and the key may move (KEY MOVES) and come home.
 * Then, after Discreet Music's chain, an Echoplex (160 ms, fed back) and a tape loop (to ~5.6 s, two heads, each
 * repeat darker; TAPE: wow and saturation), and Dattorro's plate (from FoMni), its input above ~200 Hz and as loud
 * whatever its SIZE, with SHIMMER (its tail an octave up, fed back in). Last, a look-ahead limiter: under the
 * speaker's crackle line with nothing in the jack, near full scale with headphones (en_out).
 * FREEZE holds the chord and lets the plate ring on forever.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* bell tape loops */
#define EN_LOOP_MAX 37                 /* the longest a loop grows (an era may pick 7 .. 37 beats) */
#define EN_NDRUM 3                     /* pulse: thump, closed hat, open hat */
#define EN_NPAD 4                      /* pad voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, MODE_YO, MODE_PELOG, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern volatile uint8_t en_loop_len[EN_NLOOP];   /* each loop's length now: 13, 17, 23, 29 at first; each era its own */
#define EN_LOOP_LEN en_loop_len
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_PAD, P_BELLS, P_BASS, P_PULSE,                  /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_HITS,             /* motion */
    P_ECHO, P_ETIME, P_REVERB, P_SIZE,                /* space */
    P_BRIGHT, P_SWELL, P_BTONE, P_MOTION,             /* tone */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_BOCT,  /* setup, panel encoders, bell octave */
    P_PADTONE,                                        /* the pad's colour, set by the piece: Rhodes, Voices */
    P_BASSOCT, P_GLIDE, P_FIFTH,                      /* bass: its octave, its glide, how often it leans on the fifth */
    P_SWING, P_HATS,                                  /* pulse: swing, the hats' colour */
    P_BELLSND, P_BASSSND, P_KIT,                      /* ZENITH-1: the sounds of the bells, the bass, the pulse */
    P_PWARM, P_PMOVE, P_PSPACE, P_PWIDTH,             /* pad, its second page */
    P_SHIMB, P_BSPACE, P_BDECAY,                      /* bells, theirs (and P_BOCT) */
    P_BWARM, P_BMOVE, P_BASPACE,                      /* bass, its (and P_FIFTH) */
    P_KMOVE, P_KSPACE, P_KWIDTH,                      /* pulse, its (and P_HATS: its tone) */
    P_SHIM, P_RWARM, P_TAPE, P_WIDTH,                 /* space, its second page */
    P_AIR, P_AIRSND, P_AIRTONE, P_AIRMOVE,            /* air: a quiet texture */
    P_WANDER, P_TIDE, P_KEYMOVE,                      /* garden, its second page */
    P_WORLD,                                          /* the world (set by the piece): its tuning, its chords */
    P_JOURNEY,                                        /* Still, Stay (parts and eras), Slow, Often (and other worlds) */
    P_FXLVL, P_FXAUTO, P_FXSPACE,                     /* the effects: level, how often they play by themselves, space */
    P_FXON,                                           /* Auto FX on / off */
    P_LEAD, P_LEADSND, P_LSTYLE, P_LOFTEN,            /* the lead: a melody of its own; its sound, its way, how often */
    P_LTONE, P_LSPACE, P_LEXPR, P_LOCT,               /* its second page: tone, space, vibrato (octave: OCT- / OCT+) */
    P_LGLIDE,                                         /* 1.5: how much it glides and bends between notes (0: none) */
    P_POCT,                                           /* the pad's octave (OCT- / OCT+ on its pages) */
    P_KEYS, P_STRUM,                                  /* what the keys do (Scale, Strum, Play); Strum's bell sound */
    P_PATT, P_PREL, P_LATT, P_LREL,                   /* the pad's and the lead's attack and release */
    P_SLEEP,                                          /* Sleep: off, or a fade to rest after 30, 60, 90 minutes */
    P_SLEVEL,                                         /* Strum's level */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_set_hand(int param, int value);         /* as en_set, turned by hand: Wander leaves it as it is (pinned) */
void en_unpin(void);                           /* Wander may move everything again */
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* the garden grows / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_part_next(void);                       /* the journey: the next part, now */
void en_era(void);                             /* the journey: a new era (new loops, sounds, key; with Journey, maybe
                                                * a new world: en_journey_req) */
void en_fx(int i, int how);                    /* effect i (0..EN_NFX-1): how 0 once, 1 keep it on, 2 let it go */
void en_fx_off(void);                          /* every effect let go */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops, the home chord */
void en_out(int headphones);                   /* where the sound goes: 0 the speaker (a low ceiling), 1 headphones */
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level;
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a bell just rang from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is sitting a few bars out */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */
extern volatile float en_out_level;            /* the output's level now, 0..1 (for the picture) */
extern volatile float en_bass_pulse;           /* 0..1: the bass's swell (for the picture) */
extern volatile uint32_t en_chord_count;       /* chord changes so far (the picture shifts its colours) */
extern volatile float en_lim_gr;               /* the limiter's gain now, 1 = untouched (tests) */
extern volatile uint8_t en_root_now;           /* the key's root as it sounds (Key moves can take it away a while) */
extern volatile uint8_t en_pad_snd, en_bell_snd;   /* the sounds as they play (Wander may swap one a while) */
extern volatile uint32_t en_seasons;           /* seasons so far (tests) */
/* the bells as they ring, for the picture: a ring of the last 16 (loop, note, velocity 0..127) */
/* the sound itself, for the picture's wave: every 8th sample of the output (mono, -1..1), a ring of 256 */
#define EN_SCOPE 256
extern volatile float en_scope[EN_SCOPE];
extern volatile uint32_t en_scope_w;
#define EN_BEV 16
extern volatile uint8_t en_bev[EN_BEV][3];
extern volatile uint32_t en_bev_w;

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
#define EN_NPADSND 15
#define EN_NBELLSND 18
#define EN_NBASSSND 10
#define EN_NKIT 13
#define EN_NAIRSND 10
#define EN_NPART 6
#define EN_NFX 27
#define EN_NLEADSND 16
#define EN_NLSTYLE 6
#define EN_KEYS_SCALE 0                /* P_KEYS: the keys add or take out scale notes */
#define EN_KEYS_STRUM 1                /*         a key strums the chord, from its note up */
#define EN_KEYS_PLAY 2                 /*         the keys play the lead */
#define EN_FX_VOICES 1                 /* effects sounding at once, at most (the FM-1's CPU) */
extern const char *const EN_PART_NAME[EN_NPART];
extern const char *const EN_FX_NAME[EN_NFX];
extern volatile uint8_t en_part;               /* the part of the world playing */
extern volatile uint16_t en_eras;              /* eras so far (1 the first) */
extern volatile uint32_t en_journey_n;         /* the journey's asks for another world, so far (the UI takes each up) */
extern volatile int8_t en_journey_to;          /* the world it asks for */
extern volatile uint32_t en_fx_on, en_fx_kept;    /* effects sounding, effects kept on (bits) */
extern volatile int8_t en_fx_last;             /* the effect last started (-1 none), for the screen */
extern volatile uint32_t en_fx_count;          /* effects started so far */
extern volatile uint8_t en_lead_note;          /* the lead's note now (MIDI), for the sky */
extern volatile float en_lead_glow;            /* 0..1: the lead sounding */
extern volatile uint32_t en_lead_notes;        /* lead notes so far (tests) */
extern volatile uint32_t en_heals;             /* times the sound blew up and was rebuilt (never, we hope) */
extern volatile uint32_t en_sheds;             /* times the CPU ran out and the effects and the lead were stopped */
extern volatile uint8_t en_busy;               /* 1 while new effects are refused (the CPU near full, or just ran out) */
int en_fx_costly(int i);                       /* 1: effect i plays a sound of its own (refused when the CPU is busy) */
void en_lead_key(int note, int on);
void en_strum(int note);                       /* Keys: Strum: the chord strummed from this note up */
extern volatile uint8_t en_asleep;
extern volatile uint32_t en_fx_auto_seen;       /* the effects Auto FX has played (tests) */
extern volatile uint32_t en_strum_notes;       /* strummed notes (tests) */             /* Sleep: 1 once it has faded to rest */            /* Style Keys: a key down (on 1) or up (0); note -1, on 0: all up */
void en_cpu(int pct);                          /* the audio's CPU load now, percent (from the UI, each frame) */
void en_overload(void);                        /* from the audio interrupt: the render took over 90 % of its time */
const char *en_piece_name(int i);
int en_piece_world(void);                      /* the world playing (its picture, its colours) */
void en_piece(int i, int16_t *par, uint16_t *pool);
