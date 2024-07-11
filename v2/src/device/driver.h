#ifndef __DRIVER_H__
# define __DRIVER_H__

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>     /* cpu_set_t */
# include <stdint.h>    /* uint64_t */

# include "device/address-space.h"
# include "device/stream.hpp"
# include "logger/todo.h"
# include "scheduler/task.hpp"
# include "scheduler/thread.hpp"
# include "sync/cache-line-size.h"
# include "sync/mutex.h"

void xkblas_drivers_init(void);
void xkblas_drivers_deinit(void);


# pragma message(TODO "Organize this file, split independent part in multiple files")

# define XKBLAS_DEVICES_MAX 32

# define XKBLAS_STREAM_CAPACITY 512

# define XKBLAS_DRIVER_PREFIX_NAME "XKBLAS_DRIVER_"
# define XKBLAS_DRIVER_ENTRYPOINT_NAME( func_name ) XKBLAS_DRIVER_PREFIX_NAME #func_name
# define XKBLAS_DRIVER_ENTRYPOINT( func_name ) XKBLAS_DRIVER_ ## func_name

# if 0

Romain: do we actually need this abstraction ? 1 device = 1 memory should be sufficient

////////////////////////
// Locality domain //
////////////////////////

/** \ingroup Kaapi
    Identifier to a locality domain 
*/
typedef uint64_t  xkblas_ldid_t;


/** \ingroup Kaapi
    Type of locality domain.
    For each type the associated functions with return the description
    in term of capability (#ressource, #memory etc)
*/
typedef enum 
{
  XKBLAS_LD_CORE    = 0x01, /* core type */
  XKBLAS_LD_NUMA    = 0x02, /* NUMA type */
  XKBLAS_LD_SOCKET  = 0x04, /* socket type */
  XKBLAS_LD_BOARD   = 0x08, /* machine type */
  XKBLAS_LD_GPU     = 0x10, /* GPUs type */
  XKBLAS_LD_ALLTYPE = 0x1F  /* encode union of previous type */
} xkblas_ld_type_t;

#define XKBLAS_LD_COUNTTYPE 5


/* Locality domain.
   - push/pop are multiple operations (may be called concurrently by n>1 threads)
   - push is called by external ressource to the domain
   - pop is called by ressource of the domain
   The queue is a mail box between external ressources and the ressource of the domain.
   It is managed as a bounded FIFO priority queue using xkblas_queue_fifo_push and xkblas_queue_fifo_pop.
   */
typedef struct  xkblas_localitydomain_t
{
    xkblas_ld_type_t         type;
    xkblas_ldid_t            ldid;     /* global id on 64 bits */
    unsigned int            idx;      /* in xkblas_all_lddomains[type]->ld */
    // xkblas_fifo_queue_t*     queue;    /* same data structure as a queue, but managed to be FIFO */
    uint64_t                perfrank; /* inspired from perfrank in cuda: number of affinity group with ~ same characteristic in communication performance */
    uint64_t*               affinity; /* of size perfrank -1, if affinity[perf] has bit i set to 1, then localitydomain i has affinity with it */
    struct xkblas_localitydomain_t* parent;
    unsigned int            subldcount;
    struct xkblas_localitydomain_t**subld;
}               xkblas_localitydomain_t;

# endif

typedef enum    xkblas_device_state_t
{
    XKBLAS_DEVICE_STATE_CREATE,
    XKBLAS_DEVICE_STATE_INIT,
    XKBLAS_DEVICE_STATE_COMMIT,
    XKBLAS_DEVICE_STATE_DOSTART,
    XKBLAS_DEVICE_STATE_START,
    XKBLAS_DEVICE_STATE_STOP,
    XKBLAS_DEVICE_STATE_STOPPED,
    XKBLAS_DEVICE_STATE_FINALISE,
    XKBLAS_DEVICE_STATE_FINALIZED,
    XKBLAS_DEVICE_STATE_DESTROY,
    XKBLAS_DEVICE_STATE_DESTROYED
}               xkblas_device_state_t;

typedef enum    xkblas_device_op_t
{
    XKBLAS_DEVICEOP_NOP =0,
    XKBLAS_DEVICEOP_REPLY,
    XKBLAS_DEVICEOP_WRITEBACK,
    XKBLAS_DEVICEOP_WRITEBACK_WAIT,
    XKBLAS_DEVICEOP_MEMSYNC,
    XKBLAS_DEVICEOP_INVALIDATE_CACHES
}               xkblas_device_op_t;

# pragma message(TODO "Replace 'xkblas_device_t' with a C++ abstract class")

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  xkblas_device_t
{
    xkblas_device_memory_t      memdev;            /* casted to xkblas_device */
    Stream                      stream;            /* communication streams host<->device */
    int                         driver_device_id;   /* driver device id in [0..ngpus_for_device] */

    Thread  * thread;                               /* running thread */
    std::atomic<int>             cnt_push;          /* number of times the ressource is pushed */
    pthread_t                   tid;
   // struct xkblas_driver*        driver;
    uint64_t                    spawn_count;       /* number of tasks */
    uint64_t                    exec_count;        /* number of tasks completed */
    int volatile                finalize;          /* true iff driver stop device */
    volatile xkblas_device_state_t state;           /* True if driver is initialized */

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

    # pragma message(TODO "Replace with xkblas-specific synchronization primitives")
    pthread_mutex_t             lock;              /* used to synchronize device thread */
    pthread_cond_t              cond;              /* and cpu threads if any */
    pthread_cond_t              cond_sleep;        /* and cpu threads if any */
    int                         issleeping;        /* */
    struct {
        xkblas_device_op_t      op;                   /* op request for a device */
        uintptr_t              arg;
        std::atomic<int64_t>   counter;              /* for MEMSYNC or WRITEBACK request */
        int                    err;                  /* error returned by the request */
    } request;

    /* pipline: a way to enforce execution order of kernel to device */
    pthread_mutex_t             pipe_lock __attribute__((aligned(CACHE_LINE_SIZE)));
    uint64_t                    pipe_size;
    Task **                     pipeline;          /* circular buffer to store pipeline of task to run on the device */
    uint64_t                    p_write;           /* next position in the pipeline to write a new task */
    uint64_t                    p_ready;           /* position of the first ready task submitted to stream but not yet tested finish */
    uint64_t                    p_finish;          /* position in the stream of the next task to finish */

    size_t                      free_mem;
    size_t                      size_alloc;
    size_t                      size_free;

    std::atomic<int16_t>        cnt_pending;       /* number of tasks waiting for data (not too much) */
    std::atomic<int16_t>        cnt_ready;         /* number of ready tasks inserted into device stream  (not too much) */
    std::atomic<int32_t>        cnt_exec;          /* number of tasks executed */
}               xkblas_device_t;

typedef struct  xkblas_driver_t
{
    /* Function handlers: accessor to meta data */
    const char   *(*f_get_name)(void);          /* name of the driver (human-readable) */
    unsigned int (*f_get_flags)(void);          /* flags: not really used */
    unsigned int (*f_get_ndevices)(void);       /* number of devices managed by the driver */
    unsigned int (*f_get_ndevices_max)(void);   /* return the number of devices available to the driver */

    /* life cycle functions for the driver of devices (1 device == 1 ressource) */
    int (*f_init)(void);
    void (*f_finalize)(void);

    /* driver specific functions for all devices managed by the driver */
    /* Memory registration of host memory */
    uint64_t (*f_host_register)(
            void * ptr, uint64_t sz,
            xkblas_io_callback_func_t callback,
            void * arg0, void * arg1, void * arg2
    );

    /* test completion of asynchronous host_register operation.
       Argument is the handle returned by host_register (if != -1).
       If flag == 0, then the operation is a non blocking test that allows
       to test progression of pinning.
       If flag == 1, then the operation blocks the caller until the request
       has been completed.
       If flag == 2, then the operation blocks the caller until all previous
       requests have been completed.
       Return values are:
       - 0 non error or the test operation complets
       - EINVAL: invalid handle
       - EINPROGESS: request not yet completed
       */
    int  (*f_host_register_testwait)(uint64_t handle, int flag);

    /* Memory unregistration of host memory: asynchronous version */
    uint64_t  (*f_host_unregister)(
        void * ptr, uint64_t sz,
        xkblas_io_callback_func_t callback,
        void * arg0, void * arg1, void * arg2
    );

    /* Set the cpuset of the attr for creating the thread that will manage the device dev */
    int (*f_device_set_cpuset)(cpu_set_t*, int);
    /* create device object and initialize device_id field with argument */
    xkblas_device_t * (*f_device_create)(xkblas_driver_t *, int);
    int (*f_device_destroy)(xkblas_device_t*);
    /* initialize device fields, especially with virtual functions */
    const char* (*f_device_info)(xkblas_device_t*);
    void (*f_device_init)(int device_id);
    int (*f_device_commit)(xkblas_device_t*);
    void (*f_device_finalize)(xkblas_device_t*);
    /* consider device as the current device */
    int (*f_device_attach)(xkblas_device_t*);
    /* consider device as the current device */
    int (*f_device_detach)(xkblas_device_t*);

    /* GPU blas support */
    void* (*f_get_gpublas_handle)(xkblas_device_t*);

}               xkblas_driver_t;

typedef struct  xkblas_driver_thread_arg_t
{
    xkblas_driver_t * driver;
    int driver_device_id;
    int global_device_id;
    pthread_t tid;
}               xkblas_driver_thread_arg_t;

#endif /* __DRIVER_H__ */
