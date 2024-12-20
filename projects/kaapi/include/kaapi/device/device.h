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

# include <kaapi/device/device-memory.h>
# include <kaapi/device/offloader.hpp>
# include <kaapi/device/task.hpp>
# include <kaapi/device/thread-worker.hpp>
# include <kaapi/logger/todo.h>
# include <kaapi/memory/cache-line-size.hpp>
# include <kaapi/sync/mutex.h>

typedef enum    kaapi_device_state_t : uint8_t
{
    KAAPI_DEVICE_STATE_DEALLOCATED = 0,
    KAAPI_DEVICE_STATE_CREATE      = 1,
    KAAPI_DEVICE_STATE_INIT        = 2,
    KAAPI_DEVICE_STATE_COMMIT      = 3,
    KAAPI_DEVICE_STATE_RUNNING     = 4,
    KAAPI_DEVICE_STATE_STOP        = 5,
    KAAPI_DEVICE_STATE_STOPPED     = 6,
    KAAPI_DEVICE_STATE_FINALISE    = 7,
    KAAPI_DEVICE_STATE_FINALIZED   = 8,
    KAAPI_DEVICE_STATE_DESTROY     = 9,
    KAAPI_DEVICE_STATE_DESTROYED   = 10

}               kaapi_device_state_t;

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  kaapi_device_t
{
    kaapi_device_memory_t memdev;      /* casted to kaapi_device */
    Offloader offloader;                /* communication streams host<->device */
    uint8_t driver_id;                  /* driver device id in [0..ngpus_for_device] */
    uint8_t global_id;                  /* global device id in [0, KAAPI_DEVICES_MAX[ - host is a virtual device of id 'KAAPI_DEVICES_MAX'*/
    std::atomic<uint8_t> state;         /* kaapi_device_state_t */
    ThreadWorker * thread;              /* the device worker thread */

}               kaapi_device_t;

int kaapi_device_poll(kaapi_device_t * device);
bool kaapi_device_completed(void);

#endif /* __DEVICE_H__ */
