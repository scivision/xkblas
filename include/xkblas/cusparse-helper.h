/* ************************************************************************** */
/*                                                                            */
/*   cusparse-helper.h                                            .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/14 23:28:05 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/09/12 14:58:01 by Romain PEREIRA         / _______ \       */
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

#ifndef __CUSPARSE_HELPER_H__
# define __CUSPARSE_HELPER_H__

# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-cusparse.h>

#  define XKBLAS_CUSPARSE_CALL_POST()                                                       \
    do {                                                                                    \
        CU_SAFE_CALL(cuEventRecord(stream->cu.events.buffer[idx], stream->cu.handle.high)); \
    } while (0)

# define XKBLAS_CUSPARSE_CALL(CALL)     \
    do {                                \
        CUSPARSE_SAFE_CALL(CALL);       \
        XKBLAS_CUSPARSE_CALL_POST();    \
    } while (0)

# define XKBLAS_CUSPARSE_DISPATCH_PRECISION_P(PX, T, ET) \
    if constexpr (P == xkblas_precision_t::PX) body_cuda_run<P, T, ET>(stream, instr, idx);

# define XKBLAS_CUSPARSE_DISPATCH_PRECISION_REAL()              \
    XKBLAS_CUSPARSE_DISPATCH_PRECISION_P(S, float,  CUDA_R_32F) \
    XKBLAS_CUSPARSE_DISPATCH_PRECISION_P(D, double, CUDA_R_64F)

# define XKBLAS_CUSPARSE_DISPATCH_PRECISION_COMPLEX()                       \
    XKBLAS_CUSPARSE_DISPATCH_PRECISION_P(C, cuComplex,        CUDA_C_32F)   \
    XKBLAS_CUSPARSE_DISPATCH_PRECISION_P(Z, cuDoubleComplex,  CUDA_C_64F)

# define XKBLAS_CUSPARSE_DISPATCH_PRECISION()     \
    XKBLAS_CUSPARSE_DISPATCH_PRECISION_REAL()     \
    XKBLAS_CUSPARSE_DISPATCH_PRECISION_COMPLEX()

static inline cusparseOperation_t
cblas2cusparse_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return CUSPARSE_OPERATION_NON_TRANSPOSE;
        case CblasTrans:        return CUSPARSE_OPERATION_TRANSPOSE;
        case CblasConjTrans:    return CUSPARSE_OPERATION_CONJUGATE_TRANSPOSE;
    }
    LOGGER_FATAL("Unknown trans code");
    abort();
}

#endif /* __CUSPARSE_HELPER_H__ */
