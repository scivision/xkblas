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

# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-cublas.h>

# ifdef NDEBUG
#  define XKBLAS_CUBLAS_CALL_POST()                                                             \
    do {                                                                                        \
        CU_SAFE_CALL(cudaEventRecord(stream->cu.events.buffer[idx], stream->cu.handle.high));   \
    } while (0)
# else
#  define XKBLAS_CUBLAS_CALL_POST()                                                             \
    do {                                                                                        \
        CU_SAFE_CALL(cudaEventRecord(stream->cu.events.buffer[idx], stream->cu.handle.high));   \
        cudaPointerAttributes attr;                                                             \
        for (int i = 0 ; i < task->naccesses ; ++i)                                             \
        {                                                                                       \
            Access * access = task->accesses + i;                                               \
            assert(access);                                                                     \
            int device;                                                                         \
            cudaGetDevice(&device);                                                             \
            cudaPointerGetAttributes(&attr, (const void *) access->device_view.addr);           \
            assert(attr.device == device);                                                      \
            assert(attr.type == cudaMemoryTypeDevice);                                          \
        }                                                                                       \
    } while (0)
# endif

# define XKBLAS_CUBLAS_CALL(CALL)       \
    do {                                \
        CUBLAS_SAFE_CALL(CALL);         \
        XKBLAS_CUBLAS_CALL_POST();      \
    } while (0)

#endif /* __CUBLAS_HELPER_H__ */
