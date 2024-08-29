#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include "device/stream-instruction.h"
# include "sync/spinlock.h"

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

typedef struct  xkblas_stream_instruction_queue_t
{
    /* attributes */
    xkblas_stream_instruction_t * instr;    /* instructions buffer */
    uint64_t capacity;                      /* buffer capacity */
    struct {
        volatile uint64_t r;                /* first instruction to process */
        volatile uint64_t w;                /* next position for inserting instructions */
    } pos;

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

    int
    size(void) const
    {
        return (this->pos.w - this->pos.r);
    }

}               xkblas_stream_instruction_queue_t;

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")
class xkblas_stream_t
{
    public:
        xkblas_stream_type_t type;
        spinlock_t spinlock;

        /* launch a stream instruction */
        int (*f_instruction_launch)(xkblas_stream_t * stream, xkblas_stream_instruction_t * instr);

        /* progress a stream instruction */
        int (*f_instructions_progress)(xkblas_stream_t * stream, int blocking);

        /* queue for ready instruction */
        xkblas_stream_instruction_queue_t ready;

        /* queue for pending instructions to progress */
        xkblas_stream_instruction_queue_t pending;

        /* Romain: the first event in the pending queue before which all events are completed */
        volatile uint64_t ok_p __attribute__((aligned(CACHE_LINE_SIZE)));

        # pragma message(TODO "What is the purpose of 'ok_p' ?")
        # if 0
        /* past the last position of pending notified instr in [pos_rp,pos_wp] */
        volatile uint64_t ok_p __attribute__((aligned(CACHE_LINE_SIZE)));
        # endif

    public:
        xkblas_stream_t() {}
        virtual ~xkblas_stream_t() {}

    public:

        /* allocate a new instruction to the stream (must then be commited via 'commit') */
        xkblas_stream_instruction_t * instruction_new(
            const xkblas_stream_instruction_type_t itype,
            const xkblas_stream_callback_t & callback
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
    unsigned int capacity,
    int (*f_instruction_launch)(xkblas_stream_t *, xkblas_stream_instruction_t *),
    int (*f_instructions_progress)(xkblas_stream_t * stream, int blocking)
);

void xkblas_stream_deinit(xkblas_stream_t * stream);

#endif /* __STREAM_HPP__ */
