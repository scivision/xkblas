/* ************************************************************************** */
/*                                                                            */
/*   logger-nvml.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/09 23:11:17 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
