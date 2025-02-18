/* ************************************************************************** */
/*                                                                            */
/*   stream-instruction.h                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/17 23:30:50 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STREAM_INSTRUCTION_H__
# define __STREAM_INSTRUCTION_H__

# include <xkrt/callback.h>
# include <xkrt/consts.h>
# include <xkrt/driver/memory-view.hpp>
# include <xkrt/driver/stream-instruction-type.h>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/sync/mutex.h>

# include <functional>

/* counter for the stream queues */
typedef uint32_t xkrt_stream_instruction_counter_t;

/* io instruction to move data between devices */
typedef struct  xkrt_stream_instruction_copy_t
{
    union {
        size_t m;
        size_t size;
    };
    size_t n;
    size_t sizeof_type;
    memory_replicate_view_t dst_device_view;
    memory_replicate_view_t src_device_view;

}               xkrt_stream_instruction_copy_t;

/* marker call back, acts as a full memory barrier : any write, read or kernel instructon
   before the sync are never re-ordered after the sync.
   */
typedef struct xkrt_stream_instruction_barrier_t
{

}               xkrt_stream_instruction_barrier_t;

/* io instruction kernel : to launch kernel on the device */
typedef struct  xkrt_stream_instruction_kernel_t
{
    void (*launch)(void * istream, void * instr, xkrt_stream_instruction_counter_t idx);
    void * vargs;
}               xkrt_stream_instruction_kernel_t;

/* An PTR I/O instruction */
typedef struct  xkrt_stream_instruction_t
{
    xkrt_stream_instruction_type_t type;
    xkrt_callback_t callback;
    union
    {
        xkrt_stream_instruction_copy_t      copy;
        xkrt_stream_instruction_kernel_t    kern;
        xkrt_stream_instruction_barrier_t   barrier;
    };

}               xkrt_stream_instruction_t;

#endif /* __STREAM_INSTRUCTION_H__ */
