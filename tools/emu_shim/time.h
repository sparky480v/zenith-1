#pragma once
typedef long time_t;
struct timespec { long tv_sec; long tv_nsec; };
#define CLOCK_MONOTONIC 1
int clock_gettime(int c, struct timespec *t);
