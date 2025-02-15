/* ************************************************************************** */
/*                                                                            */
/*   clblast-helper.h                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CLBLAST_HELPER_H__
# define __CLBLAST_HELPER_H__

# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-clblast.h>

# include "xkblas/cblas.h"
# include <clblast.h>

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
