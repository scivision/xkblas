/* ************************************************************************** */
/*                                                                            */
/*   logger-hip.h                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/20 15:56:37 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_HIP_H__
# define __LOGGER_HIP_H__

# include <xkrt/logger/logger.h>
# include <hip/hip_runtime.h>

# define HIP_SAFE_CALL(X)                                                           \
    do {                                                                            \
        hipError_t r = X;                                                           \
        if (r != hipSuccess)                                                        \
            LOGGER_FATAL("`%s` failed with `%s` (%d)", #X, hipGetErrorName(r), r);  \
    } while (0)

#endif /* __LOGGER_HIP_H__ */
