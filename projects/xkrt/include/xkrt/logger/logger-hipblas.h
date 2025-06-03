/* ************************************************************************** */
/*                                                                            */
/*   logger-hipblas.h                                             .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/27 15:08:32 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:37 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
