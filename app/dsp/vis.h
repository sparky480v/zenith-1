/* SPDX-License-Identifier: MPL-2.0
 * ZENITH-1: the picture. A 120 x 120 feedback field (after HADAL-1's), drawn at twice its size, moved by the music. */
#pragma once
#include <stdint.h>
#define VIS_NMODE 16
#define VIS_NPAL 9
extern const char *const VIS_MODE[VIS_NMODE];
extern const char *const VIS_PAL[VIS_NPAL];
void vis_init(void);
void vis_set_mode(int m);             /* -2: Auto (any picture, changing by itself); -1: the world's own */
void vis_set_pal(int p);              /* -2: Auto (colours changing by themselves); -1: the world's */
void vis_bright(int b);
void vis_title(int on);               /* the power-on title: stars rushing, a sun rising */
int vis_mode(void);
void vis_step(float dt);              /* one frame of the field, from the engine's state */
/* h rows of the panel from row y0 into px (240 wide, byte-swapped RGB565) */
void vis_scan(uint16_t *px, int y0, int h);
