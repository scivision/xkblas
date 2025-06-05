/* ************************************************************************** */
/*                                                                            */
/*   logger-nvml.h                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/16 16:15:23 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:53 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_NVML_H__
# define __LOGGER_NVML_H__

# include <xkrt/logger/logger.h>
# include <nvml.h>

# define NVML_SAFE_CALL(X)                                                          \
    do {                                                                            \
        nvmlReturn_t r = X;                                                         \
        if (r != NVML_SUCCESS)                                                      \
        {                                                                           \
            const char * _s = nvmlErrorString(r);                                   \
            LOGGER_FATAL("`%s` failed with `%s` (%d)", #X, _s, r);                  \
        }                                                                           \
    } while (0)

#endif /* __LOGGER_NVML_H__ */
