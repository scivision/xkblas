# include <stdint.h>
# include "sync/spinlock.h"

volatile spinlock_t XKBLAS_PRINT_MTX;

volatile double     XKBLAS_TIME_ELAPSED = 0.0;
volatile uint64_t   XKBLAS_LAST_TIME    = 0;

# define NLVL 6

char const * XKBLAS_PRINT_COLORS[NLVL] = {
    "\033[1;31m",
    "\033[1;31m",
    "\033[1;33m",
    "\033[1;32m",
    "\033[1;35m",
    "\033[1;36m",
};

char const * XKBLAS_PRINT_HEADERS[NLVL] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "IMPL",
    "DEBUG",
};

int XKBLAS_VERBOSE = NLVL;
