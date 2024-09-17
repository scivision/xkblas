#ifndef __STREAM_INSTRUCTION_H__
# define __STREAM_INSTRUCTION_H__

# include "xkblas-callback.h"
# include "device/task.hpp"
# include "logger/todo.h"
# include "sync/cache-line-size.hpp"
# include "sync/mutex.h"

# include <functional>

typedef struct  xkblas_io_status_t
{
    int error;          /* error code */
    float cpu_delay;    /* time on CPU between launch and completion (s)*/
    float gpu_delay;    /* time of CPU between launch and completion (s)*/
    uint64_t bytes;     /* bytes transfered in case of memory copy */
}               xkblas_io_status_t;

typedef enum    xkblas_stream_instruction_type_t
{
    XKBLAS_STREAM_INSTR_TYPE_NOP      = 0,
    XKBLAS_STREAM_INSTR_TYPE_COPY_H2H = 1,
    XKBLAS_STREAM_INSTR_TYPE_COPY_H2D = 2,
    XKBLAS_STREAM_INSTR_TYPE_COPY_D2H = 3,
    XKBLAS_STREAM_INSTR_TYPE_COPY_D2D = 4,
    XKBLAS_STREAM_INSTR_TYPE_BARRIER  = 5,
    XKBLAS_STREAM_INSTR_TYPE_KERN     = 6
}               xkblas_stream_instruction_type_t;

const char * xkblas_stream_instruction_type_to_str(xkblas_stream_instruction_type_t type);

# pragma message(TODO "Bad dependence on 'xkblas_memory_view_t' here")
struct xkblas_memory_view_t;

/* io instruction to move data between devices */
typedef struct  xkblas_stream_instruction_copy_t
{
    memory_view_t host_view;
    memory_replicate_view_t dst_device_view;
    memory_replicate_view_t src_device_view;

}               xkblas_stream_instruction_copy_t;

/* marker call back, acts as a full memory barrier : any write, read or kernel instructon
   before the sync are never re-ordered after the sync.
   */
typedef struct xkblas_stream_instruction_barrier_t
{

}               xkblas_stream_instruction_barrier_t;

/* io instruction kernel : to launch kernel on the device */
typedef struct  xkblas_stream_instruction_kernel_t
{
    Task * task;

}               xkblas_stream_instruction_kernel_t;

/* An XKBLAS I/O instruction */
typedef struct  xkblas_stream_instruction_t
{
    xkblas_stream_instruction_type_t type;
    xkblas_callback_t callback;
    union
    {
        xkblas_stream_instruction_copy_t    copy;
        xkblas_stream_instruction_kernel_t  kern;
        xkblas_stream_instruction_barrier_t barrier;
    };

}               xkblas_stream_instruction_t;

#endif /* __STREAM_INSTRUCTION_H__ */
