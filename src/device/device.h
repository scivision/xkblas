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

typedef enum    xkblas_device_state_t : uint8_t
{
    XKBLAS_DEVICE_STATE_DEALLOCATED = 0,
    XKBLAS_DEVICE_STATE_CREATE      = 1,
    XKBLAS_DEVICE_STATE_INIT        = 2,
    XKBLAS_DEVICE_STATE_COMMIT      = 3,
    XKBLAS_DEVICE_STATE_RUNNING     = 4,
    XKBLAS_DEVICE_STATE_STOP        = 5,
    XKBLAS_DEVICE_STATE_STOPPED     = 6,
    XKBLAS_DEVICE_STATE_FINALISE    = 7,
    XKBLAS_DEVICE_STATE_FINALIZED   = 8,
    XKBLAS_DEVICE_STATE_DESTROY     = 9,
    XKBLAS_DEVICE_STATE_DESTROYED   = 10

}               xkblas_device_state_t;

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  xkblas_device_t
{
    xkblas_device_memory_t memdev;      /* casted to xkblas_device */
    Offloader offloader;                /* communication streams host<->device */
    uint8_t driver_id;                  /* driver device id in [0..ngpus_for_device] */
    uint8_t global_id;                  /* global device id in [0, XKBLAS_DEVICES_MAX[ - host is a virtual device of id 'XKBLAS_DEVICES_MAX'*/
    std::atomic<uint8_t> state;         /* xkblas_device_state_t */
    ThreadWorker * thread;              /* the device worker thread */

}               xkblas_device_t;

int xkblas_device_poll(xkblas_device_t * device);
bool xkblas_device_completed(void);

#endif /* __DEVICE_H__ */
