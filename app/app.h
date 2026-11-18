/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (AMB-1), Polystellarflow (ZENITH-1) */
/* ZENITH-1 app: the project (everything that is saved), the app's entry points and its half of the contract with
 * fm1-base (device/unity.c, host/plat_host.c). Built on AMB-1's. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "zenith.h"
#include "vis.h"

#define APP_NAME "ZENITH-1"
#define APP_OVERLOAD                     /* app_overload: the render near its time, the effects and the lead stop */
void app_overload(void);
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
/* the canvas: 240 x 48 (the pool is the reverb's, the tape's and the picture's): the garden, 144 rows, is drawn in
 * three bands of 48, the knobs, 68, in two; the header is 28; the picture goes in strips of 24 */
#define APP_CANVAS_PX (240u * 48u)
#ifdef APP_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION APP_VERSION
#else
#define EN_VERSION "DEV"
#endif

/* the store: the project at 0xFC000 and the panel calibration at 0xDC000 (an FM-1 that ran AMB-1 keeps its
 * calibration; AMB-1's project, another magic, is not read) */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}
#define PROJ_MAGIC 0x314E455Au           /* "ZEN1" */
#define PROJ_FORMAT 7u                   /* 2: the journey, the effects, more sounds; 3: the lead, Auto FX, Auto picture;
                                          * 4: no Hold keep; 5: the lead's Glide; 7: Strum level */
#define NPIECES EN_NPIECES

typedef struct {
    int16_t par[P_NPARAMS];
    uint16_t pool;
} slot_t;

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* the world playing, as it is now (zenith.h P_*) */
    uint16_t pool;                       /* its scale, relative to the root (bit 0 = the root) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which world */
    uint8_t pic_after;                   /* the picture comes after: 0 never, 1 10 s, 2 30 s, 3 1 min, 4 3 min */
    int8_t pic_mode;                     /* -2 Auto, -1 the world's own (changing), else one picture */
    int8_t pic_pal;                      /* -2 Auto, -1 the world's colours, else one set */
    uint8_t pic_glow;                    /* 0 soft, 1 bright, 2 brightest */
    slot_t slot[NPIECES];                /* every world as it was left (Tune and MIDI out are global) */
    uint8_t rsv[16];                     /* room to grow */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */
const char *piece_name(int i);
void project_piece(int i);               /* the playing world kept, world i taken up as it was left */
void project_piece_reset(void);          /* the playing world back to its recipe */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved project (or the defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's part (app/host.c): ZENITH-1's checks, its commands, the screen reader's dump */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_SPEAKDUMP
void app_speak_dump(void);
#define APP_HOST_REBOOT
void app_host_reboot(void);              /* the project forgotten, as RAM is at a restart */
