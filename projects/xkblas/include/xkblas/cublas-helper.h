/* ************************************************************************** */
/*                                                                            */
/*   cublas-helper.h                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/27 21:19:20 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CUBLAS_HELPER_H__
# define __CUBLAS_HELPER_H__

# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-cublas.h>

# ifdef NDEBUG
#  define XKBLAS_CUBLAS_CALL_POST()                                                             \
    do {                                                                                        \
        CUDA_SAFE_CALL(cudaEventRecord(stream->cu.events.buffer[idx], stream->cu.handle.high)); \
    } while (0)
# else
#  define XKBLAS_CUBLAS_CALL_POST()                                                             \
    do {                                                                                        \
        CUDA_SAFE_CALL(cudaEventRecord(stream->cu.events.buffer[idx], stream->cu.handle.high)); \
        cudaPointerAttributes attr;                                                             \
        for (int i = 0 ; i < task->naccesses ; ++i)                                             \
        {                                                                                       \
            Access * access = task->accesses + i;                                               \
            assert(access);                                                                     \
            int device;                                                                         \
            cudaGetDevice(&device);                                                             \
            cudaPointerGetAttributes(&attr, (const void *) access->device_view.addr);           \
            assert(attr.device == device);                                                      \
            assert(attr.type == cudaMemoryTypeDevice);                                          \
        }                                                                                       \
    } while (0)
# endif

# define XKBLAS_CUBLAS_CALL(CALL)       \
    do {                                \
        CUBLAS_SAFE_CALL(CALL);         \
        XKBLAS_CUBLAS_CALL_POST();      \
    } while (0)

# include "xkblas/cblas.h"

static inline cublasOperation_t
cblas2cublas_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return CUBLAS_OP_N;
        case CblasTrans:        return CUBLAS_OP_T;
        case CblasConjTrans:    return CUBLAS_OP_C;
    }
    LOGGER_FATAL("Unknown trans code");
    abort();
}

static inline cublasSideMode_t
cblas2cublas_side(int side)
{
    switch (side)
    {
        case CblasLeft:     return CUBLAS_SIDE_LEFT;
        case CblasRight:    return CUBLAS_SIDE_RIGHT;
    }
    LOGGER_FATAL("Unknown side code");
    abort();
}

static inline
cublasFillMode_t cblas2cublas_uplo( int uplo )
{
    switch (uplo)
    {
        case CblasUpper:    return CUBLAS_FILL_MODE_UPPER;
        case CblasLower:    return CUBLAS_FILL_MODE_LOWER;
    }
    LOGGER_FATAL("Unknown uplo code");
    abort();
}

static inline
cublasDiagType_t cblas2cublas_diag(int diag)
{
    switch (diag)
    {
        case CblasNonUnit:  return CUBLAS_DIAG_NON_UNIT;
        case CblasUnit:     return CUBLAS_DIAG_UNIT;
    }
    LOGGER_FATAL("Unknown diag code");
    abort();
}

#endif /* __CUBLAS_HELPER_H__ */
