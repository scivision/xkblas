#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# pragma message(TODO "Abstract this class, currently implemented for Cuda only")
# pragma message(TODO "Split this file independen concepts")

# include "logger/todo.h"
# include "scheduler/task.hpp"
# include "sync/cache-line-size.h"
# include "sync/mutex.h"

# include <cuda.h>
# include <cuda_runtime.h>


typedef struct  xkblas_io_status_t
{
  int error;
  float cpu_delay;  /* time on CPU between launch and completion (s)*/
  float gpu_delay;  /* time of CPU between launch and completion (s)*/
  uint64_t bytes;     /* bytes transfered in case of memory copy */
}               xkblas_io_status_t;

typedef void (*xkblas_io_callback_func_t)(
    xkblas_io_status_t,
    struct xkblas_io_stream_t *,
    void *, void *, void *
);

typedef struct  xkblas_io_callback_t
{
    xkblas_io_callback_func_t func;
    void * args[3];
}               xkblas_io_callback_t;

///////////////////////////
// Driver devices memory //
///////////////////////////
typedef uint64_t xkblas_address_space_id_t;

# pragma message(TODO "What is a 'xkblas_alloc_data' struct ?")
struct xkblas_alloc_data;
typedef struct xkblas_alloc_data xkblas_alloc_data_t;

# pragma message(TODO "What is a 'xkblas_alloc_chunk' struct ?")
struct xkblas_alloc_chunk;
typedef struct xkblas_alloc_chunk xkblas_alloc_chunk_t;

/** Type of pointer for all address spaces.
    The pointer encode both the pointer (field ptr) and the location of the address space
    in asid. Pointer arithmetic is allowed on this type on the ptr field.
    If pointer is on device with disjoint adress space, meta is a host data to help storing
    meta data.
*/
typedef struct  xkblas_pointer_t
{
    xkblas_address_space_id_t asid;
    uintptr_t                ptr;
    uintptr_t                meta;
}               xkblas_pointer_t;


/** Type of allowed memory view for the memory interface:
    - 1D array (base, size)
      simple contiguous 1D array
    - 2D array (base, size[2], lda)
      assume a row major storage of the memory : the 2D array has
      size[0] rows of size[1] rowwidth. lda is used to pass from
      one row to the next one. 
    The base (xkblas_pointer_t) is not part of the view description
*/
#define XKBLAS_MEMORY_VIEW_1D 1 
#define XKBLAS_MEMORY_VIEW_2D 2  /* assume row major */
#define XKBLAS_MEMORY_VIEW_3D 3
#define XKBLAS_MEMORY_STORAGE_ROWMAJOR 1
#define XKBLAS_MEMORY_STORAGE_COLMAJOR 2
typedef struct xkblas_memory_view_t {
  uintptr_t     offset;
  size_t        size[3];
  size_t        ld;
  size_t        wordsize;
  uint8_t       type; 
  uint8_t       storage;
} xkblas_memory_view_t;


# define XKBLAS_MEMORY_VALUE_TYPE uint16_t

# define XKBLAS_MEMORY_DEVICE_FLAG_NONE          0
# define XKBLAS_MEMORY_DEVICE_FLAG_MOSTLY_FULL   0x1
# define XKBLAS_MEMORY_DEVICE_FLAG_FULL          0x2

typedef struct  xkblas_device_memory_t
{
    xkblas_address_space_id_t asid;
    // xkblas_device_t * device;
    xkblas_mutex_t mem_lock;
    xkblas_alloc_data_t * freelist_bloc;
    xkblas_alloc_data_t * freelist_metabloc;
    xkblas_alloc_chunk_t * free_chunk_list;
    xkblas_alloc_chunk_t * main_chunk;

    /* Virtualization of alloc/free on the offload memory device */
    uintptr_t (*f_alloc)(int device_id,  size_t size, int * flag);
    void  (*f_free)(int device_id, uintptr_t ptr, size_t size);

    /* returns:
       0: success
       EINPROGRESS : pending operations on the device
       else error
    */
    int   (*f_copy)(struct xkblas_device_memory_t*,
                    xkblas_pointer_t /* dest*/,
                    const xkblas_memory_view_t* /*view_dest*/,
                    xkblas_pointer_t /*src*/,
                    const xkblas_memory_view_t* /*view_src*/,
                    int flags, /* 0, 1, 2 */
                    xkblas_io_callback_func_t cbk,
                    void* arg0, void* arg1, void* arg2
    );
    int  (*f_memsync)(struct xkblas_device_memory_t*, int begend);

    /* to help to manage cache */
    size_t (*f_get_mem_info)(struct xkblas_device_memory_t*, size_t*, size_t*);
    size_t (*f_get_free_mem)(struct xkblas_device_memory_t*);

    /* return source lid to reach lid_dest knowing valid_bit and xfer_bit for the data */
    uint16_t (*f_get_source)( struct xkblas_device_memory_t*, uint16_t, XKBLAS_MEMORY_VALUE_TYPE, XKBLAS_MEMORY_VALUE_TYPE );

}               xkblas_device_memory_t;

typedef enum    xkblas_io_type
{
    XKBLAS_IO_NOP      = 0,
    XKBLAS_IO_BEGIN    = 1,
    XKBLAS_IO_END      = 2,
    XKBLAS_IO_COPY_H2H = 3,
    XKBLAS_IO_COPY_H2D = 4,
    XKBLAS_IO_COPY_D2H = 5,
    XKBLAS_IO_COPY_D2D = 6,
    XKBLAS_IO_BARRIER  = 7,
    XKBLAS_IO_KERN     = 8
}               xkblas_io_type_t;

typedef enum xkblas_io_copy_priority {
  XKBLAS_IO_COPY_PRIORITY_LOW    = 0,
  XKBLAS_IO_COPY_PRIORITY_NORMAL = 1,
  XKBLAS_IO_COPY_PRIORITY_HIGH   = 2
} xkblas_io_copy_priority_t;


/* io instruction to write/read data from the corresponding device
   src == host emitting the request
   */
struct xkblas_io_copy {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    xkblas_io_copy_priority_t     prio;
    const void*                  src;
    const xkblas_memory_view_t*   view_src;
    struct xkblas_device_memory_t *  dev_src;
    void*                        dest;
    const xkblas_memory_view_t*   view_dest;
    struct xkblas_device_memory_t *  dev_dest;
};

/* marker begin...end for group of request
*/
struct xkblas_io_begin {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    struct xkblas_io_instruction* first;
};

struct xkblas_io_end {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
    struct xkblas_io_instruction* last;
};


/* marker call back, acts as a full memory barrier : any write, read or kernel instructon
   before the sync are never re-ordered after the sync.
   */
struct xkblas_io_barrier {
    xkblas_io_callback_func_t           fnc;
    void*                        arg[3];
};

/* io instruction kernel : to launch kernel on the device
  The delay field of the status arguments of the callback, if defined, is the delay in millisecond
  to execute the kernel.
*/
struct xkblas_io_kernel {
  xkblas_io_callback_func_t           fnc;
  void*                        arg[3];
  xkblas_task_body_t            body;
  Task *                task;
};



/* A Kaapi stream of IO requests
   - bounded io instructions
   - any read/write instructions may be reordered
   - group of instructions (between marker io_begin/io_end) cannot re-ordered outside the
   group
   - io_barrier acts as a full memory barrier
   - instructions may be aggregated
   pos_r, pos_w, pos_rp and pos_wp are non decreasing integer that correspond to entries %count
   in the table.

   If IO threads are activated, then one thread manage the IO while the device thread manages
   the kernel stream. In any case the device thread manages the progression of the whole computation
   and calls the callback at when events are posted.

*/
typedef struct xkblas_io_instruction
{
  xkblas_io_type_t          type;
  union {
    struct xkblas_io_callback_t callback;   /* callback info always first fields of structure */
    struct xkblas_io_begin      f_io;
    struct xkblas_io_end        l_io;
    struct xkblas_io_copy       c_io;
    struct xkblas_io_kernel     k_io;
    struct xkblas_io_barrier    b_io;
  } inst;
} xkblas_io_instruction_t;

typedef enum xkblas_io_stream_type {
    XKBLAS_IO_STREAM_H2D  = 0, /* from CPU to GPU */
    XKBLAS_IO_STREAM_D2H  = 1, /* from GPU to CPU */
    XKBLAS_IO_STREAM_D2D  = 2, /* from GPU to GPU */
    XKBLAS_IO_STREAM_KERN = 3,
    XKBLAS_IO_STREAM_ALL       /* internal purpose */
} xkblas_io_stream_type_t;

typedef struct  xkblas_io_stream_t
{
    xkblas_io_stream_type_t       type;
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
    xkblas_io_instruction_t*      instr;       /* first instruction */
    xkblas_io_instruction_t*      pending;   /* pending instructions, not yet completed */
    struct xkblas_offload_stream* stream;
    volatile uint64_t            ok_p __attribute__((aligned(CACHE_LINE_SIZE)));
    /* past the last position of pending notified instr in [pos_rp,pos_wp] */
}               xkblas_io_stream_t;


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
        Stream() {}
        ~Stream() {}

        # pragma message(TODO "Use C++ abstract method and inheritance instead of 'C-style' abstract class")

        xkblas_io_stream_t* (*f_stream_alloc)(int device_id,  int type, unsigned int capacity);
        void (*f_stream_free)(int device_id, xkblas_io_stream_t * io_stream);

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
        int count[XKBLAS_IO_STREAM_ALL];                    /* number of iostream per type */
        std::atomic<int> next[XKBLAS_IO_STREAM_ALL];        /* next  stream fifo */
        xkblas_io_stream_t ** ios[XKBLAS_IO_STREAM_ALL];    /* basic stream */


    private:
        cudaStream_t cuStream;

};

#endif /* __STREAM_HPP__ */
