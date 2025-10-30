/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

#ifndef __CLBLAST_HELPER_H__
# define __CLBLAST_HELPER_H__

# include <xkrt/logger/logger-clblast.h>

# include "xkblas/cblas.h"
# include <clblast.h>

# define XKBLAS_CLBLAST_DISPATCH_PRECISION_P(NAME, PX, T) \
    if constexpr (P == xkblas_precision_t::PX) body_cl_run<P, CLBlast##PX##NAME, T>(queue, cmd, idx);

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
