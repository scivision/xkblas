/* ************************************************************************** */
/*                                                                            */
/*   driver-hip.h                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/17 14:41:47 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:00:04 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_HIP_H__
# define __DRIVER_HIP_H__

# ifndef __HIP_PLATFORM_AMD__
#  define __HIP_PLATFORM_AMD__
# endif

# include <xkrt/driver/stream.h>
# include <hip/hip_runtime.h>
# include <hipblas/hipblas.h>

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

        struct {
            hipblasHandle_t handle;
        } blas;

    } hip;
}               xkrt_stream_hip_t;

typedef struct  xkrt_device_hip_t
{
    xkrt_device_t inherited;

    struct  {

        hipCtx_t context;
        hipDevice_t device;

        struct {
            int pciBusID;
            int pciDeviceID;
            size_t mem_total;
            char name[64];      /* GPU name */
        } prop;

    } hip;
}               xkrt_device_hip_t;

typedef struct  xkrt_driver_hip_t
{
    xkrt_driver_t super;
}               xkrt_driver_hip_t;


#endif /* __DRIVER_HIP_H__ */
