/* ************************************************************************** */
/*                                                                            */
/*   device.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:49:01 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEVICE_H__
# define __DEVICE_H__

# include <stdint.h>    /* uint64_t */

# include <xkrt/driver/offloader.hpp>
# include <xkrt/driver/driver-type.h>
# include <xkrt/driver/thread-worker.hpp>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/area.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/sync/mutex.h>
# include <xkrt/task/task.hpp>

typedef enum    xkrt_device_state_t : uint8_t
{
    XKRT_DEVICE_STATE_DEALLOCATED = 0,
    XKRT_DEVICE_STATE_CREATE      = 1,
    XKRT_DEVICE_STATE_INIT        = 2,
    XKRT_DEVICE_STATE_COMMIT      = 3,
    XKRT_DEVICE_STATE_RUNNING     = 4,
    XKRT_DEVICE_STATE_STOP        = 5,
    XKRT_DEVICE_STATE_STOPPED     = 6,
    XKRT_DEVICE_STATE_FINALISE    = 7,
    XKRT_DEVICE_STATE_FINALIZED   = 8,
    XKRT_DEVICE_STATE_DESTROY     = 9,
    XKRT_DEVICE_STATE_DESTROYED   = 10

}               xkrt_device_state_t;

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  xkrt_device_t
{
    xkrt_driver_type_t driver_type;     /* the driver type in [0..XKRT_DRIVER_TYPE_MAX[ */
    Offloader offloader;                /* communication streams host<->device */
    uint8_t driver_id;                  /* driver device id in [0..ngpus_for_device] */
    xkrt_device_global_id_t global_id;  /* global device id in [0, XKRT_DEVICES_MAX[ - host is a virtual device of id 'XKRT_DEVICES_MAX'*/
    std::atomic<uint8_t> state;         /* xkrt_device_state_t */
    ThreadWorker * thread;              /* the device worker thread */
    xkrt_area_t memdev;                 /* memory area, only 1 per device */

    # if XKRT_SUPPORT_STATS
    struct {
        struct {
            stats_int_t freed;
            struct {
                stats_int_t total;
                stats_int_t currently;
            } allocated;
        } memory;
    } stats;
    # endif /* XKRT_SUPPORT_STATS */

}               xkrt_device_t;

int xkrt_device_poll(xkrt_device_t * device);

void xkrt_device_stream_instruction_submit_kernel(
    xkrt_device_t * device,
    void (*launch)(void * handle, void * vargs),
    void * vargs,
    const xkrt_callback_t & callback
);

void
xkrt_device_stream_instruction_submit_copy(
    xkrt_device_t                 * device,
    const memory_view_t           & host_view,
    const xkrt_device_global_id_t   dst_device_global_id,
    const memory_replicate_view_t & dst_device_view,
    const xkrt_device_global_id_t   src_device_global_id,
    const memory_replicate_view_t & src_device_view,
    const xkrt_callback_t         & callback
);

/* free all memory of the device, resetting the allocator state to chunk0 */
void xkrt_device_memory_reset(xkrt_device_t * device);

#endif /* __DEVICE_H__ */
