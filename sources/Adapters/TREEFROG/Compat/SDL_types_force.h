#ifndef TREEFROG_SDL_TYPES_FORCE_H
#define TREEFROG_SDL_TYPES_FORCE_H

#include <stdint.h>
#include <sys/time.h>

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;

typedef int8_t   Sint8;
typedef int16_t  Sint16;
typedef int32_t  Sint32;
typedef int64_t  Sint64;

static inline Uint32 SDL_GetTicks(void) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (Uint32)((tv.tv_sec * 1000UL) + (tv.tv_usec / 1000UL));
}

#endif
