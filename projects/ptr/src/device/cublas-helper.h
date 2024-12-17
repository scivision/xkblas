/* ************************************************************************** */
/*                                                                            */
/*   cublas-helper.h                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CUBLAS_HELPER_H__
# define __CUBLAS_HELPER_H__

# include <cblas.h>
# include <cublas_v2.h>

static inline cublasOperation_t
cblas2cublas_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return CUBLAS_OP_N;
        case CblasTrans:        return CUBLAS_OP_T;
        case CblasConjTrans:    return CUBLAS_OP_C;
    }
    XKBLAS_FATAL("Unknown trans code");
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
    XKBLAS_FATAL("Unknown side code");
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
    XKBLAS_FATAL("Unknown uplo code");
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
    XKBLAS_FATAL("Unknown diag code");
    abort();
}

static inline void
xkblas_cublas_status_check(cublasStatus_t status)
{
    if (status == CUBLAS_STATUS_SUCCESS)
        return ;

    # if CUBLAS_VER_MAJOR < 12
    static const char * names[] = {
        "SUCCESS",                  /*  0 */
        "NOT_INITIALIZED",          /*  1 */
        NULL,                       /*  2 */
        "ALLOC_FAILED",             /*  3 */
        NULL,                       /*  4 */
        NULL,                       /*  5 */
        NULL,                       /*  6 */
        "INVALID_VALUE",            /*  7 */
        "ARCH_MISMATCH",            /*  8 */
        NULL,                       /*  9 */
        NULL,                       /* 10 */
        "MAPPING_ERROR",            /* 11 */
        NULL,                       /* 12 */
        "EXECUTION_FAILED",         /* 13 */
        "INTERNAL_ERROR",           /* 14 */
        "NOT_SUPPORTED",            /* 15 */
        "LICENSE_ERROR",            /* 16 */
    };
    const char * name  = (status >= 0 && status <= 16) ? names[status] : NULL;
    const char * descr = NULL;
    # else
    const char * name  = cublasGetStatusName(status);
    const char * descr = cublasGetStatusString(status);
    # endif /* CUBLAS_VER_MAJOR */
    XKBLAS_FATAL("cuBlas error `%s` occured - %s", name, descr);
}

#endif /* __CUBLAS_HELPER_H__ */
