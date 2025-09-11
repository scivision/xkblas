/* ************************************************************************** */
/*                                                                            */
/*   clblast-helper.h                                             .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/23 15:33:40 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/11 21:08:55 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CLBLAST_HELPER_H__
# define __CLBLAST_HELPER_H__

# include <xkrt/logger/logger-clblast.h>

# include "xkblas/cblas.h"
# include <clblast.h>

# define XKBLAS_CLBLAST_DISPATCH_PRECISION_P(NAME, PX, T) \
    if constexpr (P == xkblas_precision_t::PX) body_cl_run<P, CLBlast##PX##NAME, T>(stream, instr, idx);

# define XKBLAS_CLBLAST_DISPATCH_PRECISION_REAL(NAME)               \
    XKBLAS_CLBLAST_DISPATCH_PRECISION_P(NAME, S, float)             \
    XKBLAS_CLBLAST_DISPATCH_PRECISION_P(NAME, D, double)

# define XKBLAS_CLBLAST_DISPATCH_PRECISION_COMPLEX(NAME)            \
    XKBLAS_CLBLAST_DISPATCH_PRECISION_P(NAME, C, cl_float2)         \
    XKBLAS_CLBLAST_DISPATCH_PRECISION_P(NAME, Z, cl_double2)

# define XKBLAS_CLBLAST_DISPATCH_PRECISION(NAME)     \
    XKBLAS_CLBLAST_DISPATCH_PRECISION_REAL(NAME)     \
    XKBLAS_CLBLAST_DISPATCH_PRECISION_COMPLEX(NAME)


static inline CLBlastTranspose
cblas2clblast_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return CLBlastTransposeNo;
        case CblasTrans:        return CLBlastTransposeYes;
        case CblasConjTrans:    return CLBlastTransposeConjugate;
    }
    LOGGER_FATAL("Unknown trans code");
    abort();
}

static inline CLBlastSide
cblas2clblast_side(int side)
{
    switch (side)
    {
        case CblasLeft:     return CLBlastSideLeft;
        case CblasRight:    return CLBlastSideRight;
    }
    LOGGER_FATAL("Unknown side code");
    abort();
}

static inline
CLBlastTriangle cblas2clblast_uplo(int uplo)
{
    switch (uplo)
    {
        case CblasUpper:    return CLBlastTriangleUpper;
        case CblasLower:    return CLBlastTriangleLower;
    }
    LOGGER_FATAL("Unknown uplo code");
    abort();
}

static inline
CLBlastDiagonal cblas2clblast_diag(int diag)
{
    switch (diag)
    {
        case CblasNonUnit:  return CLBlastDiagonalNonUnit;
        case CblasUnit:     return CLBlastDiagonalUnit;
    }
    LOGGER_FATAL("Unknown diag code");
    abort();
}

#endif /* __CLBLAST_HELPER_H__ */
