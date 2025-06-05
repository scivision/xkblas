/* ************************************************************************** */
/*                                                                            */
/*   logger-hip.h                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/16 16:15:23 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:42 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
