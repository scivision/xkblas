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

typedef struct  xkblas_stream_instruction_queue_t
{
    /* attributes */
    xkblas_stream_instruction_t * instr;    /* instructions buffer */
    uint64_t capacity;                      /* buffer capacity */
    struct {
        volatile uint64_t r;            /* first instruction to process */
        volatile uint64_t w;            /* next position for inserting instructions */
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

}               xkblas_stream_instruction_queue_t;

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")
class xkblas_stream_t
{
    public:
        xkblas_stream_type_t type;
        spinlock_t spinlock;

        xkblas_stream_instruction_queue_t ready;
        xkblas_stream_instruction_queue_t pending;

        /* past the last position of pending notified instr in [pos_rp,pos_wp] */
        volatile uint64_t ok_p __attribute__((aligned(CACHE_LINE_SIZE)));

    public:
        xkblas_stream_t() {}
        virtual ~xkblas_stream_t() {}

    public:

        /* allocate a new instruction to the stream (must then be commited via 'commit') */
        xkblas_stream_instruction_t * instruction_new(xkblas_stream_instruction_type_t itype);

        /* commit the instruction to the stream (must be allocated via 'instruction_new') */
        int commit(xkblas_stream_instruction_t * instruction);

        /* process instructions */
        int process_instructions(void);

        /* return true if the stream is full of instructions, false otherwise */
        int is_full(void) const;

        /* return true if the stream is empty, false otherwise */
        int is_empty(void) const;


};  /* xkblas_stream_t */

void xkblas_stream_init(xkblas_stream_t * stream, xkblas_stream_type_t type, unsigned int capacity);
void xkblas_stream_deinit(xkblas_stream_t * stream);

#endif /* __STREAM_HPP__ */
