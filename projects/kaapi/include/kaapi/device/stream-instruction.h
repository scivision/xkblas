/* ************************************************************************** */
/*                                                                            */
/*   stream-instruction.h                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:54:09 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STREAM_INSTRUCTION_H__
# define __STREAM_INSTRUCTION_H__

# include <kaapi/callback.h>
# include <kaapi/device/task.hpp>
# include <kaapi/logger/todo.h>
# include <kaapi/sync/cache-line-size.hpp>
# include <kaapi/sync/mutex.h>

# include <functional>

typedef struct  kaapi_io_status_t
{
    int error;          /* error code */
    float cpu_delay;    /* time on CPU between launch and completion (s)*/
    float gpu_delay;    /* time of CPU between launch and completion (s)*/
    uint64_t bytes;     /* bytes transfered in case of memory copy */
}               kaapi_io_status_t;

typedef enum    kaapi_stream_instruction_type_t
{
    KAAPI_STREAM_INSTR_TYPE_COPY_H2H = 0,
    KAAPI_STREAM_INSTR_TYPE_COPY_H2D = 1,
    KAAPI_STREAM_INSTR_TYPE_COPY_D2H = 2,
    KAAPI_STREAM_INSTR_TYPE_COPY_D2D = 3,
    KAAPI_STREAM_INSTR_TYPE_BARRIER  = 4,
    KAAPI_STREAM_INSTR_TYPE_KERN     = 5,
    KAAPI_STREAM_INSTR_TYPE_MAX      = 6,
}               kaapi_stream_instruction_type_t;

const char * kaapi_stream_instruction_type_to_str(kaapi_stream_instruction_type_t type);

# pragma message(TODO "Bad dependence on 'kaapi_memory_view_t' here")
struct kaapi_memory_view_t;

/* io instruction to move data between devices */
typedef struct  kaapi_stream_instruction_copy_t
{
    memory_view_t host_view;
    memory_replicate_view_t dst_device_view;
    memory_replicate_view_t src_device_view;

}               kaapi_stream_instruction_copy_t;

/* marker call back, acts as a full memory barrier : any write, read or kernel instructon
   before the sync are never re-ordered after the sync.
   */
typedef struct kaapi_stream_instruction_barrier_t
{

}               kaapi_stream_instruction_barrier_t;

/* io instruction kernel : to launch kernel on the device */
typedef struct  kaapi_stream_instruction_kernel_t
{
    Task * task;

}               kaapi_stream_instruction_kernel_t;

/* An PTR I/O instruction */
typedef struct  kaapi_stream_instruction_t
{
    kaapi_stream_instruction_type_t type;
    kaapi_callback_t callback;
    union
    {
        kaapi_stream_instruction_copy_t    copy;
        kaapi_stream_instruction_kernel_t  kern;
        kaapi_stream_instruction_barrier_t barrier;
    };

}               kaapi_stream_instruction_t;

#endif /* __STREAM_INSTRUCTION_H__ */
