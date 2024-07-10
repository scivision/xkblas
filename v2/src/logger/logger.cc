volatile int XKBLAS_PRINT_MTX = 0;

# define NLVL 6

char const * XKBLAS_PRINT_COLORS[NLVL] = {
    "\033[1;31m",
    "\033[1;32m",
    "\033[1;31m",
    "\033[1;33m",
    "\033[1;35m",
    "\033[1;36m",
};

char const * XKBLAS_PRINT_HEADERS[NLVL] = {
    "FATAL",
    "INFO",
    "ERROR",
    "WARN",
    "IMPL",
    "DEBUG",
};

int XKBLAS_VERBOSE = NLVL;
