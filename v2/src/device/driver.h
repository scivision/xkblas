#ifndef __DRIVER_H__
# define __DRIVER_H__



void xkblas_drivers_init(void);
void xkblas_drivers_deinit(void);



# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>     /* cpu_set_t */
# include <stdint.h>    /* uint64_t */

# include "logger/todo.h"
# include "scheduler/task.hpp"
# include "scheduler/thread.hpp"
# include "sync/mutex.h"

# pragma message(TODO "Organize this file, split independent part in multiple files")

# if !defined(CACHE_LINE_SIZE)
#  define CACHE_LINE_SIZE 64
# endif

# define XKBLAS_DEVICES_MAX 32

# define XKBLAS_DRIVER_PREFIX_NAME "XKBLAS_DRIVER_"
# define XKBLAS_DRIVER_ENTRYPOINT_NAME( func_name ) XKBLAS_DRIVER_PREFIX_NAME #func_name
# define XKBLAS_DRIVER_ENTRYPOINT( func_name ) XKBLAS_DRIVER_ ## func_name


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




///////////////////////////
// Driver devices memory //
///////////////////////////
typedef uint64_t xkblas_address_space_id_t;

# pragma message(TODO "What is a 'xkblas_alloc_data' struct ?")
struct xkblas_alloc_data;
typedef struct xkblas_alloc_data xkblas_alloc_data_t;

# pragma message(TODO "What is a 'xkblas_alloc_chunk' struct ?")
struct xkblas_alloc_chunk;
typedef struct xkblas_alloc_chunk xkblas_alloc_chunk_t;

/** Type of pointer for all address spaces.
    The pointer encode both the pointer (field ptr) and the location of the address space
    in asid. Pointer arithmetic is allowed on this type on the ptr field.
    If pointer is on device with disjoint adress space, meta is a host data to help storing
    meta data.
*/
typedef struct  xkblas_pointer_t
{
    xkblas_address_space_id_t asid;
    uintptr_t                ptr;
    uintptr_t                meta;
}               xkblas_pointer_t;



/** Type of allowed memory view for the memory interface:
    - 1D array (base, size)
      simple contiguous 1D array
    - 2D array (base, size[2], lda)
      assume a row major storage of the memory : the 2D array has
      size[0] rows of size[1] rowwidth. lda is used to pass from
      one row to the next one. 
    The base (xkblas_pointer_t) is not part of the view description
*/
#define XKBLAS_MEMORY_VIEW_1D 1 
#define XKBLAS_MEMORY_VIEW_2D 2  /* assume row major */
#define XKBLAS_MEMORY_VIEW_3D 3
#define XKBLAS_MEMORY_STORAGE_ROWMAJOR 1
#define XKBLAS_MEMORY_STORAGE_COLMAJOR 2
typedef struct xkblas_memory_view_t {
  uintptr_t     offset;
  size_t        size[3];
  size_t        ld;
  size_t        wordsize;
  uint8_t       type; 
  uint8_t       storage;
} xkblas_memory_view_t;

typedef struct  xkblas_io_status_t
{
  int error;
  float cpu_delay;  /* time on CPU between launch and completion (s)*/
  float gpu_delay;  /* time of CPU between launch and completion (s)*/
  uint64_t bytes;     /* bytes transfered in case of memory copy */
}               xkblas_io_status_t;

typedef void (*xkblas_io_callback_func_t)(
    xkblas_io_status_t,
    struct xkblas_io_stream_t *,
    void *, void *, void *
);

typedef struct  xkblas_io_callback_t
{
    xkblas_io_callback_func_t func;
    void * args[3];
}               xkblas_io_callback_t;

typedef enum    xkblas_io_type
{
    XKBLAS_IO_NOP      = 0,
    XKBLAS_IO_BEGIN    = 1,
    XKBLAS_IO_END      = 2,
    XKBLAS_IO_COPY_H2H = 3,
    XKBLAS_IO_COPY_H2D = 4,
    XKBLAS_IO_COPY_D2H = 5,
    XKBLAS_IO_COPY_D2D = 6,
    XKBLAS_IO_BARRIER  = 7,
    XKBLAS_IO_KERN     = 8
}               xkblas_io_type_t;

typedef enum xkblas_io_copy_priority {
  XKBLAS_IO_COPY_PRIORITY_LOW    = 0,
  XKBLAS_IO_COPY_PRIORITY_NORMAL = 1,
  XKBLAS_IO_COPY_PRIORITY_HIGH   = 2
} xkblas_io_copy_priority_t;



# define XKBLAS_MEMORY_VALUE_TYPE uint16_t

# define XKBLAS_MEMORY_DEVICE_FLAG_NONE          0
# define XKBLAS_MEMORY_DEVICE_FLAG_MOSTLY_FULL   0x1
# define XKBLAS_MEMORY_DEVICE_FLAG_FULL          0x2

typedef struct  xkblas_device_memory_t
{
    xkblas_address_space_id_t asid;
    // xkblas_device_t * device;
    xkblas_mutex_t mem_lock;
    xkblas_alloc_data_t * freelist_bloc;
    xkblas_alloc_data_t * freelist_metabloc;
    xkblas_alloc_chunk_t * free_chunk_list;
    xkblas_alloc_chunk_t * main_chunk;

    /* Virtualization of alloc/free on the offload memory device */
    uintptr_t (*f_alloc)(struct xkblas_device_memory_t*,  size_t, int* );
    void  (*f_free)(struct xkblas_device_memory_t*, uintptr_t, size_t);

    /* returns:
       0: success
       EINPROGRESS : pending operations on the device
       else error
    */
    int   (*f_copy)(struct xkblas_device_memory_t*,
                    xkblas_pointer_t /* dest*/,
                    const xkblas_memory_view_t* /*view_dest*/,
                    xkblas_pointer_t /*src*/,
                    const xkblas_memory_view_t* /*view_src*/,
                    int flags, /* 0, 1, 2 */
                    xkblas_io_callback_func_t cbk,
                    void* arg0, void* arg1, void* arg2
    );
    int  (*f_memsync)(struct xkblas_device_memory_t*, int begend);

    /* to help to manage cache */
    size_t (*f_get_mem_info)(struct xkblas_device_memory_t*, size_t*, size_t*);
    size_t (*f_get_free_mem)(struct xkblas_device_memory_t*);

    /* return source lid to reach lid_dest knowing valid_bit and xfer_bit for the data */
    uint16_t (*f_get_source)( struct xkblas_device_memory_t*, uint16_t, XKBLAS_MEMORY_VALUE_TYPE, XKBLAS_MEMORY_VALUE_TYPE );

}               xkblas_device_memory_t;

/* io instruction to write/read data from the corresponding device
   src == host emitting the request
   */
struct xkblas_io_copy {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    xkblas_io_copy_priority_t     prio;
    const void*                  src;
    const xkblas_memory_view_t*   view_src;
    struct xkblas_device_memory_t *  dev_src;
    void*                        dest;
    const xkblas_memory_view_t*   view_dest;
    struct xkblas_device_memory_t *  dev_dest;
};

/* marker begin...end for group of request
*/
struct xkblas_io_begin {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    struct xkblas_io_instruction* first;
};

struct xkblas_io_end {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    struct xkblas_io_instruction* last;
};


/* marker call back, acts as a full memory barrier : any write, read or kernel instructon
   before the sync are never re-ordered after the sync.
   */
struct xkblas_io_barrier {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
};

/* io instruction kernel : to launch kernel on the device
  The delay field of the status arguments of the callback, if defined, is the delay in millisecond
  to execute the kernel.
*/
struct xkblas_io_kernel {
  xkblas_io_callback_func_t           fnc;
  void*                        arg[3];
  xkblas_task_body_t            body;
  Task *                task;
};

/* A Kaapi stream of IO requests
   - bounded io instructions
   - any read/write instructions may be reordered
   - group of instructions (between marker io_begin/io_end) cannot re-ordered outside the
   group
   - io_barrier acts as a full memory barrier
   - instructions may be aggregated
   pos_r, pos_w, pos_rp and pos_wp are non decreasing integer that correspond to entries %count
   in the table.

   If IO threads are activated, then one thread manage the IO while the device thread manages
   the kernel stream. In any case the device thread manages the progression of the whole computation
   and calls the callback at when events are posted.

*/
typedef struct xkblas_io_instruction
{
  xkblas_io_type_t          type;
  union {
    struct xkblas_io_callback_t callback;   /* callback info always first fields of structure */
    struct xkblas_io_begin      f_io;
    struct xkblas_io_end        l_io;
    struct xkblas_io_copy       c_io;
    struct xkblas_io_kernel     k_io;
    struct xkblas_io_barrier    b_io;
  } inst;
} xkblas_io_instruction_t;

typedef enum xkblas_io_stream_type {
    XKBLAS_IO_STREAM_H2D  = 0, /* from CPU to GPU */
    XKBLAS_IO_STREAM_D2H  = 1, /* from GPU to CPU */
    XKBLAS_IO_STREAM_D2D  = 2, /* from GPU to GPU */
    XKBLAS_IO_STREAM_KERN = 3,
    XKBLAS_IO_STREAM_ALL       /* internal purpose */
} xkblas_io_stream_type_t;

typedef struct  xkblas_io_stream_t
{
    xkblas_io_stream_type_t       type;
    int                          sid;       /* with respect to all io_stream in the device offload_stream */
    xkblas_mutex_t                 mutex;     /*  lock */
    uint64_t                     count;     /* the size of array instr and pending */
    uint64_t                     smax;      /* maximal occupency of the stream */
    uint64_t                     smax_p;    /* maximal occupency of pending requests in the stream */
    uint64_t                     max_p;     /* ok_p..max_p should have been directly notified */
    uint64_t                     pos_r;       /* first instruction to process */
    uint64_t                     pos_w;       /* next position for writing instructions */
    volatile uint64_t            pos_rp;    /* first pending instruction into the bloc */
    volatile uint64_t            pos_wp;    /* next position for writing into the pending bloc */
    xkblas_io_instruction_t*      instr;       /* first instruction */
    xkblas_io_instruction_t*      pending;   /* pending instructions, not yet completed */
    struct xkblas_offload_stream* stream;
    volatile uint64_t            ok_p __attribute__((aligned(CACHE_LINE_SIZE)));
    /* past the last position of pending notified instr in [pos_rp,pos_wp] */
}               xkblas_io_stream_t;

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

# pragma message(TODO "Define 'xkblas_device_t'")

/* Kaapi offload stream is virtual interface to be implemented by a device.
   Streams are decoupled from H2D/D2H/D2D/kernel executions.
   The number of stream per type is subject to change at start time
   by reading environement variables. See xkblas_usage.

   ios[0] is the pointer of all the iostream_t*.
   ios[1] point to the first output stream, and ios[2] to the first kernel thread.
*/
typedef struct xkblas_offload_stream {
//  struct xkblas_device_t*   device;
  int                    count[XKBLAS_IO_STREAM_ALL];    /* number of iostream per type */
  std::atomic<int>         next[XKBLAS_IO_STREAM_ALL];     /* next  stream fifo */
  xkblas_io_stream_t**    ios[XKBLAS_IO_STREAM_ALL];      /* basic stream */

  /* virtualisation */
  struct xkblas_io_stream* (*f_stream_alloc)(
      struct xkblas_device*,
      int,
      unsigned int
  );
  void (*f_stream_free)(
      struct xkblas_device*,
      struct xkblas_io_stream*
  );
  int (*f_stream_process_pending )(
      struct xkblas_device*,
      struct xkblas_io_stream*,
      int
  );
  int (*f_stream_decode_ioinstruction)(
      struct xkblas_device*,
      struct xkblas_io_stream*,
      struct xkblas_io_instruction*
  );
} xkblas_offload_stream_t;


/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  xkblas_device_t
{
    xkblas_device_memory_t       memdev;            /* casted to xkblas_device */
    xkblas_offload_stream_t      stream;            /* communication streams host<->device */

    xkblas_localitydomain_t*     ld;                /* the device locality domain */

    Thread  * thread;                               /* running thread */
    std::atomic<int>             cnt_push;          /* number of times the ressource is pushed */
    unsigned int                device_id;          /* Interval device id */
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
    int (*f_device_init)(xkblas_device_t*);
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
  xkblas_driver_t* driver;
  int driver_device_id;
  int global_device_id;
  xkblas_localitydomain_t* ld;
  pthread_t tid;
}               xkblas_driver_thread_arg_t;

#endif /* __DRIVER_H__ */
