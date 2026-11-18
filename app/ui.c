/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (AMB-1), Polystellarflow (ZENITH-1) */
/* ZENITH-1 UI: the panel and the screen, AMB-1's grown. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays); in the FX menu,
 *                  the effects (a tap: once; held FX: all let go; FX again: back)
 *   PLAY           the garden grows / rests        EDIT  the next part of the world (held: a new era)
 *   REC            FREEZE: the chord holds, the reverb rings on, the picture holds    OCT- / OCT+   the bells' octave
 *   HOME           the Garden; again, the Journey; again, the picture
 *   ENV ARP LFO SEQ   the pad, the bells, the bass, the pulse (each again: its shape)
 *   SEL            the Space, Tape, Air      FX  the FX menu      GLO  Setup, Picture
 *   SELECT         the previous or next page; with HOME held, the tempo
 *   ALGORITHM mode (loads a pool)   PRESETS the world
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the world back to its recipe */
#include "fastmath.h"
#ifndef APP_VERSION
#define APP_VERSION "DEV"                /* a release build says its own (./build.sh --release X.Y) */
#endif

/* the pages: each button has one or more (pressed again: the next); SELECT steps through them all in this order */
enum { V_GARDEN, V_JOURNEY, V_PAD, V_PAD2, V_PAD3, V_BELLS, V_BELLS2, V_BASS, V_BASS2, V_PULSE, V_PULSE2, V_LEAD,
       V_LEAD2, V_LEAD3, V_KEYS, V_SPACE, V_TAPE, V_AIR, V_FX, V_SETUP, V_SLEEP, V_PICTURE, V_GUIDE, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Garden", "Journey", "Pad", "Pad shape", "Pad breath", "Bells",
                                              "Bell shape", "Bass", "Bass shape", "Pulse", "Pulse shape", "Lead",
                                              "Lead shape", "Lead breath", "Keys", "Space", "Tape", "Air", "FX",
                                              "Setup", "Sleep", "Picture", "Guide"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_HOME, B_ENV, B_ENV, B_ENV, B_ARP, B_ARP, B_LFO, B_LFO, B_SEQ, B_SEQ,
                                         B_SEQ, B_SEQ, B_SEQ, B_SEQ, B_SEL, B_SEL, B_SEL, B_FX, B_GLO, B_GLO,
                                         B_GLO, B_GLO};
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 100, K_CPU, K_PICMODE, K_PICAFTER, K_PICPAL, K_PICGLOW, K_FXBANK, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_JOURNEY, P_WANDER, P_TIDE, P_KEYMOVE},
    {P_PAD, P_PADTONE, P_SWELL, P_BRIGHT},
    {P_PWARM, P_PMOVE, P_PSPACE, P_PWIDTH},
    {P_PATT, P_PREL, K_NONE, K_NONE},
    {P_BELLS, P_BELLSND, P_DENSITY, P_BTONE},
    {P_BOCT, P_SHIMB, P_BSPACE, P_BDECAY},
    {P_BASS, P_BASSSND, P_BASSOCT, P_GLIDE},
    {P_FIFTH, P_BWARM, P_BMOVE, P_BASPACE},
    {P_PULSE, P_KIT, P_HITS, P_SWING},
    {P_HATS, P_KMOVE, P_KSPACE, P_KWIDTH},
    {P_LEAD, P_LEADSND, P_LSTYLE, P_LOFTEN},
    {P_LTONE, P_LSPACE, P_LEXPR, P_LGLIDE},
    {P_LATT, P_LREL, K_NONE, K_NONE},
    {P_KEYS, P_STRUM, P_SLEVEL, K_NONE},
    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
    {P_SHIM, P_RWARM, P_TAPE, P_WIDTH},
    {P_AIR, P_AIRSND, P_AIRTONE, P_AIRMOVE},
    {P_FXLVL, P_FXON, P_FXAUTO, P_FXSPACE},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
    {P_SLEEP, K_NONE, K_NONE, K_NONE},
    {K_PICMODE, K_PICAFTER, K_PICPAL, K_PICGLOW},
    {K_NONE, K_NONE, K_NONE, K_NONE},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Journey", "Wander", "Tide", "Key moves"},
    {"Level", "Sound", "Swell", "Bright"},
    {"Warmth", "Movement", "Space", "Width"},
    {"Attack", "Release", "", ""},
    {"Level", "Sound", "Density", "Tone"},
    {"Octave", "Shimmer", "Space", "Decay"},
    {"Level", "Sound", "Octave", "Glide"},
    {"Fifth", "Warmth", "Movement", "Space"},
    {"Level", "Kit", "Hits", "Swing"},
    {"Tone", "Movement", "Space", "Width"},
    {"Level", "Sound", "Style", "Often"},
    {"Tone", "Space", "Vibrato", "Glide"},
    {"Attack", "Release", "", ""},
    {"Keys", "Strum", "Strum vol", ""},
    {"Echo", "Echo time", "Reverb", "Size"},
    {"Shimmer", "Warmth", "Tape", "Width"},
    {"Level", "Sound", "Tone", "Movement"},
    {"Level", "Auto FX", "How often", "Space"},
    {"Root", "Tune", "MIDI out", "Lights"},
    {"Sleep", "", "", ""},
    {"Picture", "After", "Colours", "Glow"},
    {"", "", "", ""},
};
static const char *const PIC_AFTER_NAME[5] = {"Never", "10 s", "30 s", "1 min", "3 min"};
static const uint32_t PIC_AFTER_MS[5] = {0, 10000u, 30000u, 60000u, 180000u};
static const char *const PIC_GLOW_NAME[3] = {"Soft", "Bright", "Max"};

/* the screen's colours: a warm night sky. Deep indigo paper, warm white ink, an amber accent; the pad rose, the
 * bells sky blue, the bass gold, the pulse lavender, each with a dim tint for the shapes at rest */
#define K_BG RGB(18, 16, 34)
#define K_PANEL RGB(40, 34, 66)
#define K_LINE RGB(64, 56, 96)
#define K_DIM RGB(150, 138, 180)
#define K_TEXT RGB(248, 238, 222)
#define K_ACC RGB(255, 178, 112)         /* the accent: the page, the knob being turned, a message's word */
#define K_BELL RGB(130, 200, 236)
#define K_BELL_T RGB(54, 74, 116)
#define K_PAD RGB(240, 144, 172)
#define K_PAD_T RGB(92, 58, 98)
#define K_PULSE RGB(200, 182, 255)
#define K_PULSE_T RGB(70, 62, 112)
#define K_BASS RGB(255, 204, 124)
#define K_BASS_T RGB(98, 78, 62)
#define K_FROST RGB(160, 226, 255)
#define K_LEAD RGB(186, 246, 170)        /* the lead: a firefly green */
#define K_FROST_T RGB(60, 92, 124)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
    uint8_t pic, pic_phase;            /* the picture is up; it computes one frame, sends the next */
    uint32_t pic_ms, vis_ms;
    int8_t hp;                         /* the jack as last told to the engine (-2: not yet) */
    uint8_t free_done;                 /* EDIT held: a new era, this hold */
    uint8_t fx_bank;                   /* the FX menu's list shown: 0 gestures, 1 textures */
    uint32_t sheds_seen, heals_seen;
    uint8_t lk_on;                     /* the keys have been playing the lead (Style Keys) */               /* the CPU ran out this many times (en_sheds): said once each */
    uint8_t fx_from;                   /* the page the FX menu was opened from */
    uint32_t journey_seen, title_until, fx_seen, fx_seen_t;
} ui;

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)en_root_now % 12u;   /* the key as it sounds (Key moves) */
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    switch (k) {
    case K_LEDS: return proj.leds;
    case K_CPU: return (int)plat_cpu_pct();
    case K_PICMODE: return proj.pic_mode + 2;
    case K_PICAFTER: return proj.pic_after;
    case K_PICPAL: return proj.pic_pal + 2;
    case K_PICGLOW: return proj.pic_glow;
    case K_FXBANK: return ui.fx_bank;
    default: return 0;
    }
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
        return;
    }
    *lo = 0;
    *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : k == K_PICMODE ? VIS_NMODE + 1 : k == K_PICAFTER ? 4
        : k == K_PICPAL ? VIS_NPAL + 1 : k == K_PICGLOW ? 2 : k == K_FXBANK ? 1 : 0;
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : k == K_PICMODE ? "Picture" : k == K_PICAFTER ? "Picture after"
         : k == K_PICPAL ? "Colours" : k == K_PICGLOW ? "Glow" : k == K_FXBANK ? "Bank" : "";
}
static void copy_s(char *b, const char *n)
{
    while ((*b++ = *n++))
        ;
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        copy_s(b, v == 2 ? "Keys" : v ? "On" : "Off");
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_PICMODE) {
        copy_s(b, v > 1 ? VIS_MODE[v - 2] : v ? "World's" : "Auto");
    } else if (k == K_PICPAL) {
        copy_s(b, v > 1 ? VIS_PAL[v - 2] : v ? "World's" : "Auto");
    } else if (k == K_PICAFTER) {
        copy_s(b, PIC_AFTER_NAME[v]);
    } else if (k == K_PICGLOW) {
        copy_s(b, PIC_GLOW_NAME[v]);
    } else if (k == K_FXBANK) {
        copy_s(b, v ? "Textures" : "Gestures");
    } else {
        b[0] = 0;
    }
}

static void pic_apply(void)                     /* the picture's settings to the picture */
{
    vis_set_mode(proj.pic_mode);
    vis_set_pal(proj.pic_pal);
    vis_bright(proj.pic_glow);
}

static int knob_at(int view, int i)              /* a page's knob i: the Lead's 4th is Arp while its Style is Keys */
{
    return VIEW_KNOB[view][i];
}
static const char *label_at(int view, int i)
{
    (void)knob_at;
    return KNOB_LABEL[view][i];
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set_hand(k, v);                       /* by hand: Wander leaves it where it is put */
        if (k == P_KEYS) {                       /* what the keys do now: no note left hanging */
            en_lead_key(-1, 0);
            say("THE KEYS:", v == EN_KEYS_PLAY ? "PLAY THE LEAD" : v == EN_KEYS_STRUM ? "STRUM THE CHORD" : "THE SCALE");
        }
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_PICMODE) {
        proj.pic_mode = (int8_t)(v - 2);
        pic_apply();
    } else if (k == K_PICPAL) {
        proj.pic_pal = (int8_t)(v - 2);
        pic_apply();
    } else if (k == K_PICAFTER) {
        proj.pic_after = (uint8_t)v;
    } else if (k == K_PICGLOW) {
        proj.pic_glow = (uint8_t)v;
        pic_apply();
    } else if (k == K_FXBANK) {
        ui.fx_bank = (uint8_t)v;
        return;                                  /* only what the screen shows: not saved */
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - (int)en_root_now % 12 + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void set_piece(int i)                    /* the world playing kept as it is; world i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    en_seed(plat_ticks24() ^ plat_ms() * 2654435761u);   /* a new world, never quite the same twice */
    say_as("WORLD", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the world with its key and tempo */
    mark_dirty();
}


/* RANDOM (as the volca FM's): EDIT held. New sounds for every layer, the lead's way, the kit, the key, the mode, the
 * tempo, the space and the levels, each within a range that sounds good (the world's tuning and voicing stay, and the
 * shared settings: Journey, Wander, the effects, Tune, MIDI). SAVE held: the world's own recipe again (the undo). */
static uint32_t rnd_s = 0x9E3779B9u;
static int rnd(int lo, int hi)                   /* lo .. hi, inclusive */
{
    rnd_s ^= rnd_s << 13;
    rnd_s ^= rnd_s >> 17;
    rnd_s ^= rnd_s << 5;
    return lo + (int)(rnd_s % (uint32_t)(hi - lo + 1));
}
static void randomize(void)
{
    static const struct { uint8_t p; int8_t lo, hi, off; } R_[] = {   /* off: the chance (of 10) it is silent */
        {P_TEMPO, 50, 90, 0},   {P_EVOLVE, 25, 50, 0},  {P_MOTION, 35, 65, 0},  {P_DRIFT, 20, 55, 0},
        {P_PAD, 45, 75, 0},     {P_PADTONE, 0, EN_NPADSND - 1, 0}, {P_SWELL, 40, 90, 0}, {P_BRIGHT, 25, 65, 0},
        {P_PWARM, 25, 60, 0},   {P_PMOVE, 30, 65, 0},   {P_PSPACE, 40, 70, 0},  {P_PWIDTH, 55, 80, 0},
        {P_BELLS, 35, 65, 1},   {P_BELLSND, 0, EN_NBELLSND - 1, 0}, {P_DENSITY, 20, 55, 0}, {P_BTONE, 20, 55, 0},
        {P_BOCT, -1, 1, 0},     {P_SHIMB, 0, 70, 0},    {P_BSPACE, 45, 70, 0},  {P_BDECAY, 40, 75, 0},
        {P_BASS, 25, 50, 1},    {P_BASSSND, 0, EN_NBASSSND - 1, 0}, {P_GLIDE, 15, 60, 0}, {P_FIFTH, 10, 40, 0},
        {P_BWARM, 40, 60, 0},   {P_PULSE, 25, 50, 2},   {P_KIT, 0, EN_NKIT - 1, 0}, {P_HITS, 15, 45, 0},
        {P_SWING, 0, 35, 0},    {P_HATS, 30, 60, 0},    {P_LEAD, 30, 50, 3},    {P_LEADSND, 0, EN_NLEADSND - 1, 0},
        {P_LSTYLE, 0, EN_NLSTYLE - 1, 0}, {P_LOFTEN, 30, 60, 0}, {P_LTONE, 35, 65, 0}, {P_LSPACE, 45, 70, 0},
        {P_LEXPR, 30, 60, 0},   {P_AIR, 10, 35, 4},     {P_AIRSND, 0, EN_NAIRSND - 1, 0}, {P_ECHO, 20, 55, 0},
        {P_ETIME, 1, 5, 0},     {P_REVERB, 50, 80, 0},  {P_SIZE, 70, 95, 0},    {P_SHIM, 0, 60, 0},
        {P_RWARM, 25, 55, 0},   {P_TAPE, 0, 40, 0},     {P_ROOT, 0, 11, 0},     {P_MODE, 0, EN_NMODES - 1, 0},
    };
    unsigned k;
    rnd_s ^= plat_ms() * 2654435761u ^ plat_ticks24();
    for (k = 0; k < sizeof R_ / sizeof R_[0]; k++) {
        int v = rnd(R_[k].lo, R_[k].hi);
        if (R_[k].off && rnd(1, 10) <= R_[k].off)
            v = 0;
        if (R_[k].p == P_SWING && rnd(1, 10) <= 6)
            v = 0;                               /* mostly straight */
        proj.par[R_[k].p] = (int16_t)v;
    }
    proj.pool = EN_MODE_POOL[proj.par[P_MODE]];
    project_apply();
    en_unpin();                                  /* all of it free to wander from here */
    say("RANDOM", "HOLD SAVE: UNDO");
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    if (v == V_FX && ui.view != V_FX)
        ui.fx_from = ui.view;                    /* FX again: back here */
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void pic_show(int on)                    /* the picture up, or the pages back (all of them redrawn) */
{
    if (ui.pic == (uint8_t)on)
        return;
    ui.pic = (uint8_t)on;
    ui.pic_phase = 0;
    ui.pic_ms = plat_ms();
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
}

static void button(int b)
{
    int v;
    if (b == B_HOME && ui.view == V_JOURNEY) {   /* HOME a third time: the picture (and the Garden after it) */
        set_view(V_GARDEN);
        pic_show(1);
        return;
    }
    if (b == B_FX && ui.view == V_FX) {          /* FX again: out of the menu, back where you were */
        set_view(ui.fx_from < NVIEWS && ui.fx_from != V_FX ? ui.fx_from : V_GARDEN);
        return;
    }
    if (VIEW_BTN[ui.view] == b) {                /* the page's own button again: its next page */
        v = ui.view + 1;
        if (v >= NVIEWS || VIEW_BTN[v] != b) {   /* past its last: its first again */
            v = ui.view;
            while (v > 0 && VIEW_BTN[v - 1] == b)
                v--;
        }
        set_view(v);
        return;
    }
    for (v = 0; v < NVIEWS; v++)                 /* another page's button: its first page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_EDIT:                                 /* a tap: nothing changes (the music moves on by itself); the hint */
        say("HOLD EDIT", "FOR RANDOM");
        break;
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("GROWING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP: {                /* the octave of the layer whose page this is (the bells elsewhere) */
        const int v = ui.view;
        const int p = v == V_LEAD || v == V_LEAD2 ? P_LOCT : v == V_PAD || v == V_PAD2 ? P_POCT
                    : v == V_BASS || v == V_BASS2 ? P_BASSOCT : P_BOCT;
        const char *nm = p == P_LOCT ? "LEAD OCTAVE" : p == P_POCT ? "PAD OCTAVE" : p == P_BASSOCT ? "BASS OCTAVE"
                       : "BELL OCTAVE";
        char t[8];
        knob_set(p, proj.par[p] + (b == B_OCTUP ? 1 : -1));
        en_param_text(p, proj.par[p], t);
        say(nm, t);
        break;
    }
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt, 0)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if ((st == 0x90u || st == 0x80u) && proj.par[P_KEYS] == EN_KEYS_PLAY)
            en_lead_key((int)n, st == 0x90u && v);   /* Keys: Play: channel 1 plays the lead */
        else if (st == 0x90u && v && proj.par[P_KEYS] == EN_KEYS_STRUM)
            en_strum((int)n);                    /* Keys: Strum */
        else if (st == 0x90u && v)
            toggle_note((int)(n % 12u));
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press: it has no card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
/* (1.5: no help cards on a held button: holding does things now (EDIT Random, SAVE undo, FX + a key), and the Guide
 * page, GLO a third time, says what every button does) */

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (ui.pic || ui.title_until) {      /* the picture or the title: a button only wakes the pages */
                ui.btn_used |= m;
                if (ui.title_until)
                    vis_title(0);
                ui.title_until = 0;
                pic_show(0);
                continue;
            }
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_EDIT)
                ui.free_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    if (ui.lk_on && (ui.view == V_FX || (btn >> B_FX & 1u) || proj.par[P_KEYS] != EN_KEYS_PLAY)) {
        ui.lk_on = 0;                            /* the keys go elsewhere now: the lead let go */
        en_lead_key(-1, 0);
    }
    if (ui.view == V_FX && !ui.pic) {            /* the FX menu: the keys play the effects */
        for (i = 0; i < NKEYS; i++) {
            uint32_t m = 1u << i;
            if ((ch & keys) & m) {
                if (i >= EN_NFX) {
                    ;
                } else if (en_busy && en_fx_costly((int)i)) {            /* the CPU near full: not now */
                    warn("TOO BUSY", "TRY AGAIN");
                } else {
                    en_fx((int)i, 0);            /* once: nothing is kept on (the FM-1's CPU) */
                    say(EN_FX_NAME[i], 0);
                }
            }
        }
    } else if (btn >> B_FX & 1u) {               /* FX held, on any page: the keys play the effects (as HADAL-1) */
        for (i = 0; i < NKEYS && i < EN_NFX; i++)
            if ((ch & keys) >> i & 1u) {
                ui.btn_used |= 1u << B_FX;       /* letting go of FX then doesn't open the menu */
                if (en_busy && en_fx_costly((int)i)) {
                    warn("TOO BUSY", "TRY AGAIN");
                } else {
                    en_fx((int)i, 0);
                    say(EN_FX_NAME[i], 0);
                }
            }
    } else if (proj.par[P_KEYS] == EN_KEYS_STRUM) {   /* Keys: Strum: each key strums the chord from its note */
        for (i = 0; i < NKEYS; i++)
            if ((ch & keys) >> i & 1u)
                en_strum(53 + (int)i);           /* key 0 is F3 */
    } else if (proj.par[P_KEYS] == EN_KEYS_PLAY) {   /* Keys: Play: the keys play the lead */
        ui.lk_on = 1;
        for (i = 0; i < NKEYS; i++) {
            if ((ch & keys) >> i & 1u)
                en_lead_key(53 + (int)i, 1);     /* key 0 is F3 */
            else if ((ch & ~keys) >> i & 1u)
                en_lead_key(53 + (int)i, 0);
        }
    } else {
        for (i = 0; i < NKEYS; i++)
            if ((ch & keys) >> i & 1u)
                toggle_note((int)((5u + i) % 12u));  /* key 0 is F3 */
    }
    if (btn || keys || ch) {
        ui.act_t = plat_ms();
        if (ui.title_until && keys) {
            ui.title_until = 0;
            vis_title(0);
        }
        if (ui.pic && keys)
            pic_show(0);                         /* a key: the pages again (and it does its job) */
    }
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_GARDEN) {           /* Tempo is the Garden's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else if (!ui.pic) {                    /* (out of the picture, the first detent only wakes) */
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
        }
        ui.act_t = ui.turn_t = plat_ms();
        pic_show(0);
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.act_t = ui.turn_t = plat_ms();
        pic_show(0);
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.act_t = ui.turn_t = plat_ms();
        pic_show(0);
    }
    if (ui.view == V_FX && (btn >> B_FX & 1u) && !(ui.btn_used >> B_FX & 1u) && plat_ms() - ui.down_t[B_FX] > 600u) {
        ui.btn_used |= 1u << B_FX;               /* FX held in the menu: every effect let go (and no card) */
        en_fx_off();
        say("ALL EFFECTS", "LET GO");
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET:", "SAVE TO KEEP");
        mark_dirty();
    }
    if ((btn >> B_EDIT & 1u) && !ui.free_done && plat_ms() - ui.down_t[B_EDIT] > RESET_HOLD) {
        ui.free_done = 1;                      /* held: RANDOM (eras come by themselves) */
        ui.btn_used |= 1u << B_EDIT;
        ui.help = 0;
        randomize();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, knob_at(ui.view, (int)i), e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
            pic_show(0);
        }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the garden (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (plat_battery): 0 (low) .. 3 bars, 4 a USB host powers the FM-1, -1 not read yet. The
 * simulator's "battery N" (host.c) stands in for it */
#ifdef PLAT_HOST
static int8_t batt_host = -2;                  /* -2: plat_battery's */
static int batt(void) { return batt_host > -2 ? batt_host : plat_battery(); }
#else
#define batt() plat_battery()
#endif
#define K_LOW RGB(196, 84, 52)                 /* the battery at one bar or none */
#define BATT_W 20                              /* the body 18 wide and its nub */
/* lying down at the header's right, its middle on the beat light's: three bars, rust at one or none; a
 * bolt instead on USB power; nothing until it is read */
static void draw_batt(int32_t x, int b)
{
    static const int8_t BOLT[7][2] = {{10, 2}, {9, 2}, {8, 2}, {7, 5}, {9, 2}, {8, 2}, {7, 2}};
    uint16_t c = b <= 1 ? K_LOW : K_DIM;
    int i;
    cv_round(x, 9, 18, 10, 2, c);
    cv_rect(x + 1, 10, 16, 8, K_BG);
    cv_rect(x + 18, 12, 2, 4, c);
    if (b == 4) {
        for (i = 0; i < 7; i++)
            cv_rect(x + BOLT[i][0], 10 + i, BOLT[i][1], 1, K_TEXT);
        return;
    }
    for (i = 0; i < b && i < 3; i++)
        cv_rect(x + 2 + i * 5, 11, 4, 6, b <= 1 ? K_LOW : K_PAD);
}
static const char *batt_words(int b)
{
    static const char *const W[5] = {"Battery low", "Battery one third", "Battery two thirds", "Battery full",
                                     "On USB power"};
    return b >= 0 && b <= 4 ? W[b] : 0;
}

/* the header: the piece in a tag, its root and mode; on the right, the battery, the tempo with a slow
 * beat light (a play triangle while resting) and a dot while there is something unsaved. Messages take
 * its place for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)en_root_now << 8 | (uint32_t)proj.pool << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] |
                      (uint32_t)(batt() + 1) << 24);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (batt() >= 0) {
            x -= BATT_W;
            draw_batt(x, batt());
            x -= 6;
        }
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        if (!en_ext) {                            /* what the number is */
            x -= 3 + text_w(&FONT_XS, "TEMPO");
            cv_text(x, 9, &FONT_XS, "TEMPO", K_DIM);
        }
        if (!en_playing) {
            int32_t j;                            /* resting: a play triangle */
            x -= 16;
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        {                                         /* the world, in a warm tag (a long name in the smaller face) */
            const felucca_font_t *f = text_w(&FONT_B, piece_name(proj.piece)) + 16 > x - 40 ? &FONT_S : &FONT_B;
            w = text_w(f, piece_name(proj.piece)) + 12;
            cv_round(4, 4, w, 20, 6, K_ACC);
            cv_text(10, f == &FONT_B ? 6 : 7, f, piece_name(proj.piece), K_BG);
        }
        w += 10;                                  /* the root, then the mode, where they clear the right side */
        if (w + text_w(&FONT_XS, EN_NOTE_NAME[en_root_now % 12u]) + 4 <= x) {
            w = cv_text(w, 8, &FONT_XS, EN_NOTE_NAME[en_root_now % 12u], K_TEXT);
            if (w + 3 + text_w(&FONT_XS, mode_name()) + 4 <= x)
                cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
        }
    }
    cv_blit(0, 0);
}

static const char *chord_now(char *nm)          /* the chord's name, as the garden shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

/* THE GARDEN: a night horizon. The chord a constellation across the sky (the scale's twelve places on an arc, the
 * root at the left: its notes faint stars, the chord's bright and joined); the four bell loops planets on their
 * orbits, each ringing bell a flash, the slots that hold a note small lights on the orbit; the pad an aurora of four
 * curtains, each as high as its voice is loud; the bass a moon at the horizon, swelling; the pulse ripples on the
 * water below. The part and the era at the top left, the chord's name under them; the last effect at the top right. */
#define HZ 96                                    /* the horizon, in the garden's rows */
static uint16_t mixc(uint16_t a, uint16_t b, int t)   /* a .. b, t 0..256 */
{
    int ar = a >> 11, ag = a >> 5 & 63, ab = a & 31, br = b >> 11, bg = b >> 5 & 63, bb = b & 31;
    return (uint16_t)((ar + ((br - ar) * t >> 8)) << 11 | (ag + ((bg - ag) * t >> 8)) << 5 | (ab + ((bb - ab) * t >> 8)));
}
#define K_SKY_LO RGB(52, 34, 78)                 /* the sky just above the horizon */
#define K_WATER RGB(10, 10, 26)
#define K_AURORA_TOP RGB(120, 110, 230)
static void star_pos(int i, int32_t *x, int32_t *y)    /* the twelve places, left to right on an arc */
{
    float a = FM_PI * (0.12f + 0.76f * (float)i / 11.0f);
    *x = (int32_t)(120.0f - 104.0f * fm_cosf(a));
    *y = (int32_t)(72.0f - 54.0f * fm_sinf(a));
}
static void draw_sky(void)
{
    int32_t y, i, k;
    const uint32_t ap = pool_abs();
    const int root = (int)(en_root_now % 12u);
    for (y = 0; y < HZ; y++)                     /* the sky: indigo, lighter towards the horizon */
        cv_rect(0, y, 240, 1, mixc(K_BG, K_SKY_LO, y * 256 / HZ));
    for (k = 0; k < EN_NPAD; k++) {              /* the aurora: a curtain a pad voice */
        static const int32_t AX[EN_NPAD] = {46, 98, 146, 196};
        float a = fm_clampf(en_pad_amp[k] * 1.8f, 0.0f, 1.0f);
        int32_t hgt = (int32_t)(78.0f * a), x;
        if (hgt < 3)
            continue;
        for (x = -18; x <= 18; x += 2) {
            float sh = 1.0f - (float)(x * x) / 340.0f;   /* a curtain's edge, lower */
            int32_t hh = (int32_t)((float)hgt * sh * (0.85f + 0.15f * fm_sinf((float)(x + k * 7) * 0.7f + (float)ui.frame * 0.05f)));
            int32_t yy;
            for (yy = 0; yy < hh; yy += 2) {
                int t = (int)(a * 200.0f) * (hh - yy) / (hh + 1);
                cv_rect(AX[k] + x, HZ - 1 - yy, 2, 2, mixc(mixc(K_SKY_LO, K_AURORA_TOP, yy * 256 / (hgt + 1)), K_PAD, t / 3));
                (void)t;
            }
        }
    }
    {                                            /* the constellation */
        int32_t px = -1, py = -1, first = 1;
        for (i = 0; i < 12; i++) {
            int pc = (root + i) % 12, ch = 0, n;
            int32_t x, yy;
            star_pos(i, &x, &yy);
            for (n = 0; n < en_chord_n; n++)
                ch |= en_chord[n] % 12 == pc;
            if (ch) {
                if (!first)
                    cv_line(px, py, x, yy, K_BELL_T);
                first = 0;
                px = x;
                py = yy;
            }
        }
        for (i = 0; i < 12; i++) {
            int pc = (root + i) % 12, in = (int)(ap >> pc & 1u), ch = 0, n;
            int32_t x, yy;
            star_pos(i, &x, &yy);
            for (n = 0; n < en_chord_n; n++)
                ch |= en_chord[n] % 12 == pc;
            if (ch) {
                cv_disc(x, yy, 4, K_BELL_T);
                cv_disc(x, yy, 2, K_TEXT);
            } else if (in) {
                cv_rect(x - 1, yy, 3, 1, K_DIM);
                cv_rect(x, yy - 1, 1, 3, K_DIM);
            } else {
                cv_rect(x, yy, 1, 1, K_LINE);
            }
            if (i == 0)
                cv_text(x - 3, yy + 5, &FONT_XS, EN_NOTE_NAME[pc], K_DIM);   /* the root, named */
        }
    }
    for (k = 0; k < EN_NLOOP; k++) {             /* the orbits: the bell loops */
        const float rx = 34.0f + 15.0f * (float)k, ry = 10.0f + 6.0f * (float)k;
        const int len = EN_LOOP_LEN[k];
        int st;
        for (st = 0; st < len; st++) {
            float a = 6.2832f * (float)st / (float)len;
            int32_t x = 120 + (int32_t)(rx * fm_cosf(a)), yy = HZ - (int32_t)(ry * fm_sinf(a));
            if (yy >= HZ)
                continue;                        /* the far side: under the horizon */
            if (en_loop_note[k][st] >= 0)
                cv_rect(x - 1, yy - 1, 2, 2, K_BELL);
            else if (!(st & 1))
                cv_rect(x, yy, 1, 1, K_BELL_T);
        }
        {
            float a = 6.2832f * (float)en_loop_pos[k] / (float)len, g = en_loop_glow[k];
            int32_t x = 120 + (int32_t)(rx * fm_cosf(a)), yy = HZ - (int32_t)(ry * fm_sinf(a));
            if (yy < HZ) {
                if (g > 0.05f)
                    cv_disc(x, yy, 3 + (int32_t)(4.0f * g), mixc(K_SKY_LO, K_BELL, (int)(160.0f * g)));
                cv_disc(x, yy, 2, g > 0.3f ? K_TEXT : K_BELL);
            }
        }
    }
    {                                            /* the moon: the bass */
        int32_t r = 5 + (int32_t)(8.0f * fm_clampf(en_bass_level * 1.5f, 0.0f, 1.0f));
        cv_disc(206, HZ - 2, r + 3, mixc(K_SKY_LO, K_BASS_T, 160));
        cv_disc(206, HZ - 2, r, K_BASS);
    }
    for (y = HZ; y < (int32_t)MAIN_H; y++)       /* the water */
        cv_rect(0, y, 240, 1, mixc(K_SKY_LO, K_WATER, (y - HZ) * 256 / (MAIN_H - HZ) + 96 > 256 ? 256 : (y - HZ) * 256 / (MAIN_H - HZ) + 96));
    cv_rect(0, HZ, 240, 1, K_LINE);
    for (y = HZ + 3; y < HZ + 30; y += 3) {      /* the moon's path on the water */
        int32_t w = 12 - (y - HZ) / 4;
        cv_rect(206 - w / 2 + (int32_t)(2.0f * fm_sinf((float)(y + (int32_t)ui.frame) * 0.4f)), y, w, 1, K_BASS_T);
    }
    for (k = 0; k < EN_NDRUM; k++) {             /* the pulse: ripples */
        static const int32_t RX[EN_NDRUM] = {64, 120, 172};
        float g = en_drum_glow[k];
        if (g > 0.04f && !en_pulse_rest) {
            float r = 4.0f + 26.0f * (1.0f - g);
            int st;
            for (st = 0; st < 28; st++) {
                float a = 6.2832f * (float)st / 28.0f;
                cv_rect(RX[k] + (int32_t)(r * fm_cosf(a)), 120 + (int32_t)(r * 0.3f * fm_sinf(a)), 1, 1,
                        mixc(K_WATER, K_PULSE, (int)(230.0f * g)));
            }
        }
        cv_rect(RX[k] - 1, 120, 3, 1, K_PULSE_T);
    }
    {                                            /* words: the part and the era, the chord, the last effect */
        char cn[16];                             /* (no parts, no eras on the screen: it moves on by itself) */
        {
            int pcs[EN_NPAD], nn = en_chord_n, z;
            for (z = 0; z < nn && z < EN_NPAD; z++)
                pcs[z] = en_chord[z] % 12;
            en_chord_name(pcs, nn, cn);
            cv_text(6, 4, &FONT_B, cn, K_TEXT);
        }
        if (en_fx_last >= 0 && plat_ms() - ui.fx_seen_t < 5000u)
            cv_text(232 - text_w(&FONT_XS, EN_FX_NAME[en_fx_last]), 18, &FONT_XS, EN_FX_NAME[en_fx_last], K_ACC);
    }
}

/* THE GUIDE (GLO, a third time): every button and shortcut on one page, always on the FM-1 */
static const char *const GUIDE[][2] = {
    {"KEYS", "scale, strum or play (SEQ 6)"},
    {"HOME", "garden, journey, picture"},
    {"ENV ARP LFO", "pad, bells, bass"},
    {"SEQ", "pulse, the lead, keys"},
    {"SEL", "space, tape, air"},
    {"FX", "effects; FX + key anywhere"},
    {"GLO", "setup, sleep, picture, guide"},
    {"EDIT", "hold: RANDOM"},
    {"SAVE", "save; hold: undo random"},
    {"REC / PLAY", "freeze / rest or grow"},
    {"OCT- OCT+", "the page's layer's octave"},
    {"SELECT", "every page (HOME+: tempo)"},
};
#define NGUIDE (int)(sizeof GUIDE / sizeof GUIDE[0])
#define GUIDE_MAIN 9                             /* lines in the middle; the rest, and the CPU, below it */
static void draw_guide(void)                    /* THE GUIDE (GLO, the last page): every button and shortcut */
{
    int k;
    for (k = 0; k < GUIDE_MAIN; k++) {
        const int32_t y = 4 + k * 15;
        cv_text(6, y, &FONT_XS, GUIDE[k][0], K_ACC);
        cv_text(86, y, &FONT_XS, GUIDE[k][1], K_TEXT);
    }
}
static void draw_guide_rest(void)               /* in the knobs' place (the Guide has none) */
{
    int k;
    char c[12];
    for (k = GUIDE_MAIN; k < NGUIDE; k++) {
        const int32_t y = 12 + (k - GUIDE_MAIN) * 14;
        cv_text(6, y, &FONT_XS, GUIDE[k][0], K_ACC);
        cv_text(86, y, &FONT_XS, GUIDE[k][1], K_TEXT);
    }
    itoa_u(plat_cpu_pct(), c);
    {
        char *e = c;
        while (*e)
            e++;
        e[0] = '%';
        e[1] = 0;
    }
    cv_text(6, 12 + (NGUIDE - GUIDE_MAIN) * 14, &FONT_XS, "CPU", K_DIM);
    cv_text(86, 12 + (NGUIDE - GUIDE_MAIN) * 14, &FONT_XS, c, plat_cpu_pct() > 85u ? K_PAD : K_DIM);
}

/* THE FX MENU: all 27 on one page, three columns of nine, each under its key (F3 .. G5); a sounding one lit, a kept
 * one with a dot */
static void draw_fxmenu(void)
{
    int k;
    char t[48];
    copy_s(t, en_busy ? "BUSY: WAIT A MOMENT" : "FX AGAIN: BACK    HOLD FX: ALL OFF");
    cv_text(6, 1, &FONT_XS, t, en_busy ? K_PAD : K_DIM);
    for (k = 0; k < EN_NFX; k++) {
        const int on = (int)(en_fx_on >> k & 1u), kept = (int)(en_fx_kept >> k & 1u);
        const int32_t x = 2 + (k / 9) * 79, y = 14 + (k % 9) * 14;
        cv_round(x, y, 77, 13, 4, on ? K_PANEL : K_BG);
        cv_text(x + 2, y + 1, &FONT_XS, EN_NOTE_NAME[(5 + k) % 12], K_LINE);
        cv_text(x + 17, y + 1, &FONT_XS, EN_FX_NAME[k], on ? K_ACC : K_TEXT);
        if (kept)
            cv_disc(x + 70, y + 6, 3, K_ACC);
    }
}
/* a button's card (help_tick): its name, then what it does, in the garden's place. A line starting with a
 * space goes on from the one before (a screen reader joins them: sp_line) */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE GARDEN"); L("AGAIN: THE JOURNEY,"); L(" WANDER, TIDE, KEY MOVES"); L("AGAIN: THE PICTURE"); break;
    case B_ENV: L("THE PAD"); L("LEVEL, SOUND, SWELL, BRIGHT"); L("AGAIN: WARMTH, MOVEMENT,"); L(" SPACE, WIDTH"); break;
    case B_ARP: L("THE BELLS"); L("LEVEL, SOUND, DENSITY, TONE"); L("AGAIN: OCTAVE, SHIMMER,"); L(" SPACE, DECAY"); break;
    case B_LFO: L("THE BASS"); L("LEVEL, SOUND, OCTAVE, GLIDE"); L("AGAIN: FIFTH, WARMTH,"); L(" MOVEMENT, SPACE"); break;
    case B_SEQ: L("THE PULSE: LEVEL, KIT, HITS"); L("AGAIN: ITS SHAPE"); L("AGAIN: THE LEAD, A MELODY"); L("AGAIN: THE LEAD'S SHAPE"); break;
    case B_FX: L("FX: THE KEYS PLAY EFFECTS"); L("HOLD FX + A KEY: ANY PAGE"); L("IN THE MENU, HOLD FX: ALL OFF"); L("FX AGAIN: BACK"); break;
    case B_SEL: L("THE SPACE: ECHO, REVERB"); L("AGAIN: SHIMMER, WARMTH,"); L(" TAPE, WIDTH"); L("AGAIN: THE AIR"); break;
    case B_GLO: L("SETUP: ROOT, TUNE, MIDI,"); L(" LIGHTS"); L("AGAIN: THE PICTURE"); L("AGAIN: THE GUIDE"); break;
    case B_EDIT: L("HOLD: RANDOM"); L("NEW SOUNDS, KEY, SPACE"); L("UNDO: HOLD SAVE"); L("(IT MOVES ON BY ITSELF)"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L(" AND THE REVERB RINGS ON"); L("AGAIN: THAW"); break;
    case B_OCTDN: L("AN OCTAVE DOWN: THE PAGE'S"); L(" LAYER (ELSEWHERE THE BELLS)"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("AN OCTAVE UP: THE PAGE'S"); L(" LAYER (ELSEWHERE THE BELLS)"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT WORLDS"); L("KEEP HOLDING: THE WORLD"); L(" BACK TO ITS RECIPE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_rect(0, 0, 240, MAIN_H, K_PANEL);         /* the whole middle of the screen */
    cv_rect(0, 0, 240, 30, K_ACC);
    text_c(120, 6, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++) {                    /* the big font where the line fits, else the smaller */
        const char *t = l[k] + (l[k][0] == ' ');
        const felucca_font_t *f = text_w(&FONT_B, t) <= 232 ? &FONT_B : &FONT_S;
        text_c(120, 42 + k * 25, f, t, K_TEXT);
    }
}

/* one band of an area: the canvas (48 rows) as the area's rows 48 k .. 48 k + h - 1, drawn in the area's own
 * coordinates (gfx.c clips to cv_y0 .. cv_y1 after adding cv_oy) */
#define BAND_H 48u
static void band_begin(int k, uint32_t h)
{
    cv_begin(240, h, K_BG);
    cv_oy = -(int32_t)BAND_H * k;
    cv_y0 = 0;
    cv_y1 = (int32_t)h;
}
#define NBANDS(h) (((h) + BAND_H - 1u) / BAND_H)
#define BAND_ROWS(h, k) ((h) - BAND_H * (uint32_t)(k) < BAND_H ? (h) - BAND_H * (uint32_t)(k) : BAND_H)

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, pool_abs() | (uint32_t)proj.par[P_ROOT] << 12 | (uint32_t)en_frozen << 16 | proj.piece << 24 |
                                        (uint32_t)en_playing << 17 | (uint32_t)proj.par[P_MODE] << 20), i;
    if (ui.frame % 3u)                           /* the garden moves slowly: 20 frames a second is plenty */
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            int k;
            ui.sig[1] = h;
            for (k = 0; k < (int)NBANDS(MAIN_H); k++) {
                band_begin(k, BAND_ROWS(MAIN_H, k));
                draw_help();
                cv_blit(0, MAIN_Y + (uint32_t)k * BAND_H);
            }
        }
        return;
    }
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < EN_NLOOP; i++) {
        uint32_t s;
        h = hash(h, en_loop_pos[i] | (uint32_t)q16(en_loop_glow[i]) << 8);
        for (s = 0; s < EN_LOOP_LEN[i]; s++)
            h = hash(h, (uint32_t)(en_loop_note[i][s] >= 0));
    }
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, en_drum_pos[i] | (uint32_t)q16(en_drum_glow[i]) << 8 | en_drum_pat[i] << 13 | (uint32_t)en_pulse_rest << 31);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | (uint32_t)q16(en_pad_level) << 8 | (uint32_t)q16(en_bass_level) << 13);
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[i]);
    h = hash(h, en_root_now | (uint32_t)en_part << 8 | (uint32_t)en_eras << 12 | (uint32_t)ui.view << 24);
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, (uint32_t)q16(en_pad_amp[i]));
    h = hash(h, en_fx_on ^ en_fx_kept << 5 ^ (uint32_t)(ui.view == V_FX ? (uint32_t)proj.par[P_FXON] << 26 ^ (uint32_t)en_busy << 24 : 0u) ^ (uint32_t)(plat_ms() - ui.fx_seen_t < 5000u) << 31);
    h = hash(h, ui.frame / 6u);                  /* the aurora sways */
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    for (i = 0; i < NBANDS(MAIN_H); i++) {       /* the garden in bands (the canvas is 48 rows) */
        band_begin((int)i, BAND_ROWS(MAIN_H, i));
        if (ui.view == V_FX) {
            cv_rect(0, 0, 240, MAIN_H, K_BG);
            draw_fxmenu();
        } else if (ui.view == V_GUIDE) {             /* (its CPU line: redrawn as it changes, below) */
            cv_rect(0, 0, 240, MAIN_H, K_BG);
            draw_guide();
        } else {
            draw_sky();
        }
        if (en_frozen) {                         /* the way to the next chord, top right (held: frozen) */
            cv_text(232 - text_w(&FONT_XS, "held"), 4, &FONT_XS, "held", K_FROST);
        } else if (ui.view != V_FX && ui.view != V_GUIDE) {
            int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
            cv_round(232 - w, 8, w, 6, 3, K_LINE);
            if (f > 0)
                cv_round(232 - w, 8, f < 6 ? 6 : f, 6, 3, K_PAD);
        }
        cv_blit(0, MAIN_Y + i * BAND_H);
    }
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until, band;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(knob_at(ui.view, (int)i)));
    h = hash(h, (uint32_t)now_touch | (uint32_t)proj.par[P_KEYS] << 1 | (ui.view == V_GUIDE ? plat_cpu_pct() << 4 : 0u));
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    for (band = 0; band < (int)NBANDS(KNB_H); band++) {
    band_begin(band, BAND_ROWS(KNB_H, band));
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[12];
        int k;
        int32_t xd = 237 - NVIEWS * 5, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < NVIEWS; k++)
            cv_round(xd + k * 5, 3, 3, 4, 1, k == ui.view ? K_ACC : VIEW_BTN[k] == VIEW_BTN[ui.view] ? K_DIM : K_LINE);
    }
    if (ui.view == V_GUIDE)
        draw_guide_rest();
    for (i = 0; i < 4 && ui.view != V_GUIDE; i++) {
        int k = knob_at(ui.view, (int)i), lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_PAD;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, label_at(ui.view, (int)i), sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6 && k != P_PADTONE) {   /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        {                                        /* a long name in a smaller face, to stay in its cell */
            const felucca_font_t *f = text_w(&FONT_B, t) <= 56 ? &FONT_B : text_w(&FONT_S, t) <= 58 ? &FONT_S : &FONT_XS;
            text_c(cx, f == &FONT_B ? 50 : f == &FONT_S ? 52 : 54, f, t, hot ? K_ACC : K_TEXT);
        }
    }
    cv_blit(0, KNB_Y + (uint32_t)band * BAND_H);
    }
}

/* the picture, full screen: one pass computes a frame (vis_step), the next sends it in strips of 24 rows (the canvas
 * is shared with the pages; nothing else draws while it is up): about 30 pictures a second */
#define PIC_STRIP 24u
static void draw_picture(void)
{
    uint32_t now = plat_ms(), y;
    if (ui.pic_phase == 0) {
        float dt = (float)(now - ui.vis_ms) * 0.001f;
        ui.vis_ms = now;
        vis_step(fm_clampf(dt, 0.005f, 0.08f));
        ui.pic_phase = 1;
        return;
    }
    ui.pic_phase = 0;
    for (y = 0; y < 240u; y += PIC_STRIP) {
        lcd_sync();
        cv_w = 240;
        cv_h = PIC_STRIP;
        vis_scan(cv_px, (int)y, (int)PIC_STRIP);
        if (ui.title_until) {                    /* the title, over the stars and the rising sun */
            uint32_t t = 3800u - (ui.title_until - now);
            cv_oy = -(int32_t)y;
            cv_y0 = 0;
            cv_y1 = (int32_t)PIC_STRIP;
            if (t > 500u)
                text_c(120, 88, &FONT_L, "ZENITH-1", t > 900u ? K_TEXT : K_DIM);
            if (t > 1300u)
                text_c(120, 132, &FONT_S, "by Polystellarflow", t > 1700u ? K_ACC : K_PAD_T);
            if (t > 2300u)
                text_c(120, 204, &FONT_XS, "VERSION " APP_VERSION, K_DIM);   /* which build is in */
            cv_oy = 0;
        }
        lcd_blit(0, y, 240, PIC_STRIP, cv_px);
    }
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    b |= 1u << VIEW_BTN[ui.view];
    if (ui.pic)
        b |= 1u << B_HOME;                       /* the picture is HOME's */
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (proj.leds)
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; a help card as it comes up; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: Drift in minutes and seconds (the screen's "1:05", "45s"), the rest as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    if (k == P_DRIFT) {
        uint32_t m = 0, s = 0, i;
        for (i = 0; t[i] >= '0' && t[i] <= '9'; i++)
            s = s * 10u + (uint32_t)(t[i] - '0');
        if (t[i] == ':') {
            m = s;
            s = 0;
            for (i++; t[i] >= '0' && t[i] <= '9'; i++)
                s = s * 10u + (uint32_t)(t[i] - '0');
        }
        if (m) {
            sp_int(m);
            sp_cat(m == 1u ? " minute" : " minutes");
            if (!s)
                return;
            sp_cat(" ");
        }
        sp_int(s);
        sp_cat(s == 1u ? " second" : " seconds");
        return;
    }
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Swell 60": the name the page gives it, then its value */
{
    sp_cat(label_at(view, (int)i));
    sp_cat(" ");
    sp_value(knob_at(view, (int)i));
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, growing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    for (i = 0; i < 4; i++) {
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". World ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[en_root_now % 12u]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_GARDEN) {          /* (the Garden's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", growing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (batt_words(batt())) {
        sp_cat(". ");
        sp_cat(batt_words(batt()));
    }
}

/* the garden: the chord and how far to the next, the scale from the root up ("F and F sharp": one letter's
 * two notes joined, so they are heard as two), the pulse sitting out */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", frozen");
    } else {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    }
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = ((int)en_root_now % 12 + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
    if (en_pulse_rest && proj.par[P_PULSE])
        sp_cat(". Pulse resting");
}

/* a help card: its button, then its lines ("ENV button. THE PAD. LEVEL, SWELL, BRIGHT, COLOUR") */
static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')                             /* it goes on from the line before */
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "WORLD Glass, A Pent maj, tempo 80" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("WORLD ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(knob_at(ui.view, (int)i));
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            sp_garden();
        }
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "GROWING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {                         /* a help card: at once */
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get(knob_at(ui.view, (int)i));
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef PLAT_HOST
/* every page, knob value, help card, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"GROWING", 0}, {"FREEZE", "ON"}, {"FREEZE", "OFF"}, {"SAVED", 0},
                                         {"NEW ERA", 0}, {"NEXT PART", 0}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else if (k == K_LEDS)
                    proj.leds = (uint8_t)x;
                else if (k == K_PICMODE)
                    proj.pic_mode = (int8_t)(x - 2);
                else if (k == K_PICPAL)
                    proj.pic_pal = (int8_t)(x - 2);
                else if (k == K_PICAFTER)
                    proj.pic_after = (uint8_t)x;
                else if (k == K_PICGLOW)
                    proj.pic_glow = (uint8_t)x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: growing, resting, unsaved, a clock, frozen, custom */
        ui.view = (uint8_t)(x * 3 % NVIEWS);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -2; x <= 2; x++) {
        char o[8];
        en_param_text(P_BOCT, x, o);
        msg_text("BELL OCTAVE", o, t);
        sp_print("MSG", t);
    }
    for (x = 30; x <= 120; x += 45) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 72");
    for (x = 0; x <= 4; x++)                          /* the battery, at the end of WHERE's first part */
        sp_print("WHERE", batt_words(x));
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.hp = -2;
    lcd_fill(0, 0, 240, 240, K_BG);
    pic_apply();
    /* a new seed every power-on: the tick counter and the battery's last bits, never the same twice */
#ifdef PLAT_HOST
    en_seed(plat_ticks24() ^ plat_ms() * 2654435761u);
#else
    en_seed(plat_ticks24() ^ plat_ms() * 2654435761u);
#endif
    ui.journey_seen = en_journey_n;
    ui.fx_seen = en_fx_count;
    ui.title_until = plat_ms() + 3800u;          /* the title: ZENITH-1, by Polystellarflow */
    vis_title(1);
    if (!en_playing)
        en_play(1);                              /* it grows from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

void ui_frame(void)
{
#ifdef PLAT_WEB
    int hp = 1;                                  /* the emulator: a computer's output, as headphones */
#else
    int hp = plat_headphones();
#endif
    input();
    en_cpu((int)plat_cpu_pct());                 /* the engine refuses new effects when the CPU is near full */
    if (en_heals != ui.heals_seen) {             /* the sound blew up and was rebuilt (it should never) */
        ui.heals_seen = en_heals;
        warn("SOUND RESET", "(TELL THE AUTHOR)");
    }
    if (en_sheds != ui.sheds_seen) {             /* it ran out: the effects and the lead stopped for a few seconds */
        ui.sheds_seen = en_sheds;
        warn("TOO BUSY", "FX STOPPED");
    }
    if (hp != ui.hp) {                           /* the jack: the speaker's ceiling, or headphones' */
        ui.hp = (int8_t)hp;
        en_out(hp == 1);
    }
    autosave();
    if (en_journey_n != ui.journey_seen) {       /* the journey has dissolved this world: the next, at dawn */
        ui.journey_seen = en_journey_n;
        if (en_journey_to >= 0 && en_journey_to < NPIECES && en_journey_to != proj.piece)
            set_piece(en_journey_to);
    }
    if (en_fx_count != ui.fx_seen) {
        ui.fx_seen = en_fx_count;
        ui.fx_seen_t = plat_ms();
    }
    if (ui.title_until && (int32_t)(plat_ms() - ui.title_until) >= 0) {
        ui.title_until = 0;
        vis_title(0);
        pic_show(0);
        ui.act_t = plat_ms();
    }
    if (ui.title_until) {
        ui.pic = 1;                              /* the title is drawn as a picture */
        draw_picture();
        leds();
        ui.frame++;
        return;
    }
    if (ui.help)
        pic_show(0);
    if (!ui.pic && proj.pic_after && !ui.help && plat_ms() - ui.act_t > PIC_AFTER_MS[proj.pic_after])
        pic_show(1);                             /* nothing touched for a while: the picture */
    if (ui.pic) {
        draw_picture();
    } else {
        draw_header();
        draw_main();
        draw_knobs();
    }
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }

#ifdef APP_OVERLOAD
void app_overload(void) { en_overload(); }       /* the audio interrupt: over 90 % of its time, shed what can go */
#endif
