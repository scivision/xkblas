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

# include <xkrt/device/device-memory.h>
# include <xkrt/device/offloader.hpp>
# include <xkrt/device/task.hpp>
# include <xkrt/device/thread-worker.hpp>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/sync/mutex.h>

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
    xkrt_device_memory_t memdev;      /* casted to xkrt_device */
    Offloader offloader;                /* communication streams host<->device */
    uint8_t driver_id;                  /* driver device id in [0..ngpus_for_device] */
    uint8_t global_id;                  /* global device id in [0, XKRT_DEVICES_MAX[ - host is a virtual device of id 'XKRT_DEVICES_MAX'*/
    std::atomic<uint8_t> state;         /* xkrt_device_state_t */
    ThreadWorker * thread;              /* the device worker thread */

}               xkrt_device_t;

int xkrt_device_poll(xkrt_device_t * device);
bool xkrt_device_completed(void);

#endif /* __DEVICE_H__ */
