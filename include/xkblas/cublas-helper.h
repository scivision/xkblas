/* ************************************************************************** */
/*                                                                            */
/*   cublas-helper.h                                              .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/14 23:28:05 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/09/05 19:24:32 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CUBLAS_HELPER_H__
# define __CUBLAS_HELPER_H__

# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-cublas.h>

#  define XKBLAS_CUBLAS_CALL_POST()                                                         \
    do {                                                                                    \
        CU_SAFE_CALL(cuEventRecord(stream->cu.events.buffer[idx], stream->cu.handle.high)); \
    } while (0)

# define XKBLAS_CUBLAS_CALL(CALL)       \
    do {                                \
        CUBLAS_SAFE_CALL(CALL);         \
        XKBLAS_CUBLAS_CALL_POST();      \
    } while (0)

# define XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, PX, T) \
    if constexpr (P == xkblas_precision_t::PX) body_cuda_run<P, cublas##PX##NAME, T>(stream, instr, idx);

# define XKBLAS_CUBLAS_DISPATCH_PRECISION_REAL(NAME)            \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, S, float)          \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, D, double)

# define XKBLAS_CUBLAS_DISPATCH_PRECISION_COMPLEX(NAME)         \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, C, cuComplex)      \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, Z, cuDoubleComplex)

# define XKBLAS_CUBLAS_DISPATCH_PRECISION(NAME)     \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_REAL(NAME)     \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_COMPLEX(NAME)

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
