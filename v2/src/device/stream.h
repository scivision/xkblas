#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include "device/stream-instruction.h"
# include "sync/lockable.hpp"

/* DONT CHANGE ORDER HERE !! Can have side effects (in the Offloader class for instance) */
typedef enum    xkblas_stream_type_t
{
    XKBLAS_STREAM_TYPE_H2D  = 0, /* from CPU to GPU */
    XKBLAS_STREAM_TYPE_D2H  = 1, /* from GPU to CPU */
    XKBLAS_STREAM_TYPE_D2D  = 2, /* from GPU to GPU */
    XKBLAS_STREAM_TYPE_KERN = 3,
    XKBLAS_STREAM_TYPE_ALL       /* internal purpose */

}               xkblas_stream_type_t;

const char * xkblas_stream_type_to_str(xkblas_stream_type_t type);

/* counter for the stream queues */
typedef int xkblas_stream_instruction_counter_t;

class xkblas_stream_instruction_queue_t
{
    public:

        xkblas_stream_instruction_t * instr;            /* instructions buffer */
        xkblas_stream_instruction_counter_t capacity;   /* buffer capacity */
        struct {
            xkblas_stream_instruction_counter_t r;      /* first instruction to process */
            xkblas_stream_instruction_counter_t  w;     /* next position for inserting instructions */
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

        xkblas_stream_instruction_counter_t
        size(void) const
        {
            return (this->pos.w - this->pos.r);
        }
};

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")

/* this is a 'kaapi_io_stream' equivalent */
class xkblas_stream_t : public Lockable
{
    public:
        xkblas_stream_type_t type;

        /* launch a stream instruction */
        int (*f_instruction_launch)(xkblas_stream_t * stream, xkblas_stream_instruction_t * instr);

        /* progress a stream instruction */
        int (*f_instructions_progress)(xkblas_stream_t * stream, int blocking);

        /* queue for ready instruction */
        xkblas_stream_instruction_queue_t ready;

        /* queue for pending instructions to progress */
        xkblas_stream_instruction_queue_t pending;

        /* the first event in the pending queue before which all events are completed */
        volatile xkblas_stream_instruction_counter_t ok_p __attribute__((aligned(CACHE_LINE_SIZE)));

    public:

        /* allocate a new instruction to the stream (must then be commited via 'commit') */
        xkblas_stream_instruction_t * instruction_new(
            const xkblas_stream_instruction_type_t itype,
            const xkblas_callback_t & callback
        );

        /* commit the instruction to the stream (must be allocated via 'instruction_new') */
        int commit(xkblas_stream_instruction_t * instruction);

        /* launch instructions, and may generate pending instructions */
        int launch_ready_instructions(void);

        /* progress pending instructions */
        int progress_pending_instructions(int blocking);

        /* return true if the stream is full of instructions, false otherwise */
        int is_full(void) const;

        /* return true if the stream is empty, false otherwise */
        int is_empty(void) const;


};  /* xkblas_stream_t */

void xkblas_stream_init(
    xkblas_stream_t * stream,
    xkblas_stream_type_t type,
    xkblas_stream_instruction_counter_t capacity,
    int (*f_instruction_launch)(xkblas_stream_t *, xkblas_stream_instruction_t *),
    int (*f_instructions_progress)(xkblas_stream_t * stream, int blocking)
);

void xkblas_stream_deinit(xkblas_stream_t * stream);

#endif /* __STREAM_HPP__ */
