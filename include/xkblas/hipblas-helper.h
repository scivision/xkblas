/* ************************************************************************** */
/*                                                                            */
/*   hipblas-helper.h                                             .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/14 23:28:05 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/09/16 17:51:02 by Romain PEREIRA         / _______ \       */
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

#ifndef __HIPBLAS_HELPER_H__
# define __HIPBLAS_HELPER_H__

# include <xkrt/logger/logger-hip.h>
# include <xkrt/logger/logger-hipblas.h>

#  define XKBLAS_HIPBLAS_CALL_POST()                                                            \
    do {                                                                                        \
        HIP_SAFE_CALL(hipEventRecord(stream->hip.events.buffer[idx], stream->hip.handle.high)); \
    } while (0)

# define XKBLAS_HIPBLAS_CALL(CALL)          \
    do {                                    \
        HIPBLAS_SAFE_CALL(CALL);            \
        XKBLAS_HIPBLAS_CALL_POST();         \
    } while (0)

# define XKBLAS_HIPBLAS_DISPATCH_PRECISION_P(NAME, PX, T) \
    if constexpr (P == xkblas_precision_t::PX) body_hip_run<P, hipblas##PX##NAME, T>(stream, instr, idx);

# define XKBLAS_HIPBLAS_DISPATCH_PRECISION_REAL(NAME)            \
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_P(NAME, S, float)          \
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_P(NAME, D, double)

# define XKBLAS_HIPBLAS_DISPATCH_PRECISION_COMPLEX(NAME)                \
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_P(NAME, C, hipblasComplex)        \
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_P(NAME, Z, hipblasDoubleComplex)

# define XKBLAS_HIPBLAS_DISPATCH_PRECISION(NAME)     \
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_REAL(NAME)     \
    XKBLAS_HIPBLAS_DISPATCH_PRECISION_COMPLEX(NAME)

# include "xkblas/cblas.h"

static inline hipblasOperation_t
cblas2hipblas_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return HIPBLAS_OP_N;
        case CblasTrans:        return HIPBLAS_OP_T;
        case CblasConjTrans:    return HIPBLAS_OP_C;
    }
    LOGGER_FATAL("Unknown trans code");
    abort();
}

static inline hipblasSideMode_t
cblas2hipblas_side(int side)
{
    switch (side)
    {
        case CblasLeft:     return HIPBLAS_SIDE_LEFT;
        case CblasRight:    return HIPBLAS_SIDE_RIGHT;
    }
    LOGGER_FATAL("Unknown side code");
    abort();
}

static inline
hipblasFillMode_t cblas2hipblas_uplo( int uplo )
{
    switch (uplo)
    {
        case CblasUpper:    return HIPBLAS_FILL_MODE_UPPER;
        case CblasLower:    return HIPBLAS_FILL_MODE_LOWER;
    }
    LOGGER_FATAL("Unknown uplo code");
    abort();
}

static inline
hipblasDiagType_t cblas2hipblas_diag(int diag)
{
    switch (diag)
    {
        case CblasNonUnit:  return HIPBLAS_DIAG_NON_UNIT;
        case CblasUnit:     return HIPBLAS_DIAG_UNIT;
    }
    LOGGER_FATAL("Unknown diag code");
    abort();
}

#endif /* __HIPBLAS_HELPER_H__ */
