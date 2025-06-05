/* ************************************************************************** */
/*                                                                            */
/*   logger.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 17:56:44 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <stdint.h>

# include <xkrt/sync/spinlock.h>

volatile spinlock_t LOGGER_PRINT_MTX;

volatile double     LOGGER_TIME_ELAPSED = 0.0;
volatile uint64_t   LOGGER_LAST_TIME    = 0;

# define NLVL 6

char const * LOGGER_PRINT_COLORS[NLVL] = {
    "\033[1;31m",
    "\033[1;31m",
    "\033[1;33m",
    "\033[1;32m",
    "\033[1;35m",
    "\033[1;36m",
};

char const * LOGGER_PRINT_HEADERS[NLVL] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "IMPL",
    "DEBUG",
};

int LOGGER_VERBOSE = NLVL;
