/* ************************************************************************** */
/*                                                                            */
/*   logger-cublas.h                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_CUBLAS_H__
# define __LOGGER_CUBLAS_H__

# include <xkrt/logger/logger.h>
# include <cublas_v2.h>

static const char *
cublas_error_to_str(cublasStatus_t status)
{
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
    # else
    const char * name  = cublasGetStatusName(status);
    # endif /* CUBLAS_VER_MAJOR */
    return name;
}

# define CUBLAS_SAFE_CALL(X)                                                                \
    do {                                                                                    \
        cublasStatus_t r = X;                                                               \
        if (r != CUBLAS_STATUS_SUCCESS)                                                     \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, cublas_error_to_str(r), r);    \
    } while (0)

#endif /* __LOGGER_CUBLAS_H__ */
