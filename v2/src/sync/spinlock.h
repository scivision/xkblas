#ifndef __SPINLOCK_H__
# define __SPINLOCK_H__

typedef volatile int spinlock_t;

# define SPINLOCK_LOCK(L)                                       \
    do {                                                        \
        int zero = 0;                                           \
        while (__sync_val_compare_and_swap(&L, zero, 1) == 1);

# define SPINLOCK_UNLOCK(L)             \
        __sync_fetch_and_xor(&L, L);    \
    } while (0)

#endif /* __SPINLOCK_H__ */

