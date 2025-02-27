/* ************************************************************************** */
/*                                                                            */
/*   driver-cuda.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/27 21:42:05 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_CUDA_H__
# define __DRIVER_CUDA_H__

# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <cuda_runtime.h>
# include <cublas_v2.h>

typedef struct  xkrt_stream_cuda_t
{
    xkrt_stream_t super;

    struct {

        struct {
            cudaStream_t high;
            cudaStream_t low;
        } handle;

        struct {
            cudaEvent_t * buffer;
            xkrt_stream_instruction_counter_t capacity;
        } events;

        struct {
            cublasHandle_t handle;
        } blas;

    } cu;
}               xkrt_stream_cuda_t;

typedef struct  xkrt_driver_cuda_t
{
    xkrt_driver_t super;
}               xkrt_driver_cuda_t;

#endif /* __DRIVER_CUDA_H__ */
