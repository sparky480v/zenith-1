/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (AMB-1), Polystellarflow (ZENITH-1) */
/* The instrument (zenith.h): AMB-1's engine, grown. One compilation unit, -O2. */
#include "zenith.h"
#include "fastmath.h"

#if defined EN_HOST || defined PLAT_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Aeolian",
                                             "Pent maj", "Pent min", "Yo", "Pelog"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10),
    B(0, 2, 5, 7, 9, 9, 9),  B(0, 1, 3, 7, 8, 8, 8)};   /* yo (Japanese), pelog selisir (Balinese, its tuning below) */
#undef B
volatile uint8_t en_loop_len[EN_NLOOP] = {13, 17, 23, 29};
static const uint8_t LOOP_LEN0[EN_NLOOP] = {13, 17, 23, 29};
const uint8_t EN_DRUM_N[EN_NDRUM] = {16, 12, 7};

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Pad", 0, 100, 60},      {"Bells", 0, 100, 65},    {"Bass", 0, 100, 45},     {"Pulse", 0, 100, 40},
    {"Density", 0, 100, 40},  {"Evolve", 0, 100, 35},   {"Drift", 0, 100, 25},    {"Hits", 0, 100, 35},
    {"Echo", 0, 100, 45},     {"Echo time", 0, 5, 3},   {"Reverb", 0, 100, 65},   {"Size", 0, 100, 80},
    {"Bright", 0, 100, 40},   {"Swell", 0, 100, 60},    {"Bell tone", 0, 100, 50}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 30, 120, 72},   {"Root", 0, 11, 2},
    {"Mode", 0, EN_NMODES - 1, 1}, {"Bell oct", -2, 2, 0}, {"Pad sound", 0, EN_NPADSND - 1, 0},
    {"Bass oct", -1, 1, 0},   {"Glide", 0, 100, 20},    {"Fifth", 0, 100, 35},
    {"Swing", 0, 100, 0},     {"Hats", 0, 100, 50},
    {"Bell sound", 0, EN_NBELLSND - 1, 0}, {"Bass sound", 0, EN_NBASSSND - 1, 0}, {"Kit", 0, EN_NKIT - 1, 0},
    {"Warmth", 0, 100, 45},   {"Movement", 0, 100, 50}, {"Space", 0, 100, 50},   {"Width", 0, 100, 60},
    {"Shimmer", 0, 100, 40},  {"Space", 0, 100, 50},    {"Decay", 0, 100, 50},
    {"Warmth", 0, 100, 50},   {"Movement", 0, 100, 30}, {"Space", 0, 100, 40},
    {"Movement", 0, 100, 40}, {"Space", 0, 100, 40},    {"Width", 0, 100, 50},
    {"Shimmer", 0, 100, 30},  {"Warmth", 0, 100, 40},   {"Tape", 0, 100, 15},    {"Width", 0, 100, 60},
    {"Air", 0, 100, 0},       {"Air sound", 0, EN_NAIRSND - 1, 0}, {"Tone", 0, 100, 50}, {"Movement", 0, 100, 50},
    {"Wander", 0, 100, 50},   {"Tide", 0, 100, 50},     {"Key moves", 0, 100, 30},
    {"World", 0, EN_NPIECES - 1, 0},
    {"Journey", 0, 3, 3},     {"FX level", 0, 100, 60}, {"How often", 0, 100, 35}, {"FX space", 0, 100, 60},
    {"Auto FX", 0, 1, 1},
    {"Lead", 0, 100, 0},      {"Lead sound", 0, EN_NLEADSND - 1, 0}, {"Style", 0, EN_NLSTYLE - 1, 0}, {"Often", 0, 100, 45},
    {"Tone", 0, 100, 50},     {"Space", 0, 100, 55},    {"Vibrato", 0, 100, 45}, {"Lead oct", -1, 1, 0},
    {"Glide", 0, 100, 20},
    {"Pad oct", -1, 1, 0},
    {"Keys", 0, 2, 0},        {"Strum sound", 0, EN_NBELLSND - 1, 11},
    {"Attack", 0, 100, 50},   {"Release", 0, 100, 50}, {"Attack", 0, 100, 50},  {"Release", 0, 100, 50},
    {"Sleep", 0, 3, 0},
    {"Strum level", 0, 100, 50},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

static const char *const PADSND_NAME[EN_NPADSND] = {"Warm", "Strings", "Choir", "Rhodes", "Glass", "Breath",
                                                    "Organ", "Juno", "Bowed", "Halo", "Dream", "Brass", "Ether",
                                                    "Cathedral", "Resonant"};
static const char *const BELLSND_NAME[EN_NBELLSND] = {"Soft", "Felt", "Celesta", "Marimba", "Vibes", "Gamelan", "Crystal",
                                                      "Kalimba", "Music box", "Bowl", "Chime", "Harp", "Steel", "Glock", "Windchime",
                                                      "Tingsha", "Dulcimer", "EP bell"};
static const char *const BASSSND_NAME[EN_NBASSSND] = {"Sine", "Warm", "Round", "Drone", "Moog", "Pluck", "Throb",
                                                      "Upright", "FM", "Reese"};
static const char *const KIT_NAME[EN_NKIT] = {"Soft", "Brush", "Wood", "Gong", "Hands", "Glitch", "Lo-fi", "Toy",
                                              "Drops", "Dub", "808 Soft", "Shaker", "Glass"};
static const char *const AIRSND_NAME[EN_NAIRSND] = {"Drips", "Frogs", "Wind", "Sea", "Rain", "Leaves", "Night",
                                                    "Stream", "Birds", "Cosmos"};
static const char *const LEADSND_NAME[EN_NLEADSND] = {"Sax", "Flute", "Trumpet", "Clarinet", "Whistle", "Glass",
                                                      "Synth", "Ocarina", "Harmonica", "Pan flute", "Oboe",
                                                      "Theremin", "Flugel", "Square", "Voice", "Cello"};
static const char *const LSTYLE_NAME[EN_NLSTYLE] = {"Wander", "Answer", "Arpeggio", "Ostinato", "Hymn", "Sparse"};
static const char *const KEYS_NAME[3] = {"Scale", "Strum", "Play"};
static const char *const SLEEP_NAME[4] = {"Off", "30 min", "60 min", "90 min"};
static const char *const JOURNEY_NAME[4] = {"Off", "Stay", "Sometimes", "Often"};
const char *const EN_PART_NAME[EN_NPART] = {"Dawn", "Bloom", "Flow", "Shimmer", "Deep", "Dissolve"};
const char *const EN_FX_NAME[EN_NFX] = {"Grains", "Snowfall", "Music box", "Kalimba", "Arp", "Cascade", "Harp",
                                        "Chimes", "Raindrops", "Echo notes", "Strum", "Twinkle", "Fifths", "Lullaby",
                                        "Whirl", "Unfold", "Ripple", "Bounce", "Call & reply", "Glimmer", "Rise",
                                        "Clock", "Stars", "Meteor", "Wind harp", "Peal", "Solo"};

/* the worlds: each a recipe for the whole garden (the FM-1's PRESETS). A list of (parameter, value) pairs over the
 * defaults, ended by -1. Tune and MIDI out are taken from here by neither: they belong to the player.
 *   pad: 0 Warm 1 Strings 2 Choir 3 Rhodes 4 Glass 5 Breath   bells: 0 Soft 1 Felt 2 Celesta 3 Marimba 4 Vibes
 *   5 Gamelan 6 Crystal   bass: 0 Sine 1 Warm 2 Round 3 Drone   kit: 0 Soft 1 Brush 2 Wood 3 Gong
 *   air: 0 Hiss 1 Vinyl 2 Wind 3 Sea 4 Rain   modes: 0 Ion 1 Dor 2 Phr 3 Lyd 4 Mix 5 Aeo 6 PMaj 7 PMin 8 Yo 9 Pelog */
static const int16_t W_WARM[] = {
    P_TEMPO, 66, P_ROOT, 2, P_MODE, 3, P_EVOLVE, 35, P_MOTION, 55, P_DRIFT, 30,
    P_PAD, 64, P_PADTONE, 0, P_SWELL, 70, P_BRIGHT, 45, P_PWARM, 45, P_PMOVE, 55, P_PSPACE, 60, P_PWIDTH, 75,
    P_BELLS, 52, P_BELLSND, 0, P_DENSITY, 35, P_BTONE, 30, P_SHIMB, 55, P_BSPACE, 60, P_BDECAY, 60, P_BOCT, 0,
    P_BASS, 42, P_BASSSND, 0, P_BASSOCT, 0, P_GLIDE, 40, P_FIFTH, 30, P_BWARM, 50, P_BMOVE, 30, P_BASPACE, 30,
    P_PULSE, 34, P_HITS, 28, P_KIT, 0, P_SWING, 0,
    P_ECHO, 40, P_ETIME, 3, P_REVERB, 70, P_SIZE, 88, P_SHIM, 45, P_RWARM, 40, P_TAPE, 15, P_WIDTH, 75,
    P_AIR, 0, 
    P_LEAD, 45, P_LEADSND, 6, P_LSTYLE, 0, P_LOFTEN, 40,
    -1};
static const int16_t W_CINE[] = {
    P_TEMPO, 58, P_ROOT, 0, P_MODE, 5, P_EVOLVE, 30, P_MOTION, 50, P_DRIFT, 35,
    P_PAD, 70, P_PADTONE, 1, P_SWELL, 85, P_BRIGHT, 50, P_PWARM, 40, P_PMOVE, 55, P_PSPACE, 65, P_PWIDTH, 75,
    P_BELLS, 48, P_BELLSND, 1, P_DENSITY, 25, P_BTONE, 25, P_SHIMB, 20, P_BSPACE, 60, P_BDECAY, 65, P_BOCT, 0,
    P_BASS, 50, P_BASSSND, 3, P_BASSOCT, 0, P_GLIDE, 50, P_FIFTH, 0, P_BWARM, 55, P_BMOVE, 40, P_BASPACE, 40,
    P_PULSE, 40, P_HITS, 22, P_KIT, 4, P_SWING, 0,
    P_ECHO, 25, P_ETIME, 4, P_REVERB, 75, P_SIZE, 92, P_SHIM, 20, P_RWARM, 40, P_TAPE, 5, P_WIDTH, 75,
    P_AIR, 0, 
    P_LEAD, 45, P_LEADSND, 2, P_LSTYLE, 4, P_LOFTEN, 35,
    -1};
static const int16_t W_LOFI[] = {
    P_TEMPO, 78, P_ROOT, 5, P_MODE, 1, P_EVOLVE, 45, P_MOTION, 50, P_DRIFT, 18,
    P_PAD, 58, P_PADTONE, 3, P_SWELL, 45, P_BRIGHT, 50, P_PWARM, 68, P_PMOVE, 60, P_PSPACE, 40, P_PWIDTH, 55,
    P_BELLS, 42, P_BELLSND, 4, P_DENSITY, 35, P_BTONE, 40, P_SHIMB, 45, P_BSPACE, 45, P_BDECAY, 45, P_BOCT, 0,
    P_BASS, 45, P_BASSSND, 1, P_BASSOCT, 0, P_GLIDE, 25, P_FIFTH, 40, P_BWARM, 55, P_BMOVE, 20, P_BASPACE, 20,
    P_PULSE, 28, P_KIT, 1, P_HITS, 45, P_SWING, 40, P_HATS, 35, P_KMOVE, 40, P_KSPACE, 30, P_KWIDTH, 60,
    P_ECHO, 42, P_ETIME, 1, P_REVERB, 45, P_SIZE, 65, P_SHIM, 0, P_RWARM, 70, P_TAPE, 75, P_WIDTH, 55,
    P_AIR, 38, P_AIRSND, 4, P_AIRTONE, 40, P_AIRMOVE, 50, 
    P_LEAD, 46, P_LEADSND, 0, P_LSTYLE, 1, P_LOFTEN, 45,
    -1};
static const int16_t W_GLASS[] = {
    P_TEMPO, 80, P_ROOT, 9, P_MODE, 6, P_EVOLVE, 45, P_MOTION, 55, P_DRIFT, 25,
    P_PAD, 52, P_PADTONE, 4, P_SWELL, 60, P_BRIGHT, 58, P_PWARM, 25, P_PMOVE, 45, P_PSPACE, 70, P_PWIDTH, 70,
    P_BELLS, 58, P_BELLSND, 6, P_DENSITY, 55, P_BTONE, 55, P_SHIMB, 70, P_BSPACE, 65, P_BDECAY, 65, P_BOCT, 1,
    P_BASS, 32, P_BASSSND, 0, P_BASSOCT, 0, P_GLIDE, 15, P_FIFTH, 25, P_BWARM, 40, P_BMOVE, 20, P_BASPACE, 30,
    P_PULSE, 43, P_HITS, 32, P_KIT, 5, P_SWING, 0,
    P_ECHO, 50, P_ETIME, 2, P_REVERB, 70, P_SIZE, 90, P_SHIM, 70, P_RWARM, 20, P_TAPE, 0, P_WIDTH, 70,
    P_AIR, 10, P_AIRSND, 2, P_AIRTONE, 70, P_AIRMOVE, 40, 
    P_LEAD, 46, P_LEADSND, 5, P_LSTYLE, 2, P_LOFTEN, 45,
    -1};
static const int16_t W_KANKYO[] = {
    P_TEMPO, 70, P_ROOT, 7, P_MODE, 8, P_EVOLVE, 40, P_MOTION, 45, P_DRIFT, 25,
    P_PAD, 46, P_PADTONE, 3, P_SWELL, 55, P_BRIGHT, 35, P_PWARM, 30, P_PMOVE, 30, P_PSPACE, 50, P_PWIDTH, 65,
    P_BELLS, 62, P_BELLSND, 3, P_DENSITY, 45, P_BTONE, 35, P_SHIMB, 0, P_BSPACE, 55, P_BDECAY, 50, P_BOCT, 0,
    P_BASS, 30, P_BASSSND, 2, P_BASSOCT, 0, P_GLIDE, 20, P_FIFTH, 25, P_BWARM, 45, P_BMOVE, 20, P_BASPACE, 30,
    P_PULSE, 30, P_KIT, 2, P_HITS, 32, P_SWING, 10, P_HATS, 50, P_KMOVE, 30, P_KSPACE, 45, P_KWIDTH, 70,
    P_ECHO, 55, P_ETIME, 2, P_REVERB, 50, P_SIZE, 75, P_SHIM, 10, P_RWARM, 35, P_TAPE, 10, P_WIDTH, 65,
    P_AIR, 20, P_AIRSND, 4, P_AIRTONE, 45, P_AIRMOVE, 40, 
    P_LEAD, 42, P_LEADSND, 1, P_LSTYLE, 5, P_LOFTEN, 50,
    -1};
static const int16_t W_GAMELAN[] = {
    P_TEMPO, 72, P_ROOT, 4, P_MODE, 9, P_EVOLVE, 40, P_MOTION, 45, P_DRIFT, 40,
    P_PAD, 40, P_PADTONE, 0, P_SWELL, 75, P_BRIGHT, 32, P_PWARM, 50, P_PMOVE, 45, P_PSPACE, 60, P_PWIDTH, 75,
    P_BELLS, 66, P_BELLSND, 5, P_DENSITY, 60, P_BTONE, 50, P_SHIMB, 80, P_BSPACE, 55, P_BDECAY, 60, P_BOCT, 0,
    P_BASS, 30, P_BASSSND, 0, P_BASSOCT, 0, P_GLIDE, 30, P_FIFTH, 20, P_BWARM, 50, P_BMOVE, 20, P_BASPACE, 30,
    P_PULSE, 22, P_KIT, 3, P_HITS, 40, P_SWING, 0, P_HATS, 50, P_KMOVE, 30, P_KSPACE, 55, P_KWIDTH, 60,
    P_ECHO, 35, P_ETIME, 3, P_REVERB, 65, P_SIZE, 85, P_SHIM, 15, P_RWARM, 45, P_TAPE, 0, P_WIDTH, 80,
    P_AIR, 12, P_AIRSND, 2, P_AIRTONE, 40, P_AIRMOVE, 35, 
    P_LEAD, 42, P_LEADSND, 7, P_LSTYLE, 3, P_LOFTEN, 40,
    -1};
static const int16_t W_DEEP[] = {
    P_TEMPO, 50, P_ROOT, 9, P_MODE, 5, P_EVOLVE, 35, P_MOTION, 60, P_DRIFT, 45,
    P_PAD, 70, P_PADTONE, 2, P_SWELL, 90, P_BRIGHT, 45, P_PWARM, 35, P_PMOVE, 55, P_PSPACE, 55, P_PWIDTH, 65,
    P_BELLS, 42, P_BELLSND, 0, P_DENSITY, 25, P_BTONE, 30, P_SHIMB, 35, P_BSPACE, 70, P_BDECAY, 75, P_BOCT, 0,
    P_BASS, 50, P_BASSSND, 0, P_BASSOCT, -1, P_GLIDE, 70, P_FIFTH, 20, P_BWARM, 60, P_BMOVE, 40, P_BASPACE, 30,
    P_PULSE, 38, P_HITS, 18, P_KIT, 4, P_SWING, 0,
    P_ECHO, 50, P_ETIME, 5, P_REVERB, 80, P_SIZE, 95, P_SHIM, 35, P_RWARM, 40, P_TAPE, 20, P_WIDTH, 68,
    P_AIR, 30, P_AIRSND, 3, P_AIRTONE, 40, P_AIRMOVE, 45, 
    P_LEAD, 44, P_LEADSND, 3, P_LSTYLE, 4, P_LOFTEN, 35,
    -1};
static const int16_t W_AIRPORTS[] = {
    P_TEMPO, 60, P_ROOT, 1, P_MODE, 0, P_EVOLVE, 30, P_MOTION, 40, P_DRIFT, 40,
    P_PAD, 60, P_PADTONE, 2, P_SWELL, 70, P_BRIGHT, 45, P_PWARM, 35, P_PMOVE, 40, P_PSPACE, 55, P_PWIDTH, 60,
    P_BELLS, 70, P_BELLSND, 1, P_DENSITY, 35, P_BTONE, 20, P_SHIMB, 10, P_BSPACE, 55, P_BDECAY, 60, P_BOCT, -1,
    P_BASS, 22, P_BASSSND, 0, P_BASSOCT, 0, P_GLIDE, 30, P_FIFTH, 25, P_BWARM, 50, P_BMOVE, 20, P_BASPACE, 30,
    P_PULSE, 30, P_HITS, 16, P_KIT, 0, P_SWING, 0,
    P_ECHO, 30, P_ETIME, 3, P_REVERB, 70, P_SIZE, 85, P_SHIM, 15, P_RWARM, 35, P_TAPE, 10, P_WIDTH, 60,
    P_AIR, 0, 
    P_LEAD, 44, P_LEADSND, 4, P_LSTYLE, 5, P_LOFTEN, 40,
    -1};

/* what belongs to each world beyond its parameters: its tuning (cents off equal temperament, by the interval above
 * the root), how its chords are voiced (weights: sevenths, add9, sus2, ninths, fourths) and how loosely the bells
 * fall (the most a bell may be late, in ms) */
enum { V_7, V_ADD9, V_SUS2, V_9, V_QUART, NVOICING };
static const struct {
    const char *name;
    const int16_t *rec;
    int8_t cents[12];
    uint8_t voicing[NVOICING];
    uint8_t loose;
    int8_t trim;                                /* its level, dB */
} WORLDS[EN_NPIECES] = {
    {"Warm Space", W_WARM, {0}, {2, 5, 2, 3, 1}, 60, 3},
    {"Cinematic", W_CINE, {0}, {2, 3, 4, 1, 0}, 50, 2},
    {"Lo-fi", W_LOFI, {0}, {4, 2, 0, 4, 0}, 80, 5},
    {"Glass", W_GLASS, {0}, {0, 4, 2, 1, 3}, 40, 0},
    {"Kankyo", W_KANKYO, {0}, {1, 3, 2, 1, 4}, 110, 7},
    /* pelog selisir, about as a gamelan in Bali tunes it: 0 120 270 . . . . 670 785 (the others between) */
    {"Gamelan Dawn", W_GAMELAN, {0, 20, 0, -30, 0, 40, 0, -30, -15, 0, -50, 0}, {3, 0, 2, 0, 1}, 70, 3},
    {"Deep Water", W_DEEP, {0}, {4, 2, 3, 1, 0}, 60, -2},
    {"Airports", W_AIRPORTS, {0}, {1, 0, 0, 0, 0}, 60, 3},
};

const char *en_piece_name(int i) { return WORLDS[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    int p;
    const int16_t *r;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = PARAMS[p].def;
    for (r = WORLDS[i].rec; r[0] >= 0; r += 2)
        par[r[0]] = r[1];
    par[P_WORLD] = (int16_t)i;
    *pool = EN_MODE_POOL[par[P_MODE]];
}

static const char *const ETIME_NAME[6] = {"1/4.", "1/2", "1/2.", "1 bar", "2 bars", "4 bars"};
static const float ETIME_BEATS[6] = {1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f};

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

static float drift_secs(int v) { return 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 20 s .. 5 min */

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_ETIME)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_PADTONE)
        s = PADSND_NAME[(unsigned)v < EN_NPADSND ? v : 0];
    else if (i == P_BELLSND || i == P_STRUM)
        s = BELLSND_NAME[(unsigned)v < EN_NBELLSND ? v : 0];
    else if (i == P_BASSSND)
        s = BASSSND_NAME[(unsigned)v < EN_NBASSSND ? v : 0];
    else if (i == P_KIT)
        s = KIT_NAME[(unsigned)v < EN_NKIT ? v : 0];
    else if (i == P_AIRSND)
        s = AIRSND_NAME[(unsigned)v < EN_NAIRSND ? v : 0];
    else if (i == P_WORLD)
        s = WORLDS[(unsigned)v < EN_NPIECES ? v : 0].name;
    else if (i == P_JOURNEY)
        s = JOURNEY_NAME[(unsigned)v < 4u ? v : 0];
    else if (i == P_FXAUTO && v == 0)
        s = "Off";
    else if (i == P_FXON)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_LEADSND)
        s = LEADSND_NAME[(unsigned)v < EN_NLEADSND ? v : 0];
    else if (i == P_LSTYLE)
        s = LSTYLE_NAME[(unsigned)v < EN_NLSTYLE ? v : 0];
    else if (i == P_KEYS)
        s = KEYS_NAME[(unsigned)v < 3u ? v : 0];
    else if (i == P_SLEEP)
        s = SLEEP_NAME[(unsigned)v < 4u ? v : 0];
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_DRIFT) {
        int sec = (int)(drift_secs(v) + 0.5f);
        if (sec < 60) {
            b = itoa_s(sec, b);
            *b++ = 's';
            *b = 0;
        } else {
            b = itoa_s(sec / 60, b);
            *b++ = ':';
            *b++ = (char)('0' + sec % 60 / 10);
            *b++ = (char)('0' + sec % 10);
            *b = 0;
        }
        return;
    }
    if (i == P_SWING) {                          /* as a swing percentage: 50% (straight) .. 75% */
        b = itoa_s(50 + v / 4, b);
        *b++ = '%';
        *b = 0;
        return;
    }
    if ((i == P_TUNE || i == P_BOCT || i == P_BASSOCT || i == P_POCT || i == P_LOCT) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

/* a chord on a degree, voiced as the world likes: stacked thirds (AMB-1's sevenths), add9, sus2, ninths without
 * the fifth, or stacked fourths. Offsets are scale steps above the degree; what's left fills in */
static int chord_tones_v(uint32_t pool, int root, int deg, int voicing, int *pcs)
{
    static const int8_t OFFS[NVOICING][4] = {{0, 2, 4, 6}, {0, 2, 4, 1}, {0, 1, 4, -1}, {0, 2, 6, 1}, {0, 3, 6, -1}};
    static const uint8_t FILL[7] = {0, 2, 4, 6, 1, 3, 5};
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    deg = ((deg % m) + m) % m;
    if ((unsigned)voicing >= NVOICING)
        voicing = V_7;
    want = voicing == V_7 ? (m >= 6 ? 4 : 3) : voicing == V_SUS2 || voicing == V_QUART ? 3 : 4;
    for (i = 0; i < 4 + 7 && n < want; i++) {
        int o = i < 4 ? OFFS[voicing][i] : FILL[i - 4], pc, dup = 0;
        if (o < 0)
            continue;
        pc = (root + st[(deg + o) % m]) % 12;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}

int en_chord_tones(uint32_t pool, int root, int deg, int *pcs) { return chord_tones_v(pool, root, deg, V_7, pcs); }

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : 0];                        /* three tones: the root doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_OUT, C_SETH, C_UNPIN, C_PART, C_ERA,
       C_FX, C_FXOFF, C_LKEY, C_STRUM };
#define QN 128u                              /* a whole world (every parameter) and then some */
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_set_hand(int p, int v) { post(C_SETH, p, v); }
void en_unpin(void) { post(C_UNPIN, 0, 0); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s)
{
    post(C_SEED, 0, (int)(s & 0xFFFFu));
    post(C_SEED, 1, (int)(s >> 16));
}
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_out(int headphones) { post(C_OUT, headphones != 0, 0); }

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint8_t en_drum_pos[EN_NDRUM];
volatile uint32_t en_drum_pat[EN_NDRUM];
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_bars;
volatile float en_arc;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];
volatile uint8_t en_bev[EN_BEV][3];
volatile uint32_t en_bev_w;

static int16_t par[P_NPARAMS];
int en_piece_world(void) { return par[P_WORLD]; }
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int deg;                                 /* the chord's scale degree */
static int chord_pcs[4], chord_nt, voicing;
/* the journey's part, as it shapes the layers (journey, below): levels (pad, bells, bass, pulse, air), density,
 * brightness, shimmer */
static float shed_left;
static uint8_t shed_hard;                        /* a second overload: no new effects for a moment */                         /* seconds: no new effects, no new lead phrases (the CPU ran out) */
static int cpu_now;
static int fx_room;
static float sleep_left, sleep_g = 1.0f;         /* Sleep: seconds to rest; the fade over its last minute */
volatile uint8_t en_asleep;                              /* an effect with a sound of its own is playing: room is made */
volatile uint32_t en_sheds;
volatile uint8_t en_busy;
static volatile uint8_t overload;
void en_overload(void) { overload = 1; }
void en_cpu(int pct) { cpu_now = pct; }
static int j_rescale;
static float hand_left[6];                       /* a layer touched by hand: heard now, whatever the part (seconds) */
static int lead_now;                             /* the lead touched: a phrase at once */                            /* Journey or Tide turned: the era, the part, to their new lengths */
static float part_dens = 1.0f, pg[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}, part_br, part_sh;
/* what the effects do to the rest (effects, below), each chunk */
static float fx_sweep, fx_shim, fx_swell, fx_tapef = 1.0f, fx_wob, fx_hold, fx_trem, fx_throw;
static float fx_dive, fx_rsw, fx_frz;           /* 1.1: Filter dive, Rev swell, Freeze (0..1); Half speed: fx_tapef */
static float fx_lvl = 0.36f, fx_space = 0.6f;
static int bell_force = -1;                      /* an effect's own bell sound, while it strikes */
static float bell_fx_g = 1.0f;                   /* and its level (each effect as loud as the others) */
static float fx_adapt = 1.0f;                    /* the effects lifted as the music is louder (render_chunk) */
static void fx_start(int i, int keep);
static void fx_release(int i);
static float tunefac = 1.0f, wowf = 1.0f;        /* wowf: the tape's slow wow on the pitch (Tape) */
static const int8_t *cents = WORLDS[0].cents;     /* the world's tuning, by the interval above the root */
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)                       /* Marsaglia's xorshift (Xorshift RNGs, 2003): shifts 13, 17, 5 */
{
    uint32_t x = rng;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    rng = x ^ (x << 5);
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float mtof(float n)                      /* the world's own tuning: the nearest note's cents */
{
    int k = (int)(n + 0.5f) - par[P_ROOT];
    n += (float)cents[(k % 12 + 12) % 12] * 0.01f;
    return 440.0f * tunefac * wowf * fm_exp2f((n - 69.0f) * (1.0f / 12.0f));
}

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns: any value (|ph| < 8192), never outside the table */
{
    /* 1.4: fixed point, 8 bits of the step between table points; the index masked, so a phase below 0 (1.2's Whale
     * read before the table and crashed) or even a NaN can only land inside it. Cheaper than the float version too. */
    const uint32_t u = (uint32_t)(int32_t)(ph * (float)(SN * 256));
    const uint32_t i = (u >> 8) & (SN - 1);
    const float f = (float)(u & 255u) * (1.0f / 256.0f);
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

/* ------------------------------------------------- slow random motion --- */
#define NMOD 9                                  /* 0..5 the pads', 6..8 the echo's */
#define NMOD_PAD 6
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 7.0f + 33.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion;                            /* 0..1: Motion */

/* ---------------------------------------------------------------- pad --- */
/* Breathing voices, as the sung tape loops of Music for Airports' "2/1": each voice swells in, hangs
 * and fades on its own cycle (unequal lengths, set by Swell), taking its note of the chord as it
 * starts, so the chord forms and dissolves rather than sitting there. The timbre: two saws a few
 * cents apart through two formant band-passes, a vowel that Bright moves from "oo" through "ah" to
 * "eh", with a little of the first formant's low-pass for body. */
typedef struct {
    float ph[3], inc[3];
    float note, target;                         /* MIDI: the note of this breath, the next one's */
    float t, rate, peak, amp;                   /* t 0..1 through the breath, per 32 samples */
    float ic[4];                                /* the two filters' states */
    float xlast;                                /* its last half-rate sample (1.4: the voices run at half rate) */
    float c1[4], c2[4];                         /* their coefficients: a1, a2, a3, k */
    float vib, pan, trem, br;
    float hf;                                   /* a handoff's fade: 1 normal */
    uint8_t midi_note, hand;                    /* hand: 1 fading out of the old key, 2 into the new */
    uint8_t voices;                             /* this breath's sound (P_PADTONE) */
} pad_t;
static pad_t pad[EN_NPAD];
static float pad_gate, pad_gain, pad_vol, bright, swell, pwarm, pmove = 0.5f, pspace = 0.5f, pwidth = 0.6f;
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */
enum { PS_WARM, PS_STRINGS, PS_CHOIR, PS_RHODES, PS_GLASS, PS_BREATH, PS_ORGAN, PS_JUNO, PS_BOWED, PS_HALO, PS_DREAM,
       PS_BRASS, PS_ETHER, PS_CATH, PS_RESO };
/* each sound's three oscillators, in cents from the note (Warm's third an octave down, for body) */
static const float PAD_DET[EN_NPADSND][3] = {{-7.0f, 7.0f, -1198.0f}, {-11.0f, 4.0f, 13.0f}, {0.0f, 9.0f, 0.0f},
                                             {0.0f, 3.0f, 0.0f},     {0.0f, 5.0f, 0.0f},   {0.0f, 0.0f, 0.0f},
                                             {0.0f, 6.0f, 0.0f},     {0.0f, -9.0f, 0.0f},  {0.0f, 0.0f, 0.0f},
                                             {0.0f, 4.0f, 7.0f},     {0.0f, 6.0f, 0.0f},
                                             {-5.0f, 5.0f, 0.0f},    {0.0f, 8.0f, 0.0f},   {0.0f, 3.0f, 0.0f},
                                             {-6.0f, 6.0f, 0.0f}};
/* and their levels, matched by ear and by measure (tools/sketch: each alone, the same loudness) */
static const float PAD_LVL[EN_NPADSND] = {0.95f, 0.92f, 0.93f, 0.73f, 0.59f, 1.35f, 1.29f, 0.83f, 1.0f, 0.54f, 0.44f,
                                         0.9f, 1.0f, 0.55f, 0.9f};

static float pad_rise = 0.4f, pad_fall = 0.35f; /* the pad's Attack and Release: parts of its breath (1.1) */
static float breath(float t)                    /* rise, hang, fall, a short rest */
{
    const float h = 0.9f - pad_fall;            /* where the fall starts */
    if (t < pad_rise)
        return 0.5f - 0.5f * fm_cosf(FM_PI * t / pad_rise);
    if (t < h)
        return 1.0f;
    if (t < 0.9f)
        return 0.5f + 0.5f * fm_cosf(FM_PI * (t - h) / pad_fall);
    return 0.0f;
}

static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2);
static void pad_breathe(pad_t *v, int i)        /* a new breath: its length, its strength, its note */
{
    float secs = (4.0f + 0.12f * swell) * BREATH[i] * (0.85f + 0.3f * frand());
    v->rate = 32.0f / (secs * EN_SR);
    v->peak = 0.6f + 0.4f * frand();
    if (en_playing && v->midi_note)
        midi(0x80, v->midi_note, 0);
    v->note = v->target;
    v->voices = (uint8_t)par[P_PADTONE];
    v->midi_note = (uint8_t)v->target;
    if (en_playing)
        midi(0x90, v->midi_note, (uint32_t)(50.0f + 40.0f * v->peak));
}

/* a new key (piece, root, scale) or sound: the voices still singing the old one fade out in about a second and
 * breathe again on the new chord; a voice at rest just takes its new note. Chord drift doesn't do this: there the
 * old notes are meant to hang over */
#define HAND_STEP (32.0f / (1.0f * EN_SR))
static void pad_handoff(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        if (v->amp > 0.002f && ((float)v->midi_note != v->target || v->voices != par[P_PADTONE]))
            v->hand = 1;
        else if (!v->hand)
            v->note = v->target;
    }
}

/* a chord change: a voice singing a note the new chord doesn't have fades out in about a second and
 * breathes in on its new note; a voice on a shared note holds it to the end of its breath */
static void pad_leave(void)
{
    int i, k;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        int pc = v->midi_note % 12, shared = 0;
        for (k = 0; k < chord_nt; k++)
            shared |= chord_pcs[k] == pc;
        if (v->amp > 0.002f && !shared && !v->hand)
            v->hand = 1;
        else if (v->amp <= 0.002f && !v->hand)
            v->note = v->target;
    }
}

static void pad_coefs(pad_t *v, int i)          /* every 32 samples: the breath, pitch, the filters */
{
    float f, vib, fc, br;
    int k, snd = v->voices;
    if (v->hand == 1) {                         /* out of the old key */
        v->hf -= HAND_STEP;
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->hand = 2;
            v->t = 0.15f;                       /* a breath already rising, on the new chord */
            pad_breathe(v, i);
            snd = v->voices;
        }
    } else if (v->hand == 2) {                  /* into the new */
        v->hf += HAND_STEP * 0.6f;
        if (v->hf >= 1.0f) {
            v->hf = 1.0f;
            v->hand = 0;
        }
    }
    v->t += v->rate;
    if (v->t >= 1.0f) {
        v->t -= 1.0f;
        pad_breathe(v, i);
        snd = v->voices;
    }
    v->br = br = breath(v->t);
    v->vib += (4.6f + 0.4f * (float)i) * 32.0f / EN_SR;
    if (v->vib >= 1.0f)
        v->vib -= 1.0f;
    v->trem += (3.9f + 0.3f * (float)i) * 32.0f / EN_SR;
    if (v->trem >= 1.0f)
        v->trem -= 1.0f;
    /* a singer's or a string section's vibrato, slow and shallow; the others only drift, as tape */
    vib = (snd == PS_STRINGS ? 0.05f : snd == PS_CHOIR ? 0.07f : snd == PS_BOWED ? 0.09f : 0.0f) * (0.3f + pmove) * sine(v->vib);
    f = mtof(v->note + 12.0f * (float)par[P_POCT] + 0.08f * motion * mod[i].v + vib);
    for (k = 0; k < 3; k++) {
        float det = PAD_DET[snd][k];
        if (k < 2 && (snd == PS_WARM || snd == PS_STRINGS))
            det *= 0.6f + 0.8f * pmove;         /* Movement: the voices spread further apart */
        v->inc[k] = f * fm_exp2f(det * (1.0f / 1200.0f)) * (1.0f / EN_SR);
    }
    if (snd == PS_CHOIR) {                      /* the vowel, from "oo" through "ah" to "eh" */
        float vw = fm_clampf(bright * 0.01f + 0.25f * arcm + 0.2f * motion * mod[(i + 2) % NMOD_PAD].v, 0.0f, 1.0f) * 2.999f,
              f1, f2;
        int a = (int)vw;
        vw -= (float)a;
        f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
        f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
        svf_coefs(v->c1, 2.0f * f1 * (1.0f + 0.03f * (float)i), 0.45f);   /* (x2: the voices run at half rate) */
        svf_coefs(v->c2, 2.0f * f2 * (1.0f - 0.02f * (float)i), 0.3f);
        return;
    }
    if (snd == PS_BREATH) {                     /* a narrow band of air on the note, then a soft low-pass */
        svf_coefs(v->c1, 2.0f * f, 0.06f);
        svf_coefs(v->c2, 2.0f * 900.0f * fm_exp2f(bright * 0.03f + 1.2f * br), 0.8f);
        return;
    }
    /* the bloom: the filter opens as the breath swells, further with Bright; low notes stay rounder */
    fc = 170.0f * fm_exp2f(bright * 0.042f + (snd == PS_STRINGS || snd == PS_BOWED ? 1.5f : 2.4f) * br * (0.5f + 0.006f * bright) +
                           0.6f * arcm + 0.9f * part_br + 0.8f * fx_swell + 0.35f * (v->note - 60.0f) * (1.0f / 12.0f));
    svf_coefs(v->c1, 2.0f * fc, snd == PS_STRINGS ? 1.3f : snd == PS_BOWED ? 0.5f : snd == PS_JUNO ? 0.75f
                                : snd == PS_RESO ? 0.28f : snd == PS_BRASS ? 0.7f : 0.9f);
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* ------------------------------------------------------------------ bass --- */
static struct {
    float ph, inc, note, target, amp;
    uint8_t midi_note, rest;                    /* rest: sitting this chord out */
} bass;
static float bass_vol, glide_k = 1.0f / (0.25f * EN_SR);      /* glide_k: 1 / the glide's time in samples */

/* ----------------------------------------------------------------- bells --- */
#define NBELL 8
typedef struct {
    float cph, mph, cinc, minc, amp, adec, idx, idec, att, gl, gr;
    float c2ph, c2inc, part;                    /* the partner: the same bell a little apart (Shimmer, ombak) */
    float p2ph, p2inc, p2amp, p2dec;            /* a second partial, its own decay */
    float arate, tph, tdep;                     /* the strike's speed; the vibraphone's motor */
    uint8_t fx;                                 /* struck by an effect: at the effects' level, not the bells' */
    int32_t wait;
    uint8_t on, note, loop, snd;
    float fxg;                                  /* an effect's bell: its own level */
} bell_t;
/* the bells' sounds: the modulator's ratio (< 0: Tone picks it, as AMB-1), its index and how fast it decays, the
 * ring (t60), the strike (< 0: Tone sets it), a second partial (ratio, level, how much faster it fades), the partner's
 * detune in cents and in Hz */
static const struct {
    float rat, idx, idec, t60, arate, p2r, p2l, p2d, det, dhz;
} BSND[EN_NBELLSND] = {
    {-1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 4.0f, 0.0f},     /* Soft: AMB-1's */
    {1.0f, 0.35f, 0.25f, 1.15f, 0.0025f, 2.0f, 0.22f, 2.5f, 2.5f, 0.0f}, /* Felt: a slowed, felted piano */
    {4.0f, 0.25f, 0.15f, 0.75f, 0.03f, 2.0f, 0.3f, 1.6f, 3.0f, 0.0f},   /* Celesta */
    {4.0f, 0.6f, 0.05f, 0.42f, 0.06f, 10.0f, 0.06f, 6.0f, 0.0f, 0.0f},  /* Marimba: wood, the 4th and 10th partials */
    {1.0f, 0.12f, 0.3f, 1.5f, 0.02f, 4.0f, 0.25f, 3.0f, 0.0f, 0.0f},    /* Vibes: Shimmer is its motor */
    {2.76f, 0.5f, 0.8f, 1.5f, 0.03f, 5.4f, 0.12f, 2.0f, 0.0f, 6.0f},    /* Gamelan: a metallophone pair, 6 Hz apart */
    {5.0f, 0.18f, 0.2f, 1.5f, 0.012f, 2.0f, 0.35f, 1.2f, 6.0f, 0.0f},   /* Crystal */
    {1.0f, 0.2f, 0.1f, 0.55f, 0.05f, 5.6f, 0.35f, 4.0f, 0.0f, 0.0f},    /* Kalimba: a tine, its high partial quick */
    {1.0f, 0.15f, 0.1f, 0.6f, 0.08f, 6.0f, 0.25f, 3.0f, 2.0f, 0.0f},    /* Music box */
    {1.0f, 0.05f, 1.0f, 2.2f, 0.0002f, 2.71f, 0.3f, 1.0f, 0.0f, 1.3f},  /* Bowl: a singing bowl, slow, beating */
    {3.5f, 0.4f, 0.6f, 1.6f, 0.04f, 2.76f, 0.25f, 1.5f, 0.0f, 0.0f},    /* Chime: a tubular bell */
    {1.0f, 0.6f, 0.06f, 0.6f, 0.2f, 2.0f, 0.2f, 2.0f, 0.0f, 0.0f},      /* Harp */
    {2.0f, 0.5f, 0.25f, 0.8f, 0.05f, 4.0f, 0.2f, 2.0f, 3.0f, 0.0f},     /* Steel: a steel pan */
    {1.0f, 0.1f, 0.2f, 0.9f, 0.08f, 2.76f, 0.4f, 2.5f, 0.0f, 0.0f},     /* Glock: a glockenspiel, bright and clean */
    {2.4f, 0.3f, 1.0f, 2.0f, 0.03f, 5.9f, 0.2f, 2.0f, 0.0f, 2.2f},      /* Windchime: aluminium, long, beating */
    {1.0f, 0.08f, 1.0f, 2.4f, 0.06f, 2.4f, 0.4f, 0.8f, 0.0f, 4.0f},     /* Tingsha: two small cymbals, beating fast */
    {1.0f, 0.5f, 0.15f, 0.7f, 0.15f, 3.0f, 0.25f, 2.0f, 7.0f, 0.0f},    /* Dulcimer: hammered courses, a little apart */
    {1.0f, 0.7f, 0.35f, 0.9f, 0.02f, 3.0f, 0.15f, 3.0f, 1.5f, 0.0f},    /* EP bell: an electric piano's tine */
};
/* each bell's level, measured alone (1.5): the new ones in line with the others */
static const float BELL_LVL[EN_NBELLSND] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.85f, 0.72f, 0.74f, 1.1f, 1.15f};
static bell_t bell[NBELL];
static float bell_vol, btone, shimb = 0.4f, bspace = 0.5f, bdecay = 1.0f;
static int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];
static uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP];
static const uint8_t LOOP_BASE[EN_NLOOP] = {72, 67, 79, 62};     /* each loop's register */
static const float LOOP_PAN[EN_NLOOP] = {-0.6f, 0.35f, 0.7f, -0.25f};
static const float RATIO[5] = {1.0f, 2.0f, 3.0f, 3.5f, 4.77f};

/* ----------------------------------------------------------------- pulse --- */
typedef struct {
    float ph, inc, inc_end, amp, adec, idx, x1, x2, att, mph[4], pend;
    float l1, p2, i2, l2, nl, nc[4], ar, gl, gr;  /* the kits: the tone's level, a second partial, noise in a band */
    int32_t wait;                               /* swing: samples until a hit that waits (pend: its velocity) */
    uint8_t on, note;
} drum_t;
static float kmove = 0.4f, kspace = 0.4f, kwidth = 0.5f;
static drum_t drum[EN_NDRUM];
static uint8_t drum_pos[EN_NDRUM];
static uint32_t drum_pat[EN_NDRUM];
static float pulse_vol, swing, hat_f = 0.9f, hat_open = 0.42f;
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t noise = 22222u;
static const uint8_t DRUM_KMAX[EN_NDRUM] = {5, 7, 3};
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 42, 46};          /* kick, closed hat, open hat */
/* the hats' metal: four square waves at the 808's inharmonic ratios (Hz / EN_SR) */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};

/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.55f;
static float tape_amt = 0.15f, shim = 0.3f, width = 0.6f;   /* Tape, Shimmer, Width */
static float air_vol, air_tone = 0.5f, air_mv = 0.5f;          /* Air */
#define LN_MAX 16
static float lead_vol, lead_tone = 0.5f, lead_space = 0.55f, lead_expr = 0.45f;
static struct {
    float ph[3], f, f_t, env, t, vib_ph, gr_ph, gate_left, vel, solo_g;
    float c[4], c2[4], s[2], s2[2], nl;
    int gate, cnt;
    int8_t off[LN_MAX];
    uint8_t dur[LN_MAX];
    int8_t m_off[LN_MAX];                        /* the last phrase: its answer may come next */
    uint8_t m_dur[LN_MAX];
    int n, i, m_n, wait, answer, solo;
} ld = {.wait = 32};
static float spk_tgt = 1.0f;                     /* 1 the speaker, 0 headphones (en_out) */

static float world_trim = 1.0f;                  /* the world's level, so each sits as loud as the others */
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

static int bell_note(int l, int off)            /* a loop slot's note, from the chord's degree */
{
    int d = deg + off, oct, s, base, root = par[P_ROOT], n;
    oct = d >= 0 ? d / nsteps : -((-d + nsteps - 1) / nsteps);   /* (below the root too: 1.2's effects reach there) */
    s = steps[d - oct * nsteps];
    base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;   /* the root at or below the register */
    n = base + s + 12 * oct + 12 * par[P_BOCT];
    while (n > 103)
        n -= 12;
    while (n < 36)
        n += 12;
    return n;
}

static void pad_retarget(void)                  /* the chord's tones to the four voices */
{
    int from[4], to[4], i;
    chord_nt = chord_tones_v(pool, par[P_ROOT], deg, voicing, chord_pcs);
    for (i = 0; i < EN_NPAD; i++)
        from[i] = (int)(pad[i].target + 0.5f);
    en_voice_lead(from, chord_pcs, chord_nt, to, 48, 77);
    for (i = 0; i < EN_NPAD; i++) {             /* each voice takes its note at its next breath */
        pad[i].target = (float)to[i];
        en_chord[i] = (uint8_t)to[i];
    }
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}

static void bass_to(int pc)
{
    int n = 36 + pc % 12 + 12 * par[P_BASSOCT];     /* C2..B2, an octave down or up */
    bass.target = (float)n;
    if (en_playing && bass.midi_note != n) {
        midi(0x82, bass.midi_note, 0);
        midi(0x92, (uint32_t)n, 80);
    }
    bass.midi_note = (uint8_t)n;
}

static float drift_left, drift_total, evolve;   /* evolve: Evolve, 0..1 */
static int key_off, key_pending;                /* Key moves (wander, below) */
static void motif_tick(void);                   /* the journey (below) */
static void lead_tick(void);                    /* the lead (below) */
static void lead_key(int note, int on);
static uint8_t loop_len_next[EN_NLOOP], mo_on;
static float wander, tidef = 1.0f;              /* Wander 0..1; Tide: how long the tides and seasons are */
static int eff_value(int p);
static void apply(int p, int v);
static uint8_t change_pending, fifth_done;

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

static int pick_voicing(void)                   /* the world's voicings, by their weights */
{
    int w = par[P_WORLD], i, sum = 0, r;
    for (i = 0; i < NVOICING; i++)
        sum += WORLDS[w].voicing[i];
    if (sum <= 0)
        return V_7;
    r = irand(sum);
    for (i = 0; i < NVOICING; i++)
        if ((r -= WORLDS[w].voicing[i]) < 0)
            return i;
    return V_7;
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i;
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        sum += w[i];
    }
    r = frand() * sum;
    for (i = 0; i < nsteps; i++) {
        r -= w[i];
        if (r <= 0.0f && w[i] > 0.0f)
            break;
    }
    if (i >= nsteps)
        i = deg == 0 ? (nsteps > 1 ? 1 : 0) : 0;
    deg = i;
    voicing = pick_voicing();
    en_chord_count++;
    if (key_pending != -99) {                   /* Key moves: the key goes, or comes home, on a chord */
        key_off = key_pending;
        key_pending = -99;
        apply(P_ROOT, eff_value(P_ROOT));
    }
    pad_retarget();
    pad_leave();
    bass_to(chord_pcs[0]);
    /* Evolve: the bass sometimes sits a chord out, more often when the arc is sparse */
    bass.rest = frand() < evolve * 0.3f + 0.25f * fm_maxf(-arcm, 0.0f);
    drift_reset();
}

/* ------------------------------------------------------------- the loops --- */
static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (frand() < 0.72f) {                      /* a chord tone, sometimes an octave up */
        int o = 2 * irand(chord_nt);
        return o + (frand() < 0.3f ? nsteps : 0);
    }
    return 1 + 2 * irand(3);                    /* a passing note: the 2nd, 4th or 6th */
}

static int loop_count(int l)
{
    int i, c = 0;
    for (i = 0; i < EN_LOOP_LEN[l]; i++)
        c += loop_note[l][i] >= 0;
    return c;
}

static int loop_target(void)                    /* 0..5 notes: Density, swelled and thinned by the arc */
{
    float d = (float)par[P_DENSITY] * 0.05f * (1.0f + 0.8f * arcm) * part_dens;
    return (int)(fm_clampf(d, 0.0f, 5.0f) + frand());
}
static uint8_t loop_rest[EN_NLOOP];             /* this pass: silent (Evolve) */

static void loop_add(int l)
{
    int len = EN_LOOP_LEN[l], i, s;
    for (i = 0; i < 8; i++) {
        s = irand(len);
        if (loop_note[l][s] < 0) {
            loop_note[l][s] = (int8_t)rand_offset();
            loop_vel[l][s] = (uint8_t)(50 + irand(50));
            return;
        }
    }
}

static int loop_pick(int l)                     /* a random sounding slot, -1 none */
{
    int len = EN_LOOP_LEN[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (loop_note[l][s] >= 0)
            return s;
    return -1;
}

static void loop_publish(int l)
{
    int i;
    for (i = 0; i < EN_LOOP_MAX; i++)
        en_loop_note[l][i] = i < EN_LOOP_LEN[l] ? loop_note[l][i] : -1;
}

static void loop_regrow(int l)
{
    int i, n = loop_target();
    for (i = 0; i < EN_LOOP_MAX; i++)
        loop_note[l][i] = -1;
    for (i = 0; i < n; i++)
        loop_add(l);
    loop_publish(l);
}

static void loop_pass(int l)                    /* the end of a pass: grow, prune, evolve */
{
    int c = loop_count(l), t = loop_target(), s;
    for (; c < t; c++)                          /* Density: to its count by the end of a pass */
        loop_add(l);
    for (; c > t && (s = loop_pick(l)) >= 0; c--)
        loop_note[l][s] = -1;
    loop_rest[l] = frand() < evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    if (par[P_JOURNEY] > 0 && frand() < 0.03f + 0.08f * evolve) {   /* the journey: half the phrase written anew */
        int k;
        for (k = 0; k < 3 && (s = loop_pick(l)) >= 0; k++)
            loop_note[l][s] = -1;
        for (k = loop_count(l); k < t; k++)
            loop_add(l);
    }
    if (frand() < evolve * 0.7f && (s = loop_pick(l)) >= 0) {
        if (frand() < 0.5f) {
            loop_note[l][s] = (int8_t)rand_offset();             /* another note */
        } else {
            int d = irand(EN_LOOP_LEN[l]);                       /* the same note, elsewhere */
            if (loop_note[l][d] < 0) {
                loop_note[l][d] = loop_note[l][s];
                loop_vel[l][d] = loop_vel[l][s];
                loop_note[l][s] = -1;
            }
        }
    }
    loop_publish(l);
}

/* each loop its own bell: index, decay and ratio a little apart, so the four can be told apart */
static const float LOOP_IDX[EN_NLOOP] = {1.0f, 0.55f, 1.5f, 0.8f}, LOOP_DEC[EN_NLOOP] = {1.0f, 1.35f, 0.75f, 1.6f};
static const int8_t LOOP_RAT[EN_NLOOP] = {0, 0, 1, -1};

static void bell_voice(int l, int n, float vel, int32_t wait)
{
    int i, best = 0, ri;
    float lo = 9.0f, t60, p;
    bell_t *v;
    const int lim = bell_force >= 0 ? (cpu_now >= 80 ? 6 : NBELL)              /* (1.2) an effect's: six when busy */
                  : fx_room && cpu_now >= 80 ? 4 : cpu_now >= 80 || fx_room ? 5 : NBELL;   /* the music's: four then */
    for (i = 0; i < lim; i++) {   /* a free voice, or the quietest (CPU
                                                 * high: five of the eight) */
        float a = bell[i].on ? bell[i].amp : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    v = &bell[best];
    if (v->on)
        midi(0x81, v->note, 0);
    v->on = 1;
    v->note = (uint8_t)n;
    v->loop = (uint8_t)l;
    {
        const int k = bell_force >= 0 ? bell_force : par[P_BELLSND];
        float f = mtof((float)n);
        v->snd = (uint8_t)k;
        v->fx = bell_force >= 0;
        v->fxg = bell_fx_g;
        v->cph = v->mph = v->c2ph = v->p2ph = 0.0f;
        v->cinc = f * (1.0f / EN_SR);
        if (BSND[k].rat < 0.0f) {
            ri = par[P_BTONE] * 5 / 101 + LOOP_RAT[l];
            v->minc = v->cinc * RATIO[ri < 0 ? 0 : ri > 4 ? 4 : ri];
        } else {
            v->minc = v->cinc * BSND[k].rat;
        }
        v->c2inc = (f * fm_exp2f(BSND[k].det * (1.0f / 1200.0f)) + BSND[k].dhz) * (1.0f / EN_SR);
        v->part = BSND[k].dhz > 0.0f ? 0.6f + 0.4f * shimb : k == 4 ? 0.0f : 0.7f * shimb;
        v->tdep = k == 4 ? 0.6f * shimb : 0.0f;
        v->tph = 0.0f;
        v->amp = vel * BELL_LVL[k];
        t60 = 4.2f - (float)(n - 60) * 0.06f;   /* the low bells ring longer */
        t60 = fm_clampf(t60, 1.6f, 5.0f) * LOOP_DEC[l] * BSND[k].t60 * bdecay;
        v->adec = fm_expf(-6.9f / (t60 * EN_SR));
        v->p2inc = f * BSND[k].p2r * (1.0f / EN_SR);
        v->p2amp = BSND[k].p2l * (0.5f + btone) * vel;
        v->p2dec = fm_expf(-6.9f / (t60 / BSND[k].p2d * EN_SR));
        if (v->p2inc >= 0.45f)
            v->p2amp = 0.0f;                    /* above Nyquist's reach: none */
        /* Tone: 0 a round, nearly pure tone; 100 a long metallic clang (index in radians, as turns) */
        v->idx = (0.15f + 6.5f * btone * btone) * LOOP_IDX[l] * BSND[k].idx * (0.6f + 0.4f * vel) * (1.0f / FM_TWO_PI);
        v->idec = fm_expf(-6.9f / ((0.35f + 2.6f * btone) * BSND[k].idec * EN_SR));
        v->arate = BSND[k].arate >= 0.0f ? BSND[k].arate * (0.6f + 0.8f * btone)
                                        : 0.0004f + 0.0056f * fm_minf(btone * 2.0f, 1.0f);
    }
    v->att = 0.0f;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    v->wait = wait;
    en_loop_glow[l] = 1.0f;
    {
        uint32_t w = en_bev_w % EN_BEV;
        en_bev[w][0] = (uint8_t)l;
        en_bev[w][1] = (uint8_t)n;
        en_bev[w][2] = (uint8_t)(vel * 127.0f);
        BARRIER();
        en_bev_w++;
    }
    midi(0x91, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void bell_ring(int l, int slot)
{
    int n = bell_note(l, loop_note[l][slot]);
    float vel = (float)loop_vel[l][slot] * 0.01f * (mo_on ? 0.55f : 1.0f);   /* the motif speaking: the loops step back */
    int32_t wait = irand((int)WORLDS[par[P_WORLD]].loose * 44 + 1);   /* a human hand: a little late, as the world */
    bell_voice(l, n, vel, wait);
    if (n + 12 <= 103 && frand() < 0.08f + 0.12f * fm_maxf(arcm, 0.0f))
        bell_voice(l, n + 12, vel * 0.4f, wait + 300 + irand(2000));   /* now and then, its octave */
}

/* STRUM (1.2, in Plant's place): a key strums the chord like a harp, from its note up, in its own bell sound, at
 * its own level (the effects' way: heard over the music) */
#define NSTRUM 3
static struct {
    uint8_t on, k, note;
    float t;
} sm[NSTRUM];
static int sm_rr;
volatile uint32_t en_strum_notes;               /* strummed notes (tests) */
static int strum_up(int n)                       /* the next chord tone above n */
{
    int k, j;
    for (k = 1; k <= 12; k++)
        for (j = 0; j < chord_nt; j++)
            if ((((chord_pcs[j] - n - k) % 12) + 12) % 12 == 0)
                return n + k;
    return n + 12;
}
static void strum(int note)
{
    int n = note + 12;
    while (n > 91)
        n -= 12;
    while (n < 48)
        n += 12;
    sm[sm_rr].on = 1;
    sm[sm_rr].k = 0;
    sm[sm_rr].note = (uint8_t)n;
    sm[sm_rr].t = 0.0f;
    sm_rr = (sm_rr + 1) % NSTRUM;
}
static void strum_step(float dt)                 /* once a chunk: each strum's next strings */
{
    int i;
    for (i = 0; i < NSTRUM; i++) {
        if (!sm[i].on)
            continue;
        sm[i].t += dt;
        while (sm[i].k < 6 && sm[i].t >= 0.055f * (float)sm[i].k) {
            int n = sm[i].k ? strum_up(sm[i].note) : sm[i].note;
            while (n > 103)
                n -= 12;
            sm[i].note = (uint8_t)n;
            bell_force = par[P_STRUM];
            bell_fx_g = 2.0f * (float)par[P_SLEVEL] * 0.01f;   /* Strum level: 50, the default, a little under 1.2's */
            bell_voice(sm[i].k % EN_NLOOP, n, 0.85f - 0.07f * (float)sm[i].k, 0);
            bell_force = -1;
            en_strum_notes++;
            sm[i].k++;
        }
        if (sm[i].k >= 6)
            sm[i].on = 0;
    }

}

/* ---------------------------------------------------------------- pulse --- */
static const uint8_t DRUM_ROT[EN_NDRUM] = {0, 2, 3};
static int drum_k[EN_NDRUM], drum_kb[EN_NDRUM], drum_rot[EN_NDRUM], pulse_rest;   /* rest: bars left */

static void drum_publish(int d)
{
    drum_pat[d] = en_euclid(drum_k[d], EN_DRUM_N[d], drum_rot[d]);
    en_drum_pat[d] = drum_pat[d];
}

static void drums_pattern(void)                 /* Hits: the patterns' home */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        drum_kb[d] = drum_k[d] = (par[P_HITS] * DRUM_KMAX[d] + 50) / 100;
        drum_rot[d] = DRUM_ROT[d];
        drum_publish(d);
    }
}

static void drums_evolve(void)                  /* a bar: a pattern turns, or gains or loses a hit */
{
    int d = irand(EN_NDRUM), n = EN_DRUM_N[d];
    if (!drum_kb[d])
        return;
    if (frand() < 0.5f) {
        drum_rot[d] = (drum_rot[d] + (frand() < 0.5f ? 1 : n - 1)) % n;
    } else {
        int k = drum_k[d] + (frand() < 0.5f ? 1 : -1);
        if (k >= drum_kb[d] - 1 && k <= drum_kb[d] + 1 && k >= 1 && k <= n)
            drum_k[d] = k;
    }
    drum_publish(d);
}

static void kit_fire(drum_t *v, int d, float vel);
static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    if (par[P_KIT]) {
        kit_fire(v, d, vel);
        return;
    }
    switch (d) {
    case 0: {                                   /* thump: the chord's root, low, falling into place */
        float f = mtof((float)(36 + chord_pcs[0]));
        v->note = DRUM_GM[0];
        v->inc = f * 2.2f * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / (0.45f * EN_SR));
        break;
    }
    case 1:                                     /* closed hat: short; it chokes the open one */
        v->note = DRUM_GM[1];
        v->adec = fm_expf(-6.9f / (0.06f * EN_SR));
        drum[2].adec = fm_expf(-6.9f / (0.02f * EN_SR));
        break;
    default:                                    /* open hat: a long, soft "tss" */
        v->note = DRUM_GM[2];
        v->adec = fm_expf(-6.9f / (hat_open * EN_SR));
        break;
    }
    en_drum_glow[d] = 1.0f;
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
}

/* the other kits: a tone (falling to its note), a second partial, noise in a band; all tuned to the chord's root */
static void kit_fire(drum_t *v, int d, float vel)
{
    const int kit = par[P_KIT];
    const float f0 = mtof((float)(36 + chord_pcs[0])), tone = (float)par[P_HATS] * 0.01f;
    float dec = 0.1f, pan = 0.0f;
    v->note = DRUM_GM[d];
    v->ph = v->p2 = 0.0f;
    v->l1 = v->l2 = v->nl = 0.0f;
    v->i2 = 0.0f;
    v->x1 = v->x2 = 0.0f;
    v->ar = 0.3f;
    switch (kit * 3 + d) {
    case 3:                                     /* Brush: a soft, low kick */
        v->inc = f0 * 1.6f * (1.0f / EN_SR), v->inc_end = f0 * (1.0f / EN_SR), v->l1 = 0.9f, dec = 0.3f, v->ar = 0.01f;
        break;
    case 4:                                     /* a brush's tap */
        v->nl = 1.3f, dec = 0.09f, v->ar = 0.004f, pan = 0.4f;
        svf_coefs(v->nc, 2200.0f * (0.7f + tone), 1.2f);
        break;
    case 5:                                     /* a brush's swish: slow in, slow out */
        v->nl = 1.0f, dec = 0.38f, v->ar = 0.0006f, pan = -0.45f;
        svf_coefs(v->nc, 3800.0f * (0.7f + 0.6f * tone), 1.5f);
        break;
    case 6:                                     /* Wood: a block, three octaves up */
        v->inc = v->inc_end = f0 * 8.0f * (1.0f / EN_SR), v->l1 = 0.55f, dec = 0.07f;
        v->i2 = f0 * 8.0f * 2.7f * (1.0f / EN_SR), v->l2 = 0.25f;
        break;
    case 7:                                     /* a shaker */
        v->nl = 0.9f, dec = 0.05f, v->ar = 0.004f, pan = 0.5f;
        svf_coefs(v->nc, 5500.0f * (0.7f + 0.6f * tone), 2.0f);
        break;
    case 8:                                     /* a click, as a rim */
        v->inc = v->inc_end = f0 * 16.0f * (1.0f / EN_SR), v->l1 = 0.3f, dec = 0.025f, v->nl = 0.3f, pan = -0.5f;
        svf_coefs(v->nc, 3000.0f, 1.5f);
        break;
    case 9:                                     /* Gong: low, its partial apart, bending down into its note */
        v->inc = f0 * 1.03f * (1.0f / EN_SR), v->inc_end = f0 * (1.0f / EN_SR), v->l1 = 0.9f, dec = 3.5f, v->ar = 0.002f;
        v->i2 = f0 * 1.48f * (1.0f / EN_SR), v->l2 = 0.35f;
        break;
    case 10:                                    /* a small kettle, high (a bonang) */
        v->inc = v->inc_end = f0 * 8.0f * (1.0f / EN_SR), v->l1 = 0.35f, dec = 0.5f, v->ar = 0.02f, pan = 0.45f;
        v->i2 = f0 * 8.0f * 2.76f * (1.0f / EN_SR), v->l2 = 0.12f + 0.15f * tone;
        break;
    case 11:                                    /* a larger one, lower (a kenong) */
        v->inc = v->inc_end = f0 * 4.0f * (1.0f / EN_SR), v->l1 = 0.4f, dec = 1.2f, v->ar = 0.01f, pan = -0.45f;
        v->i2 = f0 * 4.0f * 2.4f * (1.0f / EN_SR), v->l2 = 0.1f + 0.1f * tone;
        break;
    case 12:                                    /* Hands: a frame drum */
        v->inc = f0 * 2.3f * (1.0f / EN_SR), v->inc_end = f0 * 2.0f * (1.0f / EN_SR), v->l1 = 0.9f, dec = 0.35f;
        v->ar = 0.05f, v->nl = 0.25f;
        svf_coefs(v->nc, 900.0f, 1.2f);
        break;
    case 13:                                    /* a tabla's ring, bending up */
        v->inc = f0 * 3.9f * (1.0f / EN_SR), v->inc_end = f0 * 4.25f * (1.0f / EN_SR), v->l1 = 0.5f, dec = 0.3f;
        v->ar = 0.2f, pan = 0.4f, v->i2 = f0 * 8.0f * (1.0f / EN_SR), v->l2 = 0.15f;
        break;
    case 14:                                    /* a slap */
        v->nl = 1.0f, dec = 0.05f, pan = -0.4f;
        svf_coefs(v->nc, 1600.0f + 1400.0f * tone, 1.6f);
        break;
    case 15:                                    /* Glitch: a blip */
        v->inc = v->inc_end = f0 * 16.0f * (1.0f / EN_SR), v->l1 = 0.5f, dec = 0.012f, v->nl = 0.2f;
        svf_coefs(v->nc, 6000.0f, 2.0f);
        break;
    case 16:                                    /* a tick */
        v->nl = 1.0f, dec = 0.006f, pan = 0.6f;
        svf_coefs(v->nc, 7000.0f + 3000.0f * tone, 3.0f);
        break;
    case 17:                                    /* a chirp */
        v->inc = v->inc_end = f0 * 32.0f * (1.0f / EN_SR), v->l1 = 0.35f, dec = 0.025f, pan = -0.6f;
        v->i2 = f0 * 48.0f * (1.0f / EN_SR), v->l2 = 0.2f;
        break;
    case 18:                                    /* Lo-fi: a dusty kick, round, falling into its note */
        v->inc = f0 * 2.6f * (1.0f / EN_SR), v->inc_end = f0 * (1.0f / EN_SR), v->l1 = 1.0f, dec = 0.32f, v->ar = 0.25f;
        v->nl = 0.12f;
        svf_coefs(v->nc, 220.0f, 1.0f);
        break;
    case 19:                                    /* a soft snare, a little tone in it */
        v->nl = 0.9f, dec = 0.16f, v->ar = 0.1f, pan = -0.15f;
        v->inc = v->inc_end = f0 * 6.0f * (1.0f / EN_SR), v->l1 = 0.25f;
        svf_coefs(v->nc, 1700.0f + 900.0f * tone, 1.0f);
        break;
    case 20:                                    /* a closed hat, dull, as off a tape */
        v->nl = 0.55f, dec = 0.045f, v->ar = 0.3f, pan = 0.35f;
        svf_coefs(v->nc, 5200.0f + 2000.0f * tone, 1.8f);
        break;
    case 21:                                    /* Toy: a small tom, tuned */
        v->inc = f0 * 2.2f * (1.0f / EN_SR), v->inc_end = f0 * 2.0f * (1.0f / EN_SR), v->l1 = 0.7f, dec = 0.45f, v->ar = 0.1f;
        break;
    case 22:                                    /* a toy piano's tine */
        v->inc = v->inc_end = f0 * 12.0f * (1.0f / EN_SR), v->l1 = 0.35f, dec = 0.4f, v->ar = 0.3f, pan = 0.45f;
        v->i2 = f0 * 12.0f * 3.9f * (1.0f / EN_SR), v->l2 = 0.12f + 0.1f * tone;
        break;
    case 23:                                    /* a tiny ping, high */
        v->inc = v->inc_end = f0 * 24.0f * (1.0f / EN_SR), v->l1 = 0.3f, dec = 0.15f, v->ar = 0.4f, pan = -0.5f;
        break;
    case 24:                                    /* Drops: a drop, its pitch rising */
        v->inc = f0 * 12.0f * (1.0f / EN_SR), v->inc_end = f0 * 20.0f * (1.0f / EN_SR), v->l1 = 0.6f, dec = 0.06f;
        v->ar = 0.5f;
        break;
    case 25:                                    /* a smaller one */
        v->inc = f0 * 20.0f * (1.0f / EN_SR), v->inc_end = f0 * 32.0f * (1.0f / EN_SR), v->l1 = 0.45f, dec = 0.04f;
        v->ar = 0.5f, pan = 0.5f;
        break;
    case 26:                                    /* a splash, soft */
        v->nl = 0.5f, dec = 0.12f, v->ar = 0.05f, pan = -0.45f;
        svf_coefs(v->nc, 2600.0f + 2000.0f * tone, 1.4f);
        break;
    case 27:                                    /* Dub: a deep soft kick */
        v->inc = f0 * 2.2f * (1.0f / EN_SR), v->inc_end = f0 * 0.9f * (1.0f / EN_SR), v->l1 = 1.0f, dec = 0.5f, v->ar = 0.15f;
        break;
    case 28:                                    /* a rimshot, ringing (the echo carries it) */
        v->inc = v->inc_end = f0 * 8.5f * (1.0f / EN_SR), v->l1 = 0.45f, dec = 0.07f, v->ar = 0.6f, pan = -0.2f;
        v->i2 = f0 * 8.5f * 2.4f * (1.0f / EN_SR), v->l2 = 0.25f;
        v->nl = 0.25f;
        svf_coefs(v->nc, 2400.0f + 1200.0f * tone, 1.2f);
        break;
    case 29:                                    /* a gentle shaker */
        v->nl = 0.4f, dec = 0.07f, v->ar = 0.02f, pan = 0.4f;
        svf_coefs(v->nc, 6000.0f + 2500.0f * tone, 1.6f);
        break;
    case 30:                                    /* 808 Soft: the boom, rounded */
        v->inc = f0 * 3.0f * (1.0f / EN_SR), v->inc_end = f0 * (1.0f / EN_SR), v->l1 = 1.0f, dec = 0.7f, v->ar = 0.3f;
        break;
    case 31:                                    /* a clap, soft: a few quick bursts in one */
        v->nl = 0.85f, dec = 0.14f, v->ar = 0.0f, pan = -0.1f;
        svf_coefs(v->nc, 1300.0f + 700.0f * tone, 1.3f);
        break;
    case 32:                                    /* a hat, quiet and short */
        v->nl = 0.45f, dec = 0.035f, v->ar = 0.1f, pan = 0.3f;
        svf_coefs(v->nc, 8000.0f + 2500.0f * tone, 2.0f);
        break;
    case 33:                                    /* Shaker: a felt kick */
        v->inc = f0 * 2.0f * (1.0f / EN_SR), v->inc_end = f0 * 1.2f * (1.0f / EN_SR), v->l1 = 0.8f, dec = 0.25f, v->ar = 0.4f;
        v->nl = 0.08f;
        svf_coefs(v->nc, 400.0f, 1.0f);
        break;
    case 34:                                    /* a soft clap, warm */
        v->nl = 0.7f, dec = 0.12f, v->ar = 0.05f, pan = 0.2f;
        svf_coefs(v->nc, 1100.0f + 500.0f * tone, 1.1f);
        break;
    case 35:                                    /* a shaker, longer */
        v->nl = 0.5f, dec = 0.11f, v->ar = 0.02f, pan = -0.4f;
        svf_coefs(v->nc, 5000.0f + 2000.0f * tone, 1.4f);
        break;
    case 36:                                    /* Glass: a tiny glassy click, low */
        v->inc = v->inc_end = f0 * 16.0f * (1.0f / EN_SR), v->l1 = 0.35f, dec = 0.05f, v->ar = 0.7f;
        v->i2 = f0 * 16.0f * 2.76f * (1.0f / EN_SR), v->l2 = 0.25f;
        break;
    case 37:                                    /* a bell-like tick */
        v->inc = v->inc_end = f0 * 24.0f * (1.0f / EN_SR), v->l1 = 0.3f, dec = 0.18f, v->ar = 0.6f, pan = 0.45f;
        v->i2 = f0 * 24.0f * 3.9f * (1.0f / EN_SR), v->l2 = 0.15f + 0.1f * tone;
        break;
    default:                                    /* a glass grain, very high */
        v->inc = v->inc_end = f0 * 40.0f * (1.0f / EN_SR), v->l1 = 0.22f, dec = 0.03f, v->ar = 0.8f, pan = -0.5f;
        break;
    }
    pan *= 0.4f + 1.2f * kwidth;
    pan = (fm_clampf(pan, -1.0f, 1.0f) + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(pan);
    v->gr = fm_sinf(pan);
    v->adec = fm_expf(-6.9f / (dec * EN_SR));
    (void)vel;
    en_drum_glow[d] = 1.0f;
    midi(0x99, v->note, (uint32_t)(v->amp * 100.0f));
    midi(0x89, v->note, 0);
}

static void drum_hit(int d, float vel, int off)  /* off: an off sixteenth, which Swing delays */
{
    int32_t w = off ? (int32_t)(swing * step_len) : 0;
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static void tick(void)
{
    int d, l;
    const float pa = fm_clampf(1.0f + 1.3f * arcm, 0.3f, 1.15f);   /* the arc: the pulse fades (never out) */
    if ((sixteenth & 15u) == 0u) {              /* a bar: Evolve's rests and turns */
        if (pulse_rest > 0)
            pulse_rest--;
        else if (frand() < evolve * 0.015f)
            pulse_rest = 2 + irand(3);           /* a short breath: 2 to 4 bars */
        en_pulse_rest = (uint8_t)(pulse_rest > 0);
        if (frand() < evolve * 0.25f + kmove * 0.15f)
            drums_evolve();
    }
    for (d = 0; d < EN_NDRUM; d++) {
        if (drum_pat[d] >> drum_pos[d] & 1u && !pulse_rest)
            drum_hit(d, ((drum_pos[d] == 0 ? 0.95f : 0.6f) + (0.1f + 0.35f * kmove) * frand()) * pa, (int)(sixteenth & 1u));
        en_drum_pos[d] = drum_pos[d];
        if (++drum_pos[d] >= EN_DRUM_N[d])
            drum_pos[d] = 0;
    }
    if ((sixteenth & 3u) == 0u) {               /* the loops step in quarter notes */
        for (l = 0; l < EN_NLOOP; l++) {
            int s = loop_pos[l];
            if (loop_note[l][s] >= 0 && !loop_rest[l])
                bell_ring(l, s);
            en_loop_pos[l] = (uint8_t)s;
            if (++loop_pos[l] >= EN_LOOP_LEN[l]) {
                loop_pos[l] = 0;
                if (loop_len_next[l]) {          /* a new era's length, from the top of a pass */
                    int k;
                    en_loop_len[l] = loop_len_next[l];
                    loop_len_next[l] = 0;
                    for (k = EN_LOOP_LEN[l]; k < EN_LOOP_MAX; k++)
                        loop_note[l][k] = -1;
                }
                loop_pass(l);
            }
        }
    }
    motif_tick();
    lead_tick();
    if ((sixteenth & 15u) == 0u && change_pending && !en_frozen) {   /* chords move on the bar */
        change_pending = 0;
        chord_change();
    }
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni): a predelay, four input diffusers,
 * then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass, an
 * allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
/* 1.4: the plate runs at half rate, 22.05 kHz (its tail is dark: nothing lost), every other sample, for the FM-1's CPU */
#define DS(n) (((n) * 7409 + 5000) / 10000)           /* 22050 / 29761 */
#define PRE_N 662                                    /* 30 ms */
#define EXC DS(24)                                  /* 1.1: a deeper sway: a lusher tail */
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;
static float rv_bw = 0.6f, rv_damp = 0.45f;     /* Warmth: the plate's input bandwidth and its damping */

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static float dv_c[4], dv_s[2][2];               /* Filter dive's filter (svf_coefs writes four) */
static float pl_hp;                             /* the plate's input, nothing under ~200 Hz: no mud in the tail */
static float rv_in, rv_l0, rv_l1, rv_r0, rv_r1;   /* the half-rate plate: its last input, its last two outputs */
static uint32_t rv_ph;
static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    const float pl_dec = pl_decay + (0.9995f - pl_decay) * fx_frz;   /* Freeze: the tail held, nothing new in */
    in *= 1.0f - fx_frz;
    pl_hp += 0.055f * (in - pl_hp);
    in -= pl_hp;
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += rv_bw * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.00026f * lfo_c, c = lfo_c - 0.00026f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_dec * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += rv_damp * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_dec, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_dec * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += rv_damp * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_dec, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = pl_hp = 0.0f;
    rv_in = rv_l0 = rv_l1 = rv_r0 = rv_r1 = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (long: up to ~5.6 s, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 62000                             /* 5.6 s at 11.025 kHz */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
static const float HEAD2[5] = {0.75f, 0.6667f, 0.625f, 0.5f, 0.375f};   /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */
#define PLEX_N 7100                              /* 160 ms at 44.1 kHz */
#define PLEX_TAP 2700                            /* the right's tap: ~100 ms */
static int16_t plex[PLEX_N];
static int plex_w;
static float plex_lp, plex_lp2;

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[i] * beat;
    while (t * TAPE_SR > (float)(TAPE_N - 8) && i > 0)
        t = ETIME_BEATS[--i] * beat;             /* too long for the tape: the next shorter */
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the heads sit in the field, how the feedback leans, the second head's slide */
static void heads_step(uint32_t n)
{
    float p, want, vmax;
    int i;
    for (i = 0; i < 2; i++) {
        p = fm_clampf((i ? 0.7f : -0.7f) + 0.9f * motion * mod[6 + i].v, -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.25f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((20.0f + 40.0f * frand()) * EN_SR);
        if (en_playing && !en_frozen && frand() < motion)
            head2_t = HEAD2[(int)(frand() * 4.999f)];
    } else {
        head_left -= n;
    }
    /* its speed (a part of the loop per tape sample): at most a bend of ~14 cents, easing in and out */
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
}

/* one sample of both: the input (a mono send) to the tape's heads (el, er) and the Echoplex (xl, xr) */
static inline void echoes(float in, float *el, float *er, float *xl, float *xr)
{
    float y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    int t = plex_w - PLEX_TAP;
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tape_acc += in + 0.3f * plex_lp;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + (0.003f * motion + 0.008f * tape_amt) * mod[5].v);   /* wow, more with Tape */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        h1 = tape_read(d);
        h2 = tape_read(d * head2);
        tape_lp += (0.55f - 0.25f * tape_amt) * (h1 + fb_lean * (h2 - h1) - tape_lp);   /* each time round: darker, thinner */
        tape_hp += 0.03f * (tape_lp - tape_hp);
        fb = tape_lp - tape_hp;
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        {
            float w = tape_acc * 0.25f + fm_maxf(echo_fb, 0.96f * fx_hold) * fb;   /* Echo hold: it keeps going */
            w = w * (1.0f + tape_amt) / (1.0f + 2.0f * tape_amt * fm_fabsf(w));   /* Tape: it saturates, softly */
            tape[tape_w] = s16(w);
        }
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        *el = hg[0] * h1 + hg[2] * h2;
        *er = hg[1] * h1 + hg[3] * h2;
    }
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;

static float tempo_cur, tempo_off;              /* the tempo as it plays: it glides to Tempo (and an era's lean) */
static void tempo_set(void)
{
    if (tempo_cur <= 0.0f)
        tempo_cur = (float)par[P_TEMPO];
    step_len = EN_SR * 60.0f / (tempo_cur * 4.0f);
}

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_PAD: pad_vol = (float)(v * v) * 0.0001f; break;
    case P_BELLS: bell_vol = (float)(v * v) * 0.0001f; break;
    case P_BASS: bass_vol = (float)(v * v) * 0.0001f; break;
    case P_PULSE: pulse_vol = (float)(v * v) * 0.0001f; break;
    case P_HITS: drums_pattern(); break;
    case P_BASSOCT:
        if (nsteps)
            bass_to(bass.midi_note % 12);
        break;
    case P_GLIDE: glide_k = 1.0f / ((0.02f + 0.012f * (float)v) * EN_SR); break;   /* 20 ms .. 1.2 s */
    case P_SWING: swing = (float)v * 0.005f; break;      /* the off sixteenths up to half a step late */
    case P_HATS:
        hat_f = 0.55f + 0.0070f * (float)v;               /* the high-pass: ~4 kHz dark .. ~10 kHz bright */
        hat_open = 0.6f - 0.0035f * (float)v;             /* the open hat: long and soft .. short and crisp */
        break;
    case P_DRIFT:
        if (drift_total > 0.0f) {                /* keep the progress, at the new pace */
            float frac = drift_left / drift_total;
            drift_total = drift_secs(v) * EN_SR;
            drift_left = frac * drift_total;
        }
        break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_SIZE:
        pl_decay_t = 0.6f + 0.385f * (float)v * 0.01f;     /* to 0.985: a long hall */
        echo_fb = 0.5f + 0.32f * (float)v * 0.01f;
        break;
    case P_BRIGHT: bright = (float)v; break;
    case P_SWELL: swell = (float)v; break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_BTONE: btone = (float)v * 0.01f; break;
    case P_PADTONE:
        en_pad_snd = (uint8_t)v;
        pad_handoff();
        break;
    case P_BELLSND: en_bell_snd = (uint8_t)v; break;        /* the voices cross over as they did for a new key */
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_PWARM: pwarm = (float)v; break;
    case P_PMOVE: pmove = (float)v * 0.01f; break;
    case P_PSPACE: pspace = (float)v * 0.01f; break;
    case P_PWIDTH: pwidth = (float)v * 0.01f; break;
    case P_SHIMB: shimb = (float)v * 0.01f; break;
    case P_BSPACE: bspace = (float)v * 0.01f; break;
    case P_BDECAY: bdecay = fm_exp2f((float)(v - 50) * 0.03f); break;   /* a third .. three times as long */
    case P_KMOVE: kmove = (float)v * 0.01f; break;
    case P_KSPACE: kspace = (float)v * 0.01f; break;
    case P_KWIDTH: kwidth = (float)v * 0.01f; break;
    case P_SHIM: shim = (float)v * 0.01f; break;
    case P_RWARM:                                /* the plate: brighter and longer-lit .. darker, softer */
        rv_bw = 0.8f - 0.5f * (float)v * 0.01f;
        rv_damp = 0.7f - 0.55f * (float)v * 0.01f;
        rv_bw = 1.0f - (1.0f - rv_bw) * (1.0f - rv_bw);         /* the same corners at half rate */
        rv_damp = 1.0f - (1.0f - rv_damp) * (1.0f - rv_damp);
        break;
    case P_TAPE: tape_amt = (float)v * 0.01f; break;
    case P_WIDTH: width = (float)v * 0.01f; break;
    case P_AIR: air_vol = (float)(v * v) * 0.0001f; break;
    case P_AIRTONE: air_tone = (float)v * 0.01f; break;
    case P_AIRMOVE: air_mv = (float)v * 0.01f; break;
    case P_SLEEP:                                /* from now: 30, 60 or 90 minutes */
        sleep_left = (float)v * 1800.0f;
        en_asleep = 0;
        break;
    case P_PATT:
    case P_PREL: {                               /* Attack 0..100: 8 % .. 72 % of the breath rising; Release: falling */
        float ri = 0.08f + 0.64f * (float)par[P_PATT] * 0.01f, fa = 0.07f + 0.56f * (float)par[P_PREL] * 0.01f;
        if (ri + fa > 0.88f) {                   /* both long: share the breath */
            const float k = 0.88f / (ri + fa);
            ri *= k;
            fa *= k;
        }
        pad_rise = ri;
        pad_fall = fa;
        break;
    }
    case P_LEAD: lead_vol = (float)(v * v) * 0.0001f; break;
    case P_LTONE: lead_tone = (float)v * 0.01f; break;
    case P_LSPACE: lead_space = (float)v * 0.01f; break;
    case P_LEXPR: lead_expr = (float)v * 0.01f; break;
    case P_WANDER: wander = (float)v * 0.01f; break;
    case P_FXLVL: fx_lvl = (float)(v * v) * 0.0001f; break;
    case P_FXSPACE: fx_space = (float)v * 0.01f; break;
    case P_TIDE:
        tidef = fm_exp2f((float)(v - 50) * 0.04f);   /* a quarter .. four times AMB-1's tide */
        j_rescale = 1;
        break;
    case P_JOURNEY: j_rescale = 1; break;
    case P_WORLD:
        cents = WORLDS[v].cents;
        world_trim = fm_exp2f((float)WORLDS[v].trim * (1.0f / 6.0206f));
        break;
    case P_BOCT:                                 /* let it be heard now: a bell from the first loop */
        if (en_playing && nsteps) {
            int l, k;
            for (l = 0; l < EN_NLOOP; l++)
                for (k = 0; k < EN_LOOP_LEN[l]; k++)
                    if (loop_note[l][k] >= 0) {
                        bell_ring(l, k);
                        return;
                    }
        }
        break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO:
        tempo_set();
        echo_time();
        break;
    case P_ROOT:
        en_root_now = (uint8_t)v;
        if (nsteps) {
            pad_retarget();
            pad_handoff();
            bass_to(chord_pcs[0]);
        }
        break;
    case P_MIDI:
        if (!v)
            en_midi_out(0xB0, 123, 0);
        break;
    default: break;
    }
}

/* ------------------------------------------------------------- wander --- */
/* Wander: the garden tends itself. Under every value set (user[]), a slow offset of its own, each finding a new
 * place every minute or few (Tide stretches it) and gliding there, as far as Wander lets it; a layer the world keeps
 * silent stays silent. A value turned by hand is pinned: Wander leaves it where the hand put it (until EDIT is held,
 * or the world is new). Then the seasons, every 6 to 14 minutes: with Wander past half, the pad or the bells may
 * take a sibling's sound a while; with Key moves, the key may leave home (up or down a fourth, a fifth, a step) on
 * the next chord, and comes back a season or two later. At Wander 0 only what is set plays (Evolve and Motion
 * still move, as AMB-1's). */
static int16_t user[P_NPARAMS];
static float woff[P_NPARAMS], wtgt[P_NPARAMS];
static uint32_t wleft[P_NPARAMS];
static uint8_t pinned[P_NPARAMS];
static int snd_over[2] = {-1, -1};
static uint32_t season_left, wander_acc;
volatile uint8_t en_root_now, en_pad_snd, en_bell_snd;
volatile uint32_t en_seasons;
static const struct {
    uint8_t p, span, level;                     /* level: 0 stays 0 */
} WANDERS[] = {
    {P_PAD, 18, 1},    {P_BELLS, 20, 1},  {P_BASS, 15, 1},   {P_PULSE, 18, 1},  {P_AIR, 10, 1},   {P_DENSITY, 22, 0},
    {P_BRIGHT, 20, 0}, {P_SWELL, 18, 0},  {P_BTONE, 18, 0},  {P_PWARM, 14, 0},  {P_PMOVE, 22, 0}, {P_SHIMB, 22, 0},
    {P_SHIM, 18, 0},   {P_ECHO, 16, 0},   {P_REVERB, 10, 0}, {P_HITS, 18, 1},   {P_BDECAY, 14, 0}, {P_PWIDTH, 14, 0},
    {P_EVOLVE, 14, 0}, {P_AIRTONE, 20, 0},
};
#define NWANDER (int)(sizeof WANDERS / sizeof WANDERS[0])
/* each world's family of sounds: its own first, then the three its pad and its bells may take a season or an era
 *   pad: 0 Warm 1 Strings 2 Choir 3 Rhodes 4 Glass 5 Breath 6 Organ 7 Juno 8 Bowed 9 Halo 10 Dream
 *   bells: 0 Soft 1 Felt 2 Celesta 3 Marimba 4 Vibes 5 Gamelan 6 Crystal 7 Kalimba 8 Music box 9 Bowl 10 Chime
 *   11 Harp 12 Steel */
static const uint8_t FAM[EN_NPIECES][2][4] = {
    {{0, 7, 9, 10}, {0, 6, 10, 9}}, {{1, 8, 2, 9}, {1, 11, 10, 0}}, {{3, 7, 6, 0}, {4, 1, 7, 8}},
    {{4, 9, 10, 5}, {6, 2, 8, 12}}, {{3, 10, 5, 4}, {3, 7, 4, 8}},  {{0, 6, 9, 2}, {5, 9, 10, 12}},
    {{2, 8, 9, 5}, {0, 9, 1, 6}},   {{2, 0, 1, 9}, {1, 0, 11, 9}},
};

static int eff_value(int p)                    /* what plays: the value set, and Wander's offset, the sounds' swaps */
{
    int i, v = user[p];
    if (p == P_ROOT)
        return ((v + key_off) % 12 + 12) % 12;
    if (p == P_PADTONE && snd_over[0] >= 0 && !pinned[p])
        return snd_over[0];
    if (p == P_BELLSND && snd_over[1] >= 0 && !pinned[p])
        return snd_over[1];
    if (pinned[p] || wander <= 0.0f)
        return v;
    for (i = 0; i < NWANDER; i++)
        if (WANDERS[i].p == p) {
            if (WANDERS[i].level && v == 0)
                return 0;
            v += (int)(woff[p] * (float)WANDERS[i].span * wander + (woff[p] < 0.0f ? -0.5f : 0.5f));
            return v < PARAMS[p].lo ? PARAMS[p].lo : v > PARAMS[p].hi ? PARAMS[p].hi : v;
        }
    return v;
}
static void eff_apply(int p)
{
    int v = eff_value(p);
    if (v != par[p])
        apply(p, v);
}

static void wander_reset(void)                 /* a new world: home, nothing pinned, no offsets */
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        pinned[i] = 0;
        woff[i] = wtgt[i] = 0.0f;
        wleft[i] = 0;
    }
    key_off = 0;
    key_pending = -99;
    snd_over[0] = snd_over[1] = -1;
    season_left = (uint32_t)((360.0f + 480.0f * frand()) * EN_SR);
}

static void season(void)                        /* a new season: a sound may change a while, the key may move */
{
    const int w = par[P_WORLD];
    en_seasons++;
    if (wander > 0.5f && frand() < (wander - 0.5f) * 1.4f) {
        int k = irand(2), pp = k ? P_BELLSND : P_PADTONE;
        if (!pinned[pp] && user[k ? P_BELLS : P_PAD] > 0) {
            snd_over[k] = snd_over[k] >= 0 && frand() < 0.6f ? -1 : FAM[w][k][1 + irand(3)];
            eff_apply(pp);
        }
    } else if (snd_over[0] >= 0 || snd_over[1] >= 0) {
        snd_over[0] = snd_over[1] = -1;          /* home again */
        eff_apply(P_PADTONE);
        eff_apply(P_BELLSND);
    }
    if (!pinned[P_ROOT] && frand() < (float)par[P_KEYMOVE] * 0.01f) {
        static const int8_t MOVES[6] = {5, -5, 7, -7, 2, -2};
        key_pending = key_off != 0 && frand() < 0.7f ? 0 : MOVES[irand(6)];
        change_pending = 1;                      /* on the next bar */
    } else if (key_off != 0 && frand() < 0.5f) {
        key_pending = 0;
        change_pending = 1;
    }
}

static void wander_step(uint32_t n)             /* once a chunk; the offsets every ~90 ms */
{
    int i;
    if (en_playing && !en_frozen) {
        if (season_left <= n) {
            season_left = (uint32_t)((360.0f + 480.0f * frand()) * fm_sqrtf(tidef) * EN_SR);
            season();
        } else {
            season_left -= n;
        }
    }
    wander_acc += n;
    if (wander_acc < 4096u)
        return;
    n = wander_acc;
    wander_acc = 0;
    for (i = 0; i < NWANDER; i++) {
        int p = WANDERS[i].p;
        if (wleft[p] <= n) {                    /* a new place to drift to: one to four minutes on, as Tide */
            wleft[p] = (uint32_t)((60.0f + 180.0f * frand()) * tidef * EN_SR);
            wtgt[p] = frand() * 2.0f - 1.0f;
        } else {
            wleft[p] -= n;
        }
        if (en_playing && !en_frozen)           /* gliding there, about a minute */
            woff[p] += (wtgt[p] - woff[p]) * fm_minf((float)n / (60.0f * tidef * EN_SR), 1.0f);
        eff_apply(p);
    }
}

/* ------------------------------------------------------------ journey --- */
/* As HADAL-1 travels: a world plays in parts, one leading to the next by chance (a Markov chain whose odds each era
 * stirs), each part a different balance of the layers: Dawn (the pad alone, dark), Bloom, Flow (everything), Shimmer
 * (the bells in front), Deep (the bass and a low pad), Dissolve (almost nothing). Every 15 to 40 minutes an era:
 * the loops take new lengths (primes, 7 to 37 beats), a new motif, the sounds may change within the world's family,
 * the key may move, the pace leans a little, and what was turned by hand is free again. With Journey Slow (half the
 * eras) or Often (every one), the era ends the world: it dissolves, and another one rises at dawn (en_journey_n,
 * which the UI takes up). Journey Still: none of it, AMB-1's garden. The motif: three to six notes, every minute or
 * two on the bells, each time changed: up a step, upside down, backwards, slower, an octave up. */
volatile uint8_t en_part;
volatile uint16_t en_eras = 1;
volatile uint32_t en_journey_n;
volatile int8_t en_journey_to = -1;
enum { PT_DAWN, PT_BLOOM, PT_FLOW, PT_SHIMMER, PT_DEEP, PT_DISSOLVE };
static const float PART_LVL[EN_NPART][6] = {      /* pad, bells, bass, pulse, air, lead */
    {1.0f, 0.25f, 0.35f, 0.4f, 1.0f, 0.0f}, {1.0f, 0.9f, 0.7f, 0.7f, 0.8f, 0.8f}, {0.9f, 1.0f, 1.0f, 1.2f, 0.7f, 1.0f},
    {0.65f, 1.2f, 0.45f, 0.8f, 0.6f, 0.75f}, {1.1f, 0.4f, 1.25f, 0.8f, 1.0f, 0.7f}, {0.45f, 0.25f, 0.2f, 0.0f, 1.2f, 0.0f}};
static const float PART_DENS[EN_NPART] = {0.4f, 1.0f, 1.1f, 1.45f, 0.6f, 0.3f};
static const float PART_BR[EN_NPART] = {-0.45f, 0.1f, 0.15f, 0.35f, -0.5f, -0.3f};
static const float PART_SH[EN_NPART] = {0.0f, 0.05f, 0.0f, 0.25f, 0.0f, 0.15f};
static const uint8_t TRANS0[EN_NPART][EN_NPART] = {   /* from (row) to (column): Dissolve now and then */
    {0, 6, 0, 2, 2, 0}, {0, 0, 6, 4, 2, 1}, {0, 4, 0, 4, 2, 1},
    {0, 4, 3, 0, 2, 1}, {2, 4, 2, 0, 0, 1}, {4, 2, 0, 0, 2, 0}};
static float trans[EN_NPART][EN_NPART], pg_t[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}, part_dens_t = 1.0f, part_br_t, part_sh_t;
static float part_left, era_left, mo_left;
static int jpart = PT_BLOOM, jnext_world = -1, jarrive = 1;
static int8_t mo_off[6];
static uint8_t mo_dur[6];
static int mo_n, mo_i, mo_wait, mo_tf;

static void part_set(int p)
{
    int i;
    const int on = par[P_JOURNEY] > 0;
    jpart = p;
    en_part = (uint8_t)p;
    for (i = 0; i < 6; i++)
        pg_t[i] = on ? PART_LVL[p][i] : 1.0f;
    part_dens_t = on ? PART_DENS[p] : 1.0f;
    part_br_t = on ? PART_BR[p] : 0.0f;
    part_sh_t = on ? PART_SH[p] : 0.0f;
    part_left = (p == PT_DISSOLVE ? 40.0f + 40.0f * frand() : 70.0f + 130.0f * frand()) * fm_sqrtf(tidef) * EN_SR;
    if (on && p != PT_DAWN && p != PT_DISSOLVE)
        loop_regrow(irand(EN_NLOOP));             /* a loop starts a new phrase */
}

static void part_next(void)
{
    int i, sum = 0, r;
    float tot = 0.0f, x;
    if (jnext_world >= 0) {
        if (jpart == PT_DISSOLVE) {               /* the world has dissolved: the next one, at dawn */
            en_journey_to = (int8_t)jnext_world;
            BARRIER();
            en_journey_n++;
            jnext_world = -1;
            jarrive = 1;
            part_set(PT_DAWN);
            return;
        }
        part_set(PT_DISSOLVE);
        return;
    }
    for (i = 0; i < EN_NPART; i++)
        tot += trans[jpart][i];
    x = frand() * tot;
    for (i = 0; i < EN_NPART; i++)
        if ((x -= trans[jpart][i]) < 0.0f)
            break;
    (void)sum;
    (void)r;
    part_set(i < EN_NPART ? i : PT_BLOOM);
}

static void motif_make(void)
{
    int i, o = irand(5);
    mo_n = 3 + irand(4);
    for (i = 0; i < mo_n; i++) {
        mo_off[i] = (int8_t)o;
        o += irand(5) - 2;
        o = o < 0 ? 0 : o > 8 ? 8 : o;
        mo_dur[i] = (uint8_t)(2 + 2 * irand(4));
    }
}

static void motif_tick(void)                    /* each sixteenth: the motif, while it speaks */
{
    int k, o;
    if (!mo_on || --mo_wait > 0)
        return;
    k = mo_tf == 3 ? mo_n - 1 - mo_i : mo_i;    /* backwards */
    o = mo_off[k];
    if (mo_tf == 1)
        o += 2;                                 /* up a step (of the chord: a third) */
    else if (mo_tf == 2)
        o = 2 * mo_off[0] - o + 4;              /* upside down */
    else if (mo_tf == 5)
        o += nsteps;                            /* an octave up */
    o = o < 0 ? 0 : o;
    bell_voice(1, bell_note(0, o), 0.82f, 0);
    mo_wait = mo_dur[k] * (mo_tf == 4 ? 2 : 1); /* slower */
    if (++mo_i >= mo_n)
        mo_on = 0;
}

static void pick_lengths(uint8_t *out)          /* four different primes, 7 to 37, short to long */
{
    static const uint8_t PRIMES[9] = {7, 11, 13, 17, 19, 23, 29, 31, 37};
    int i, j, k, pick[EN_NLOOP];
    for (i = 0; i < EN_NLOOP; i++) {
        do {
            k = PRIMES[irand(9)];
            for (j = 0; j < i && pick[j] != k; j++)
                ;
        } while (j < i);
        pick[i] = k;
    }
    for (i = 1; i < EN_NLOOP; i++)
        for (j = i; j > 0 && pick[j] < pick[j - 1]; j--) {
            k = pick[j];
            pick[j] = pick[j - 1];
            pick[j - 1] = k;
        }
    for (i = 0; i < EN_NLOOP; i++)
        out[i] = (uint8_t)pick[i];
}

static void era_new(void)
{
    const int w = par[P_WORLD];
    int i, j, k;
    en_eras++;
    era_left = (par[P_JOURNEY] == 3 ? 300.0f + 600.0f * frand() : 900.0f + 1500.0f * frand()) * fm_sqrtf(tidef) * EN_SR;
    pick_lengths(loop_len_next);
    motif_make();
    ld.m_n = 0;                                  /* the lead's memory: a new era, new ideas */
    for (i = 0; i < P_NPARAMS; i++)              /* what was turned by hand is free again */
        pinned[i] = 0;
    for (k = 0; k < 2; k++)                      /* the sounds, within the world's family */
        snd_over[k] = frand() < 0.55f ? FAM[w][k][1 + irand(3)] : -1;
    if (par[P_KEYMOVE] > 0 && frand() < 0.6f) {
        static const int8_t MOVES[6] = {5, -5, 7, -7, 2, -2};
        key_pending = key_off != 0 && frand() < 0.4f ? 0 : MOVES[irand(6)];
        change_pending = 1;
    }
    tempo_off = (frand() * 2.0f - 1.0f) * 0.07f;   /* the pace leans a little */
    for (i = 0; i < EN_NPART; i++)
        for (j = 0; j < EN_NPART; j++)
            trans[i][j] = TRANS0[i][j] ? (float)TRANS0[i][j] * (0.45f + 1.1f * frand()) : 0.0f;
    for (k = 0; k < P_NPARAMS; k++)
        eff_apply(k);
    drums_pattern();
    for (k = 0; k < 3; k++)
        drums_evolve();
    if (par[P_JOURNEY] >= 2 && (par[P_JOURNEY] == 3 || frand() < 0.5f)) {   /* this era ends the world */
        int nw = irand(EN_NPIECES - 1);
        jnext_world = nw >= w ? nw + 1 : nw;
        part_left = fm_minf(part_left, 6.0f * EN_SR);
    }
}

static void journey_reset(void)                 /* a new world: its first era, at dawn (or in bloom, chosen by hand) */
{
    int i, j;
    for (i = 0; i < EN_NPART; i++)
        for (j = 0; j < EN_NPART; j++)
            trans[i][j] = (float)TRANS0[i][j];
    for (i = 0; i < EN_NLOOP; i++) {             /* AMB-1's lengths, or (arriving on the journey) the era's own */
        en_loop_len[i] = LOOP_LEN0[i];
        loop_len_next[i] = 0;
    }
    if (jarrive && en_eras > 1) {
        uint8_t l2[EN_NLOOP];
        pick_lengths(l2);
        for (i = 0; i < EN_NLOOP; i++)
            en_loop_len[i] = l2[i];
        for (i = 0; i < EN_NLOOP; i++)
            loop_regrow(i);
    }
    jnext_world = -1;
    era_left = (par[P_JOURNEY] == 3 ? 300.0f + 600.0f * frand() : 900.0f + 1500.0f * frand()) * fm_sqrtf(tidef) * EN_SR;
    motif_make();
    mo_on = 0;
    mo_left = (40.0f + 50.0f * frand()) * EN_SR;
    tempo_off = 0.0f;
    part_set(jarrive ? PT_DAWN : PT_BLOOM);
    jarrive = 0;
}

static void journey_step(uint32_t n)            /* once a chunk */
{
    const float k = fm_minf((float)n / (14.0f * EN_SR), 1.0f);
    int i;
    for (i = 0; i < 6; i++) {
        if (hand_left[i] > 0.0f) {               /* turned by hand: up in a second or two */
            hand_left[i] -= (float)n * (1.0f / EN_SR);
            pg[i] += (fm_maxf(pg_t[i], 0.9f) - pg[i]) * fm_minf((float)n / (1.0f * EN_SR), 1.0f);
        } else {
            pg[i] += (pg_t[i] - pg[i]) * k;
        }
    }
    part_dens += (part_dens_t - part_dens) * k;
    part_br += (part_br_t - part_br) * k;
    part_sh += (part_sh_t - part_sh) * k;
    {                                            /* the pace glides: to Tempo, as the era leans it */
        float tt = (float)par[P_TEMPO] * (1.0f + (par[P_JOURNEY] > 0 ? tempo_off : 0.0f));
        tempo_cur += (tt - tempo_cur) * fm_minf((float)n / (6.0f * EN_SR), 1.0f);
        if (!en_ext)
            step_len = EN_SR * 60.0f / (tempo_cur * 4.0f);
    }
    if (!en_playing || en_frozen || par[P_JOURNEY] == 0)
        return;
    if (j_rescale) {                             /* quicker now: what is left, no longer than the new longest */
        const float mx = (par[P_JOURNEY] == 3 ? 900.0f : 2400.0f) * fm_sqrtf(tidef) * EN_SR,
                    mp = 200.0f * fm_sqrtf(tidef) * EN_SR;
        j_rescale = 0;
        if (era_left > mx)
            era_left = mx * (0.4f + 0.6f * frand());
        if (part_left > mp)
            part_left = mp * (0.4f + 0.6f * frand());
    }
    part_left -= (float)n;
    era_left -= (float)n;
    mo_left -= (float)n;
    if (part_left <= 0.0f)
        part_next();
    if (era_left <= 0.0f)
        era_new();
    if (mo_left <= 0.0f) {
        mo_left = (50.0f + 70.0f * frand()) * fm_sqrtf(tidef) * EN_SR;
        if (par[P_BELLS] > 0 && jpart != PT_DISSOLVE && !mo_on) {
            mo_tf = irand(6);
            mo_i = 0;
            mo_wait = 1;
            mo_on = 1;
        }
    }
}

static void journey_reseed(void)                /* a new seed: the first part's and era's lengths, the motif, the phrases */
{
    int l;
    era_left = (par[P_JOURNEY] == 3 ? 300.0f + 600.0f * frand() : 900.0f + 1500.0f * frand()) * fm_sqrtf(tidef) * EN_SR;
    part_left = (70.0f + 130.0f * frand()) * fm_sqrtf(tidef) * EN_SR;
    mo_left = (40.0f + 50.0f * frand()) * EN_SR;
    motif_make();
    for (l = 0; l < EN_NLOOP; l++)
        loop_regrow(l);
    drift_reset();
}

void en_part_next(void) { post(C_PART, 0, 0); }
void en_era(void) { post(C_ERA, 0, 0); }

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    if (deg >= nsteps)
        deg = 0;
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
}

static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    int l;
    wander_reset();
    for (l = 0; l < P_NPARAMS; l++)
        eff_apply(l);
    for (l = 0; l < EN_NLOOP; l++) {
        loop_regrow(l);
        loop_rest[l] = 0;
    }
    deg = 0;
    voicing = pick_voicing();
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
    bass.rest = 0;
    pulse_rest = 0;
    en_pulse_rest = 0;
    drums_pattern();
    drift_reset();
    change_pending = 0;
    journey_reset();
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    int i;
    for (i = 0; i < NBELL; i++)
        bell[i].on = 0;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    for (i = 0; i < EN_NPAD; i++)
        pad[i].amp = 0.0f;
    bass.amp = 0.0f;
    notes_off();
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    for (i = 0; i < PLEX_N; i++)
        plex[i] = 0;
    tape_w = plex_w = tape_ph = 0;
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target = (float)(50 + 7 * i);
        pad[i].pan = PAD_PAN[i];
        pad[i].amp = 0.0f;
        pad[i].ph[0] = 0.0f;
        pad[i].ph[1] = 0.37f;
        pad[i].vib = 0.3f * (float)i;
        pad[i].midi_note = 0;
        pad[i].hf = 1.0f;
        pad[i].hand = 0;
    }
    arcm = 0.0f;
    pad_gain = 0.0f;
    for (i = 0; i < P_NPARAMS; i++) {
        user[i] = PARAMS[i].def;
        apply(i, PARAMS[i].def);
    }
    wander_reset();
    journey_reset();                            /* the journey from its start: dawn, the first era */
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (i = 0; i < EN_NPAD; i++)
        pad[i].note = pad[i].target;
    bass.note = bass.target;
    for (i = 0; i < EN_NLOOP; i++) {
        loop_pos[i] = 0;
        loop_regrow(i);
    }
    drums_pattern();
    drift_reset();
    pl_decay = pl_decay_t;
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        for (i = 0; i < EN_NDRUM; i++)
            drum_pos[i] = 0;
        for (i = 0; i < EN_NLOOP; i++)
            loop_pos[i] = 0;
    }
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = 1.0f;
    for (i = 0; i < EN_NPAD; i++)               /* from silence: the breaths start over, overlapping */
        if (pad[i].amp < 1e-4f) {
            pad[i].t = START_T[i];
            pad[i].midi_note = 0;
            pad_breathe(&pad[i], i);
        }
    if (bass.amp < 1e-4f)
        bass.note = bass.target;
    midi(0x92, bass.midi_note, 80);
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = 0;                        /* a swung hit still waiting stays unplayed */
    en_playing = 0;
    pad_gate = 0.0f;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
        case C_SETH:
            if (a < P_NPARAMS) {
                user[a] = (int16_t)(b < PARAMS[a].lo ? PARAMS[a].lo : b > PARAMS[a].hi ? PARAMS[a].hi : b);
                if (c >> 24 == C_SETH) {        /* by hand: it stays where the hand put it */
                    int ly = a == P_PAD || a == P_PADTONE ? 0 : a == P_BELLS || a == P_BELLSND || a == P_DENSITY ? 1
                           : a == P_BASS || a == P_BASSSND ? 2 : a == P_PULSE || a == P_KIT || a == P_HITS ? 3
                           : a == P_AIR || a == P_AIRSND ? 4 : a >= P_LEAD && a <= P_LOCT ? 5 : -1;
                    pinned[a] = 1;
                    if (ly >= 0 && b > 0)           /* heard now, for a few minutes, whatever part the world is in */
                        hand_left[ly] = 180.0f;
                    if (ly == 5 && b > 0)
                        lead_now = 1;
                    if (a == P_ROOT)
                        key_off = 0;
                    if (a == P_PADTONE)
                        snd_over[0] = -1;
                    if (a == P_BELLSND)
                        snd_over[1] = -1;
                }
                apply(a, eff_value(a));
            }
            break;
        case C_UNPIN: {
            int k;
            for (k = 0; k < P_NPARAMS; k++)
                pinned[k] = 0;
            for (k = 0; k < P_NPARAMS; k++)
                eff_apply(k);
            break;
        }
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            rng ^= ((uint32_t)(b & 0xFFFF) << (a ? 16 : 0)) * 2654435761u;
            rnd();
            if (a)                               /* the seed's second half: what was drawn before it, drawn again */
                journey_reseed();
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_REGROW:
            regrow();
            break;
        case C_STRUM: strum(a); break;
        case C_OUT: spk_tgt = a ? 0.0f : 1.0f; break;
        case C_PART: part_next(); break;
        case C_FX:
            if (a >= 0 && a < EN_NFX) {
                if (b == 2)
                    fx_release(a);
                else
                    fx_start(a, b == 1);
            }
            break;
        case C_FXOFF: fx_release(-1); break;
        case C_ERA: era_new(); break;
        case C_LKEY: lead_key(a, b); break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];

/* the pad's bus: Warmth's 12 dB low-pass (Brian Eno darkened everything: the Synthi's filter below half, then tape),
 * then a slow ensemble: two taps a side on short delays, each swayed by its own slow LFO, uncorrelated, so the pad
 * widens and shimmers without the even pulsing of a single chorus (which, against pure tones on the FM-1's speaker,
 * read as a beat: why AMB-1 had none). Movement sets its depth, Width how far the voices and the ensemble spread. */
static float padl[BLK], padr[BLK];
static uint32_t pad_hph;                         /* the pad voices' half rate: 1 when the last sample was computed */
static float plp[4];
#define CHN 1024                                 /* 23 ms a side */
static int16_t chbuf[2][CHN] EN_POOL;
static int chw;
static float chph[4], chd[4], chs[4];           /* the ensemble's sways; its four delays now, and their steps */
static uint32_t chn16;                           /* samples to the sways' next point (every 16: they are slow) */
static const float CH_RATE[4] = {0.11f / EN_SR, 0.17f / EN_SR, 0.13f / EN_SR, 0.23f / EN_SR};
static const float CH_BASE[4] = {0.007f * EN_SR, 0.011f * EN_SR, 0.009f * EN_SR, 0.013f * EN_SR};

static inline float ch_read(const int16_t *b, float d)
{
    int k = (int)d, i0 = chw - k, i1;
    float f = d - (float)k;
    if (i0 < 0)
        i0 += CHN;
    i1 = i0 - 1;
    if (i1 < 0)
        i1 += CHN;
    return ((float)b[i0] + ((float)b[i1] - (float)b[i0]) * f) * (1.0f / 16000.0f);
}

static float trem_ph;
static void pad_bus(uint32_t n)
{
    const float fc = fm_clampf(14000.0f * fm_exp2f(-pwarm * 0.036f + 0.4f * arcm + 0.5f * part_br + fx_sweep), 300.0f, 16000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    const float cm = fm_clampf(pmove + (par[P_PADTONE] == PS_STRINGS ? 0.25f : 0.0f), 0.0f, 1.2f);
    const float depth = (0.001f + 0.0032f * cm) * EN_SR, wet = 0.2f + 0.35f * cm;   /* at most 13 + 9.7 ms: inside the line */
    const float sw = 0.3f + 0.95f * pwidth, mw = 1.0f - 0.12f * pwidth, ssend = pspace * 2.0f;
    uint32_t j;
    int k;
    for (j = 0; j < n; j++) {
        float l, r, a, b, m, sd;
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        l = plp[1];
        r = plp[3];
        {
            int16_t *bl = chbuf[0], *br = chbuf[1];
            float x = l * 16000.0f, y = r * 16000.0f;
            bl[chw] = (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
            br[chw] = (int16_t)(y > 32767.0f ? 32767.0f : y < -32767.0f ? -32767.0f : y);
            if (chn16 == 0u) {                   /* the sways, a point every 16 samples, straight lines between (1.4:
                                                  * the same curves, a sixteenth of the work) */
                chn16 = 16u;
                for (k = 0; k < 4; k++) {
                    float nx;
                    chph[k] += 16.0f * CH_RATE[k];
                    if (chph[k] >= 1.0f)
                        chph[k] -= 1.0f;
                    nx = CH_BASE[k] + depth * (1.0f + sine(chph[k]));
                    if (chd[k] <= 0.0f)
                        chd[k] = nx;
                    chs[k] = (nx - chd[k]) * (1.0f / 16.0f);
                }
            }
            chn16--;
            for (k = 0; k < 4; k++)
                chd[k] += chs[k];
            a = 0.5f * (ch_read(bl, chd[0]) + ch_read(br, chd[1]));
            b = 0.5f * (ch_read(br, chd[2]) + ch_read(bl, chd[3]));
            if (++chw >= CHN)
                chw = 0;
        }
        l = l * (1.0f - 0.5f * wet) + a * wet;
        r = r * (1.0f - 0.5f * wet) + b * wet;
        if (fx_trem > 0.0f) {                    /* Tremolo: 4.5 a second */
            float tg;
            trem_ph += 4.5f / EN_SR;
            if (trem_ph >= 1.0f)
                trem_ph -= 1.0f;
            tg = 1.0f - 0.7f * fx_trem * (0.5f + 0.5f * sine(trem_ph));
            l *= tg;
            r *= tg;
        }
        m = (l + r) * 0.5f * mw;
        sd = (l - r) * 0.5f * sw;
        l = m + sd;
        r = m - sd;
        mixl[j] += l * 0.8f;
        mixr[j] += r * 0.8f;
        rsend[j] += (l + r) * 0.6f * ssend;
        esend[j] += (l + r) * 0.2f * ssend;
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

static inline float tri(float p) { return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; }
static inline float choir_src(float p, float dt) { return tri(p) * 1.4f + 0.45f * blep_saw(p, dt) + 0.6f * sine(p); }

static void render_pad(uint32_t n)
{
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;
    pad_gain += (pad_gate - pad_gain) * fm_minf((float)n / ((pad_gate > 0.0f ? 2.0f : 5.0f) * EN_SR), 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], amp = v->amp, s0 = v->ic[0], s1 = v->ic[1], s2 = v->ic[2],
              s3 = v->ic[3], gl, gr, pn, tgt = 0.0f, idx = 0.0f, g, trem = 1.0f, a2 = 0.0f, a3 = 0.0f;
        uint32_t j, hp = pad_hph;
        float d0 = 0.0f, d1 = 0.0f, d2 = 0.0f, xl = v->xlast;
        if (amp < 1e-5f && pad_gain < 1e-4f) {
            v->xlast = 0.0f;
            v->amp = 0.0f;
            continue;
        }
        pn = fm_clampf(v->pan * (0.3f + 1.1f * pwidth) + 0.35f * motion * mod[(i + 3) % NMOD_PAD].v, -1.0f, 1.0f);
        g = pad_vol * 0.5f * PAD_LVL[v->voices] * pg[0] * (1.0f + 0.9f * fx_swell);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        for (j = 0; j < n; j++) {
            float x, v1, v2, v3;
            if (!(j & 31u)) {
                pad_coefs(v, i);
                tgt = v->br * v->peak * pad_gain * v->hf;
                /* the Rhodes blooms: more overtones as the breath swells (index in turns) */
                idx = (0.04f + 0.1f * bright * 0.01f + 0.1f * v->br) * (1.0f + 0.4f * arcm);
                trem = 1.0f - 0.32f * pmove * (0.5f + 0.5f * sine(v->trem));
                a2 = 0.3f + 0.3f * v->br;       /* the glass's partials grow with the breath */
                a3 = (0.08f + 0.3f * bright * 0.01f) * v->br;
                d0 = 2.0f * v->inc[0];          /* a step of the half rate */
                d1 = 2.0f * v->inc[1];
                d2 = 2.0f * v->inc[2];
            }
            hp ^= 1u;
            if (!hp) {                          /* between two half-rate samples: the newer */
                padl[j] += xl * gl;
                padr[j] += xl * gr;
                continue;
            }
            amp += (tgt - amp) * 0.02f;
            switch (v->voices) {
            case PS_RHODES: {                   /* FM, the modulator at the carrier's pitch: one carrier, no beating */
                float m = sine(p0) * idx;
                x = (sine(p0 + m + 1.0f) + 0.12f * sine(2.0f * p0)) * 1.2f * trem;
                break;
            }
            case PS_GLASS:                      /* sines: the note, a slowly beating octave, the twelfth */
                x = sine(p0) + a2 * sine(2.0f * p1) + a3 * sine(3.0f * p0) + 0.04f * v->br * sine(5.0f * p1);
                break;
            case PS_CHOIR:                      /* two singers through two formants, as AMB-1's Voices */
                x = choir_src(p0, d0) + 0.8f * choir_src(p1, d1);
                v3 = x - s1;
                v1 = v->c1[0] * s0 + v->c1[1] * v3;
                v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
                s0 = 2.0f * v1 - s0;
                s1 = 2.0f * v2 - s1;
                {
                    float b1 = v1 * v->c1[3], l1 = v2, b2;
                    v3 = x - s3;
                    v1 = v->c2[0] * s2 + v->c2[1] * v3;
                    v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
                    s2 = 2.0f * v1 - s2;
                    s3 = 2.0f * v2 - s3;
                    b2 = v1 * v->c2[3];
                    x = (b1 + 0.5f * b2 + 0.45f * l1) * 0.6f;
                }
                break;
            case PS_BREATH:                     /* air on the note: noise in a narrow band, a soft flute under it */
                noise = noise * 1664525u + 1013904223u;
                x = (float)(int32_t)noise * (1.0f / 2147483648.0f);
                v3 = x - s1;
                v1 = v->c1[0] * s0 + v->c1[1] * v3;
                v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
                s0 = 2.0f * v1 - s0;
                s1 = 2.0f * v2 - s1;
                x = v1 * 0.35f + 0.5f * tri(p0);
                v3 = x - s3;
                v1 = v->c2[0] * s2 + v->c2[1] * v3;
                v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
                s2 = 2.0f * v1 - s2;
                s3 = 2.0f * v2 - s3;
                x = v2;
                break;
            case PS_CATH:                       /* Cathedral: an organ's flue pipes, the note, its octave, its fifteenth */
                x = sine(p0) + 0.55f * sine(2.0f * p1) + 0.3f * sine(4.0f * p0) + (0.06f + 0.1f * v->br) * sine(3.0f * p1);
                break;
            case PS_HALO:                       /* sines: the note, its octave and twelfth beating slowly, two above */
                x = sine(p0) + (0.35f + 0.25f * v->br) * sine(2.0f * p1) + 0.22f * v->br * sine(3.0f * p2) +
                    0.12f * sine(4.0f * p0);
                break;
            case PS_DREAM: {                    /* FM, the modulator an octave up, a detuned pair: a bell that breathes */
                float m = sine(2.0f * p0) * (0.03f + 0.12f * v->br * (0.4f + bright * 0.01f));
                x = sine(p0 + m + 1.0f) + 0.7f * sine(p1 + m + 1.0f);
                break;
            }
            default:                            /* the filtered ones: saws, squares, pulses, a bow */
                if (v->voices == PS_ORGAN) {    /* a reed: a square and its octave */
                    float q = p0 + 0.5f;
                    q -= q >= 1.0f ? 1.0f : 0.0f;
                    x = (blep_saw(p0, d0) - blep_saw(q, d0)) * 0.55f + 0.45f * sine(2.0f * p1) +
                        0.2f * sine(4.0f * p0);
                } else if (v->voices == PS_JUNO) {   /* a pulse whose width wanders, and a saw beside it */
                    float q = p0 + 0.5f + 0.3f * mod[(i + 1) % NMOD_PAD].v;
                    q -= q >= 1.0f ? 1.0f : 0.0f;
                    x = blep_saw(p0, d0) - blep_saw(q, d0) + 0.7f * blep_saw(p1, d1);
                } else if (v->voices == PS_BRASS || v->voices == PS_RESO) {   /* two saws (soft brass; a resonance) */
                    x = blep_saw(p0, d0) + blep_saw(p1, d1);
                } else if (v->voices == PS_ETHER) {  /* a saw and a sine, air on them as the breath swells */
                    noise = noise * 1664525u + 1013904223u;
                    x = blep_saw(p0, d0) * 0.6f + sine(p1) * 0.7f + (float)(int32_t)noise * (0.35f / 2147483648.0f) * v->br;
                } else if (v->voices == PS_BOWED) {  /* one saw, a little bow noise, a resonant body */
                    noise = noise * 1664525u + 1013904223u;
                    x = blep_saw(p0, d0) * 1.4f + (float)(int32_t)noise * (0.12f / 2147483648.0f) * v->br;
                } else {                        /* Warm and Strings: three saws */
                    x = blep_saw(p0, d0) + blep_saw(p1, d1) +
                        (v->voices == PS_WARM ? 0.7f : 1.0f) * blep_saw(p2, d2);
                }
                v3 = x - s1;
                v1 = v->c1[0] * s0 + v->c1[1] * v3;
                v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
                s0 = 2.0f * v1 - s0;
                s1 = 2.0f * v2 - s1;
                x = v2 * 0.5f;
                break;
            }
            p0 += d0;
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            p1 += d1;
            if (p1 >= 1.0f)
                p1 -= 1.0f;
            p2 += d2;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            x *= amp;
            {                                   /* halfway from the last to this one */
                const float o = (xl + x) * 0.5f;
                xl = x;
                padl[j] += o * gl;
                padr[j] += o * gr;
            }
        }
        v->xlast = fm_flush(xl);
        v->ic[0] = fm_flush(s0);
        v->ic[1] = fm_flush(s1);
        v->ic[2] = fm_flush(s2);
        v->ic[3] = fm_flush(s3);
        v->amp = amp;
        v->ph[0] = p0;
        v->ph[1] = p1;
        v->ph[2] = p2;
        en_pad_note[i] = (uint8_t)v->note;
        en_pad_amp[i] = amp;
        lvl = fm_maxf(lvl, amp);
    }
    pad_hph ^= n & 1u;                           /* where the half rate stands, for the next chunk */
    en_pad_level = lvl;
    pad_bus(n);
}

/* the bass: Sine (AMB-1's sine and its sub-octave), Warm (a triangle and a little saw, low-passed), Round (a sine with
 * soft overtones, as a fretless), Drone (the root and its fifth, held, never resting). Warmth darkens and rounds it,
 * Movement lets it breathe slowly, Space sends it to the plate */
static float bass_lp[2], bass_mv, bass_sv[2], bass_c[4], bass_pl, bass_last = -1.0f, bass_th;
static const float BASS_LVL[EN_NBASSSND] = {1.0f, 0.8f, 1.0f, 0.75f, 0.8f, 1.05f, 1.25f, 1.45f, 0.9f, 1.4f};
static float bass_ph2;                           /* Reese: its second saw */
static void render_bass(uint32_t n)
{
    uint32_t j;
    const int snd = par[P_BASSSND];
    const float g = bass_vol * 0.42f * pg[2] * BASS_LVL[snd],
                tgt = en_playing && (!bass.rest || snd == 3) ? 1.0f : 0.0f,
                rate = tgt > 0.0f ? 1.0f / (2.5f * EN_SR) : 1.0f / (4.0f * EN_SR),
                warm = (float)par[P_BWARM] * 0.01f, mv = (float)par[P_BMOVE] * 0.01f,
                sp = (float)par[P_BASPACE] * 0.01f * 0.25f;
    float ph = bass.ph, amp = bass.amp, inc, c, l0 = bass_lp[0], l1 = bass_lp[1], breathe;
    if (amp < 1e-5f && !en_playing) {
        bass.amp = 0.0f;
        en_bass_level = 0.0f;
        return;
    }
    bass.note += (bass.target - bass.note) * fm_minf((float)n * glide_k, 1.0f);   /* Glide */
    bass_mv += (float)n * (1.0f / (13.0f * EN_SR));                               /* a slow breath, 13 s */
    if (bass_mv >= 1.0f)
        bass_mv -= 1.0f;
    breathe = 1.0f - 0.35f * mv * (0.5f + 0.5f * sine(bass_mv));
    inc = mtof(bass.note + 0.05f * mv * mod[4].v) * (1.0f / EN_SR);
    c = 1.0f - fm_expf(-FM_TWO_PI * (1400.0f * fm_exp2f(-3.0f * warm)) * (1.0f / EN_SR));   /* 1.4 kHz .. 175 Hz */
    if (snd == 4)                                /* Moog: a resonant low-pass, Warmth closing it */
        svf_coefs(bass_c, 90.0f * fm_exp2f(3.5f * (1.0f - warm)), 0.35f);
    if (bass.target != bass_last) {              /* Pluck: each new note plucked */
        bass_last = bass.target;
        bass_pl = 1.0f;
    }
    for (j = 0; j < n; j++) {
        float x, p1 = ph - (float)(int)ph;
        switch (snd) {
        case 1:
            x = tri(p1) * 0.8f + blep_saw(p1, inc) * 0.25f + sine(ph * 0.5f + 0.25f) * 0.4f;
            l0 += c * (x - l0);
            l1 += c * (l0 - l1);
            x = l1;
            break;
        case 2:
            x = sine(ph) * 0.8f + sine(ph * 2.0f) * (0.3f - 0.2f * warm) + sine(ph * 3.0f) * (0.1f - 0.08f * warm) +
                sine(ph * 0.5f + 0.25f) * 0.25f;
            break;
        case 3:
            x = sine(ph) * 0.6f + sine(ph * 1.5f) * 0.35f + sine(ph * 0.5f + 0.25f) * 0.45f + sine(ph * 2.0f) * 0.1f * (1.0f - warm);
            break;
        case 4: {
            float v1, v2, v3 = blep_saw(p1, inc) - bass_sv[1];
            v1 = bass_c[0] * bass_sv[0] + bass_c[1] * v3;
            v2 = bass_sv[1] + bass_c[1] * bass_sv[0] + bass_c[2] * v3;
            bass_sv[0] = 2.0f * v1 - bass_sv[0];
            bass_sv[1] = 2.0f * v2 - bass_sv[1];
            x = v2 * 0.9f + sine(ph * 0.5f + 0.25f) * 0.3f;
            break;
        }
        case 5:                                  /* Pluck: struck, then a soft hold */
            bass_pl *= 0.99995f;
            x = (sine(ph) * 0.8f + tri(p1) * 0.35f * bass_pl + sine(ph * 2.0f) * 0.25f * bass_pl) *
                    (0.6f + 0.4f * bass_pl) + sine(ph * 0.5f + 0.25f) * 0.35f;
            break;
        case 6:                                  /* Throb: it pulses in half notes */
            bass_th += tempo_cur * (1.0f / (120.0f * EN_SR));
            if (bass_th >= 1.0f)
                bass_th -= 1.0f;
            x = (sine(ph) * 0.75f + sine(ph * 0.5f + 0.25f) * 0.45f) * (0.55f - 0.45f * sine(bass_th + 0.25f));
            break;
        case 7:                                  /* Upright: plucked, woody, its overtones dying first */
            bass_pl *= 0.99992f;
            x = (sine(ph) * 0.8f + sine(ph * 2.0f) * 0.35f * bass_pl + sine(ph * 3.0f) * 0.15f * bass_pl * bass_pl) *
                    (0.5f + 0.5f * bass_pl) + sine(ph * 0.5f + 0.25f) * 0.2f;
            l0 += c * (x - l0);
            x = l0 * 0.7f + x * 0.3f;
            break;
        case 8:                                  /* FM: a DX bass, the modulator at the note, Warmth softening it */
            x = sine(ph + 1.0f + (0.06f + 0.22f * (1.0f - warm)) * (0.5f + 0.5f * bass_pl) * sine(ph)) * 0.8f +
                sine(ph * 0.5f + 0.25f) * 0.3f;
            bass_pl *= 0.99997f;
            break;
        case 9: {                                /* Reese: two saws, slowly beating, low-passed */
            float p2;
            bass_ph2 += inc * 1.006f;
            if (bass_ph2 >= 1.0f)
                bass_ph2 -= 1.0f;
            p2 = bass_ph2;
            x = (blep_saw(p1, inc) + blep_saw(p2, inc * 1.006f)) * 0.4f + sine(ph * 0.5f + 0.25f) * 0.3f;
            l0 += c * (x - l0);
            l1 += c * (l0 - l1);
            x = l1;
            break;
        }
        default:                                 /* AMB-1's, with Warmth's soft second harmonic */
            x = sine(ph) * 0.75f + sine(ph * 0.5f + 0.25f) * 0.45f + sine(ph * 2.0f) * (0.18f - 0.12f * warm);
            break;
        }
        ph += inc;
        if (ph >= 2.0f)                          /* two turns: the sub-octave's period */
            ph -= 2.0f;
        amp += (tgt - amp) * rate;
        x *= amp * g * breathe;
        mixl[j] += x;
        mixr[j] += x;
        rsend[j] += x * sp;
    }
    bass_lp[0] = fm_flush(l0);
    bass_lp[1] = fm_flush(l1);
    bass_sv[0] = fm_flush(bass_sv[0]);
    bass_sv[1] = fm_flush(bass_sv[1]);
    bass.ph = ph;
    bass.amp = amp;
    en_bass_level = amp;
    en_bass_pulse = amp * breathe;
}

static void render_bells(uint32_t n)
{
    int i;
    const float gb = bell_vol * 0.62f * pg[1], gfx = 0.62f * (0.1f + 0.95f * fx_lvl);
    for (i = 0; i < NBELL; i++) {
        bell_t *v = &bell[i];
        float cph = v->cph, mph = v->mph, amp = v->amp, idx = v->idx, att = v->att, c2 = v->c2ph, p2 = v->p2ph,
              p2a = v->p2amp, part = v->part, ar = v->arate, trem = 1.0f, g = v->fx ? gfx * v->fxg * fx_adapt : gb,
              rs = v->fx ? fx_space * 0.9f : 0.7f * bspace * 2.0f, es = v->fx ? fx_space * 0.4f : 0.85f * bspace * 2.0f;
        uint32_t j = 0;
        if (!v->on)
            continue;
        if ((cpu_now >= 80 || fx_room) && i >= (fx_room && cpu_now >= 80 ? 4 : 5) && !v->fx) {   /* CPU high, or an effect: the extra voices fade */
            v->amp *= 0.85f;
            if (v->amp < 1e-4f) {
                v->on = 0;
                continue;
            }
            amp = v->amp;
        }
        if (v->wait > 0) {
            if ((uint32_t)v->wait >= n) {
                v->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)v->wait;
            v->wait = 0;
        }
        if (v->tdep > 0.0f) {                   /* the vibraphone's motor: 5.5 Hz, a chunk at a time */
            v->tph += 5.5f * (float)n * (1.0f / EN_SR);
            if (v->tph >= 1.0f)
                v->tph -= 1.0f;
            trem = 1.0f - v->tdep * (0.5f + 0.5f * sine(v->tph));
        }
        {                                        /* (1.2: what has died away is not drawn: a tail is one sine) */
            const int fm = idx > 0.008f, pt = p2a > 0.0015f;   /* (both ~30 dB down by then) */
            if (!fm)
                idx = 0.0f;
            if (!pt)
                p2a = 0.0f;
        for (; j < n; j++) {
            float m = fm ? sine(mph) * idx : 0.0f, x;
            x = sine(cph + m + 4.0f);            /* + 4 turns: positive for the table */
            if (part > 0.0f)
                x = (x + part * sine(c2 + m + 4.0f)) * (1.0f / (1.0f + 0.5f * part));
            x = (x * amp + (pt ? sine(p2) * p2a : 0.0f)) * att * g * trem;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * rs;
            esend[j] += x * es;
            cph += v->cinc;
            mph += v->minc;
            c2 += v->c2inc;
            p2 += v->p2inc;
            if (cph >= 1.0f)
                cph -= 1.0f;
            if (mph >= 1.0f)
                mph -= 1.0f;
            if (c2 >= 1.0f)
                c2 -= 1.0f;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            amp *= v->adec;
            p2a *= v->p2dec;
            idx *= v->idec;
            att += (1.0f - att) * ar;            /* the strike: a few ms, or soft as a slowed piano */
        }
        }
        v->cph = cph;
        v->mph = mph;
        v->c2ph = c2;
        v->p2ph = p2;
        v->amp = amp;
        v->p2amp = p2a;
        v->idx = idx;
        v->att = att;
        if (amp < 0.0008f && p2a < 0.0008f) {
            v->on = 0;
            midi(0x81, v->note, 0);
        }
    }
}

/* a hat: the metal and noise, high-passed (Chamberlin SVF, ~4 to 10 kHz by Hats); no pitch, little echo */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = 0.5f * in + 0.6f * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += hat_f * hp;
        x2 += hat_f * x1;
        x = hp * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.2f;
        esend[j] += x * 0.08f;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

static void render_kit(drum_t *v, uint32_t n, float g)
{
    uint32_t j;
    float ph = v->ph, p2 = v->p2, inc = v->inc, amp = v->amp, att = v->att, s0 = v->x1, s1 = v->x2;
    const float rs = kspace * 0.5f, es = kspace * 0.2f;
    if (!v->on)
        return;
    for (j = 0; j < n; j++) {
        float x = v->l1 > 0.0f ? sine(ph) * v->l1 : 0.0f;
        if (v->l2 > 0.0f) {
            x += sine(p2) * v->l2;
            p2 += v->i2;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
        }
        if (v->nl > 0.0f) {
            float w, v1, v2, v3;
            noise = noise * 1664525u + 1013904223u;
            w = (float)(int32_t)noise * (1.0f / 2147483648.0f);
            v3 = w - s1;
            v1 = v->nc[0] * s0 + v->nc[1] * v3;
            v2 = s1 + v->nc[1] * s0 + v->nc[2] * v3;
            s0 = 2.0f * v1 - s0;
            s1 = 2.0f * v2 - s1;
            x += v1 * v->nl;
        }
        ph += inc;
        if (ph >= 1.0f)
            ph -= 1.0f;
        inc += (v->inc_end - inc) * 0.0012f;
        x *= amp * att * g;
        mixl[j] += x * v->gl;
        mixr[j] += x * v->gr;
        rsend[j] += x * rs;
        esend[j] += x * es;
        amp *= v->adec;
        att += (1.0f - att) * v->ar;
    }
    v->ph = ph;
    v->p2 = p2;
    v->inc = inc;
    v->amp = amp;
    v->att = att;
    v->x1 = fm_flush(s0);
    v->x2 = fm_flush(s1);
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 4.2f * pg[3];   /* heard, under the rest (it was 20 dB down) */
    drum_t *v;
    if (par[P_KIT]) {
        /* each kit as loud as the Soft one (1.5, measured: Brush and Wood were 12 dB under it, Lo-fi's quiet pulse) */
        static const float KIT_LVL[EN_NKIT] = {1.0f, 3.6f, 5.5f, 1.15f, 1.6f, 21.0f, 1.5f, 1.5f, 3.8f, 1.75f, 1.3f, 2.0f, 2.4f};
        const float kg = KIT_LVL[(unsigned)par[P_KIT] < EN_NKIT ? par[P_KIT] : 0];
        render_kit(&drum[0], n, g * (par[P_KIT] == 3 ? 0.55f : 0.8f) * kg);
        render_kit(&drum[1], n, g * 0.35f * kg);
        render_kit(&drum[2], n, g * 0.28f * kg);
        return;
    }
    v = &drum[0];                                /* thump: a sine falling to its note */
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * amp * att * g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * 0.25f * kspace;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.0012f;
            amp *= v->adec;
            att += (1.0f - att) * 0.02f;
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->on = amp > 0.0005f;
    }
    render_hat(&drum[1], n, g * 0.2f, 1.0f - 0.4f * kwidth, 1.0f);    /* closed: a little right */
    render_hat(&drum[2], n, g * 0.14f, 1.0f, 1.0f - 0.3f * kwidth);   /* open: a little left */
}

/* ----------------------------------------------------------------- air --- */
/* a quiet texture under it all: tape hiss, a record's crackle, wind, the sea, rain. Tone brightens it, Movement
 * quickens its gusts, waves and drops */
static struct {
    float s[2][2], c[2][4], lp[2], lp2[2], hp[2], ph[4], crk, crkf, dropa[2], ck[2], c28;
    int32_t cb[2], cw[2];
    uint32_t n1, n2;
} air = {.n1 = 12345u, .n2 = 98765u};
static const float AIR_RATE[4] = {0.071f, 0.053f, 0.097f, 0.083f};   /* its slow movers, Hz (times Movement) */

static void render_air(uint32_t n)
{
    const int snd = par[P_AIRSND];
    /* each sound about as loud as the others (1.5, measured on the speaker): Hiss, Vinyl, Wind, Sea, Rain, Fire, Night,
     * Stream */
    static const float AIR_LVL[EN_NAIRSND] = {2.4f, 2.0f, 1.0f, 1.5f, 1.8f, 1.0f, 1.3f, 1.6f, 1.0f, 0.6f};
    static float room = 1.0f;                    /* an effect playing: the air steps back for it */
    const float g = air_vol * 0.22f * pg[4] * AIR_LVL[(unsigned)snd < EN_NAIRSND ? snd : 0] *
                    (room += ((fx_room ? 0.35f : 1.0f) - room) * fm_minf((float)n / (0.5f * EN_SR), 1.0f)),
                sm = 0.3f + 1.4f * air_mv, t = air_tone;
    float m[4], lvl[2] = {1.0f, 1.0f}, c1, c2;
    uint32_t j;
    int k;
    if (g < 1e-6f)
        return;
    for (k = 0; k < 4; k++) {
        air.ph[k] += (float)n * AIR_RATE[k] * sm * (1.0f / EN_SR);
        if (air.ph[k] >= 1.0f)
            air.ph[k] -= 1.0f;
        m[k] = sine(air.ph[k]);
    }
    if (snd == 5) {                              /* leaves: their band, Tone brightening it */
        for (k = 0; k < 2; k++)
            svf_coefs(air.c[k], (k ? 3400.0f : 2900.0f) * fm_exp2f(0.8f * t - 0.3f), 1.1f);
    } else if (snd == 9) {                       /* cosmos: a low band, resonant, drifting very slowly, the two apart */
        for (k = 0; k < 2; k++) {
            float d = 0.5f + 0.35f * m[k] + 0.15f * m[k + 2];
            svf_coefs(air.c[k], 240.0f * fm_exp2f(1.8f * d + 0.8f * t), 0.22f);
            lvl[k] = 0.5f + 0.5f * d;
        }
    } else if (snd == 2) {                       /* wind: a band that wanders, in gusts */
        for (k = 0; k < 2; k++) {
            float gust = 0.5f + 0.35f * m[k] + 0.15f * m[k + 2];
            svf_coefs(air.c[k], 260.0f * fm_exp2f(2.2f * gust + 1.2f * t), 0.7f);
            lvl[k] = 0.25f + 0.9f * gust * gust;
        }
    } else if (snd == 3) {                       /* the sea: waves, the two sides apart */
        for (k = 0; k < 2; k++) {
            float w = 0.5f - 0.5f * sine(air.ph[k] * 1.0f + 0.25f);
            w = w * w;
            lvl[k] = 0.15f + 0.85f * w;
            m[k] = w;
        }
    } else if (snd == 4) {
        for (k = 0; k < 2; k++)
            lvl[k] = 0.8f + 0.2f * m[k];
    } else {
        lvl[0] = lvl[1] = 0.85f + 0.15f * m[0];
    }

    c1 = 1.0f - fm_expf(-FM_TWO_PI * 2500.0f * fm_exp2f(1.6f * t - 0.4f) * (1.0f / EN_SR));   /* the bed's low-pass */
    for (j = 0; j < n; j++) {
        float x[2], w[2];
        if (snd == 6) {                          /* the crickets' wing beat, 28 a second */
            air.c28 += 28.0f / EN_SR;
            if (air.c28 >= 1.0f)
                air.c28 -= 1.0f;
        }
        air.n1 = air.n1 * 1664525u + 1013904223u;
        air.n2 = air.n2 * 22695477u + 1u;
        w[0] = (float)(int32_t)air.n1 * (1.0f / 2147483648.0f);
        w[1] = (float)(int32_t)air.n2 * (1.0f / 2147483648.0f);
        for (k = 0; k < 2; k++) {
            switch (snd) {
            case 8: {                            /* birds: a soft bed, a chirp now and then, each side its own bird */
                air.lp[k] += c1 * (w[k] - air.lp[k]);
                air.hp[k] += 0.15f * (air.lp[k] - air.hp[k]);
                x[k] = (air.lp[k] - air.hp[k]) * 0.05f;
                if (air.cb[k] > 0) {             /* a chirp: ~70 ms, a quick sweep down (or up), a sine's arch */
                    const float q = (float)air.cb[k] * (1.0f / (0.07f * EN_SR));
                    const float f = (k ? 3900.0f : 3300.0f) + (air.dropa[k] > 0.0f ? 1400.0f : -900.0f) * q;
                    air.ck[k] += f * (1.0f / EN_SR);
                    if (air.ck[k] >= 1.0f)
                        air.ck[k] -= 1.0f;
                    x[k] += sine(air.ck[k]) * sine(q * 0.5f) * 0.7f;
                    air.cb[k]--;
                } else if (--air.cw[k] <= 0) {
                    air.cb[k] = (int32_t)(0.07f * EN_SR);
                    air.dropa[k] = w[1 - k];                     /* down or up */
                    air.cw[k] = (int32_t)((0.08f + (w[k] > 0.3f ? 1.4f : 0.05f) * (0.5f + 0.5f * w[1 - k])) * EN_SR /
                                          (0.5f + air_mv));      /* a few in a run, then a rest */
                }
                break;
            }
            case 9:
            case 2: {
                float v1, v2, v3 = w[k] - air.s[k][1];
                v1 = air.c[k][0] * air.s[k][0] + air.c[k][1] * v3;
                v2 = air.s[k][1] + air.c[k][1] * air.s[k][0] + air.c[k][2] * v3;
                air.s[k][0] = 2.0f * v1 - air.s[k][0];
                air.s[k][1] = 2.0f * v2 - air.s[k][1];
                x[k] = v1 * 1.2f;
                break;
            }
            case 3:
                c2 = 0.01f + 0.08f * m[k] * (0.6f + t);
                air.lp[k] += c2 * (w[k] - air.lp[k]);
                air.lp2[k] += c2 * (air.lp[k] - air.lp2[k]);
                x[k] = air.lp2[k] * 3.5f;
                break;
            case 6: {                            /* night: crickets, a chirp burst every second or so, each side its own */
                air.lp[k] += c1 * (w[k] - air.lp[k]);
                air.hp[k] += 0.15f * (air.lp[k] - air.hp[k]);
                x[k] = (air.lp[k] - air.hp[k]) * 0.08f;
                if (air.cb[k] > 0) {
                    float g28 = 0.5f + 0.5f * sine(air.c28);
                    air.ck[k] += (k ? 4650.0f : 4310.0f) * (1.0f / EN_SR);
                    if (air.ck[k] >= 1.0f)
                        air.ck[k] -= 1.0f;
                    x[k] += sine(air.ck[k]) * g28 * g28 * 0.9f;
                    air.cb[k]--;
                } else if (--air.cw[k] <= 0) {
                    air.cb[k] = (int32_t)(0.12f * EN_SR + 0.08f * EN_SR * (0.5f + 0.5f * w[k]));
                    air.cw[k] = (int32_t)((0.7f + 0.9f * (0.5f + 0.5f * w[1 - k])) * EN_SR / (0.5f + air_mv));
                }
                break;
            }
            case 4:
            case 7: {                            /* rain: a soft bed, and drops ringing in a band; a stream: lower, many */
                float v1, v2, v3, imp = 0.0f;
                const int stream = snd == 7;
                air.lp[k] += c1 * (w[k] - air.lp[k]);
                if ((air.n1 >> 9 & 0x3FFFu) < (uint32_t)((stream ? 6.0f : 0.4f) + (stream ? 8.0f : 1.4f) * air_mv) &&
                    (k == 0 ? air.n2 & 1u : !(air.n2 & 1u))) {
                    if (stream)
                        svf_coefs(air.c[k], 300.0f + 1200.0f * (0.5f + 0.5f * w[1 - k]) * (0.6f + 0.6f * t), 0.12f);
                    else
                        svf_coefs(air.c[k], 1400.0f + 4200.0f * (0.5f + 0.5f * w[1 - k]) * (0.6f + 0.6f * t), 0.04f);
                    imp = (0.6f + 0.4f * w[k]) * (stream ? 1.1f : 1.0f);
                }
                v3 = imp - air.s[k][1];
                v1 = air.c[k][0] * air.s[k][0] + air.c[k][1] * v3;
                v2 = air.s[k][1] + air.c[k][1] * air.s[k][0] + air.c[k][2] * v3;
                air.s[k][0] = 2.0f * v1 - air.s[k][0];
                air.s[k][1] = 2.0f * v2 - air.s[k][1];
                x[k] = (air.lp[k] - air.hp[k]) * 0.35f + v1 * (stream ? 1.6f : 0.5f);
                air.hp[k] += 0.02f * (air.lp[k] - air.hp[k]);
                break;
            }
            case 5: {                            /* leaves (1.6, in Fire's place): a rustle, ~2 .. 5 kHz, in gusts, each
                                                  * side its own, little flurries in it */
                float gust = 0.35f + 0.65f * (0.5f + 0.5f * m[k]) * (0.5f + 0.5f * m[k + 2]), v1, v2, v3;
                if ((air.n2 >> 7 & 0x3FFu) < (uint32_t)(2.0f + 10.0f * air_mv * gust))
                    air.dropa[k] = 1.0f;         /* a flurry: a few ms brighter, louder */
                air.dropa[k] *= 0.9993f;
                v3 = w[k] - air.s[k][1];
                v1 = air.c[k][0] * air.s[k][0] + air.c[k][1] * v3;
                v2 = air.s[k][1] + air.c[k][1] * air.s[k][0] + air.c[k][2] * v3;
                air.s[k][0] = 2.0f * v1 - air.s[k][0];
                air.s[k][1] = 2.0f * v2 - air.s[k][1];
                x[k] = v1 * gust * gust * (0.6f + 1.4f * air.dropa[k]);
                break;
            }
            case 0: {                            /* drips (1.2, in Hiss's place): water in a cave, a dark hush, a drop now
                                                  * and then, each side its own, ringing up as drops do */
                air.lp[k] += c1 * 0.25f * (w[k] - air.lp[k]);
                x[k] = air.lp[k] * 0.12f;
                if (air.cb[k] > 0) {
                    const float q = 1.0f - (float)air.cb[k] * (1.0f / (0.08f * EN_SR));
                    air.ck[k] += air.dropa[k] * (1.0f + 0.7f * q) * (1.0f / EN_SR);
                    if (air.ck[k] >= 1.0f)
                        air.ck[k] -= 1.0f;
                    x[k] += sine(air.ck[k]) * (1.0f - q) * (1.0f - q) * fm_minf(q * 40.0f, 1.0f) * 0.8f;
                    air.cb[k]--;
                } else if (--air.cw[k] <= 0) {
                    air.cb[k] = (int32_t)(0.08f * EN_SR);
                    air.ck[k] = 0.0f;
                    air.dropa[k] = (700.0f + 1500.0f * (0.5f + 0.5f * w[1 - k])) * (0.7f + 0.6f * t);
                    air.cw[k] = (int32_t)((0.2f + 1.5f * (0.5f + 0.5f * w[k])) * EN_SR / (0.5f + air_mv));
                }
                break;
            }
            case 1: {                            /* frogs (1.2, in Vinyl's place): a night hush, croaks in twos and threes,
                                                  * each side its own frog */
                air.lp[k] += c1 * (w[k] - air.lp[k]);
                air.hp[k] += 0.15f * (air.lp[k] - air.hp[k]);
                x[k] = (air.lp[k] - air.hp[k]) * 0.04f;
                if (air.cb[k] > 0) {
                    const float q = 1.0f - (float)air.cb[k] * (1.0f / (0.2f * EN_SR));
                    float pz;
                    air.dropa[k] += (k ? 34.0f : 29.0f) * (1.0f / EN_SR);   /* the throat's pulses */
                    if (air.dropa[k] >= 1.0f)
                        air.dropa[k] -= 1.0f;
                    air.ck[k] += (k ? 520.0f : 430.0f) * (1.1f - 0.2f * q) * (0.8f + 0.4f * t) * (1.0f / EN_SR);
                    if (air.ck[k] >= 1.0f)
                        air.ck[k] -= 1.0f;
                    pz = 0.5f + 0.5f * sine(air.dropa[k]);
                    pz *= pz;
                    pz *= pz;
                    x[k] += (sine(air.ck[k]) + 0.4f * sine(2.0f * air.ck[k])) * pz * sine(q * 0.5f) * 0.8f;
                    air.cb[k]--;
                } else if (--air.cw[k] <= 0) {
                    air.cb[k] = (int32_t)(0.2f * EN_SR);
                    air.cw[k] = (int32_t)((w[k] > 0.1f ? 0.1f : 1.4f + 2.0f * (0.5f + 0.5f * w[1 - k])) * EN_SR /
                                          (0.5f + air_mv));
                }
                break;
            }
            default: {                           /* (a plain hush) */
                air.lp[k] += c1 * (w[k] - air.lp[k]);
                air.hp[k] += 0.15f * (air.lp[k] - air.hp[k]);
                x[k] = (air.lp[k] - air.hp[k]) * (snd == 1 ? 0.4f : 0.8f);
                break;
            }
            }
        }
        x[0] *= lvl[0] * g;
        x[1] *= lvl[1] * g;
        mixl[j] += x[0];
        mixr[j] += x[1];
        rsend[j] += (x[0] + x[1]) * 0.25f;
    }
    for (k = 0; k < 2; k++) {
        air.s[k][0] = fm_flush(air.s[k][0]);
        air.s[k][1] = fm_flush(air.s[k][1]);
        air.lp[k] = fm_flush(air.lp[k]);
        air.lp2[k] = fm_flush(air.lp2[k]);
        air.hp[k] = fm_flush(air.hp[k]);
    }
}

/* ------------------------------------------------------------- shimmer --- */
/* the plate's tail, an octave up, fed back into it: each time round the shimmer climbs, and fades. Two heads on a
 * short loop, each faded in and out on a half sine, read at twice the speed they are written */
#define SHN 2048
static int16_t shbuf[SHN] EN_POOL;              /* 16-bit */
static int sh_w, sh_d = SHN - 1;
static float sh_hp, sh_lp, sh_y;
static inline float shimmer(float x)
{
    int da = sh_d, db = sh_d + SHN / 2, ia, ib;
    float y;
    if (db >= SHN)
        db -= SHN;
    {
        float q = x * 16000.0f;
        shbuf[sh_w] = (int16_t)(q > 32767.0f ? 32767.0f : q < -32767.0f ? -32767.0f : q);
    }
    ia = sh_w - da;
    ib = sh_w - db;
    if (ia < 0)
        ia += SHN;
    if (ib < 0)
        ib += SHN;
    {                                            /* the half-sine fades, read from the table straight (index da / 4) */
        const float wa = sint[da >> 2] + (sint[(da >> 2) + 1] - sint[da >> 2]) * (float)(da & 3) * 0.25f,
                    wb = sint[db >> 2] + (sint[(db >> 2) + 1] - sint[db >> 2]) * (float)(db & 3) * 0.25f;
        y = ((float)shbuf[ia] * wa + (float)shbuf[ib] * wb) * (1.0f / 16000.0f);
    }
    if (++sh_w >= SHN)
        sh_w = 0;
    if (--sh_d <= 0)
        sh_d = SHN - 1;
    sh_lp += 0.45f * (y - sh_lp);                /* soft above ~5 kHz, nothing under ~250 Hz */
    sh_hp += 0.035f * (sh_lp - sh_hp);
    return fm_flush(sh_lp - sh_hp);
}

/* ------------------------------------------------------------------ lead --- */
/* A melody of its own, now and then: phrases from the scale over the chord, with rests between. It remembers its last
 * phrase and often answers it (the same rhythm, moved, its ending changed), so it sounds written rather than random.
 * Six ways: Wander (free, slow), Answer (a call, then its answer), Arpeggio (up the chord), Ostinato (a short figure
 * again and again, a note changing), Hymn (long notes, stepwise), Sparse (a note or three, much space). One voice,
 * sixteen sounds: a breathy sax, a flute, a muted trumpet, a clarinet, a whistle, a glass lead (FM), a soft synth lead
 * (as a CS-80's, swelling open), an ocarina; and (1.5) a harmonica, a pan flute, an oboe, a theremin, a flugelhorn,
 * a bansuri, a voice singing "ooh" and a cello. Each: an oscillator (render_lead), a body low-pass that opens as it is
 * blown, a formant band over it; the table says how much of each is heard, and how far a note slides up into place. */
static const struct {
    float att, rel, glide;                       /* seconds */
    int8_t reg;                                  /* its register: the root at or below this note, and up */
    float vib, vrate, breath, cut, cut_env, form, lvl;
    float q1, q2, lp_m, bp_m, fm_m, form_env, slide;   /* the filters' damping; the mix: body, its band, the formant;
                                                         * the formant's sweep with the breath; the slide (semitones) */
} LEADSND[EN_NLEADSND] = {
    {0.06f, 0.18f, 0.06f, 62, 0.35f, 5.2f, 0.10f, 900.0f, 2.2f, 1350.0f, 1.0f, 0.9f, 0.5f, 1.0f, 0.0f, 0.6f, 0.0f, 0.0f},   /* Sax */
    {0.05f, 0.20f, 0.04f, 72, 0.25f, 5.0f, 0.16f, 3200.0f, 0.4f, 2600.0f, 1.0f, 0.9f, 0.5f, 1.0f, 0.0f, 0.3f, 0.0f, 0.0f},  /* Flute */
    {0.03f, 0.14f, 0.03f, 64, 0.15f, 5.5f, 0.04f, 1300.0f, 1.2f, 1700.0f, 0.9f, 0.9f, 0.25f, 0.25f, 0.0f, 1.5f, 0.0f, 0.0f}, /* Trumpet */
    {0.04f, 0.16f, 0.03f, 60, 0.12f, 5.0f, 0.04f, 1000.0f, 1.6f, 1500.0f, 1.0f, 0.9f, 0.5f, 1.0f, 0.0f, 0.3f, 0.0f, 0.0f},  /* Clarinet */
    {0.04f, 0.15f, 0.09f, 79, 0.45f, 5.8f, 0.05f, 6000.0f, 0.0f, 3000.0f, 0.9f, 0.9f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},  /* Whistle */
    {0.01f, 0.60f, 0.02f, 72, 0.12f, 4.5f, 0.00f, 6000.0f, 0.0f, 3000.0f, 1.0f, 0.9f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},  /* Glass */
    {0.30f, 0.80f, 0.12f, 64, 0.30f, 4.8f, 0.00f, 450.0f, 3.0f, 1500.0f, 1.5f, 0.45f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, /* Synth */
    {0.06f, 0.25f, 0.05f, 72, 0.18f, 4.6f, 0.07f, 2600.0f, 0.3f, 2000.0f, 1.0f, 0.9f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},  /* Ocarina */
    /* Harmonica: a reed's narrow pulse, a cupped hand's wah (the formant opens with the breath), a bend up into notes */
    {0.03f, 0.15f, 0.07f, 67, 0.22f, 5.6f, 0.05f, 1800.0f, 1.0f, 1100.0f, 1.0f, 0.9f, 0.35f, 0.4f, 0.0f, 1.2f, 1.1f, 1.0f},
    /* Pan flute: hollow (odd partials), much breath, a chiff */
    {0.04f, 0.22f, 0.03f, 69, 0.15f, 4.8f, 0.24f, 3000.0f, 0.4f, 2200.0f, 1.0f, 0.9f, 0.6f, 1.0f, 0.0f, 0.35f, 0.0f, 0.0f},
    /* Oboe: a narrower reed, nasal: a bright formant */
    {0.04f, 0.15f, 0.03f, 64, 0.18f, 5.3f, 0.03f, 1500.0f, 1.3f, 1550.0f, 0.68f, 0.9f, 0.3f, 0.35f, 0.0f, 1.3f, 0.0f, 0.0f},
    /* Theremin: a near sine, wide vibrato, long glides */
    {0.10f, 0.35f, 0.28f, 70, 0.70f, 6.2f, 0.00f, 5000.0f, 0.0f, 3000.0f, 1.0f, 0.9f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    /* Flugelhorn: soft brass, round, opening slowly */
    {0.08f, 0.22f, 0.05f, 60, 0.18f, 5.0f, 0.03f, 650.0f, 1.6f, 900.0f, 1.7f, 0.8f, 0.6f, 1.0f, 0.0f, 0.35f, 0.0f, 0.0f},
    /* Square (1.0, in the Bansuri's place): a soft square, a detuned saw under it, warm and round, no breath */
    {0.02f, 0.25f, 0.05f, 67, 0.20f, 5.0f, 0.00f, 1100.0f, 1.2f, 1500.0f, 1.0f, 0.8f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    /* Voice: a soft "ooh", two formants (~350 and ~800 Hz) */
    {0.12f, 0.35f, 0.06f, 62, 0.30f, 5.0f, 0.02f, 360.0f, 0.4f, 820.0f, 0.52f, 0.35f, 0.4f, 0.15f, 1.0f, 0.55f, 0.0f, 0.0f},
    /* Cello: a bowed string, low and singing, bow noise, a wooden body */
    {0.10f, 0.30f, 0.05f, 48, 0.24f, 5.2f, 0.05f, 1600.0f, 0.8f, 420.0f, 1.1f, 0.8f, 0.45f, 1.0f, 0.0f, 0.5f, 0.0f, 0.0f},
};
volatile uint8_t en_lead_note;
volatile float en_lead_glow;
volatile uint32_t en_lead_notes;

static int lead_note(int off)                   /* a step of the scale, over the chord, in the sound's register */
{
    const int reg = LEADSND[par[P_LEADSND]].reg + 12 * par[P_LOCT], root = par[P_ROOT];
    int d = deg + off, oct, n;
    while (d < 0)
        d += nsteps;
    oct = d / nsteps;
    n = reg - ((reg - root) % 12 + 12) % 12 + steps[d % nsteps] + 12 * oct;
    while (n > reg + 19)
        n -= 12;
    while (n < reg - 7)
        n += 12;
    return n;
}

static int near_chord(int o)                    /* the chord tone (0, 2, 4 steps over the degree) nearest o */
{
    int best = 0, bd = 99, k, c;
    for (k = -1; k <= 2; k++)
        for (c = 0; c <= 4; c += 2) {
            int t = c + k * nsteps, dd = t > o ? t - o : o - t;
            if (dd < bd) {
                bd = dd;
                best = t;
            }
        }
    return best;
}

static void lead_phrase(void)                   /* a new phrase, in the lead's way */
{
    const int st = par[P_LSTYLE];
    int k, o, n = 0;
    if (st == 1 && ld.answer && ld.m_n) {        /* the answer: the call's rhythm, moved, home at its end */
        const int sh = irand(2) ? 1 : -1;
        for (k = 0; k < ld.m_n; k++) {
            ld.off[k] = (int8_t)(ld.m_off[k] + sh * (k > 0));
            ld.dur[k] = ld.m_dur[k];
        }
        n = ld.m_n;
        if (n > 0)
            ld.off[n - 1] = (int8_t)near_chord(irand(2) ? 0 : nsteps);
        if (n > 0)
            ld.dur[n - 1] = (uint8_t)(ld.dur[n - 1] + 4);
        ld.answer = 0;
    } else if (st != 1 && ld.m_n && frand() < 0.35f) {   /* the last phrase again, moved: a sequence */
        const int sh = irand(3) - 1;
        for (k = 0; k < ld.m_n; k++) {
            ld.off[k] = (int8_t)(ld.m_off[k] + sh);
            ld.dur[k] = ld.m_dur[k];
        }
        n = ld.m_n;
        if (frand() < 0.6f)                      /* its ending, changed */
            if (n > 0)
                ld.off[n - 1] = (int8_t)near_chord(ld.off[n - 1] + irand(5) - 2);
    } else {
        switch (st) {
        case 2: {                                /* Arpeggio: up (and down) the chord, in eighths */
            static const int8_t T[4] = {0, 2, 4, 7};
            const int len = 6 + irand(7), base = irand(2) ? 0 : -nsteps, ud = irand(2), t7 = nsteps > 5 ? 6 : 4;
            for (k = 0; k < len; k++) {
                int q = ud ? (k % 6 < 4 ? k % 6 : 6 - k % 6) : k % 4;
                o = T[q] == 7 ? nsteps : T[q];
                if (T[q] == 7 && irand(3) == 0)
                    o = t7;
                ld.off[n] = (int8_t)(o + base + nsteps * (k / 8));
                ld.dur[n++] = 2;
            }
            if (n > 0)
                ld.dur[n - 1] = 6;
            break;
        }
        case 3: {                                /* Ostinato: a figure, again, a note changing each time */
            int8_t f[4];
            const int fl = 3 + irand(2), reps = 3 + irand(2);
            o = near_chord(irand(5));
            for (k = 0; k < fl; k++) {
                f[k] = (int8_t)o;
                o += irand(5) - 2;
            }
            for (o = 0; o < reps && n + fl <= LN_MAX; o++) {
                if (o)
                    f[irand(fl)] += (int8_t)(irand(3) - 1);
                for (k = 0; k < fl; k++) {
                    ld.off[n] = f[k];
                    ld.dur[n++] = (uint8_t)(k == fl - 1 ? 3 : 2);
                }
            }
            break;
        }
        case 4: {                                /* Hymn: long notes, stepwise */
            const int len = 3 + irand(3);
            o = near_chord(irand(5));
            for (k = 0; k < len; k++) {
                ld.off[n] = (int8_t)o;
                ld.dur[n++] = (uint8_t)(8 + 4 * irand(3));
                o += irand(2) ? (irand(2) ? 1 : -1) : 0;
            }
            if (n > 0)
                ld.off[n - 1] = (int8_t)near_chord(ld.off[n - 1]);
            if (n > 0)
                ld.dur[n - 1] = 16;
            break;
        }
        case 5: {                                /* Sparse: a note or three */
            const int len = 1 + irand(3);
            for (k = 0; k < len; k++) {
                ld.off[n] = (int8_t)near_chord(irand(nsteps + 3));
                ld.dur[n++] = (uint8_t)(6 + 2 * irand(4));
            }
            break;
        }
        default: {                               /* Wander (and Answer's call): a walk, home to a chord tone */
            static const uint8_t D[8] = {2, 2, 3, 4, 4, 6, 6, 8};
            const int len = st == 1 ? 3 + irand(3) : 4 + irand(5);
            static const int8_t W[7] = {-2, -1, -1, 1, 1, 2, 0};
            o = near_chord(irand(5));
            for (k = 0; k < len; k++) {
                ld.off[n] = (int8_t)o;
                ld.dur[n++] = D[irand(8)];
                o += W[irand(7)];
                o = o < -3 ? -2 : o > nsteps + 5 ? nsteps + 3 : o;
            }
            if (n > 0)
                ld.off[n - 1] = (int8_t)near_chord(ld.off[n - 1]);
            if (n > 0)
                ld.dur[n - 1] = (uint8_t)(8 + 4 * irand(2));
            if (st == 1)
                ld.answer = 1;
            break;
        }
        }
    }
    ld.n = n;
    ld.i = 0;
    for (k = 0; k < n; k++) {
        ld.m_off[k] = ld.off[k];
        ld.m_dur[k] = ld.dur[k];
    }
    ld.m_n = n;
}

/* Style Keys: the keys play the lead, one voice, the last key held sounding (when it is let go, the one before) */
static int8_t lk_held[8];
static int lk_n;
static void lead_key(int note, int on)
{
    int k;
    if (note < 0 || note > 127) {                /* all up (-1 arrives as 255: the queue's 8 bits) */
        lk_n = 0;
        ld.gate = 0;
        return;
    }
    for (k = 0; k < lk_n; k++)                   /* out of the held list (a key pressed again goes to the top) */
        if (lk_held[k] == note) {
            for (; k < lk_n - 1; k++)
                lk_held[k] = lk_held[k + 1];
            lk_n--;
            break;
        }
    if (on) {
        if (lk_n >= 8) {
            for (k = 0; k < 7; k++)
                lk_held[k] = lk_held[k + 1];
            lk_n = 7;
        }
        lk_held[lk_n++] = (int8_t)note;
    }
    if (!lk_n) {
        ld.gate = 0;                             /* the last let go: it fades as its sound does */
        return;
    }
    {
        const int nn = lk_held[lk_n - 1] + 12 * par[P_LOCT];
        const float f = mtof((float)nn);
        if (!ld.gate || ld.env < 0.05f || par[P_LEADSND] == 5 || par[P_LGLIDE] <= 0) {
            ld.f = f;                            /* a new breath (or no glide: straight there) */
            ld.t = 0.0f;
            ld.ph[1] = 0.0f;
        }
        if (on && !ld.gate && LEADSND[par[P_LEADSND]].slide > 0.0f && par[P_LGLIDE] >= 30)
            ld.f = f * fm_exp2f(-LEADSND[par[P_LEADSND]].slide * fm_minf((float)par[P_LGLIDE] * 0.02f, 1.0f) * (1.0f / 12.0f));
        ld.f_t = f;
        ld.gate = 1;
        ld.vel = 0.9f;
        ld.gate_left = 1e9f;                     /* held: as long as the key */
        ld.i = ld.n = 0;
        en_lead_note = (uint8_t)nn;
        en_lead_notes++;
    }
}

static void lead_tick(void)                     /* each sixteenth */
{
    if (par[P_KEYS] == EN_KEYS_PLAY) {           /* the keys play it: no phrases of its own */
        ld.i = ld.n;
        return;
    }
    const float often = (float)par[P_LOFTEN] * 0.01f;
    const int st = par[P_LSTYLE];
    int forced = 0;
    if (lead_now && ld.i >= ld.n) {              /* touched by hand: the next phrase now, not after its rest */
        lead_now = 0;
        ld.wait = 1;
        ld.m_n = 0;
        forced = 1;
    }
    if (lead_vol <= 0.0f && !ld.solo && ld.solo_g <= 0.0f)
        ld.i = ld.n;                             /* turned off: the phrase stops too */
    if (--ld.wait > 0)
        return;
    if (ld.i >= ld.n) {                          /* between phrases: a new one, or a while longer */
        const int live = (lead_vol * pg[5] > 1e-4f || ld.solo) && en_playing && !en_frozen && nsteps && shed_left <= 0.0f &&
                         cpu_now < 92;
        if (live && (ld.solo || forced || frand() < 0.25f + 0.75f * often)) {
            lead_phrase();
            if (ld.solo)
                ld.solo--;                       /* Solo: two phrases (a call and its answer) */
        } else {
            ld.wait = 8;
            return;
        }
    }
    {
        const int nn = lead_note(ld.off[ld.i]);
        const float f = mtof((float)nn);
        if (!ld.gate || ld.env < 0.05f || par[P_LEADSND] == 5)
            ld.f = f;                            /* a new breath; the glass strikes each note */
        if (LEADSND[par[P_LEADSND]].slide > 0.0f && par[P_LGLIDE] >= 30 && (ld.i == 0 || frand() < 0.35f))
            ld.f = f * fm_exp2f(-LEADSND[par[P_LEADSND]].slide * fm_minf((float)par[P_LGLIDE] * 0.02f, 1.0f) *
                                (1.0f / 12.0f));         /* a bend up into the note (Glide 30 and over) */
        ld.f_t = f;
        if (!ld.gate || par[P_LEADSND] == 5 || ld.i == 0) {
            ld.t = 0.0f;
            ld.ph[1] = 0.0f;
        }
        ld.gate = 1;
        ld.vel = (ld.i == 0 ? 0.95f : 0.7f) + 0.25f * frand();
        ld.wait = ld.dur[ld.i];
        ld.gate_left = (float)ld.wait * step_len * (st == 2 ? 0.7f : 0.93f);
        en_lead_note = (uint8_t)nn;
        en_lead_notes++;
        if (++ld.i >= ld.n) {                    /* the phrase ends: its rest */
            float rest = (8.0f + 140.0f * (1.0f - often) * (1.0f - often)) * (0.6f + 0.8f * frand()) / fm_maxf(part_dens, 0.3f);
            if (st == 5)
                rest *= 1.8f;
            if (st == 1 && ld.answer)
                rest = 4.0f + 4.0f * frand();    /* the answer comes soon */
            ld.wait += (int)rest;
        }
    }
}

static void render_lead(uint32_t n)
{
    const int snd = par[P_LEADSND];
    const float g = fm_maxf(lead_vol * (par[P_KEYS] == EN_KEYS_PLAY ? 0.9f + 0.55f * fx_adapt : pg[5]),
                            ld.solo_g * fx_adapt) * 0.6f   /* played from the keys: lifted as the music is louder */ * LEADSND[snd].lvl, dt = 1.0f / EN_SR,
                ar = dt / (LEADSND[snd].att * fm_exp2f((float)(par[P_LATT] - 50) * 0.04f)),   /* Attack: 1/4 .. 4x */
                rr = dt / (LEADSND[snd].rel * fm_exp2f((float)(par[P_LREL] - 50) * 0.04f)),   /* Release */ gk = par[P_LGLIDE] <= 0 ? 1.0f   /* Glide: 0 none, 50 the sound's own, 100 twice */
                   : fm_minf(16.0f * dt / (LEADSND[snd].glide * (float)par[P_LGLIDE] * 0.02f), 1.0f),
                vd = LEADSND[snd].vib * lead_expr * 2.0f, br = LEADSND[snd].breath * (0.5f + lead_expr),
                rs = lead_space * 1.1f, es = lead_space * 0.6f;
    uint32_t j;
    if (g < 1e-6f || (!ld.gate && ld.env < 1e-5f)) {
        ld.env = 0.0f;
        en_lead_glow = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float x = 0.0f, f, e, w, v1, v2, v3, vib, inc;
        if (ld.gate) {
            ld.env += (1.0f - ld.env) * ar * 3.0f;
            if ((ld.gate_left -= 1.0f) <= 0.0f)
                ld.gate = 0;
        } else {
            ld.env -= ld.env * rr * 3.0f;
        }
        e = ld.env * ld.vel;
        ld.t += dt;
        if (!(j & 15u))
            ld.f += (ld.f_t - ld.f) * gk;
        ld.vib_ph += LEADSND[snd].vrate * dt;
        if (ld.vib_ph >= 1.0f)
            ld.vib_ph -= 1.0f;
        vib = sine(ld.vib_ph) * vd * fm_minf(ld.t * 1.5f, 1.0f);   /* the vibrato comes in late */
        f = ld.f * (1.0f + 0.0578f * vib);   /* vib in semitones */
        inc = f * dt;
        noise = noise * 1664525u + 1013904223u;
        w = (float)(int32_t)noise * (1.0f / 2147483648.0f);
        if (!(j & 31u)) {
            float cf = LEADSND[snd].cut * fm_exp2f(LEADSND[snd].cut_env * e + 1.6f * (lead_tone - 0.5f));
            svf_coefs(ld.c, cf, LEADSND[snd].q1);
            svf_coefs(ld.c2, LEADSND[snd].form * fm_exp2f(0.6f * (lead_tone - 0.5f) + LEADSND[snd].form_env * (e - 0.5f)),
                      LEADSND[snd].q2);
        }
        switch (snd) {
        case 0:                                  /* Sax: a reed's buzz, brighter as it is blown harder, breath, a growl */
            x = blep_saw(ld.ph[0], inc) * 0.7f - blep_saw(ld.ph[1], inc) * 0.45f;
            x *= 1.0f + 0.25f * lead_expr * sine(ld.gr_ph) * fm_expf(-ld.t * 3.0f);
            ld.gr_ph += 31.0f * dt;
            if (ld.gr_ph >= 1.0f)
                ld.gr_ph -= 1.0f;
            break;
        case 1:                                  /* Flute: near a sine, breath, a chiff at the start */
            x = sine(ld.ph[0]) + 0.18f * sine(2.0f * ld.ph[0]) + w * 0.5f * fm_expf(-ld.t * 30.0f);
            break;
        case 2: x = blep_saw(ld.ph[0], inc); break;                       /* Trumpet: through its mute (below) */
        case 3: x = (blep_saw(ld.ph[0], inc) - blep_saw(ld.ph[1], inc)) * 0.7f; break;   /* Clarinet: odd harmonics */
        case 4: x = sine(ld.ph[0]); break;                                /* Whistle */
        case 5: {                                /* Glass: FM, bright at the strike, a bell's sustain */
            float idx = 0.25f + 1.1f * fm_expf(-ld.t * 2.5f);
            x = sine(ld.ph[0] + 1.0f + idx * sine(ld.ph[2]) * 0.16f);   /* (+1: never below 0) */
            break;
        }
        case 6: x = (blep_saw(ld.ph[0], inc) + blep_saw(ld.ph[2], inc * 1.004f)) * 0.5f; break;   /* Synth */
        case 7: x = sine(ld.ph[0]) + 0.12f * sine(3.0f * ld.ph[0]); break;   /* Ocarina */
        case 8: {                                /* Harmonica: a narrow pulse (a reed), its buzz */
            float q = ld.ph[0] + 0.78f;
            q -= q >= 1.0f ? 1.0f : 0.0f;
            x = (blep_saw(ld.ph[0], inc) - blep_saw(q, inc)) * 0.8f;
            break;
        }
        case 9:                                  /* Pan flute: odd partials, hollow, a chiff */
            x = sine(ld.ph[0]) + 0.1f * sine(3.0f * ld.ph[0]) + w * 0.8f * fm_expf(-ld.t * 25.0f);
            break;
        case 10: {                               /* Oboe: a narrower pulse still */
            float q = ld.ph[0] + 0.86f;
            q -= q >= 1.0f ? 1.0f : 0.0f;
            x = (blep_saw(ld.ph[0], inc) - blep_saw(q, inc)) * 0.9f;
            break;
        }
        case 11: x = sine(ld.ph[0]) + 0.08f * sine(2.0f * ld.ph[0]); break;   /* Theremin */
        case 12: x = blep_saw(ld.ph[0], inc) * 0.8f + 0.3f * sine(ld.ph[0]); break;   /* Flugelhorn */
        case 13: x = (blep_saw(ld.ph[0], inc) - blep_saw(ld.ph[1], inc)) * 0.6f + 0.3f * blep_saw(ld.ph[2], inc * 1.004f);
            break;                               /* Square: a square (ph[1] is its other edge), a saw a little apart */
        case 14: x = choir_src(ld.ph[0], inc) * 0.8f; break;   /* Voice: a sung source, through its formants (below) */
        default: {                               /* Cello: a saw, its bow's grain */
            x = blep_saw(ld.ph[0], inc) + w * 0.12f * (0.5f + e);
            break;
        }
        }
        ld.ph[0] += inc;
        if (ld.ph[0] >= 1.0f)
            ld.ph[0] -= 1.0f;
        ld.ph[1] = ld.ph[0] + 0.5f;              /* the pulse's other edge */
        if (ld.ph[1] >= 1.0f)
            ld.ph[1] -= 1.0f;
        ld.ph[2] += snd == 5 ? inc * 3.0f : inc * 1.004f;
        if (ld.ph[2] >= 1.0f)
            ld.ph[2] -= 1.0f;
        ld.nl += 0.25f * (w - ld.nl);
        x += ld.nl * br * 4.0f * (0.4f + 0.6f * e);   /* breath */
        v3 = x - ld.s[1];                        /* the body: a low-pass that opens as it is blown */
        v1 = ld.c[0] * ld.s[0] + ld.c[1] * v3;
        v2 = ld.s[1] + ld.c[1] * ld.s[0] + ld.c[2] * v3;
        ld.s[0] = 2.0f * v1 - ld.s[0];
        ld.s[1] = 2.0f * v2 - ld.s[1];
        if (LEADSND[snd].fm_m > 0.0f) {          /* a formant over it (the reeds, the brass, the voice, the cello) */
            float b;
            v3 = x - ld.s2[1];
            b = ld.c2[0] * ld.s2[0] + ld.c2[1] * v3;
            v3 = ld.s2[1] + ld.c2[1] * ld.s2[0] + ld.c2[2] * v3;
            ld.s2[0] = 2.0f * b - ld.s2[0];
            ld.s2[1] = 2.0f * v3 - ld.s2[1];
            x = v2 * LEADSND[snd].lp_m + v1 * LEADSND[snd].bp_m + b * LEADSND[snd].fm_m;
        } else {
            x = v2 * LEADSND[snd].lp_m + v1 * LEADSND[snd].bp_m;
        }
        x *= e * g;
        mixl[j] += x * 0.78f;
        mixr[j] += x * 0.62f;
        rsend[j] += x * rs;
        esend[j] += x * es;
    }
    ld.s[0] = fm_flush(ld.s[0]);
    ld.s[1] = fm_flush(ld.s[1]);
    ld.s2[0] = fm_flush(ld.s2[0]);
    ld.s2[1] = fm_flush(ld.s2[1]);
    ld.env = fm_flush(ld.env);
    if (!ld.gate && ld.env < 0.01f && !ld.solo && ld.i >= ld.n)
        ld.solo_g = 0.0f;                        /* Solo's phrases over */
    en_lead_glow = ld.env;
}

/* ------------------------------------------------------------- effects --- */
/* The FX menu's 24 (as HADAL-1's, fewer and fitting): gestures that play once (a swell, a comet, a cascade of bells,
 * a gong) and textures that can be kept on (a drone, rain, a grain cloud, a tremolo). Six voices; a kept gesture comes
 * back every few seconds, a kept texture stays until let go. Some play sound of their own (render_fx), some move what
 * is there (fx_control: the swell, the tape stopping, the filter sweeping, the echo held). With By itself, the world's
 * own few play now and then. */
#define NFXV EN_FX_VOICES
enum { FX_GRAINS, FX_SNOW, FX_MBOX, FX_KALIMBA, FX_ARP, FX_CASCADE, FX_HARP, FX_CHIMES, FX_RAIN, FX_ECHON,
       FX_STRUM, FX_TWINKLE, FX_FIFTHS, FX_LULLABY, FX_WHIRL, FX_UNFOLD, FX_RIPPLE, FX_BOUNCE, FX_CALL, FX_GLIMMER,
       FX_RISE, FX_CLOCK, FX_STARS, FX_METEOR, FX_WINDHARP, FX_PEAL, FX_SOLO };
static const float FX_DUR[EN_NFX] = {
    7.0f, 6.0f, 3.4f, 2.4f, 3.4f, 1.6f, 1.4f, 5.0f, 5.0f, 4.0f, 2.6f, 3.0f, 2.2f, 5.6f, 2.6f, 4.8f, 2.6f, 3.2f, 3.4f,
    2.8f, 2.0f, 5.0f, 7.0f, 2.6f, 7.0f, 5.4f, 8.0f
};
/* each effect's level, measured alone, so each sounds about as loud as the others */
static const float FX_LVL[EN_NFX] = {2.0f, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                     1, 1, 2.8f, 1, 1, 1, 1};
static const uint8_t FX_TEXTURE[EN_NFX] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0
};
typedef struct {
    uint8_t on, type, kept, rel;
    float t, dur, env, gap;                      /* seconds in; its length; its level; a kept gesture's wait */
    float ph[4], inc[4], s[4], c[4], s2[4], c2[4];
    float a, b, pan, amp;
    int32_t n;
    float next;
} fxv_t;
static fxv_t fxv[NFXV];
static float fx_auto_left = 60.0f, fx_duck_t, fx_duck, mus_env;
#define MUS_REF 0.05f                            /* the music's level (rms, before the effects) where they need no lift */
/* the effects that play a sound of their own: the rest steps back for them (the bells' ones and Solo are in the mix) */
#define FX_AUDIBLE (((1u << EN_NFX) - 1u) & ~(1u << FX_SOLO))   /* (1.2: all but Solo, which is in the mix) */
volatile uint32_t en_fx_on, en_fx_kept, en_fx_count;
volatile int8_t en_fx_last = -1;

static void fx_tune(fxv_t *v) { (void)v; }       /* (1.2: the effects take their notes as they play) */

static void fx_begin(fxv_t *v)                   /* from the top (a kept gesture, again) */
{
    int k;
    v->t = 0.0f;
    v->n = 0;
    v->next = 0.0f;
    v->amp = 0.0f;
    v->a = v->b = 0.0f;
    for (k = 0; k < 4; k++)
        v->ph[k] = v->s[k] = v->s2[k] = 0.0f;
    v->pan = frand() * 0.8f - 0.4f;
    fx_tune(v);
}


static void fx_start(int i, int keep)
{
    int k, best = -1, kept = 0;
    float oldest = -1.0f;
    keep = 0;                                    /* (1.2: nothing is kept on any more: the CPU) */
    if (shed_hard && en_fx_costly(i))    /* only just after a real overload (1.2: never for "near full": the
                                                  * effects play in the bells' own voices, five while one plays) */
        return;
    for (k = 0; k < NFXV; k++) {
        kept += fxv[k].on && fxv[k].kept;
        if (fxv[k].on && fxv[k].type == i && !fxv[k].rel) {   /* already sounding: keep it, or start it again */
            fx_begin(&fxv[k]);
            return;
        }
    }
    for (k = 0; k < NFXV; k++) {
        if (!fxv[k].on) {
            best = k;
            break;
        }
        if (!fxv[k].kept && fxv[k].t > oldest) {
            oldest = fxv[k].t;
            best = k;
        }
    }
    if (best < 0)
        return;
    fxv[best].on = 1;
    fxv[best].type = (uint8_t)i;
    fxv[best].kept = (uint8_t)keep;
    fxv[best].rel = 0;
    fxv[best].dur = FX_DUR[i];
    fxv[best].env = 0.0f;
    fx_begin(&fxv[best]);
    en_fx_last = (int8_t)i;
    en_fx_count++;
}

static void fx_strike(int force, int l, int off, float vel, float g)   /* an effect's bell, at its level */
{
    bell_force = force;
    bell_fx_g = g;
    bell_voice(l, bell_note(l, off), vel, irand(400));
    bell_force = -1;
}


static uint8_t fx_bag[EN_NFX], fx_bag_n;
volatile uint32_t en_fx_auto_seen;               /* which effects Auto FX has played (tests) */
static int fx_bag_next(void)                     /* the next of a shuffled deck of all the effects */
{
    int k, f;
    if (fx_bag_n == 0) {
        const int last = en_fx_last;
        for (k = 0; k < EN_NFX; k++)
            fx_bag[k] = (uint8_t)k;
        for (k = EN_NFX - 1; k > 0; k--) {
            const int r = irand(k + 1);
            const uint8_t t = fx_bag[k];
            fx_bag[k] = fx_bag[r];
            fx_bag[r] = t;
        }
        if (fx_bag[EN_NFX - 1] == last) {        /* not the same one twice in a row across decks */
            fx_bag[EN_NFX - 1] = fx_bag[0];
            fx_bag[0] = (uint8_t)last;
        }
        fx_bag_n = EN_NFX;
    }
    f = fx_bag[--fx_bag_n];
    en_fx_auto_seen |= 1u << f;
    return f;
}
#define FXC(c) (((c) % 3) * 2 + ((c) / 3) * nsteps)  /* the chord's c-th tone, up the octaves (scale degrees) */
static float fx_beat(void) { return 60.0f / fm_maxf((float)par[P_TEMPO], 30.0f); }

static void fx_control(uint32_t n)               /* once a chunk: time, the envelopes, the global moves, events */
{
    const float dt = (float)n / EN_SR;
    uint32_t on = 0, kp = 0;
    int k;
    fx_sweep = fx_shim = fx_swell = fx_wob = fx_hold = fx_trem = fx_throw = 0.0f;
    fx_dive = fx_rsw = fx_frz = 0.0f;
    fx_tapef = 1.0f;
    strum_step(dt);
    if (overload) {                              /* the CPU ran out (1.2: gentler, shorter): first the music makes room (its
                                                  * quiet bells, the lead's phrase) and the effect plays on; only a second
                                                  * time, soon after, do the effects stop. Three seconds, not ten */
        int b;
        const int again = shed_left > 0.0f;
        overload = 0;
        if (!again)
            en_sheds++;
        shed_left = 3.0f;
        shed_hard = (uint8_t)again;
        if (again)
            for (k = 0; k < NFXV; k++)
                fxv[k].on = 0;
        ld.gate = 0;
        ld.i = ld.n;
        ld.solo = 0;
        ld.env *= 0.5f;
        for (b = 0; b < NBELL; b++)              /* the music's quietest bells (and, the second time, the effects') */
            if (bell[b].on && (bell[b].fx ? again : bell[b].amp < 0.05f))
                bell[b].on = 0;
    }
    if (shed_left > 0.0f)
        shed_left -= dt;
    if (shed_left <= 0.0f)
        shed_hard = 0;
    en_busy = shed_hard;
    for (k = 0; k < NFXV; k++) {
        fxv_t *v = &fxv[k];
        if (!v->on)
            continue;
        if (v->gap > 0.0f) {                     /* a kept gesture, between its times */
            on |= 1u << v->type;
            kp |= v->kept ? 1u << v->type : 0u;
            v->gap -= dt;
            if (v->gap <= 0.0f)
                fx_begin(v);
            else
                continue;
        }
        v->t += dt;
        if (v->rel) {
            v->env -= dt / 2.0f;
            if (v->env <= 0.0f) {
                v->on = 0;
                continue;
            }
        } else if (FX_TEXTURE[v->type]) {
            v->env = fm_minf(1.0f, v->env + dt / 2.5f);
            if (!v->kept && v->t >= v->dur)
                v->rel = 1;
        } else {
            v->env = 1.0f;
            if (v->t >= v->dur) {
                if (v->kept)
                    v->gap = 2.0f + 4.0f * frand();
                else
                    v->on = 0;
                continue;
            }
        }
        on |= 1u << v->type;
        kp |= v->kept ? 1u << v->type : 0u;
        switch (v->type) {
        case FX_SNOW:                            /* snow: soft high notes drifting down, slowly, unevenly */
            while (v->n < 12 && v->t >= v->next) {
                fx_strike(v->n % 3 ? 8 : 13, 2, nsteps * 2 - v->n + irand(2), 0.4f + 0.2f * frand(), 3.64f);
                v->n++;
                v->next += 0.22f + 0.3f * frand();
            }
            break;
        case FX_SOLO:                            /* the lead, now: a phrase, heard even with the Lead down */
            if (v->n == 0) {
                ld.solo = 2;
                ld.wait = 1;
                ld.i = ld.n;
                ld.solo_g = fm_maxf(0.35f, 0.75f * fm_sqrtf(fx_lvl));
                v->n = 1;
            }
            break;
        case FX_MBOX: {                          /* a music box: a little tune from the chord */
            static const int8_t TUNE[10] = {0, 2, 4, 7, 6, 4, 2, 4, 5, 4};
            while (v->n < 10 && v->t >= v->next) {
                fx_strike(8, 0, TUNE[v->n] * nsteps / 7, 0.62f, 1.95f);
                v->next += (v->n & 3) == 3 ? 0.44f : 0.27f;
                v->n++;
            }
            break;
        }
        case FX_KALIMBA:                         /* thumbs rolling low and high over the chord */
            while (v->n < 12 && v->t >= v->next) {
                fx_strike(7, v->n & 1 ? 0 : 1, FXC(v->n / 2 % 3) + (v->n & 1) * nsteps, 0.6f, 1.53f);
                v->n++;
                v->next += 0.15f;
            }
            break;
        case FX_ARP: {                           /* up and down the chord, two octaves, in sixteenths */
            static const int8_t UD[13] = {0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0, 1, 2};
            while (v->n < 13 && v->t >= v->next) {
                fx_strike(3, 0, FXC(UD[v->n]), 0.58f, 2.14f);
                v->n++;
                v->next += fx_beat() * 0.25f;
            }
            break;
        }
        case FX_RAIN:                            /* drops on glass, here and there */
            if (v->t >= v->next && v->t < v->dur - 0.4f) {
                fx_strike(6, 2, irand(nsteps * 2), 0.25f + 0.3f * frand(), 4.00f);
                v->n++;
                v->next = v->t + 0.08f + 0.4f * frand();
            }
            break;
        case FX_ECHON:                           /* one note, echoing in time, fading */
            if (v->n == 0)
                v->a = (float)FXC(irand(3));
            while (v->n < 7 && v->t >= v->next) {
                fx_strike(17, 0, (int)v->a + (v->n & 1) * nsteps, 0.7f * fm_exp2f(-0.55f * (float)v->n), 2.63f);
                v->n++;
                v->next += fx_beat() * 0.75f;
            }
            break;
        case FX_STRUM:                           /* strummed down from high, then again, softer */
            while (v->n < 14 && v->t >= v->next) {
                int q = v->n % 7;
                fx_strike(16, 0, FXC(6 - q), v->n < 7 ? 0.62f : 0.42f, 1.95f);
                v->n++;
                v->next += v->n == 7 ? 1.0f : 0.045f;
            }
            break;
        case FX_FIFTHS:                          /* climbing in fifths */
            while (v->n < 6 && v->t >= v->next) {
                fx_strike(13, 3, v->n * (nsteps * 4 / 7), 0.55f + 0.03f * (float)v->n, 1.90f);
                v->n++;
                v->next += 0.2f;
            }
            break;
        case FX_LULLABY:                         /* a slow melody, wandering a step or two at a time */
            if (v->n == 0)
                v->a = 4.0f;
            while (v->n < 7 && v->t >= v->next) {
                fx_strike(2, 0, (int)v->a, 0.6f, 3.40f);
                v->a += (float)(irand(2) ? 1 + irand(2) : -1 - irand(2));
                v->a = fm_maxf(0.0f, fm_minf(v->a, (float)(nsteps * 2)));
                v->n++;
                v->next += v->n == 4 ? 1.1f : 0.72f;
            }
            break;
        case FX_WHIRL:                           /* low and high, closing in on each other */
            while (v->n < 12 && v->t >= v->next) {
                int q = v->n / 2;
                fx_strike(4, 3, v->n & 1 ? nsteps * 3 - q : q, 0.5f, 1.60f);
                v->n++;
                v->next += 0.1f;
            }
            break;
        case FX_UNFOLD:                          /* the chord, one note at a time, ringing on */
            while (v->n < 5 && v->t >= v->next) {
                fx_strike(9, 1, FXC(v->n), 0.55f, 3.00f);
                v->n++;
                v->next += 0.85f;
            }
            break;
        case FX_RIPPLE:                          /* a trill, slowing down */
            if (v->n == 0)
                v->b = 0.06f;
            while (v->n < 16 && v->t >= v->next) {
                fx_strike(6, 2, FXC(1) + (v->n & 1), 0.55f * fm_exp2f(-0.08f * (float)v->n), 2.40f);
                v->n++;
                v->next += v->b;
                v->b *= 1.12f;
            }
            break;
        case FX_BOUNCE:                          /* a note bouncing like a ball, faster and softer */
            if (v->n == 0) {
                v->a = (float)FXC(irand(3));
                v->b = 0.42f;
            }
            while (v->n < 12 && v->t >= v->next) {
                fx_strike(3, 2, (int)v->a, 0.7f * fm_exp2f(-0.25f * (float)v->n), 3.80f);
                v->n++;
                v->next += v->b;
                v->b *= 0.76f;
            }
            break;
        case FX_CALL: {                          /* a phrase up high, answered lower */
            static const int8_t PH[6] = {4, 5, 7, 2, 1, 0};
            while (v->n < 6 && v->t >= v->next) {
                fx_strike(v->n < 3 ? 10 : 11, v->n < 3 ? 2 : 0, PH[v->n] * nsteps / 7 + (v->n < 3) * nsteps / 2, 0.6f, 2.50f);
                v->n++;
                v->next += v->n == 3 ? 0.9f : 0.2f;
            }
            break;
        }
        case FX_GLIMMER:                         /* the chord at once, then an octave up, then higher, softer */
            while (v->n < 9 && v->t >= v->next) {
                fx_strike(v->n < 6 ? 2 : 15, 0, FXC(v->n % 3) + (v->n / 3) * nsteps, 0.55f - 0.06f * (float)(v->n / 3), 3.15f);
                v->n++;
                v->next += v->n % 3 ? 0.012f : 0.8f;
            }
            break;
        case FX_RISE:                            /* up the scale, quickly, from low */
            while (v->n < 12 && v->t >= v->next) {
                fx_strike(12, 1, v->n, 0.5f + 0.02f * (float)v->n, 1.60f);
                v->n++;
                v->next += 0.1f;
            }
            break;
        case FX_CLOCK:                           /* a clock tower: two notes, tick, tock, in time */
            while (v->n < 8 && v->t >= v->next) {
                fx_strike(10, 0, v->n & 1 ? FXC(2) : nsteps, 0.6f, 2.30f);
                v->n++;
                v->next += fx_beat();
            }
            break;
        case FX_METEOR:                          /* a streak up three octaves, then one high note, ringing */
            while (v->n < 11 && v->t >= v->next) {
                fx_strike(v->n < 10 ? 13 : 9, 3, v->n < 10 ? FXC(v->n) : FXC(9), v->n < 10 ? 0.45f : 0.7f, 2.46f);
                v->n++;
                v->next += v->n < 10 ? 0.035f : 0.35f;
            }
            break;
        case FX_WINDHARP:                        /* the wind in a harp's strings: soft notes of the chord, slowly */
            if (v->t >= v->next && v->t < v->dur - 0.6f) {
                fx_strike(11, irand(3), FXC(irand(6)), 0.3f + 0.2f * frand(), 3.40f);
                v->n++;
                v->next = v->t + 0.35f + 0.55f * frand();
            }
            break;
        case FX_PEAL: {                          /* church bells, ringing down, changing order */
            static const int8_t ORD[3][4] = {{0, 1, 2, 3}, {1, 0, 3, 2}, {0, 2, 1, 3}};
            while (v->n < 12 && v->t >= v->next) {
                fx_strike(10, 0, nsteps - ORD[v->n / 4][v->n % 4], 0.6f, 1.44f);
                v->n++;
                v->next += v->n % 4 ? 0.36f : 0.6f;
            }
            break;
        }
        case FX_TWINKLE:                         /* the chord's notes, high, twinkling here and there (1.1, in Stutter's
                                                  * place): a music box's and a celesta's bells */
            while (v->n < 9 && v->t >= v->next) {
                fx_strike(v->n & 1 ? 8 : 2, (v->n & 1) + 2, irand(3) * 2 + nsteps * (1 + irand(2)),
                          0.45f + 0.3f * frand(), 1.7f);
                v->n++;
                v->next += 0.16f + 0.22f * frand();
            }
            break;
        case FX_CASCADE:                         /* down the scale from high, quickly */
            while (v->n < 7 && v->t >= v->next) {
                fx_strike(2, 2, 12 - v->n, 0.75f - 0.04f * (float)v->n, 2.52f);
                v->n++;
                v->next += 0.12f;
            }
            break;
        case FX_HARP:                            /* up the chord, two octaves */
            while (v->n < 7 && v->t >= v->next) {
                fx_strike(11, 3, (v->n % 3) * 2 + (v->n / 3) * nsteps, 0.6f, 1.40f);
                v->n++;
                v->next += 0.09f;
            }
            break;
        case FX_CHIMES:                          /* wind in the chimes */
            if (v->t >= v->next && v->t < v->dur - 0.5f) {
                fx_strike(v->n & 1 ? 10 : 6, 2, 7 + irand(7), 0.3f + 0.3f * frand(), 2.24f);
                v->n++;
                v->next = v->t + 0.18f + 0.45f * frand();
            }
            break;
        default:
            break;
        }
    }
    en_fx_on = on;
    en_fx_kept = kp;
    if (par[P_FXON] && par[P_FXAUTO] > 0 && en_playing && !en_frozen && !en_busy) {   /* Auto FX: now and then, by chance */
        fx_auto_left -= dt;
        if (fx_auto_left <= 0.0f) {
            int f;
            fx_auto_left = 300.0f * fm_exp2f(-(float)par[P_FXAUTO] * 0.045f) * (0.5f + frand());
            do                                   /* (1.0) every effect in turn, shuffled: all 27 before any again (Solo
                                                  * only with a lead) */
                f = fx_bag_next();
            while (f == FX_SOLO && lead_vol <= 0.0f);
            fx_start(f, 0);
        }
    }
    {                                            /* while an effect sounds, the rest steps back a little */
        float pr = 0.0f;
        for (k = 0; k < NFXV; k++)
            if (fxv[k].on && fxv[k].gap <= 0.0f && FX_AUDIBLE >> fxv[k].type & 1u)
                pr = fm_maxf(pr, FX_TEXTURE[fxv[k].type] ? 0.6f * fxv[k].env : 1.0f);
        if (par[P_KEYS] == EN_KEYS_PLAY && ld.gate && lead_vol > 0.0f)
            pr = fm_maxf(pr, 0.55f);             /* a key held: the music steps back a little for it */
        fx_duck_t = pr;
        fx_room = pr > 0.0f && !(par[P_KEYS] == 2 && ld.gate);   /* (not for the keys' own lead) */
    }
}

static void render_fx(uint32_t n)
{
    int k;
    const float G = fx_lvl * 1.3f * fx_adapt, rs = fx_space * 1.2f, es = fx_space * 0.5f;
    for (k = 0; k < NFXV; k++) {
        fxv_t *v = &fxv[k];
        uint32_t j;
        float gl, gr, t0 = v->t;
        if (!v->on || v->gap > 0.0f)
            continue;
        switch (v->type) {
        case FX_STARS: case FX_GRAINS:
            break;
        default:
            continue;                            /* the others move what is there */
        }
        for (j = 0; j < n; j++) {
            float t = t0 + (float)j * (1.0f / EN_SR), x = 0.0f, tgt = 0.0f, pan = v->pan;
            switch (v->type) {
            case FX_GRAINS: {                    /* a cloud of tiny grains of the chord, high and low, never still */
                int g;
                for (g = 0; g < 3; g++) {
                    v->s2[g] -= 1.0f;
                    if (v->s2[g] <= 0.0f && v->rel == 0) {
                        const int note = 72 + chord_pcs[irand(chord_nt)] % 12 + 12 * irand(3) - 12 * (g == 0);
                        v->c2[g] = (0.05f + 0.12f * frand()) * EN_SR;
                        v->s2[g] = v->c2[g] + (0.01f + 0.08f * frand()) * EN_SR;
                        v->inc[g] = mtof((float)note) * (1.0f + 0.004f * (frand() - 0.5f)) * (1.0f / EN_SR);
                        v->ph[g] = 0.0f;
                        v->c[g] = v->c2[g];
                    }
                    if (v->c[g] > 0.0f) {
                        const float w = sine(0.5f * v->c[g] / v->c2[g]);   /* a smooth grain: up and down */
                        x += sine(v->ph[g]) * w * w * (g ? 0.7f : 0.9f);
                        v->c[g] -= 1.0f;
                    }
                }
                tgt = v->env * 0.5f;
                pan = 0.5f * sine(t * 0.13f);
                break;
            }
            case FX_STARS: {                     /* events: high pings of the chord */
                int g;
                for (g = 0; g < 2; g++) {
                    v->s2[g] -= 1.0f;
                    if (v->s2[g] <= 0.0f && v->rel == 0) {
                        int note;
                        note = 84 + chord_pcs[irand(chord_nt)] + 12 * irand(2);
                        v->c2[g] = (0.3f + 0.4f * frand()) * EN_SR;
                        v->s2[g] = (0.2f + 1.0f * frand()) * EN_SR;
                        v->inc[g] = mtof((float)note) * (1.0f / EN_SR);
                        v->ph[g] = 0.0f;
                        v->c[g] = v->c2[g];      /* its age left */
                    }
                    if (v->c[g] > 0.0f) {
                        float a = (v->c[g] / v->c2[g]) * (v->c[g] / v->c2[g]);
                        float y = sine(v->ph[g]) * a;
                        v->c[g] -= 1.0f;
                        x += g ? y * 0.7f : y;
                    }
                }
                tgt = v->env * 0.4f;
                pan = (float)((v->n++ >> 13) & 1) * 0.6f - 0.3f;
                break;
            }
            default:
                break;
            }
            v->amp += (tgt * FX_LVL[v->type] - v->amp) * 0.004f;
            {
                int g;
                for (g = 0; g < 3; g++) {
                    v->ph[g] += v->inc[g];
                    if (v->ph[g] >= 1.0f)
                        v->ph[g] -= 1.0f;
                }
            }
            x *= v->amp * G;
            gl = 0.7071f - 0.5f * pan;
            gr = 0.7071f + 0.5f * pan;
            mixl[j] += x * gl;
            mixr[j] += x * gr;
            rsend[j] += x * rs;
            esend[j] += x * es;
        }
        for (j = 0; j < 4; j++) {
            v->s[j] = fm_flush(v->s[j]);
            v->s2[j] = v->type == FX_STARS ? v->s2[j] : fm_flush(v->s2[j]);
        }
    }
}

void en_fx(int i, int how) { post(C_FX, i, how); }
int en_fx_costly(int i)                          /* 0: it only moves what is playing (costs nothing), so always allowed */
{
    (void)i;                                     /* (1.2: they all play bells or a few sines) */
    return 1;
}
void en_fx_off(void) { post(C_FXOFF, 0, 0); }
void en_lead_key(int note, int on) { post(C_LKEY, note, on); }
void en_strum(int note) { post(C_STRUM, note, 0); }
static void fx_release(int i)
{
    int k;
    for (k = 0; k < NFXV; k++)
        if (fxv[k].on && (i < 0 || fxv[k].type == i)) {
            fxv[k].rel = 1;
            fxv[k].kept = 0;
            if (fxv[k].gap > 0.0f)
                fxv[k].on = 0;
        }
}

/* ---------------------------------------------------- the output's guard --- */
/* No speaker setting: the jack says where the sound goes. On the FM-1's speaker (its amplifier and cone crackle above
 * about -8 dB: HADAL-1's measure) the lows it cannot play are taken out, it is driven less, and a limiter that sees
 * each peak 2 ms ahead keeps it under 0.15 of full scale (HADAL-1's tested ceiling: nothing clicked) at full volume.
 * On headphones the same limiter sits at 0.85, then a soft knee that never passes full scale. */
#define LA_N 96
static int broken;                               /* the output blew up: rebuild the sound at the next block */
volatile uint32_t en_heals;
static float la_buf[2][LA_N], la_g = 1.0f, la_min = 1.0f, spk_mix = 1.0f, spk_hp[4], out_env;
static int la_i, la_hold;
volatile float en_out_level, en_bass_pulse, en_lim_gr = 1.0f;
volatile float en_scope[EN_SCOPE];
volatile uint32_t en_scope_w;
static uint32_t scope_ph;
volatile uint32_t en_chord_count;

static inline float softclip(float x)            /* untouched below .6, then a smooth knee that never passes 1 */
{
    float a = fm_fabsf(x), d;
    if (a <= 0.6f)
        return x;
    d = (a - 0.6f) * 2.5f;
    a = 0.6f + 0.4f * d / (1.0f + d);
    return x < 0.0f ? -a : a;
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    /* Size sets how long the plate rings, not how loud: its input comes down as its tail grows (by its energy,
     * 1 / (1 - decay^2), against a decay of 0.9) */
    const float rnorm = fm_sqrtf((1.0f - pl_decay_t * pl_decay_t) * (1.0f / 0.19f));
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm) * rnorm, eg = echo_send, sw = 0.3f + 0.95f * width, mw = 1.0f - 0.12f * width,
                sh = fm_minf((shim + part_sh + fx_shim) * 0.62f, 0.72f), er_g = 0.22f * rev_send * rnorm, trim = 0.36f * world_trim;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    render_pad(n);
    render_bass(n);
    render_bells(n);
    render_drums(n);
    render_air(n);
    {                                            /* how loud the music is now: the louder, the more the effects are
                                                  * lifted (and the more it steps back for them) */
        float e = 0.0f;
        for (j = 0; j < n; j++)
            e += mixl[j] * mixl[j] + mixr[j] * mixr[j];
        e = fm_sqrtf(e / (float)(2u * n));
        mus_env += (e - mus_env) * fm_minf((float)n / (0.4f * EN_SR), 1.0f);
        fx_adapt = fm_clampf(fm_sqrtf(mus_env * (1.0f / MUS_REF)), 0.85f, 2.6f);
    }
    fx_duck += (fx_duck_t * fm_minf((0.25f + 0.25f * fx_lvl) * (0.7f + 0.3f * fx_adapt), 0.6f) - fx_duck) * fm_minf((float)n / (0.4f * EN_SR), 1.0f);
    if (fx_duck > 0.002f) {                      /* the effects come forward: the rest a few dB back */
        const float d = 1.0f - fx_duck;
        for (j = 0; j < n; j++) {
            mixl[j] *= d;
            mixr[j] *= d;
            rsend[j] *= d;
            esend[j] *= d;
        }
    }
    render_lead(n);                              /* after the step-back: the lead (and what the keys play) in front */
    render_fx(n);
    spk_mix += (spk_tgt - spk_mix) * fm_minf((float)n * (1.0f / (0.05f * EN_SR)), 1.0f);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, xl, xr, m, sd, pk, want, ceil;
        echoes((esend[j] * eg + (mixl[j] + mixr[j]) * 0.35f * fx_throw) * (1.0f - 0.9f * fx_hold), &el, &er, &xl, &xr);
        {                                        /* the plate at half rate: a new pair every other sample, between them
                                                  * the halfway point */
            const float rin = (rsend[j] * rg + (el + er + xl + xr) * er_g) * 0.8f + sh * sh_y;
            if ((rv_ph ^= 1u) != 0u) {
                rv_in = rin;
                wl = (rv_l0 + rv_l1) * 0.5f;
                wr = (rv_r0 + rv_r1) * 0.5f;
            } else {
                rv_l0 = rv_l1;
                rv_r0 = rv_r1;
                reverb((rin + rv_in) * 0.5f, &rv_l1, &rv_r1);
                wl = rv_l1;
                wr = rv_r1;
            }
        }
        sh_y = sh > 0.001f ? shimmer((wl + wr) * 0.5f) : 0.0f;   /* (none: no cost) */
        wl = wl * 1.0f + 0.5f * el + 0.2f * xl;            /* the space, as wide as Width (1.2: less: it was washing
                                                            * everything out) */
        wr = wr * 1.0f + 0.5f * er + 0.2f * xr;
        {                                        /* Rev swell: the space up, the music a little down; Freeze: the music
                                                  * fades, the held space stays */
            const float wg = 1.0f + 2.5f * fx_rsw + 0.6f * fx_frz, dg = (1.0f - 0.35f * fx_rsw) * (1.0f - 0.92f * fx_frz);
            m = (wl + wr) * 0.5f * mw * wg;
            sd = (wl - wr) * 0.5f * sw * wg;
            l = (mixl[j] * dg + m + sd) * trim * gain;
            r = (mixr[j] * dg + m - sd) * trim * gain;
        }
        if (fx_dive > 0.001f) {                  /* Filter dive: everything dark and muffled, then open */
            float v1, v2, v3;
            if (!(j & 31u))
                svf_coefs(dv_c, 16000.0f * fm_exp2f(-5.7f * fx_dive), 0.8f);
            v3 = l - dv_s[0][1];
            v1 = dv_c[0] * dv_s[0][0] + dv_c[1] * v3;
            v2 = dv_s[0][1] + dv_c[1] * dv_s[0][0] + dv_c[2] * v3;
            dv_s[0][0] = 2.0f * v1 - dv_s[0][0];
            dv_s[0][1] = 2.0f * v2 - dv_s[0][1];
            l = v2;
            v3 = r - dv_s[1][1];
            v1 = dv_c[0] * dv_s[1][0] + dv_c[1] * v3;
            v2 = dv_s[1][1] + dv_c[1] * dv_s[1][0] + dv_c[2] * v3;
            dv_s[1][0] = 2.0f * v1 - dv_s[1][0];
            dv_s[1][1] = 2.0f * v2 - dv_s[1][1];
            r = v2;
        } else {
            dv_s[0][0] = dv_s[0][1] = dv_s[1][0] = dv_s[1][1] = 0.0f;
        }
        l = l / (1.0f + 0.15f * fm_fabsf(l));    /* a little tape glue: the peaks rounded, the rest together */
        r = r / (1.0f + 0.15f * fm_fabsf(r));
        if (spk_mix > 0.0005f) {                 /* the speaker: no lows it cannot play (12 dB under ~150 Hz), driven less */
            const float c = 0.0211f;
            spk_hp[0] += c * (l - spk_hp[0]);
            spk_hp[1] += c * ((l - spk_hp[0]) - spk_hp[1]);
            spk_hp[2] += c * (r - spk_hp[2]);
            spk_hp[3] += c * ((r - spk_hp[2]) - spk_hp[3]);
            l += ((l - spk_hp[0] - spk_hp[1]) * 0.5f - l) * spk_mix;
            r += ((r - spk_hp[2] - spk_hp[3]) * 0.5f - r) * spk_mix;
        }
        ceil = 0.85f + (0.15f - 0.85f) * spk_mix;
        pk = fm_maxf(fm_fabsf(l), fm_fabsf(r));
        if (!(pk < 64.0f)) {                     /* NaN or a blow-up: silence now, and the sound rebuilt (en_render) */
            broken = 1;
            l = r = pk = 0.0f;
        }
        want = pk > ceil ? ceil / pk : 1.0f;
        want = want < 0.002f ? 0.002f : want;
        if (want <= la_min) {
            la_min = want;
            la_hold = LA_N;
        } else if (want < la_min * 1.02f) {
            la_hold = LA_N;                      /* nearly as loud: hold on for it too */
        } else if (la_hold > 0) {
            la_hold--;
        } else {                                 /* back up over about half a second: at most 2 % in the look-ahead */
            la_min += la_min * (1.0f - la_min) * 0.0002f + (1.0f - la_min) * 0.000002f;
        }
        la_g += (la_min - la_g) * (la_min < la_g ? 0.12f : 1.0f);   /* down in well under the look-ahead */
        {
            float dl = la_buf[0][la_i], dr = la_buf[1][la_i];
            la_buf[0][la_i] = l;
            la_buf[1][la_i] = r;
            la_i = la_i + 1 < LA_N ? la_i + 1 : 0;
            l = softclip(dl * la_g);
            r = softclip(dr * la_g);
        }
        out_env += (fm_maxf(fm_fabsf(l), fm_fabsf(r)) - out_env) * 0.0005f;
        if (!(++scope_ph & 7u)) {                /* the picture's wave */
            en_scope[en_scope_w % EN_SCOPE] = (l + r) * 0.5f;
            en_scope_w++;
        }
        out[2 * j] = (int32_t)(l * 8300000.0f);
        out[2 * j + 1] = (int32_t)(r * 8300000.0f);
    }
    for (j = 0; j < 4; j++)
        spk_hp[j] = fm_flush(spk_hp[j]);
    en_out_level = out_env / (0.25f + 0.6f * (1.0f - spk_mix));
    en_lim_gr = la_g;
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */
static float arc, arc_tgt;
static uint32_t arc_left;

static float wow_ph, flut_ph;
static void play_stop(void);
static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    if (sleep_left > 0.0f && en_playing) {       /* Sleep: the last minute fades, then it rests */
        sleep_left -= (float)n * (1.0f / EN_SR);
        sleep_g = fm_clampf(sleep_left * (1.0f / 60.0f), 0.0f, 1.0f);
        if (sleep_left <= 0.0f) {
            sleep_left = 0.0f;
            play_stop();
            en_asleep = 1;
        }
    } else if (sleep_g < 1.0f && !en_playing) {
        sleep_g = 1.0f;                          /* resting: back to full for the next time */
    }
    if (en_playing && en_asleep && par[P_SLEEP]) {   /* woken by PLAY: the timer from the top */
        en_asleep = 0;
        sleep_left = (float)par[P_SLEEP] * 1800.0f;
    }
    mods_step(n);
    wander_step(n);
    journey_step(n);
    fx_control(n);
    wow_ph += (float)n * (0.37f / EN_SR);       /* Tape: a slow wow and a little flutter, on the pitch */
    flut_ph += (float)n * (4.3f / EN_SR);
    if (wow_ph >= 1.0f)
        wow_ph -= 1.0f;
    if (flut_ph >= 1.0f)
        flut_ph -= 1.0f;
    wowf = (1.0f + (tape_amt + 3.0f * fx_wob) * (0.0028f * sine(wow_ph) + 0.0007f * sine(flut_ph))) * fx_tapef;
    heads_step(n);
    if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand()) + (r > 0.0f ? 0.25f : -0.25f);   /* away from the middle */
        arc_left = (uint32_t)((50.0f + 100.0f * frand()) * tidef * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n / (35.0f * tidef * EN_SR);
    arcm = arc * motion;
    en_arc = arcm;
    pl_decay += ((en_frozen ? 0.9995f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += ((en_frozen ? 0.0f : 1.0f) - rev_in_gain) * 0.01f;
    if (en_playing && !en_frozen && !change_pending) {
        drift_left -= (float)n;
        if (!fifth_done && drift_left < 0.5f * drift_total) {
            fifth_done = 1;                      /* halfway: the bass may lean on the fifth */
            if (frand() < (float)par[P_FIFTH] * 0.01f && chord_nt >= 3)
                bass_to(chord_pcs[2]);
        }
        if (drift_left <= 0.0f)
            change_pending = 1;
    }
    en_drift = drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.95f;
}

static void heal(void)                          /* everything rebuilt from silence, the settings and the scale kept */
{
    static int16_t keep[P_NPARAMS];
    const uint32_t pl = pool;
    const int playing = en_playing;
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        keep[i] = user[i];
    en_init();
    for (i = 0; i < P_NPARAMS; i++) {
        user[i] = keep[i];
        apply(i, eff_value(i));
    }
    pool_set(pl);
    for (i = 0; i < LA_N; i++)
        la_buf[0][i] = la_buf[1][i] = 0.0f;
    la_g = la_min = 1.0f;
    for (i = 0; i < 4; i++)
        spk_hp[i] = 0.0f;
    out_env = sh_y = 0.0f;
    for (i = 0; i < NFXV; i++)
        fxv[i].on = 0;
    ld.s[0] = ld.s[1] = ld.s2[0] = ld.s2[1] = ld.env = ld.nl = 0.0f;
    ld.gate = 0;
    ld.i = ld.n = 0;
    if (playing)
        play_start(0);
    broken = 0;
    en_heals++;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f * sleep_g;
    if (broken)
        heal();
    drain();
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample */
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
}
