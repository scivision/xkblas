/* ************************************************************************** */
/*                                                                            */
/*   driver-hip.h                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/26 04:53:32 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_HIP_H__
# define __DRIVER_HIP_H__

# define __HIP_PLATFORM_AMD__
# include <hip/hip_runtime.h>

# include <xkrt/driver/stream.h>

typedef struct  xkrt_stream_hip_t
{
    xkrt_stream_t super;

    struct {

        struct {
            hipStream_t high;
            hipStream_t low;
        } handle;

        struct {
            hipEvent_t * buffer;
            xkrt_stream_instruction_counter_t capacity;
        } events;

//        struct {
//            cublasHandle_t handle;
//        } blas;

    } hip;
}               xkrt_stream_hip_t;

#endif /* __DRIVER_HIP_H__ */
