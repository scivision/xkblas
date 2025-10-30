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

#ifndef __CUSPARSE_HELPER_H__
# define __CUSPARSE_HELPER_H__

# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-cusparse.h>

#  define XKBLAS_CUSPARSE_CALL_POST()                                                       \
    do {                                                                                    \
        CU_SAFE_CALL(cuEventRecord(queue->cu.events.buffer[idx], queue->cu.handle.high)); \
    } while (0)

# define XKBLAS_CUSPARSE_CALL(CALL)     \
    do {                                \
        CUSPARSE_SAFE_CALL(CALL);       \
        XKBLAS_CUSPARSE_CALL_POST();    \
    } while (0)

# define XKBLAS_CUSPARSE_DISPATCH_PRECISION_P(PX, T, ET) \
    if constexpr (P == xkblas_precision_t::PX) body_cuda_run<P, T, ET>(queue, cmd, idx);

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
