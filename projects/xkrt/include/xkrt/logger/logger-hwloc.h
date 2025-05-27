/* ************************************************************************** */
/*                                                                            */
/*   logger-hwloc.h                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@anl.gov>               .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/22 20:22:28 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
