/* ************************************************************************** */
/*                                                                            */
/*   logger-cu.h                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/16 16:15:23 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:23 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_CU_H__
# define __LOGGER_CU_H__

# include <xkrt/logger/logger.h>
# include <cuda.h>

# define CUDA_SAFE_CALL(X)                                                          \
    do {                                                                            \
        cudaError_t r = X;                                                          \
        if (r != cudaSuccess)                                                       \
            LOGGER_FATAL("`%s` failed with `%s` (%d)", #X, cudaGetErrorName(r), r); \
    } while (0)

# define CU_SAFE_CALL(X)                                                            \
    do {                                                                            \
        CUresult r = X;                                                             \
        if (r != CUDA_SUCCESS)                                                      \
        {                                                                           \
            const char * _s;                                                        \
            cuGetErrorString(r, &_s);                                               \
            LOGGER_FATAL("`%s` failed with `%s` (%d)", #X, _s, r);                  \
        }                                                                           \
    } while (0)

#endif /* __LOGGER_CU_H__ */
