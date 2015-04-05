#ifndef _UTIL_H_
#define _UTIL_H_

#include <inttypes.h>
#include <sys/time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

extern int verbose_level;

void     _verbprintf(int verb_level, const char *fmt, ...);
void     _verbprintft(int verb_level, const char *fmt, ...);
void     _print_block(int verb_level, const uint8_t *pblock, int size);

int      timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y);
uint32_t ts_us(struct timeval *x);

float    rssi_dbm(uint8_t rssi_dec);
uint8_t  get_crc_lqi(uint8_t crc_lqi, uint8_t *lqi);

#if !defined(MAX_VERBOSE_LEVEL)
#   define MAX_VERBOSE_LEVEL 0
#endif
#define verbprintf(level, ...) \
    do { if (level <= MAX_VERBOSE_LEVEL) _verbprintf(level, __VA_ARGS__); } while (0)

#define verbprintft(level, ...) \
    do { if (level <= MAX_VERBOSE_LEVEL) _verbprintft(level, __VA_ARGS__); } while (0)

#define print_block(level, pblock, size) \
    do { if (level <= MAX_VERBOSE_LEVEL) _print_block(level, pblock, size); } while(0)

#endif