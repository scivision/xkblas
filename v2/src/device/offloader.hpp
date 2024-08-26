#ifndef __OFFLOADER_HPP__
# define __OFFLOADER_HPP__

# include "conf/conf.h"
# include "device/stream.h"
# include "logger/todo.h"
# include "sync/mutex.h"

# define XKBLAS_STREAM_
# define XKBLAS_STREAM_IO_INSTR_CAPACITY 512

/* Kaapi offload stream is virtual interface to be implemented by a device.
   Offloaders are decoupled from H2D/D2H/D2D/kernel executions.
   The number of stream per type is subject to change at start time
   by reading environement variables. See xkblas_usage.
*/
class Offloader
{
    public:
        Offloader();
        virtual ~Offloader();

    public:

        /* initialise the streams */
        void init(
            xkblas_conf_offloader_t * conf,
            xkblas_stream_t * (*f_stream_create)(xkblas_stream_type_t type, unsigned int capacity)
        );

        # pragma message(TODO "Use C++ abstract method and inheritance instead of 'C-style' abstract class")
        xkblas_stream_t* (*f_stream_alloc)(int device_id,  int type, unsigned int capacity);
        void (*f_stream_free)(int device_id, xkblas_stream_t * io_stream);

    public:
        int submit(xkblas_stream_instruction_t * instr);

        /* create a new instruction on the given stream type, and return the instruction and the assigned stream for execution */
        void instruction_new(
            xkblas_stream_type_t stype,             /* IN  */
            xkblas_stream_t ** stream,              /* OUT */
            xkblas_stream_instruction_type_t itype, /* IN  */
            xkblas_stream_instruction_t ** instr    /* OUT */
        );

        /* TODO */
        bool is_empty(xkblas_stream_type_t type) const;
        int process_instruction(xkblas_stream_type_t type);
        int test(xkblas_stream_type_t type);
        int wait(xkblas_stream_type_t type);

        # if 0
        int (*f_stream_process_pending)(
                struct xkblas_device*,
                struct xkblas_io_stream*,
                int
                );
        int (*f_stream_decode_ioinstruction)(
                struct xkblas_device*,
                struct xkblas_io_stream*,
                struct xkblas_io_instruction*
                );
        # endif

    public:

        /* number of iostream per type */
        int count[XKBLAS_STREAM_TYPE_ALL];

        /* next stream fifo */
        std::atomic<int> next[XKBLAS_STREAM_TYPE_ALL];

        /* basic stream */
        xkblas_stream_t ** streams[XKBLAS_STREAM_TYPE_ALL];

    private:

        /* get next stream for the given type */
        xkblas_stream_t * stream_next(xkblas_stream_type_t type);


};

#endif /* __OFFLOADER_HPP__ */
