#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include "device/memory.h"
# include "device/io.h"
# include "logger/todo.h"
# include "device/task.hpp"
# include "sync/cache-line-size.h"
# include "sync/mutex.h"

# include <cuda.h>
# include <cuda_runtime.h>

# pragma message(TODO "Abstract this class, currently implemented for Cuda only")


/* Kaapi offload stream is virtual interface to be implemented by a device.
   Streams are decoupled from H2D/D2H/D2D/kernel executions.
   The number of stream per type is subject to change at start time
   by reading environement variables. See xkblas_usage.

   ios[0] is the pointer of all the iostream_t*.
   ios[1] point to the first output stream, and ios[2] to the first kernel thread.
*/

class Stream
{
    public:
        Stream();
        virtual ~Stream();

        # pragma message(TODO "Use C++ abstract method and inheritance instead of 'C-style' abstract class")
        xkblas_io_stream_t* (*f_stream_alloc)(int device_id,  int type, unsigned int capacity);
        void (*f_stream_free)(int device_id, xkblas_io_stream_t * io_stream);

        bool is_empty(xkblas_io_stream_type_t type) const;

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

        int count[XKBLAS_IO_STREAM_ALL];                    /* number of iostream per type */
        std::atomic<int> next[XKBLAS_IO_STREAM_ALL];        /* next  stream fifo */
        xkblas_io_stream_t ** ios[XKBLAS_IO_STREAM_ALL];    /* basic stream */

    private:
        cudaStream_t cuStream;

};

#endif /* __STREAM_HPP__ */
