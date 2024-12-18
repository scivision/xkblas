/* ************************************************************************** */
/*                                                                            */
/*   device.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:10:34 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEVICE_H__
# define __DEVICE_H__

# include <stdint.h>    /* uint64_t */

# include "device/device-memory.h"
# include "device/offloader.hpp"
# include "device/task.hpp"
# include "device/thread-worker.hpp"
# include "logger/todo.h"
# include "sync/cache-line-size.hpp"
# include "sync/mutex.h"

typedef enum    ptr_device_state_t : uint8_t
{
    PTR_DEVICE_STATE_DEALLOCATED = 0,
    PTR_DEVICE_STATE_CREATE      = 1,
    PTR_DEVICE_STATE_INIT        = 2,
    PTR_DEVICE_STATE_COMMIT      = 3,
    PTR_DEVICE_STATE_RUNNING     = 4,
    PTR_DEVICE_STATE_STOP        = 5,
    PTR_DEVICE_STATE_STOPPED     = 6,
    PTR_DEVICE_STATE_FINALISE    = 7,
    PTR_DEVICE_STATE_FINALIZED   = 8,
    PTR_DEVICE_STATE_DESTROY     = 9,
    PTR_DEVICE_STATE_DESTROYED   = 10

}               ptr_device_state_t;

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  ptr_device_t
{
    ptr_device_memory_t memdev;      /* casted to ptr_device */
    Offloader offloader;                /* communication streams host<->device */
    uint8_t driver_id;                  /* driver device id in [0..ngpus_for_device] */
    uint8_t global_id;                  /* global device id in [0, PTR_DEVICES_MAX[ - host is a virtual device of id 'PTR_DEVICES_MAX'*/
    std::atomic<uint8_t> state;         /* ptr_device_state_t */
    ThreadWorker * thread;              /* the device worker thread */

}               ptr_device_t;

int ptr_device_poll(ptr_device_t * device);
bool ptr_device_completed(void);

#endif /* __DEVICE_H__ */
