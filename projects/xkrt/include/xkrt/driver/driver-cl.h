/* ************************************************************************** */
/*                                                                            */
/*   driver-cl.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:48:24 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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

/* cl works on 'cl_mem' buffers and do not support pointer arithmetic directly
 * on these, so this routine returns the buffer and the offset for the given
 * addr */
void xkrt_driver_cl_get_buffer_and_offset(
    xkrt_device_cl_t * device,
    uintptr_t addr,
    cl_mem * mem,
    size_t * offset
);

cl_mem
xkrt_driver_cl_get_subbuffer(
    xkrt_device_cl_t * device,
    const void * ptr,
    int mode
);

#endif /* __DRIVER_CL_H__ */
