#ifndef __XKBLAS_PRINT_H__
# define __XKBLAS_PRINT_H__

# include "spinlock.h"

# include <unistd.h>
# include <stdio.h>
# include <stdlib.h>

extern volatile int XKBLAS_PRINT_MTX;

# define XKBLAS_PRINT_FATAL_ID    0
# define XKBLAS_PRINT_INFO_ID     1
# define XKBLAS_PRINT_ERROR_ID    2
# define XKBLAS_PRINT_WARN_ID     3
# define XKBLAS_PRINT_DEBUG_ID    4

extern char * XKBLAS_PRINT_COLORS[5];
extern char * XKBLAS_PRINT_HEADERS[5];

extern int XKBLAS_VERBOSE;

# define XKBLAS_PRINT(LVL, ...)                                         \
    do {                                                                \
        if (LVL <= XKBLAS_VERBOSE)                                      \
        {                                                               \
            SPINLOCK_LOCK(XKBLAS_PRINT_MTX);                            \
            if (isatty(STDOUT_FILENO))                                  \
                fprintf(stdout, "[\033[1;37mXKBLAS\033[0m] "            \
                                "[%s%s\033[0m] ",                       \
                                XKBLAS_PRINT_COLORS[LVL],               \
                                XKBLAS_PRINT_HEADERS[LVL]);             \
            else                                                        \
                fprintf(stdout, "[XKBLAS] [%s] ",                       \
                        XKBLAS_PRINT_HEADERS[LVL]);                     \
            fprintf(stdout, __VA_ARGS__);                               \
            fprintf(stdout, "\n");                                      \
            SPINLOCK_UNLOCK(XKBLAS_PRINT_MTX);                          \
            if (LVL == XKBLAS_PRINT_FATAL_ID)                           \
            {                                                           \
                fprintf(stderr,                                         \
                        "%s:%d (%s)", __FILE__, __LINE__, __func__);    \
                abort();                                                \
            }                                                           \
        }                                                               \
    } while (0)

# define XKBLAS_INFO(...)  XKBLAS_PRINT(XKBLAS_PRINT_INFO_ID,  __VA_ARGS__)
# define XKBLAS_WARN(...)  XKBLAS_PRINT(XKBLAS_PRINT_WARN_ID,  __VA_ARGS__)
# define XKBLAS_ERROR(...) XKBLAS_PRINT(XKBLAS_PRINT_ERROR_ID, __VA_ARGS__)
# define XKBLAS_DEBUG(...) XKBLAS_PRINT(XKBLAS_PRINT_DEBUG_ID, __VA_ARGS__)
# define XKBLAS_FATAL(...) XKBLAS_PRINT(XKBLAS_PRINT_FATAL_ID, __VA_ARGS__)

#endif /* __XKBLAS_PRINT_H__*/

