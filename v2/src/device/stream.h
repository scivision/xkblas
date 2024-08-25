#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include "device/stream-instruction.h"

/* DONT CHANGE ORDER HERE !! Can have side effects (in the Offloader class for instance) */
typedef enum    xkblas_stream_type_t
{
    XKBLAS_STREAM_TYPE_H2D  = 0, /* from CPU to GPU */
    XKBLAS_STREAM_TYPE_D2H  = 1, /* from GPU to CPU */
    XKBLAS_STREAM_TYPE_D2D  = 2, /* from GPU to GPU */
    XKBLAS_STREAM_TYPE_KERN = 3,
    XKBLAS_STREAM_ALL       /* internal purpose */

}               xkblas_stream_type_t;

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")
class xkblas_stream_t
{
    public:
        xkblas_stream_type_t type;
        xkblas_mutex_t               mutex;     /*  lock */
        uint64_t                     smax;      /* maximal occupency of the stream */
        uint64_t                     smax_p;    /* maximal occupency of pending requests in the stream */
        uint64_t                     max_p;     /* ok_p..max_p should have been directly notified */
        uint64_t                     pos_r;       /* first instruction to process */
        uint64_t                     pos_w;       /* next position for writing instructions */
        volatile uint64_t            pos_rp;    /* first pending instruction into the bloc */
        volatile uint64_t            pos_wp;    /* next position for writing into the pending bloc */
        struct {
            uint64_t                       capacity;     /* the size of array instr and pending */
            xkblas_stream_instruction_t *  buffer;       /* first instruction */
            # if 0
            xkblas_stream_instruction_t *  pending;   /* pending instructions, not yet completed */
            # endif

        } instr;
        volatile uint64_t            ok_p __attribute__((aligned(CACHE_LINE_SIZE)));
        /* past the last position of pending notified instr in [pos_rp,pos_wp] */


    public:
        xkblas_stream_t() {}
        virtual ~xkblas_stream_t() {}

    public:
        int submit(xkblas_stream_instruction_t * instruction);

};  /* xkblas_stream_t */

void xkblas_stream_init(xkblas_stream_t * stream, xkblas_stream_type_t type, unsigned int capacity);
void xkblas_stream_deinit(xkblas_stream_t * stream);

#endif /* __STREAM_HPP__ */
