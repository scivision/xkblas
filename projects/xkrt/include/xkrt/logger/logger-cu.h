/* ************************************************************************** */
/*                                                                            */
/*   logger-cu.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/28 23:55:11 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
            const char * s;                                                         \
            cuGetErrorString(r, &s);                                                \
            LOGGER_FATAL("`%s` failed with `%s` (%d)", #X, s, r);                   \
        }                                                                           \
    } while (0)

#endif /* __LOGGER_CU_H__ */
