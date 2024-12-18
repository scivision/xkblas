/* ************************************************************************** */
/*                                                                            */
/*   stream.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include "device/stream-instruction.h"
# include "sync/lockable.hpp"

/* DONT CHANGE ORDER HERE !! Can have side effects (in the Offloader class for instance) */
typedef enum    ptr_stream_type_t
{
    PTR_STREAM_TYPE_H2D  = 0, /* from CPU to GPU */
    PTR_STREAM_TYPE_D2H  = 1, /* from GPU to CPU */
    PTR_STREAM_TYPE_D2D  = 2, /* from GPU to GPU */
    PTR_STREAM_TYPE_KERN = 3,
    PTR_STREAM_TYPE_ALL       /* internal purpose */

}               ptr_stream_type_t;

const char * ptr_stream_type_to_str(ptr_stream_type_t type);

/* counter for the stream queues */
typedef uint32_t ptr_stream_instruction_counter_t;

class ptr_stream_instruction_queue_t
{
    public:

        ptr_stream_instruction_t * instr;                /* instructions buffer */
        ptr_stream_instruction_counter_t capacity;       /* buffer capacity */
        struct {
            volatile ptr_stream_instruction_counter_t r; /* first instruction to process */
            volatile ptr_stream_instruction_counter_t w; /* next position for inserting instructions */
        } pos;

    public:

        /* methods */
        int
        is_full(void) const
        {
            return (this->pos.w - this->pos.r >= this->capacity);
        }

        int
        is_empty(void) const
        {
            return (this->pos.r == this->pos.w);
        }

        ptr_stream_instruction_counter_t
        size(void) const
        {
            return (this->pos.w - this->pos.r);
        }
};

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")

/* this is a 'kaapi_io_stream' equivalent */
class ptr_stream_t : public Lockable
{
    public:
        ptr_stream_type_t type;

        /* launch a stream instruction */
        int (*f_instruction_launch)(ptr_stream_t * stream, ptr_stream_instruction_t * instr);

        /* progress a stream instruction */
        int (*f_instructions_progress)(ptr_stream_t * stream, int blocking);

        /* queue for ready instruction */
        ptr_stream_instruction_queue_t ready;

        /* queue for pending instructions to progress */
        ptr_stream_instruction_queue_t pending;

        /* the first event in the pending queue before which all events are completed */
        volatile ptr_stream_instruction_counter_t ok_p __attribute__((aligned(CACHE_LINE_SIZE)));

    public:

        /* allocate a new instruction to the stream (must then be commited via 'commit') */
        ptr_stream_instruction_t * instruction_new(
            const ptr_stream_instruction_type_t itype,
            const ptr_callback_t & callback
        );

        /* complete the instruction at the i-th position in the pending queue (invoke the callback) */
        void complete(const ptr_stream_instruction_counter_t i);

        /* commit the instruction to the stream (must be allocated via 'instruction_new') */
        int commit(ptr_stream_instruction_t * instruction);

        /* launch instructions, and may generate pending instructions */
        int launch_ready_instructions(void);

        /* progress pending instructions */
        int progress_pending_instructions(int blocking);

        /* return true if the stream is full of instructions, false otherwise */
        int is_full(void) const;

        /* return true if the stream is empty, false otherwise */
        int is_empty(void) const;


};  /* ptr_stream_t */

void ptr_stream_init(
    ptr_stream_t * stream,
    ptr_stream_type_t type,
    ptr_stream_instruction_counter_t capacity,
    int (*f_instruction_launch)(ptr_stream_t *, ptr_stream_instruction_t *),
    int (*f_instructions_progress)(ptr_stream_t * stream, int blocking)
);

void ptr_stream_deinit(ptr_stream_t * stream);

#endif /* __STREAM_HPP__ */
