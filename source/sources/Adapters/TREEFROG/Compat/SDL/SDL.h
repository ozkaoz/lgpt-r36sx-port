#ifndef TREEFROG_SDL_COMPAT_H
#define TREEFROG_SDL_COMPAT_H

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_mutex {
    pthread_mutex_t mutex;
} SDL_mutex;

static inline SDL_mutex *SDL_CreateMutex(void) {
    SDL_mutex *m = (SDL_mutex *)malloc(sizeof(SDL_mutex));
    if (!m) return 0;
    if (pthread_mutex_init(&m->mutex, 0) != 0) {
        free(m);
        return 0;
    }
    return m;
}

static inline void SDL_DestroyMutex(SDL_mutex *m) {
    if (!m) return;
    pthread_mutex_destroy(&m->mutex);
    free(m);
}

static inline int SDL_LockMutex(SDL_mutex *m) {
    return m ? pthread_mutex_lock(&m->mutex) : -1;
}

static inline int SDL_UnlockMutex(SDL_mutex *m) {
    return m ? pthread_mutex_unlock(&m->mutex) : -1;
}

static inline void SDL_Delay(unsigned int ms) {
    usleep((useconds_t)ms * 1000U);
}

#ifdef __cplusplus
}
#endif

#endif
