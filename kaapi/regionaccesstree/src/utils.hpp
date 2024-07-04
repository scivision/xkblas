#ifndef __UTILS_HPP__
# define __UTILS_HPP__

# ifndef MIN
#  define MIN(X, Y) ((Y) < (X) ? (Y) : (X))
# endif /* MIN */

# ifndef MAX
#  define MAX(X, Y) ((X) < (Y) ? (Y) : (X))
# endif /* MAX */

#ifdef DEBUG
# undef DEBUG
# define DEBUG(...)             \
    do {                        \
        printf(__VA_ARGS__);    \
        printf("\n");           \
    } while (0);
#else
# define DEBUG(...)
#endif

# include <cstdint>

static inline uint64_t
get_nanotime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t) ts.tv_nsec;
}

static inline int
log2(int n)
{
    return 31 - __builtin_clz(n);
}

static inline int
twopow(int n)
{
    return (1 << n);
}

#endif /* __UTILS_HPP__ */
