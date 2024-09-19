#ifndef __LOGGER_H__
# define __LOGGER_H__

# include "sync/spinlock.h"

# include <time.h>
# include <unistd.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

extern volatile int XKBLAS_PRINT_MTX;

# define XKBLAS_PRINT_FATAL_ID      0
# define XKBLAS_PRINT_ERROR_ID      1
# define XKBLAS_PRINT_WARN_ID       2
# define XKBLAS_PRINT_INFO_ID       3
# define XKBLAS_PRINT_IMPL_ID       4
# define XKBLAS_PRINT_DEBUG_ID      5

extern char * XKBLAS_PRINT_COLORS[6];
extern char * XKBLAS_PRINT_HEADERS[6];

extern int XKBLAS_VERBOSE;

extern volatile double   XKBLAS_TIME_ELAPSED;
extern volatile uint64_t XKBLAS_LAST_TIME;

# define XKBLAS_PRINT_LINE() \
    fprintf(stderr, "%s:%d (%s)\n", __FILE__, __LINE__, __func__);

# define XKBLAS_PRINT(LVL, ...)                                         \
    do {                                                                \
        if (LVL <= XKBLAS_VERBOSE)                                      \
        {                                                               \
            SPINLOCK_LOCK(XKBLAS_PRINT_MTX);                            \
            struct timespec ts;                                         \
            clock_gettime(CLOCK_MONOTONIC, &ts);                        \
            uint64_t t = (uint64_t)(ts.tv_sec * 1000000000) +           \
                            (uint64_t) ts.tv_nsec;                      \
            if (XKBLAS_LAST_TIME != 0)                                  \
                XKBLAS_TIME_ELAPSED += (t - XKBLAS_LAST_TIME) / 1e9;    \
            XKBLAS_LAST_TIME = t;                                       \
            if (isatty(STDOUT_FILENO))                                  \
                fprintf(stdout, "[%8lf] "                               \
                                "[TID=%d] "                             \
                                "[\033[1;37mXKBLAS\033[0m] "            \
                                "[%s%s\033[0m] ",                       \
                                XKBLAS_TIME_ELAPSED,                    \
                                gettid(),                               \
                                XKBLAS_PRINT_COLORS[LVL],               \
                                XKBLAS_PRINT_HEADERS[LVL]);             \
            else                                                        \
                fprintf(stdout, "[%8lf]"                                \
                                "[TID=%d] "                             \
                                "[XKBLAS] "                             \
                                "[%s] ",                                \
                                XKBLAS_TIME_ELAPSED,                    \
                                gettid(),                               \
                                XKBLAS_PRINT_HEADERS[LVL]);             \
            fprintf(stdout, __VA_ARGS__);                               \
            fprintf(stdout, "\n");                                      \
            fflush(stdout);                                             \
            SPINLOCK_UNLOCK(XKBLAS_PRINT_MTX);                          \
            if (LVL == XKBLAS_PRINT_FATAL_ID)                           \
            {                                                           \
                XKBLAS_PRINT_LINE();                                    \
                fflush(stderr);                                         \
                abort();                                                \
            }                                                           \
        }                                                               \
    } while (0)

# define XKBLAS_NOT_IMPLEMENTED() XKBLAS_NOT_IMPLEMENTED_WARN("")

# define XKBLAS_NOT_IMPLEMENTED_WARN(S)                                 \
    XKBLAS_IMPL("'%s' at %s:%d in %s()",                                \
            S, __FILE__, __LINE__, __func__);

# define XKBLAS_INFO(...)  XKBLAS_PRINT(XKBLAS_PRINT_INFO_ID,  __VA_ARGS__)
# define XKBLAS_WARN(...)  XKBLAS_PRINT(XKBLAS_PRINT_WARN_ID,  __VA_ARGS__)
# define XKBLAS_ERROR(...) XKBLAS_PRINT(XKBLAS_PRINT_ERROR_ID, __VA_ARGS__)
# define XKBLAS_IMPL(...)  XKBLAS_PRINT(XKBLAS_PRINT_IMPL_ID,  __VA_ARGS__)
# ifndef NDEBUG
#  define XKBLAS_DEBUG(...) XKBLAS_PRINT(XKBLAS_PRINT_DEBUG_ID, __VA_ARGS__)
# else
#  define XKBLAS_DEBUG(...)
# endif
# define XKBLAS_FATAL(...) XKBLAS_PRINT(XKBLAS_PRINT_FATAL_ID, __VA_ARGS__)

#endif /* __LOGGER_H__*/

