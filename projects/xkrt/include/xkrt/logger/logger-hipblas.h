/* ************************************************************************** */
/*                                                                            */
/*   logger-hipblas.h                                                         */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/22 19:59:40 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_HIPBLAS_H__
# define __LOGGER_HIPBLAS_H__

# include <xkrt/logger/logger.h>
# include <hipblas/hipblas.h>

static const char *
hipblas_error_to_str(hipblasStatus_t status)
{
    # if 0
    HIPBLAS_STATUS_SUCCESS                // Operation completed successfully
    HIPBLAS_STATUS_NOT_INITIALIZED  = 1,  // The library was not initialized
    HIPBLAS_STATUS_ALLOC_FAILED     = 2,  // Resource allocation failed
    HIPBLAS_STATUS_INVALID_VALUE    = 3,  // Invalid value was passed
    HIPBLAS_STATUS_MAPPING_ERROR    = 4,  // Memory mapping error
    HIPBLAS_STATUS_EXECUTION_FAILED = 5,  // Execution of GPU operations failed
    HIPBLAS_STATUS_INTERNAL_ERROR   = 6,  // An internal library error occurred
    HIPBLAS_STATUS_NOT_SUPPORTED    = 7,  // The operation is not supported
    HIPBLAS_STATUS_ARCH_MISMATCH    = 8   // The device architecture is not supported
    # endif

    static const char * names[] = {
        "SUCCESS",
        "NOT_INITIALIZED",
        "ALLOC_FAILED",
        "INVALID_VALUE",
        "MAPPING_ERROR",
        "EXECUTION_FAILED",
        "INTERNAL_ERROR",
        "NOT_SUPPORTED",
        "ARCH_MISMATCH"
    };
    const char * name  = (status >= 0 && status <= 8) ? names[status] : NULL;
    return name;
}

# define HIPBLAS_SAFE_CALL(X)                                                               \
    do {                                                                                    \
        hipblasStatus_t r = X;                                                              \
        if (r != HIPBLAS_STATUS_SUCCESS)                                                    \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, hipblas_error_to_str(r), r);   \
    } while (0)

#endif /* __LOGGER_HIPBLAS_H__ */
