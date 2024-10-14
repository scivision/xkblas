#ifndef __SPINLOCK_H__
# define __SPINLOCK_H__

# include "sync/mem.h"

#if 1

typedef volatile int spinlock_t;

# define SPINLOCK_LOCK(L)                                       \
    do {                                                        \
        int zero = 0;                                           \
        while (__sync_val_compare_and_swap(&L, zero, 1) == 1)   \
            mem_pause();                                        \
    } while (0)

# define SPINLOCK_UNLOCK(L)             \
    do {                                \
        __sync_fetch_and_xor(&L, L);    \
    } while (0)

# else

# include <pthread.h>
typedef pthread_mutex_t spinlock_t;
# define SPINLOCK_LOCK(L)       pthread_mutex_lock(&L)
# define SPINLOCK_UNLOCK(L)     pthread_mutex_unlock(&L)

# endif

#endif /* __SPINLOCK_H__ */
