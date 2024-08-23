// TODO : clear this file formatting

#ifndef __IO_H__
# define __IO_H__

# include "device/memory.h"
# include "logger/todo.h"
# include "device/task.hpp"
# include "sync/cache-line-size.hpp"
# include "sync/mutex.h"

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

# pragma message(TODO "Bad dependence on 'xkblas_memory_view_t' here")
struct xkblas_memory_view_t;

/* io instruction to write/read data from the corresponding device
   src == host emitting the request
   */
typedef struct xkblas_io_copy_t {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    xkblas_io_copy_priority_t     prio;
    const void*                  src;
    const xkblas_memory_view_t*   view_src;
    struct xkblas_device_memory_t *  dev_src;
    void*                        dest;
    const xkblas_memory_view_t*   view_dest;
    struct xkblas_device_memory_t *  dev_dest;
} xkblas_io_copy_t;

/* marker begin...end for group of request
*/
typedef struct xkblas_io_begin_t {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    struct xkblas_io_instruction* first;
} xkblas_io_begin_t;

typedef struct xkblas_io_end_t {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    struct xkblas_io_instruction* last;
} xkblas_io_end_t;


/* marker call back, acts as a full memory barrier : any write, read or kernel instructon
   before the sync are never re-ordered after the sync.
   */
typedef struct xkblas_io_barrier_t {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
} xkblas_io_barrier_t;

/* io instruction kernel : to launch kernel on the device
   The delay field of the status arguments of the callback, if defined, is the delay in millisecond
   to execute the kernel.
   */
typedef struct xkblas_io_kernel_t {
    xkblas_io_callback_func_t fnc;
    void * arg[3];
    Task * task;
} xkblas_io_kernel_t;

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
typedef struct  xkblas_io_instruction_t
{
    xkblas_io_type_t type;
    union {
        xkblas_io_callback_t callback;   /* callback info always first fields of structure. Romain: why ?*/
        xkblas_io_begin_t   f_io;
        xkblas_io_end_t     l_io;
        xkblas_io_copy_t    c_io;
        xkblas_io_kernel_t  k_io;
        xkblas_io_barrier_t b_io;
    } inst;
}               xkblas_io_instruction_t;

typedef enum xkblas_io_stream_type {
    XKBLAS_IO_STREAM_H2D  = 0, /* from CPU to GPU */
    XKBLAS_IO_STREAM_D2H  = 1, /* from GPU to CPU */
    XKBLAS_IO_STREAM_D2D  = 2, /* from GPU to GPU */
    XKBLAS_IO_STREAM_KERN = 3,
    XKBLAS_IO_STREAM_ALL       /* internal purpose */
} xkblas_io_stream_type_t;

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual")
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



#endif /* __IO_H__ */
