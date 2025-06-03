/* ************************************************************************** */
/*                                                                            */
/*   driver-cu.h                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/17 14:41:47 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 17:59:50 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_CU_H__
# define __DRIVER_CU_H__

# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <cuda.h>
# include <cublas_v2.h>

typedef struct  xkrt_stream_cu_t
{
    xkrt_stream_t super;

    struct {

        struct {
            CUstream  high;
            CUstream  low;
        } handle;

        struct {
            CUevent * buffer;
            xkrt_stream_instruction_counter_t capacity;
        } events;

        struct {
            cublasHandle_t handle;
        } blas;

    } cu;
}               xkrt_stream_cu_t;


typedef struct  xkrt_device_cu_t
{
    xkrt_device_t inherited;

    struct  {

        CUcontext context;
        CUdevice device;

        struct {
            int pciBusID;
            int pciDeviceID;
            size_t mem_total;
            char name[64];      /* GPU name */
        } prop;

    } cu;
}               xkrt_device_cu_t;

typedef struct  xkrt_driver_cu_t
{
    xkrt_driver_t super;
}               xkrt_driver_cu_t;

#endif /* __DRIVER_CU_H__ */
