#ifndef __DRIVER_H__
# define __DRIVER_H__

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>     /* cpu_set_t */
# include <stdint.h>    /* uint64_t */

/** Load drivers */
void xkblas_drivers_init(void);

typedef struct  xkblas_io_status_t
{
  int error;
  float cpu_delay;  /* time on CPU between launch and completion (s)*/
  float gpu_delay;  /* time of CPU between launch and completion (s)*/
  uint64_t bytes;     /* bytes transfered in case of memory copy */
}               xkblas_io_status_t;

typedef void (*xkblas_io_callback_func_t)(
    xkblas_io_status_t,
    struct xkblas_io_stream *,
    void *, void *, void *
);

typedef struct  xkblas_io_callback_t
{
    xkblas_io_callback_func_t func;
    void * args[3];
}               xkblas_io_callback_t;

# if 0

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


typedef struct xkblas_io_stream
{
  xkblas_io_stream_type_t       type;
  int                          sid;       /* with respect to all io_stream in the device offload_stream */
  xkblas_lock_t                 mutex;     /*  lock */
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
  volatile uint64_t            ok_p __attribute__((aligned(XKBLAS_CACHE_LINE)));
                                          /* past the last position of pending notified instr in [pos_rp,pos_wp] */
} xkblas_io_stream_t;

# endif

# include "logger/todo.h"
# pragma message(TODO "Define 'xkblas_device_t'")

typedef struct  xkblas_device_t
{
}               xkblas_device_t;

typedef struct  xkblas_driver_t
{
    char const * name;    /* name of the driver (.so) */
    unsigned int ndevices;      /* number of devices managed by this driver */
    void * handle;              /* driver handle */

    /* Function handlers: accessor to meta data */
    const char   *(*f_get_name)(void);      /* name of the driver (human-readable) */
    unsigned int (*f_get_flags)(void);      /* flags: not really used */
    unsigned int (*f_get_type)(void);       /* type of ressource CPU,GPU etc */
    unsigned int (*f_get_number)(void);     /* number of devices managed by the driver */
    unsigned int (*f_get_ndevices)(void);   /* return the number of devices available to the driver */

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
    xkblas_device_t* (*f_device_create)(struct xkblas_driver*, int);
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
#endif /* __DRIVER_H__ */
