/* ************************************************************************** */
/*                                                                            */
/*   driver-cl.h                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/17 14:41:47 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 17:59:44 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_CL_H__
# define __DRIVER_CL_H__

# include <xkrt/driver/stream.h>
# include <CL/cl.h>

// cl memory buffers, as opencl do not allow direct pointer arithmetic on device buffers and xkrt requires it
typedef struct  xkrt_device_cl_buffer_t
{
    uintptr_t addr;
    size_t size;
    struct {
        cl_mem mem;
    } cl;
}               xkrt_device_cl_buffer_t;

# define XKRT_DRIVER_CL_MAX_BUFFERS 8

// devices
typedef struct  xkrt_device_cl_t
{
    xkrt_device_t inherited;

    struct {
        cl_device_id id;
        cl_context context;
    } cl;

    struct {
        xkrt_device_cl_buffer_t buffers[XKRT_DRIVER_CL_MAX_BUFFERS];
        int nbuffers;
        uintptr_t head;
    } memory;
}               xkrt_device_cl_t;

typedef struct  xkrt_stream_cl_t
{
    xkrt_stream_t super;

    struct {
        cl_command_queue queue;
        cl_event * events;
    } cl;

    xkrt_device_cl_t * device;

}               xkrt_stream_cl_t;

typedef struct  xkrt_driver_cl_t
{
    xkrt_driver_t super;
}               xkrt_driver_cl_t;

/* cl works on 'cl_mem' buffers and do not support pointer arithmetic directly
 * on these, so this routine returns the buffer and the offset for the given
 * addr */
void xkrt_driver_cl_get_buffer_and_offset_1D(
    xkrt_device_cl_t * device,
    uintptr_t addr,
    cl_mem * mem,
    size_t * offset
);

void
xkrt_driver_cl_get_buffer_and_offset_1D(
    xkrt_device_cl_t * device,
    uintptr_t addr,
    cl_mem * mem,
    size_t * offset
);

void
xkrt_driver_cl_get_buffer_and_offset_2D(
    xkrt_device_cl_t * device,
    uintptr_t addr,
    size_t pitch,
    cl_mem * mem,
    size_t * offset
);

#endif /* __DRIVER_CL_H__ */
