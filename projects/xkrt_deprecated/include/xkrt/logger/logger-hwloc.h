/* ************************************************************************** */
/*                                                                            */
/*   logger-hwloc.h                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/16 16:15:23 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:48 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_HWLOC_H__
# define __LOGGER_HWLOC_H__

# include <xkrt/logger/logger.h>
# include <hwloc.h>
# include <errno.h>

static const char *
hwloc_errstring(int err)
{
    switch (err)
    {
        case  0: return "Success";
        case -1: return "Generic error";
        default: return "Unknown error code";
    }
}

# define HWLOC_SAFE_CALL(X)                                                                                   \
    do {                                                                                                      \
        int r = X;                                                                                            \
        if (r != 0)                                                                                           \
            LOGGER_FATAL("`%s` failed with `%s` `%s` - (%d)", #X, hwloc_errstring(r), strerror(errno), r);    \
    } while (0)

#endif /* __LOGGER_HWLOC_H__ */
