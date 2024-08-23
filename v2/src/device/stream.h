#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include "device/stream-instruction.h"

typedef enum    xkblas_stream_type_t
{
    XKBLAS_STREAM_H2D  = 0, /* from CPU to GPU */
    XKBLAS_STREAM_D2H  = 1, /* from GPU to CPU */
    XKBLAS_STREAM_D2D  = 2, /* from GPU to GPU */
    XKBLAS_STREAM_KERN = 3,
    XKBLAS_STREAM_ALL       /* internal purpose */

}               xkblas_stream_type_t;

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual")
typedef struct  xkblas_stream_t
{
    /* the io stream type */
    xkblas_stream_type_t type;

    # if 0
    xkblas_stream_type_t       type;
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
    xkblas_stream_instruction_t*      instr;       /* first instruction */
    xkblas_stream_instruction_t*      pending;   /* pending instructions, not yet completed */
    struct xkblas_offload_stream* stream;
    volatile uint64_t            ok_p __attribute__((aligned(CACHE_LINE_SIZE)));
    /* past the last position of pending notified instr in [pos_rp,pos_wp] */
    # endif
}               xkblas_stream_t;

#endif /* __STREAM_HPP__ */
