#ifndef __DEVICE_H__
# define __DEVICE_H__

# include <stdint.h>    /* uint64_t */

# include "device/address-space.h"
# include "device/stream.hpp"
# include "logger/todo.h"
# include "device/task.hpp"
# include "device/thread-worker.hpp"
# include "sync/cache-line-size.h"
# include "sync/mutex.h"

typedef enum    xkblas_device_state_t : uint8_t
{
    XKBLAS_DEVICE_STATE_CREATE      = 0,
    XKBLAS_DEVICE_STATE_INIT        = 1,
    XKBLAS_DEVICE_STATE_COMMIT      = 2,
    XKBLAS_DEVICE_STATE_RUNNING     = 3,
    XKBLAS_DEVICE_STATE_STOP        = 4,
    XKBLAS_DEVICE_STATE_STOPPED     = 5,
    XKBLAS_DEVICE_STATE_FINALISE    = 6,
    XKBLAS_DEVICE_STATE_FINALIZED   = 7,
    XKBLAS_DEVICE_STATE_DESTROY     = 8,
    XKBLAS_DEVICE_STATE_DESTROYED   = 9,
}               xkblas_device_state_t;

typedef enum    xkblas_device_op_t
{
    XKBLAS_DEVICEOP_NOP                 = 0,
    XKBLAS_DEVICEOP_REPLY               = 1,
    XKBLAS_DEVICEOP_WRITEBACK           = 2,
    XKBLAS_DEVICEOP_WRITEBACK_WAIT      = 3,
    XKBLAS_DEVICEOP_MEMSYNC             = 4,
    XKBLAS_DEVICEOP_INVALIDATE_CACHES   = 5
}               xkblas_device_op_t;


/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  xkblas_device_t
{
    xkblas_device_memory_t      memdev;             /* casted to xkblas_device */
    Stream                      stream;             /* communication streams host<->device */
    uint8_t                     driver_id;          /* driver device id in [0..ngpus_for_device] */
    uint8_t                     global_id;          /* global device id in [0, XKBLAS_DEVICES_MAX[ */
    std::atomic<uint8_t>        state;              /* True if driver is initialized */

    ThreadWorker                * thread;           /* the device worker thread */

    struct {
        xkblas_device_op_t      op;                 /* op request for a device */
        uintptr_t               arg;
        std::atomic<uint64_t> * counter;            /* for MEMSYNC or WRITEBACK request */
        int                     err;                /* error returned by the request */
    } request;

    /* pipline: a way to enforce execution order of kernel to device */
    pthread_mutex_t             pipe_lock __attribute__((aligned(CACHE_LINE_SIZE)));
    uint64_t                    pipe_size;
    Task **                     pipeline;          /* circular buffer to store pipeline of task to run on the device */
    uint64_t                    p_write;           /* next position in the pipeline to write a new task */
    uint64_t                    p_ready;           /* position of the first ready task submitted to stream but not yet tested finish */
    uint64_t                    p_finish;          /* position in the stream of the next task to finish */


# if 0

    Thread  * thread;                               /* running thread */
    std::atomic<int>             cnt_push;          /* number of times the ressource is pushed */
    pthread_t                   tid;
   // struct xkblas_driver*        driver;
    uint64_t                    spawn_count;       /* number of tasks */
    uint64_t                    exec_count;        /* number of tasks completed */
    int volatile                finalize;          /* true iff driver stop device */

    double                      time_tasks;        /* cumulative time for all executed tasks */
    uint64_t                    exectasks;         /* #tasks executed */
    double                      flops_exectasks;   /* cumulative flops for all executed tasks */
    double                      data_exectasks;    /* cumulative data for all executed tasks */
    uint64_t                    submittasks;       /* #tasks (between prepare data and end of execution) */
    double                      flops_submittasks; /* idem for pending tasks (between prepare data and end of execution) */
    double                      data_submittasks;  /* idem for pending tasks */
    // const char*                 name;              /* Device name */

    size_t                      mem_limit;
    size_t                      mem_total;

    int                         issleeping;        /* */
    struct {
        xkblas_device_op_t      op;                   /* op request for a device */
        uintptr_t              arg;
        std::atomic<int64_t>   counter;              /* for MEMSYNC or WRITEBACK request */
        int                    err;                  /* error returned by the request */
    } request;

    size_t                      free_mem;
    size_t                      size_alloc;
    size_t                      size_free;

    std::atomic<int16_t>        cnt_pending;       /* number of tasks waiting for data (not too much) */
    std::atomic<int16_t>        cnt_ready;         /* number of ready tasks inserted into device stream  (not too much) */
    std::atomic<int32_t>        cnt_exec;          /* number of tasks executed */
    # endif

}               xkblas_device_t;

#endif /* __DEVICE_H__ */
