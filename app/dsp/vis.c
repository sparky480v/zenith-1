/* SPDX-License-Identifier: MPL-2.0
 * ZENITH-1: the picture (vis.h), after HADAL-1's (Polystellarflow): a mesh of 13 x 13 points moves an 8-bit field in
 * fixed point (the MilkDrop way: each frame the last one, warped and a little faded, then new marks drawn into it),
 * and a 256-entry palette colours it. What moves it is the music: the bells drop ripples where they ring (the note's
 * place on the circle of twelve, the loop's distance from the middle), the pad's swell glows, the bass breathes the
 * zoom, a new chord turns the colours a little, and a freeze holds it all. Each world has its colours and its own few
 * pictures; one melts into the next every minute or so. -O2. */
#include "vis.h"
#include "zenith.h"
#include "fastmath.h"

#if defined EN_HOST || defined PLAT_HOST
#define POOLD
#else
#define POOLD __attribute__((section(".pool")))
#endif
#define VW 120
#define GRID 12
#define GN (GRID + 1)
static uint8_t fld[2][VW * VW] POOLD;
static uint8_t *A, *Bf;
static int32_t mx[GN * GN], my[GN * GN];
static uint16_t lut[256];
static float cur[18], hue, hue_tgt, vt, glow_e;
static int pal_now = -1, pal_tick, bright, mode_auto = 2, mode, pal_fix = -2, title, pal_roam = -1;
static float pal_left = 20.0f;              /* Auto colours: to the next set */
static float title_t;
static uint32_t bev_r, chords_seen;

const char *const VIS_MODE[VIS_NMODE] = {"Nebula", "Aurora", "Mandala", "Tunnel", "Star drift", "Orbits", "Ripples",
                                         "Horizon", "Bloom", "Silk", "Embers", "Spiral", "Petals", "Halo", "Veils",
                                         "Dawn"};
enum { M_NEBULA, M_AURORA, M_MANDALA, M_TUNNEL, M_STARS, M_ORBITS, M_RIPPLES, M_HORIZON, M_BLOOM, M_SILK, M_EMBERS,
       M_SPIRAL, M_PETALS, M_HALO, M_VEILS, M_DAWN };
const char *const VIS_PAL[VIS_NPAL] = {"Nebula", "Dusk", "Aurora", "Golden", "Moonlight", "Sakura", "Dawn", "Ocean",
                                       "Lo-fi"};
/* six stops each, dark to light: warm and spacey, never cold black-and-white */
static const uint8_t PALS[VIS_NPAL][18] = {
    {0, 0, 0, 20, 8, 40, 70, 24, 110, 180, 60, 130, 250, 150, 120, 255, 235, 200},
    {0, 0, 0, 30, 10, 20, 110, 30, 50, 210, 90, 60, 250, 170, 90, 255, 240, 210},
    {0, 0, 0, 4, 20, 30, 10, 80, 90, 60, 180, 140, 170, 120, 220, 240, 230, 255},
    {0, 0, 0, 30, 16, 6, 110, 60, 20, 220, 140, 50, 255, 200, 110, 255, 248, 220},
    {0, 0, 0, 8, 12, 30, 30, 50, 100, 90, 120, 180, 180, 200, 235, 245, 248, 255},
    {0, 0, 0, 26, 10, 24, 90, 40, 80, 200, 110, 150, 250, 190, 200, 255, 245, 245},
    {0, 0, 0, 20, 10, 30, 80, 40, 90, 230, 110, 70, 255, 190, 100, 255, 245, 200},
    {0, 0, 0, 2, 18, 30, 10, 60, 90, 40, 140, 160, 150, 220, 210, 240, 255, 250},
    {0, 0, 0, 26, 14, 18, 90, 50, 50, 190, 120, 90, 240, 200, 150, 255, 245, 225},
};
/* each world: its colours and its pictures */
static const uint8_t WORLD_PAL[EN_NPIECES] = {0, 3, 8, 2, 5, 6, 7, 4};
static const uint8_t WORLD_MODES[EN_NPIECES][4] = {
    {M_NEBULA, M_STARS, M_SPIRAL, M_HALO},   {M_DAWN, M_VEILS, M_TUNNEL, M_BLOOM},
    {M_SILK, M_EMBERS, M_HORIZON, M_RIPPLES}, {M_AURORA, M_MANDALA, M_RIPPLES, M_STARS},
    {M_PETALS, M_RIPPLES, M_SILK, M_HORIZON}, {M_DAWN, M_MANDALA, M_ORBITS, M_HALO},
    {M_HORIZON, M_VEILS, M_BLOOM, M_RIPPLES}, {M_ORBITS, M_NEBULA, M_HALO, M_BLOOM},
};

static struct { float x, y, r, v, a; uint8_t on; } rings[12];
static struct { float x, y, v, ph; } dust[32];
static uint8_t dabs[3][81];
static const int DAB_D[3] = {3, 5, 9};
static uint32_t vrng = 0x2468ACEu;
static float vr(void)
{
    vrng ^= vrng << 13;
    vrng ^= vrng >> 17;
    vrng ^= vrng << 5;
    return (float)(vrng >> 8) * (1.0f / 16777216.0f);
}
static inline float sn(float x) { return fm_sinf(x); }

static int world(void) { return en_piece_world(); }

static void palette_build(void)
{
    static const int X[6] = {0, 34, 82, 138, 198, 255};
    int v, k = 0, hk = (int)(fm_clampf(hue, -1.0f, 1.0f) * 110.0f), ha = hk < 0 ? -hk : hk;
    for (v = 0; v < 256; v++) {
        float u;
        int r, g, b, rr, gg, bb;
        uint32_t c;
        while (k < 4 && v > X[k + 1])
            k++;
        u = (float)(v - X[k]) / (float)(X[k + 1] - X[k]);
        rr = (int)(cur[k * 3] + (cur[k * 3 + 3] - cur[k * 3]) * u);
        gg = (int)(cur[k * 3 + 1] + (cur[k * 3 + 4] - cur[k * 3 + 1]) * u);
        bb = (int)(cur[k * 3 + 2] + (cur[k * 3 + 5] - cur[k * 3 + 2]) * u);
        /* a slow turn of the colours, a little at each new chord: warm towards rose, or towards gold */
        r = (rr * (256 - ha) + (hk > 0 ? rr : gg) * ha) >> 8;
        g = (gg * (256 - ha) + (hk > 0 ? bb : rr) * ha) >> 8;
        b = (bb * (256 - ha) + (hk > 0 ? gg : bb) * ha) >> 8;
        if (bright) {                    /* lift the dark half, for daylight */
            int lift = bright * 30, mul = 256 + bright * 60;
            r = (r * mul >> 8) + (lift * (255 - r) >> 8) * v / 255;
            g = (g * mul >> 8) + (lift * (255 - g) >> 8) * v / 255;
            b = (b * mul >> 8) + (lift * (255 - b) >> 8) * v / 255;
        }
        r = r > 255 ? 255 : r;
        g = g > 255 ? 255 : g;
        b = b > 255 ? 255 : b;
        c = (uint32_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
        lut[v] = (uint16_t)((c >> 8) | (c & 255u) << 8);
    }
}
static void palette_step(void)
{
    int i, tgt = pal_fix >= 0 ? pal_fix : pal_fix == -2 && pal_roam >= 0 ? pal_roam : WORLD_PAL[world()];
    if (pal_now < 0) {
        for (i = 0; i < 18; i++)
            cur[i] = PALS[tgt][i];
        pal_now = tgt;
        palette_build();
        return;
    }
    if (++pal_tick < 3)
        return;
    pal_tick = 0;
    for (i = 0; i < 18; i++)
        cur[i] += ((float)PALS[tgt][i] - cur[i]) * 0.04f;   /* a few seconds from one world's colours to the next */
    pal_now = tgt;
    hue += (hue_tgt - hue) * 0.02f;
    palette_build();
}

void vis_init(void)
{
    int s, i, x, y;
    A = fld[0];
    Bf = fld[1];
    for (i = 0; i < VW * VW; i++)
        fld[0][i] = fld[1][i] = 0;
    for (s = 0; s < 3; s++) {
        int d = DAB_D[s];
        float c = (float)(d - 1) * 0.5f;
        for (y = 0; y < d; y++)
            for (x = 0; x < d; x++) {
                float dx = (float)x - c, dy = (float)y - c, r = fm_sqrtf(dx * dx + dy * dy) / (c + 0.6f);
                dabs[s][y * d + x] = r < 1.0f ? (uint8_t)(255.0f * (1.0f - r) * (1.0f - r)) : 0;
            }
    }
    for (i = 0; i < 32; i++) {
        dust[i].x = vr() * VW;
        dust[i].y = vr() * VW;
        dust[i].v = 0.08f + vr() * 0.3f;
        dust[i].ph = vr() * 6.283f;
    }
    pal_now = -1;
    mode = WORLD_MODES[world()][0];
}
void vis_bright(int b)
{
    bright = b < 0 ? 0 : b > 2 ? 2 : b;
    pal_tick = 99;
}
void vis_set_pal(int p) { pal_fix = p >= 0 && p < VIS_NPAL ? p : p == -2 ? -2 : -1; }
static int m_from = -1;
static float m_w = 1.0f, next_mode = 60.0f;
void vis_set_mode(int m)
{
    mode_auto = m == -2 ? 2 : m < 0;          /* 2: Auto, any picture; 1: the world's own */
    if (m >= 0 && m < VIS_NMODE && m != mode) {
        m_from = mode;
        mode = m;
        m_w = 0.0f;
    }
}
int vis_mode(void) { return mode; }
void vis_title(int on)
{
    int i;
    title = on;
    title_t = 0.0f;
    if (on) {
        for (i = 0; i < VW * VW; i++)
            fld[0][i] = fld[1][i] = 0;
        m_from = -1;
        m_w = 1.0f;
        mode = M_STARS;
        pal_now = -1;
    } else if (mode_auto) {
        m_from = mode;
        mode = mode_auto == 2 && vr() < 0.5f ? (int)(vr() * VIS_NMODE) % VIS_NMODE : WORLD_MODES[world()][0];
        m_w = 0.0f;
    }
}

/* how the field moves at (u, v) in -1..1, r its distance from the middle: dx, dy in field pixels a frame */
static void field(int m, float u, float v, float r, float t, float en, float *ox, float *oy)
{
    float dx = 0.0f, dy = 0.0f;
    switch (m) {
    case M_AURORA:              /* curtains, rising and folding */
        dx = 0.5f * sn(v * 3.0f + t * 0.1f);
        dy = -0.9f - 0.8f * sn(u * 3.1f + t * 0.2f + sn(u * 7.0f - t * 0.1f) * 1.4f);
        break;
    case M_MANDALA: {           /* breathing petals (mirrored) */
        float br = 0.7f * sn(t * 0.2f) + 0.4f * sn(r * 9.0f - t * 0.4f);
        dx = u * br - v * 0.8f;
        dy = v * br + u * 0.8f;
        break;
    }
    case M_TUNNEL:              /* falling slowly forward */
        dx = -u * (1.4f + 1.2f * en) + 0.3f * sn(t * 0.2f);
        dy = -v * (1.4f + 1.2f * en) + 0.3f * sn(t * 0.17f);
        break;
    case M_STARS:               /* out from the middle */
        dx = u * (1.2f + 1.4f * en);
        dy = v * (1.2f + 1.4f * en);
        break;
    case M_ORBITS: {            /* a steady wheel */
        float q = 0.55f + 0.3f * en;
        dx = -v * q * 1.4f;
        dy = u * q * 1.4f;
        break;
    }
    case M_RIPPLES: {           /* two sources, interfering */
        float ua = u - 0.45f, ub = u + 0.45f, a1 = fm_sqrtf(ua * ua + v * v), a2 = fm_sqrtf(ub * ub + v * v);
        float w1 = sn(a1 * 15.0f - t * 1.1f), w2 = sn(a2 * 15.6f - t * 1.14f);
        dx = 0.9f * (w1 * ua / (a1 + 0.1f) + w2 * ub / (a2 + 0.1f));
        dy = 0.9f * (w1 * v / (a1 + 0.1f) + w2 * v / (a2 + 0.1f));
        break;
    }
    case M_HORIZON:             /* light running sideways over water (reflected) */
        dx = 1.0f + 0.4f * sn(v * 9.0f + t * 0.3f);
        dy = 0.35f * sn(u * 6.0f - t * 0.25f + v * 3.0f);
        break;
    case M_BLOOM: {             /* breathing in and out, turning one way, then the other */
        float br = 0.8f * sn(t * 0.3f) + 0.8f * en, tw = 0.5f * sn(t * 0.11f);
        dx = u * br - v * tw + 0.2f * sn(v * 7.0f + t * 0.5f);
        dy = v * br + u * tw + 0.2f * sn(u * 7.0f - t * 0.5f);
        break;
    }
    case M_SILK:                /* ink in slow water */
        dx = 0.7f * sn(v * 5.0f + t * 0.09f) + 0.3f * sn(u * 9.0f + v * 4.0f + t * 0.13f);
        dy = 0.7f * sn(u * 5.0f - t * 0.08f) + 0.3f * sn(v * 9.0f - u * 4.0f + t * 0.11f);
        break;
    case M_EMBERS:              /* warm air rising */
        dx = 0.35f * sn(v * 8.0f + t * 0.9f + u * 2.0f);
        dy = -0.9f - 0.4f * en;
        break;
    case M_SPIRAL: {            /* everything turning into itself */
        float pull = -0.35f - 0.4f * en;
        dx = u * pull - v * 1.2f;
        dy = v * pull + u * 1.2f;
        break;
    }
    case M_PETALS:              /* a soft breeze */
        dx = 0.45f * sn(v * 4.0f + t * 0.07f) + 0.2f * sn(u * 13.0f + t * 0.1f);
        dy = 0.3f + 0.3f * sn(u * 3.0f - t * 0.06f);
        break;
    case M_HALO: {              /* the skin of a struck gong */
        float w = sn(r * 12.0f - t * 1.2f) * (0.6f + 1.2f * en);
        dx = u / r * w;
        dy = v / r * w;
        break;
    }
    case M_VEILS:               /* slowly sagging, each fold its own pace */
        dx = 0.3f * sn(v * 5.0f + t * 0.2f) - u * 0.1f;
        dy = 0.6f + 0.4f * sn(u * 6.5f + t * 0.27f) + 0.25f * sn(u * 15.0f - t * 0.4f) + 0.3f * en;
        break;
    case M_DAWN:                /* rising, softly, spreading from the sun */
        dx = 0.35f * sn(v * 6.0f + t * 0.15f) + u * 0.25f;
        dy = -0.35f + 0.2f * sn(u * 5.0f + t * 0.2f);
        break;
    default: {                  /* NEBULA: a slow whirl around a pull */
        float pull = -0.45f / (r * r + 0.15f), swirl = 0.9f / (r + 0.25f);
        dx = (u * pull - v * swirl * 0.55f) * (1.0f + 0.5f * en);
        dy = (v * pull + u * swirl * 0.55f) * (1.0f + 0.5f * en);
        break;
    }
    }
    *ox = dx;
    *oy = dy;
}

static void mesh(float en, int frozen)
{
    float t = vt, fz = frozen ? 0.0f : 1.0f, ease = m_w * m_w * (3.0f - 2.0f * m_w);
    int gx, gy;
    for (gy = 0; gy < GN; gy++)
        for (gx = 0; gx < GN; gx++) {
            float u = (float)gx / GRID * 2.0f - 1.0f, v = (float)gy / GRID * 2.0f - 1.0f, r = fm_sqrtf(u * u + v * v) + 1e-4f;
            float dx, dy;
            int j = gy * GN + gx;
            field(mode, u, v, r, t, en, &dx, &dy);
            if (m_from >= 0 && ease < 1.0f) {   /* one picture's motion melting into the next */
                float ax, ay;
                field(m_from, u, v, r, t, en, &ax, &ay);
                dx = ax + (dx - ax) * ease;
                dy = ay + (dy - ay) * ease;
            }
            mx[j] = (int32_t)(((float)gx * VW / GRID + dx * fz) * 256.0f);
            my[j] = (int32_t)(((float)gy * VW / GRID + dy * fz) * 256.0f);
        }
}

static void warp(int decay)
{
    int cy, cx, W1 = VW - 1;
    uint8_t *S = A, *D = Bf, *t;
    for (cy = 0; cy < GRID; cy++) {
        int y0 = cy * VW / GRID, y1 = (cy + 1) * VW / GRID, H = y1 - y0;
        for (cx = 0; cx < GRID; cx++) {
            int x0 = cx * VW / GRID, x1 = (cx + 1) * VW / GRID, Wd = x1 - x0, j = cy * GN + cx, y;
            int32_t lx = mx[j], ly = my[j], rx = mx[j + 1], ry = my[j + 1];
            int32_t dlx = (mx[j + GN] - lx) / H, dly = (my[j + GN] - ly) / H;
            int32_t drx = (mx[j + GN + 1] - rx) / H, dry = (my[j + GN + 1] - ry) / H;
            for (y = y0; y < y1; y++) {
                int32_t sx = lx, sy = ly, ix = (rx - lx) / Wd, iy = (ry - ly) / Wd;
                uint8_t *o = D + y * VW + x0;
                int x;
                for (x = 0; x < Wd; x++) {
                    int px = sx >> 8, py = sy >> 8, fx = (sx & 255) >> 4, fy = (sy & 255) >> 4, q, top, bot, val;
                    px = px < 0 ? 0 : px >= W1 ? W1 - 1 : px;
                    py = py < 0 ? 0 : py >= W1 ? W1 - 1 : py;
                    q = py * VW + px;
                    top = S[q] * (16 - fx) + S[q + 1] * fx;
                    bot = S[q + VW] * (16 - fx) + S[q + VW + 1] * fx;
                    val = (top * (16 - fy) + bot * fy) >> 8;
                    val = (val * decay >> 8) - 1;
                    *o++ = (uint8_t)(val < 0 ? 0 : val);
                    sx += ix;
                    sy += iy;
                }
                lx += dlx;
                ly += dly;
                rx += drx;
                ry += dry;
            }
        }
    }
    t = A;
    A = D;
    Bf = t;
}
static void mirror(void)                 /* four-fold, for the mandala */
{
    int H = VW / 2, x, y;
    for (y = 0; y < H; y++) {
        int top = y * VW, bot = (VW - 1 - y) * VW;
        for (x = 0; x < H; x++) {
            int a = A[top + x], b = A[x * VW + y], v = a > b ? a : b;
            A[top + x] = (uint8_t)v;
            A[top + VW - 1 - x] = (uint8_t)v;
            A[bot + x] = (uint8_t)v;
            A[bot + VW - 1 - x] = (uint8_t)v;
        }
    }
}
static void reflect(void)                /* the lower half a dimmer mirror of the upper: water */
{
    int y, x;
    for (y = 0; y < VW / 2; y++) {
        const uint8_t *a = A + y * VW;
        uint8_t *b = A + (VW - 1 - y) * VW;
        for (x = 0; x < VW; x++)
            b[x] = (uint8_t)(a[x] * 5 >> 3);
    }
}
static inline void add(float x, float y, int v)
{
    int ix = (int)x, iy = (int)y, o, z;
    if (x < 0.0f || y < 0.0f || ix >= VW || iy >= VW || v <= 0)
        return;
    o = iy * VW + ix;
    z = A[o] + v;
    A[o] = (uint8_t)(z > 255 ? 255 : z);
}
static void dab(float x, float y, int size, int gain)
{
    int d = DAB_D[size], r = (d - 1) >> 1, x0 = (int)x - r, y0 = (int)y - r, i, j;
    for (j = 0; j < d; j++)
        for (i = 0; i < d; i++)
            add((float)(x0 + i), (float)(y0 + j), dabs[size][j * d + i] * gain >> 8);
}
static void line(float x0, float y0, float x1, float y1, int v)
{
    float ax = fm_fabsf(x1 - x0), ay = fm_fabsf(y1 - y0), m = ax > ay ? ax : ay;
    int n = (int)m, i;
    if (n < 1)
        n = 1;
    if (n > 170)
        n = 170;
    for (i = 0; i <= n; i++)
        add(x0 + (x1 - x0) * (float)i / (float)n, y0 + (y1 - y0) * (float)i / (float)n, v);
}
static void ring(float cx, float cy, float r, int n, int v)
{
    int i;
    float px = 0.0f, py = 0.0f;
    if (n > 72)
        n = 72;
    for (i = 0; i <= n; i++) {
        float a = (float)i / (float)n * 6.28318f, x = cx + fm_cosf(a) * r, y = cy + sn(a) * r;
        if (i)
            line(px, py, x, y, v);
        px = x;
        py = y;
    }
}
static void ribbon(float base, float amp, int k, float t, int v)
{
    float py = 0.0f;
    int x;
    for (x = 0; x < VW; x += 2) {
        float fx = (float)x, yy = base + amp * sn(fx * 0.05f * (1.0f + (float)k * 0.6f) + t * (0.3f + (float)k * 0.17f)) *
                                              sn(fx * 0.017f + t * 0.1f);
        if (x)
            line(fx - 2.0f, py, fx, yy, v);
        py = yy;
    }
}
static float fmodf1(float x, float m) { return x - fm_floorf(x / m) * m; }

/* where a bell is: its note's place on the circle of twelve, its loop's distance from the middle */
static void bell_place(int loop, int note, float *x, float *y)
{
    float a = (float)(note % 12) * (6.28318f / 12.0f) - 1.5708f, r = 16.0f + 9.0f * (float)loop;
    *x = 60.0f + fm_cosf(a) * r;
    *y = 60.0f + sn(a) * r;
}

/* the wave, as MilkDrop draws it: the last 120 points of the sound, each frame, as a ring around the middle or a
 * line across, then warped into trails with everything else. Its gain follows the output, so a quiet world draws as
 * wide a wave as a loud one */
static float wave_gain = 40.0f;
static void wave(int round, float pad, float t)
{
    float pts[121], pk = 0.0f, px = 0.0f, py = 0.0f;
    uint32_t w = en_scope_w;
    int i, v = 60 + (int)(90.0f * pad);
    for (i = 0; i <= 120; i++) {
        float x = en_scope[(w - 121u + (uint32_t)i) % EN_SCOPE];
        pts[i] = x;
        pk = fm_maxf(pk, fm_fabsf(x));
    }
    wave_gain += (fm_clampf(16.0f / (pk + 0.02f), 6.0f, 120.0f) - wave_gain) * 0.05f;
    for (i = 0; i <= 120; i++) {
        float d = pts[i] * wave_gain, x, y;
        if (round) {
            float a = (float)i * (6.28318f / 120.0f) + t * 0.05f, r = 26.0f + 8.0f * pad + d;
            x = 60.0f + fm_cosf(a) * r;
            y = 60.0f + sn(a) * r;
        } else {
            x = (float)i;
            y = (mode == M_DAWN ? 92.0f : 60.0f) + d + 6.0f * sn(t * 0.2f + (float)i * 0.03f);
        }
        if (i)
            line(px, py, x, y, v);
        px = x;
        py = y;
    }
}

static void marks(float pad, float en, float t)
{
    int i, k;
    const int v = 30 + (int)(60.0f * pad);
    switch (mode) {
    case M_AURORA:
        for (k = 0; k < 3; k++)
            ribbon(38.0f + (float)k * 18.0f, 10.0f + 8.0f * pad, k, t, 26 + (int)(46.0f * pad));
        break;
    case M_ORBITS:                       /* the four bell loops as orbits, a body on each at its playhead */
        for (k = 0; k < EN_NLOOP; k++) {
            float r = 16.0f + 9.0f * (float)k, a = (float)en_loop_pos[k] / (float)EN_LOOP_LEN[k] * 6.28318f - 1.5708f;
            ring(60.0f, 60.0f, r, 18 + k * 6, 6);
            dab(60.0f + fm_cosf(a) * r, 60.0f + sn(a) * r, 1, 80 + (int)(150.0f * en_loop_glow[k]));
        }
        break;
    case M_HALO:
        ring(60.0f, 60.0f, 14.0f + 26.0f * pad, 40, v >> 1);
        break;
    case M_DAWN: {                       /* a sun, rising and setting with the tide, its rays turning */
        float sy = 78.0f - 20.0f * (0.5f + 0.5f * en_arc) - 6.0f * pad;
        for (k = 0; k < 7; k++)
            dab(60.0f + 5.0f * fm_cosf((float)k * 0.9f), sy + 5.0f * sn((float)k * 0.9f), 2, 70 + (int)(80.0f * pad));
        for (k = 0; k < 12; k++) {
            float a = (float)k * 0.5236f + t * 0.03f;
            line(60.0f + fm_cosf(a) * 14.0f, sy + sn(a) * 14.0f, 60.0f + fm_cosf(a) * (22.0f + 8.0f * pad),
                 sy + sn(a) * (22.0f + 8.0f * pad), 12 + (int)(20.0f * pad));
        }
        break;
    }
    case M_HORIZON:
        line(0.0f, 60.0f + 2.0f * sn(t * 0.3f), 119.0f, 60.0f - 2.0f * sn(t * 0.3f), 18 + (int)(40.0f * pad));
        break;
    case M_TUNNEL:
        ring(60.0f, 60.0f, 50.0f + 4.0f * en, 36, 10 + (int)(24.0f * pad));
        break;
    default:
        break;
    }
    for (k = 0; k < 3; k++) {            /* three slow comets, everywhere: their trails paint the field */
        float cx = 60.0f + 44.0f * sn(t * (0.071f + 0.013f * (float)k) + (float)k * 2.1f),
              cy = 60.0f + 44.0f * sn(t * (0.053f + 0.017f * (float)k) + (float)k * 1.3f);
        dab(cx, cy, 1, 50 + (int)(60.0f * pad));
    }
    if (title) {                         /* the title: a sun rising to the zenith, its rays turning */
        float sy = 104.0f - 46.0f * fm_minf(title_t / 3.0f, 1.0f);
        for (k = 0; k < 7; k++)
            dab(60.0f + 4.0f * fm_cosf((float)k * 0.9f), sy + 4.0f * sn((float)k * 0.9f), 2, 90);
        for (k = 0; k < 16; k++) {
            float a = (float)k * 0.3927f + t * 0.2f, r0 = 10.0f, r1 = 16.0f + 10.0f * fm_minf(title_t, 1.0f);
            line(60.0f + fm_cosf(a) * r0, sy + sn(a) * r0, 60.0f + fm_cosf(a) * r1, sy + sn(a) * r1, 30);
        }
        for (k = 0; k < 6; k++)              /* stars, rushing out of the middle */
            add(60.0f + (vr() - 0.5f) * 8.0f, 60.0f + (vr() - 0.5f) * 8.0f, 160);
        return;
    }
    if (mode != M_ORBITS)                /* the wave: round in the round pictures, a line in the others */
        wave(mode == M_NEBULA || mode == M_MANDALA || mode == M_TUNNEL || mode == M_STARS || mode == M_BLOOM ||
             mode == M_SPIRAL || mode == M_HALO, pad, t);
    /* the pad's glow: a soft light in the middle, as it swells */
    if (mode != M_HORIZON && mode != M_DAWN && mode != M_AURORA)
        dab(60.0f + 6.0f * sn(t * 0.13f), 60.0f + 6.0f * sn(t * 0.11f), 2, (int)(110.0f * pad));
    /* dust: stars, embers, petals, as the picture is */
    for (i = 0; i < 32; i++) {
        float b = 0.0f;
        switch (mode) {
        case M_EMBERS:
            dust[i].y -= dust[i].v * 1.2f;
            dust[i].x += 0.15f * sn(t * 0.6f + dust[i].ph);
            b = 40.0f + 50.0f * dust[i].v;
            break;
        case M_PETALS:
            dust[i].y += dust[i].v * 0.8f;
            dust[i].x += 0.35f * sn(t * 0.5f + dust[i].ph + dust[i].y * 0.05f);
            b = 36.0f + 60.0f * dust[i].v;
            break;
        case M_STARS:
        case M_NEBULA:
        case M_SPIRAL:
        case M_ORBITS:                   /* stars: they stay, and twinkle */
            b = (12.0f + 30.0f * dust[i].v) * (0.6f + 0.4f * sn(t * (1.0f + dust[i].v * 3.0f) + dust[i].ph));
            break;
        default:
            dust[i].y -= dust[i].v * 0.2f;
            b = 10.0f + 20.0f * dust[i].v;
            break;
        }
        if (dust[i].y < 0.0f) {
            dust[i].y = VW - 1;
            dust[i].x = vr() * VW;
        } else if (dust[i].y >= VW) {
            dust[i].y = 0.0f;
            dust[i].x = vr() * VW;
        }
        add(dust[i].x, dust[i].y, (int)b);
    }
}

void vis_step(float dt)
{
    /* how long a trace lasts in each picture (of 256, each frame) */
    static const uint8_t DEC[VIS_NMODE] = {252, 251, 251, 249, 249, 252, 251, 252, 251, 253, 250, 251, 252, 250, 253,
                                           252};
    const int frozen = en_frozen != 0;
    float en, pad, t;
    int i;
    vt += dt;
    if (vt > 20000.0f)
        vt -= 19998.0f;
    t = vt;
    pad = fm_clampf(en_pad_level * 1.3f, 0.0f, 1.0f) * (0.3f + 0.7f * fm_clampf(en_out_level * 2.0f, 0.0f, 1.0f));
    glow_e += (pad - glow_e) * fm_minf(dt * 2.0f, 1.0f);
    pad = glow_e;
    en = fm_clampf(en_bass_pulse * 0.8f, 0.0f, 1.0f);
    if (title) {
        title_t += dt;
        en = 1.6f;                       /* the stars rush */
    }
    if (chords_seen != en_chord_count) {  /* a new chord: the colours turn a little */
        chords_seen = en_chord_count;
        hue_tgt = fm_clampf(hue_tgt + (vr() - 0.5f) * 0.8f, -0.8f, 0.8f);
    }
    if (m_w < 1.0f)
        m_w = fm_minf(1.0f, m_w + dt / 8.0f);
    next_mode -= dt;
    if (mode_auto && !frozen && !title && next_mode <= 0.0f) {   /* the world's next picture, or now and then another */
        int nm = vr() < (mode_auto == 2 ? 0.35f : 0.8f) ? WORLD_MODES[world()][(int)(vr() * 3.999f)]
                                                         : (int)(vr() * VIS_NMODE) % VIS_NMODE;
        if (mode_auto == 2 && nm == mode)
            nm = (nm + 1 + (int)(vr() * (VIS_NMODE - 1))) % VIS_NMODE;
        if (nm != mode) {
            m_from = mode;
            mode = nm;
            m_w = 0.0f;
        }
        next_mode = mode_auto == 2 ? 18.0f + vr() * 22.0f : 45.0f + vr() * 60.0f;
    }
    if (pal_fix == -2 && !frozen && !title) {    /* Auto colours: another set every half minute or so */
        pal_left -= dt;
        if (pal_left <= 0.0f) {
            int np = (int)(vr() * VIS_NPAL) % VIS_NPAL;
            if (np == pal_roam || np == pal_now)
                np = (np + 1 + (int)(vr() * (VIS_NPAL - 1))) % VIS_NPAL;
            pal_roam = np;
            pal_left = 22.0f + vr() * 25.0f;
            hue_tgt = (vr() - 0.5f) * 1.2f;      /* and a turn of its own */
        }
    }
    if (mode_auto && !title) {           /* a new world: its own picture at once */
        static int last_world = -1;
        if (world() != last_world) {
            int nm = WORLD_MODES[world()][0];
            last_world = world();
            if (nm != mode) {
                m_from = mode;
                mode = nm;
                m_w = 0.0f;
            }
            next_mode = 45.0f + vr() * 60.0f;
        }
    }
    palette_step();
    if (frozen)                           /* a freeze holds the picture as it is */
        return;
    mesh(en, 0);
    warp(DEC[mode]);
    if (mode == M_MANDALA)
        mirror();
    if (mode == M_HORIZON)
        reflect();
    marks(pad, en, t);
    while (bev_r != en_bev_w) {          /* the bells: a ripple where each rang */
        uint32_t r = bev_r % EN_BEV;
        int l = en_bev[r][0], n = en_bev[r][1], vel = en_bev[r][2];
        float x, y;
        bev_r++;
        if (en_bev_w - bev_r > EN_BEV)
            bev_r = en_bev_w - EN_BEV;
        bell_place(l, n, &x, &y);
        for (i = 0; i < 12; i++)
            if (!rings[i].on)
                break;
        if (i == 12)
            i = 0;
        rings[i].on = 1;
        rings[i].x = x;
        rings[i].y = y;
        rings[i].r = 1.0f;
        rings[i].v = 0.8f + vr() * 0.6f;
        rings[i].a = (float)vel / 127.0f;
        dab(x, y, 1, 140 + vel);
    }
    for (i = 0; i < 12; i++) {
        float q = rings[i].r;
        int steps, v, k;
        if (!rings[i].on)
            continue;
        rings[i].r = q + rings[i].v * dt * 14.0f;
        rings[i].a *= 1.0f - dt * 1.1f;
        steps = 12 + (int)(q * 4.0f);
        steps = steps > 90 ? 90 : steps;
        v = (int)(110.0f * rings[i].a);
        if (v > 6)
            for (k = 0; k < steps; k++) {
                float a = (float)k / (float)steps * 6.283f;
                add(rings[i].x + fm_cosf(a) * q, rings[i].y + sn(a) * q, v);
            }
        if (rings[i].a < 0.05f || q > 80.0f)
            rings[i].on = 0;
    }
    (void)fmodf1;
}

/* the sky behind it all: a soft glow from the middle, breathing with the pad, so the picture is never empty black.
 * Separable (1 - dx^2 - dy^2 = a[x] + b[y]): two small tables, made each frame from the glow's strength */
static int16_t bga[240], bgb[240];
static void glow_tables(void)
{
    int i;
    float g = 16.0f + 34.0f * glow_e;
    for (i = 0; i < 240; i++) {
        float d = ((float)i - 119.5f) * (1.0f / 120.0f);
        bga[i] = (int16_t)(g * (0.5f - d * d));
        bgb[i] = bga[i];
    }
}

void vis_scan(uint16_t *px, int y0, int h)
{
    static const int8_t BAYER[4] = {-3, 1, 3, -1};
    int y, x;
    if (y0 == 0)
        glow_tables();
    for (y = y0; y < y0 + h; y++) {
        const uint8_t *row = A + (y >> 1) * VW;
        int by = (y & 1) << 1, gb = bgb[y];
        for (x = 0; x < 240; x++) {
            int v = row[x >> 1], g = bga[x] + gb;
            if (g > 0)
                v += g;
            if (v)
                v += BAYER[by | (x & 1)];
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            *px++ = lut[v];
        }
    }
}
