# include "xkblas-context.h"
# include "conf/conf.h"
# include "device/device.h"
# include "device/driver.h"
# include "logger/logger.h"
# include "sync/mutex.h"

# define XKBLAS_DRIVER_ENTRYPOINT(N) XKBLAS_DRIVER_CUDA_ ## N

# include <cuda_runtime.h>
# include <cublas_v2.h>
# include <hwloc.h>
# include <hwloc/cuda.h>
# include <hwloc/cudart.h>
# include <hwloc/glibc-sched.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>

typedef struct  xkblas_stream_cuda_t
{
    xkblas_stream_t super;

    struct {
        cudaStream_t handle;

        struct {
            cudaEvent_t * end;
            uint32_t capacity;
        } events;

        struct {
            cublasHandle_t handle;
        } blas;

    } cu;
}               xkblas_stream_cuda_t;

typedef struct  xkblas_device_cuda_t
{
    xkblas_device_t inherited;
    int save_device_id;

    size_t mem_limit;
    size_t mem_total;
    size_t free_mem;
    size_t size_alloc;
    size_t size_free;


    /* device properties (from NVIDIA website) */
    struct
    {
        int pciBusID;
        int pciDeviceID;
        bool overlap;       /* if the device can concurrently copy memory between host and device while executing a kernel */
        bool integrated;    /* if the device is integrated with the memory subsystem */
        bool map;           /* if the device can map host memory into the CUDA address space */
        bool concurrent;    /* if the device supports executing multiple kernels within the same context simultaneously */
        int async_engines;  /* Number of asynchronous engines */
        char name[64];      /* GPU name */
    } prop;

}               xkblas_device_cuda_t;

/* number of used device for this run */
static xkblas_device_cuda_t DEVICES[XKBLAS_DEVICES_MAX];

static inline xkblas_device_t *
__get_device(int device_id)
{
    return (xkblas_device_t *) (DEVICES + device_id);
}

static inline xkblas_device_cuda_t *
__get_device_cuda(int device_id)
{
    return (xkblas_device_cuda_t *) __get_device(device_id);
}

/* Convert xkblas driver device id (in [0..ngpus-1]) to the cuda driver device id (in [0, INT_MAX[) */
static int CUDA_DEVICE_ID[XKBLAS_DEVICES_MAX];

static inline void
__set_device_cuda_id(int device_id, int cuda_device_id)
{
    CUDA_DEVICE_ID[device_id] = cuda_device_id;
    // XKBLAS_DEBUG("driver device id = %d ; cuda device id = %d", device_id, cuda_device_id);
}

static inline int
__get_device_cuda_id(int device_id)
{
    return CUDA_DEVICE_ID[device_id];
}

/* initialization synchronization */
static bool INITIALIZED = false;
static xkblas_mutex_t DRIVER_MUTEX = XKBLAS_MUTEX_INITIALIZER;

/* hwloc topology */
static hwloc_topology_t TOPOLOGY;

static int
__check_error(cudaError_t err)
{
    if (cudaSuccess != err)
    {
        const char * errstr = cudaGetErrorName(err);
        XKBLAS_ERROR("cuCheckError() error: %s (%i)", errstr, err);
        return 1;
    }
    return 0;
}

// NVLINK TOPOLOGY

/* cuda_perf_topo[device1,device2] returns the perfRank of the communication link between
   device.
   cuda_perf_device[d][i] for i=0,..,cuda_count_perfrank-1 is the mask of device
   for which the device d has link with performance i.
*/

static int cuda_device_count = 0;
static int* cuda_perf_topo = 0;
static int cuda_count_perfrank = 0;
static uint64_t* cuda_perf_device = 0;
static int* cuda_routing_table = 0;

/* buffer must be at least device_count */
static void _print_mask( char* buffer, ssize_t sz, uint64_t v )
{
  int device_count;
  for (int i=0; i<sz; ++i)
  {
     if ( v & (1ULL<<i)) buffer[sz-1-i]='1';
     else buffer[sz-1-i]='0';
  }
}

static void
_xkblas_get_gpu_topo(void)
{
    cudaError_t res;
    int min_perf= 0; /* min_perf >= max_perf */
    int max_perf= 0;
    int device_count;
    __check_error(cudaGetDeviceCount(&device_count));

    if (device_count ==0) return;
    cuda_device_count = device_count;
    cuda_perf_topo = (int*)malloc(sizeof(int)*device_count*device_count);

    // Enumerates Device <-> Device links and store perfRank
    for (int device1 = 0; device1 < device_count; device1++)
    {
        for (int device2 = 0; device2 < device_count; device2++)
        {
            if (device1 == device2)
                cuda_perf_topo[device1*device_count+device2] = 0;
            else
            {
                int perfRank = 0;
                int accessSupported = 0;

                __check_error(
                        cudaDeviceGetP2PAttribute(&accessSupported, cudaDevP2PAttrAccessSupported,
                            device1, device2));
                if (accessSupported)
                {
                    __check_error(
                            cudaDeviceGetP2PAttribute(&perfRank, cudaDevP2PAttrPerformanceRank,
                                device1, device2));
                    /* max perf if 0 with cudaDevP2PAttrPerformanceRank */
                    if (perfRank < max_perf)
                        max_perf= perfRank;
                    if (perfRank > min_perf)
                        min_perf= perfRank;
                    cuda_perf_topo[device1*device_count+device2] = 1+perfRank;
                }
                else
                    cuda_perf_topo[device1*device_count+device2] = -1; /* should be higher than previous value: computed after */
            }
        }
    }

    /* number of performance links: max_perf-min_perf+3
       - max_perf-min_perf+1 if GPUs peer access is enable
       - +1 if GPU peer access is not enable
       - +1 for local inter access
       */
    min_perf++;
    cuda_count_perfrank= min_perf-max_perf+2;
    int perfrank_nolink= min_perf-max_perf+1;
    int rank;
    for (int device = 0; device < device_count*device_count; device++)
        if (cuda_perf_topo[device] == -1) cuda_perf_topo[device] = min_perf+1;
    size_t size = device_count*cuda_count_perfrank*sizeof(uint64_t);
    cuda_perf_device = (uint64_t *) malloc( size );
    for (int i=0; i<device_count*cuda_count_perfrank; ++i)
        cuda_perf_device[i] = 0;
    /* GCC bug in warning about memset: memset(cuda_perf_device, 0, size ); */
    for (int device1 = 0; device1 < device_count; device1++)
    {
        for (int device2 = 0; device2 < device_count; device2++)
        {
            rank = cuda_perf_topo[device1*device_count+device2];
            assert( 0<= device1*device_count+ rank );
            assert( device1*cuda_count_perfrank+ rank <= device_count*cuda_count_perfrank);
            cuda_perf_device[device1*cuda_count_perfrank+ rank] |= (1UL<<device2);
        }
    }

#if XKBLAS_DEBUG
    xkblas_context_t * ctx = xkblas_context_get();
    if (ctx->conf.verbose)
    {
        char buffer[device_count+1];
        buffer[device_count] = 0;
        printf("Connection between GPU, #perf rank: %i.\nLowest values means best performance interconnection.\n", cuda_count_perfrank );
        printf("Local performance rank:0, Link perf. perf rank from %i (max perf) to %i (perf)\n", max_perf+1, min_perf+1);
        printf("%10s:", "src\\dest");
        for (int device1 = 0; device1 < device_count; device1++)
            printf("      GPU%i", device1);
        printf("\n");

        for (int device1 = 0; device1 < device_count; device1++)
        {
            printf("      GPU%i:", device1 );
            for (int device2 = 0; device2 < device_count; device2++)
                printf("%10i", cuda_perf_topo[device1*device_count+device2]);
            printf("\n");
        }

        /* mask */
        printf("\nDevice performance rank mask\n");
        if (1)
            for (int device1 = 0; device1 < device_count; device1++)
            {
                printf("GPU%i: ", device1);
                for (int rank = 0; rank < cuda_count_perfrank; ++rank)
                {
                    _print_mask( buffer, device_count, cuda_perf_device[device1*cuda_count_perfrank+ rank] );
                    printf("%s",buffer);
                    if (rank != cuda_count_perfrank-1) printf(", ");
                }
                printf("\n");
            }
    }
#endif
}

static
uint64_t cuda_get_free_mem(int device_id)
{
    cudaError_t res = cudaSetDevice(__get_device_cuda_id(device_id));
    __check_error(res);

    uint64_t free, total;
    res = cudaMemGetInfo(&free, &total);
    __check_error(res);

    xkblas_device_cuda_t * device = __get_device_cuda(device_id);
    device->free_mem = (size_t)free;
    return device->free_mem;
}

/*
static uintptr_t
cuda_alloc(int device_id, size_t size, int * flag)
{
    xkblas_device_cuda_t * device = __get_device_cuda(device_id);

    # pragma message(TODO "Cache system is always disabled, do we need this ?")
    // here we limit the size of allocated memory for the cache system
    if (((device->size_alloc - device->size_free) + size) > device->mem_limit)
    {
        if (flag)
            *flag = XKBLAS_MEMORY_DEVICE_FLAG_FULL;
        return 0;
    }

    assert(INITIALIZED == true);

    void * ptr;
    cudaError_t res;
    res = cudaMalloc( &ptr, size );
    __check_error(res);
    assert(ptr);

    XKBLAS_DEBUG("cuda alloc ptr=%p size=%ld (CUDA)\n", ptr, size);
    device->size_alloc += size;

    if (flag)
    {
        if ( 1.0*(device->size_alloc - device->size_free) / device->mem_limit >= 0.9)
            *flag = XKBLAS_MEMORY_DEVICE_FLAG_MOSTLY_FULL;
    }
    return (uintptr_t)ptr;
}
*/

/*
static void
cuda_free(int device_id, uintptr_t ptr, size_t size)
{
  xkblas_device_cuda_t * device = __get_device_cuda(device_id);
  cudaError_t res;

  assert(INITIALIZED == true);

  res = cudaFree((void*)ptr);
  __check_error(res);
  device->size_free += size;

#if XKBLAS_DEBUG
  fprintf(stdout, "cuda:%s: self:%p, tid:%i, free ptr=%p size=%ld\n", __FUNCTION__, pthread_self(), device->inherited.ctxt->tid, (void*)ptr, size);
#endif
}
*/

# pragma message(TODO "Finish cuda driver implementation")
#if 0


/* Implementation of xkblas_memory_copy_async through f_copy interface of memory device.
*/
static int cuda_copy(
    int device_id,
    xkblas_pointer_t dest, const xkblas_memory_view_t * view_dest,
    xkblas_pointer_t src,  const xkblas_memory_view_t * view_src,
    int flags,
    xkblas_stream_instruction_callback_t callback,
    void* arg0, void* arg1, void* arg2
)
{
    xkblas_device_cuda_t * device = __get_device_cuda(device_id);

    /* enforce method to only process H2D or D2D or D2H copy */
    assert( (xkblas_memory_asid_get_arch(dest.asid) != XKBLAS_PROC_TYPE_HOST)
            ||  (xkblas_memory_asid_get_arch(src.asid) != XKBLAS_PROC_TYPE_HOST) );
    assert( (dest.ptr != 0)
            &&  (src.ptr != 0) );

    xkblas_stream_instruction_type_t io_type = 0;
    xkblas_stream_type_t tstream = 0;
    xkblas_io_copy_priority_t priority = XKBLAS_STREAM_INSTR_TYPE_COPY_PRIORITY_NORMAL;

    if (flags == 0) /* low */
        priority = XKBLAS_STREAM_INSTR_TYPE_COPY_PRIORITY_LOW;
    else if (flags == 2)
        priority = XKBLAS_STREAM_INSTR_TYPE_COPY_PRIORITY_HIGH;
    /* else == normal */

    if ( (xkblas_memory_asid_get_arch(src.asid) == XKBLAS_PROC_TYPE_HOST)
            && (xkblas_memory_asid_get_arch(dest.asid) != XKBLAS_PROC_TYPE_HOST) )
    {
        io_type = XKBLAS_STREAM_INSTR_TYPE_COPY_H2D;
        tstream = XKBLAS_STREAM_TYPE_H2D;
        XKBLAS_CTXT_PERFREG_ADD(device->inherited.ctxt,XKBLAS_PERF_ID_CPYH2D_BYTES, xkblas_memory_view_size( view_dest ));
    }
    else if ( (xkblas_memory_asid_get_arch(src.asid) != XKBLAS_PROC_TYPE_HOST)
            && (xkblas_memory_asid_get_arch(dest.asid) == XKBLAS_PROC_TYPE_HOST) )
    {
        io_type = XKBLAS_STREAM_INSTR_TYPE_COPY_D2H;
        tstream = XKBLAS_STREAM_TYPE_D2H;
        XKBLAS_CTXT_PERFREG_ADD(device->inherited.ctxt,XKBLAS_PERF_ID_CPYD2H_BYTES, xkblas_memory_view_size( view_dest ));
    }
    else if ( (xkblas_memory_asid_get_arch(src.asid) != XKBLAS_PROC_TYPE_HOST)
            && (xkblas_memory_asid_get_arch(dest.asid) != XKBLAS_PROC_TYPE_HOST) )
    {
        io_type = XKBLAS_STREAM_INSTR_TYPE_COPY_D2D;
        tstream = XKBLAS_STREAM_TYPE_D2D;
        XKBLAS_CTXT_PERFREG_ADD(device->inherited.ctxt,XKBLAS_PERF_ID_CPYD2D_BYTES, xkblas_memory_view_size( view_dest ));
    }

    /* verify iff all inputs are in local node */
    assert( device->inherited.stream.device == &device->inherited );

    xkblas_stream_insert_io_copy_inst(
            &device->inherited.stream,
            tstream,
            io_type,
            priority,
            xkblas_pointer2void(src), view_src, xkblas_memory_device_get(src.asid),
            xkblas_pointer2void(dest), view_dest, xkblas_memory_device_get(dest.asid),
            callback, arg0, arg1, arg2
            );

    return EINPROGRESS;
}

/*
*/
static int cuda_memsync(xkblas_device_memory_t* dev, int begend)
{
#if _DRIVER_DEBUG || XKBLAS_USE_CUDA_RUNTIME_API
    xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)device->inherited.device;
#endif
    cudaError_t res;
    res = cudaSetDevice(__get_device_cuda_id(device->inherited.device_id]);
    __check_error(res);
    res = cudaDeviceSynchronize();
    __check_error(res);
    return 0;
}



/*
*/
static size_t cuda_get_mem_info(xkblas_device_memory_t* dev, size_t* mem_total, size_t* mem_limit)
{
    xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)device->inherited.device;
#if _DRIVER_DEBUG
    fprintf(stdout, "cuda:%s: device %d init\n", __FUNCTION__, device->inherited.device_id);
#endif
    if (mem_total) *mem_total = device->inherited.mem_total;
    if (mem_limit) *mem_limit = device->inherited.mem_limit;
#if _DRIVER_DEBUG
    fprintf(stdout, "cuda:%s: device %d init\n", __FUNCTION__, device->inherited.device_id, (mem_total ==0 ? -1 : *mem_total), (mem_limit ==0 ? -1 : *mem_limit));
#endif
    return device->inherited.mem_total;
}

static void _xkblas_cuda_create_event( xkblas_stream_cuda_t* stream, int k )
{
    cudaError_t res = cudaEventCreateWithFlags(&stream->end_events[k], cudaEventDisableTiming);
    __check_error(res);
}

static void _xkblas_cuda_destroy_event( xkblas_stream_cuda_t* stream, int k )
{
    cudaError_t res = cudaEventDestroy(stream->end_events[k]);
    __check_error(res);
}

/*
*/
static void xkblas_cuda_init_cuda_stream(
    xkblas_stream_cuda_t* stream,
    int type,
    unsigned int capacity
)
{
  int leastPriority, greatestPriority;

#if CONFIG_USE_EVENT
  /* */
  for (int k=0; k<capacity; ++k)
    _xkblas_cuda_create_event(stream, k);
#endif

  cudaError_t res;
  res = cudaDeviceGetOffloaderPriorityRange ( &leastPriority, &greatestPriority );
  __check_error(res);

  res = cudaStreamCreateWithPriority (&stream->stream, cudaStreamNonBlocking, greatestPriority);
  __check_error(res);
  /* used by prefetching operation */
  res = cudaStreamCreateWithPriority (&stream->stream_low, cudaStreamNonBlocking, leastPriority);
  __check_error(res);
#if XKBLAS_USE_PERSTREAM_BLASHANDLE
  stream->handle = 0;
#endif
  if (type == XKBLAS_STREAM_TYPE_KERN)
  {
    assert( thread_type == 0 );
#if XKBLAS_USE_PERSTREAM_BLASHANDLE
    /*
     */
    cublasStatus_t cres = cublasCreate(&stream->handle);
    assert(cres == CUBLAS_STATUS_SUCCESS);
    cres = cublasSetOffloader( stream->handle, stream->stream);
    assert(cres == CUBLAS_STATUS_SUCCESS);
#endif
  }
  else
  {
#if XKBLAS_HAVE_IO_THREADS
    assert( ((type == XKBLAS_STREAM_TYPE_H2D) && (thread_type == 1))
                     || ((type == XKBLAS_STREAM_TYPE_D2H) && (thread_type == 2)) );
#endif
  }
}


/*
*/
typedef enum host_register_request_op {
  DEVICE_REGISTER_REQUEST,
  DEVICE_UNREGISTER_REQUEST
} host_register_request_op_t;

/*
*/
typedef enum register_state {
  REQUEST_INIT,
  REQUEST_POST,
  REQUEST_WAIT,
  REQUEST_DONE
} request_state_t;

/* Request for asynchronous memory pinning operation.
   Store request for pinning memory region (ptr,size).
   Once the operation complets, call the callback
   callback(status, arg0, arg1, arg2), if callback is not null.
   The caller thread waiting on the request lock the request' mutex
   and wait on the request condition.
*/
typedef struct host_register_request {
  host_register_request_op_t op;
  request_state_t            state;
  int                        err;    /* error code if any */
  pthread_mutex_t            lock;
  pthread_cond_t             cond;
  void*                      ptr;
  size_t                     size;
  xkblas_stream_instruction_callback_t callback;
  void* arg0; void* arg1; void* arg2;
} host_register_request_t;


typedef struct host_register_queue {
  #define XKBLAS_MAX_REGLIST 256
  pthread_t         thread;
  volatile uint64_t posw;  /* index of the next request to store */
  volatile uint64_t posr;  /* index of the next request to read */
  pthread_mutex_t   lock;
  pthread_cond_t    cond;
  pthread_cond_t    cond_waitall;  /* broadcasted when request is processed */
  host_register_request_t req[XKBLAS_MAX_REGLIST];
} host_register_queue_t __attribute__ ((aligned (XKBLAS_CACHE_LINE_SIZE)));


/* array of lists of requests */
static int all_rrl_size = 0;
static host_register_queue_t* all_rrl = 0;

static void xkblas_cuda_init_reqreg_list( host_register_queue_t* rrl )
{
  rrl->posw = 0;
  rrl->posr = 0;
  assert(0 == pthread_mutex_init(&rrl->lock, 0));
  assert(0 == pthread_cond_init(&rrl->cond, 0));
  assert(0 == pthread_cond_init(&rrl->cond_waitall, 0));
  memset(rrl->req, 0, sizeof(rrl->req));
  for (int i=0; i<XKBLAS_MAX_REGLIST; ++i)
  {
    assert(0 == pthread_mutex_init(&rrl->req[i].lock, 0));
    assert(0 == pthread_cond_init(&rrl->req[i].cond, 0));
    rrl->req[i].state = REQUEST_INIT;
  }
};

/* daemon thread, one per queue
*/
void* xkblas_cuda_register_thread(void* dummy )
{
  xkblas_thread_t* kthread = xkblas_thread_bind(XKBLAS_PROC_TYPE_INTERNAL,0);
  assert( kthread != 0);
  xkblas_context_t* kctxt = xkblas_thread2context(kthread);

  int tid = (int)(uintptr_t)dummy;
  assert( tid < all_rrl_size );
  host_register_queue_t* rrl = &all_rrl[tid];

  assert( 0 == pthread_mutex_lock(&rrl->lock) );
  while (1)
  {
    /* wait for request until plugin is finished */
    while (INITIALIZED && (rrl->posw == rrl->posr))
      assert(0 == pthread_cond_wait(&rrl->cond, &rrl->lock));

    /* yeh, one request ? */
    if (rrl->posw > rrl->posr)
    {
      uint64_t index = rrl->posr % XKBLAS_MAX_REGLIST;

      /* recopy request */
      host_register_request_t req = rrl->req[index];
      req.err = 0;

      assert(0 == pthread_mutex_unlock(&rrl->lock));
      cudaError_t err = cudaSuccess;
      if (req.size >0)
      {
        if (req.op == DEVICE_REGISTER_REQUEST)
        {
          err = cudaHostRegister(req.ptr, req.size, cudaHostRegisterPortable);
          //printf("%p:: cudaHostRegister ptr: %p, size: %lu\n", pthread_self(), req.ptr, req.size);
          if (!( (cudaSuccess == err) || (cudaErrorHostMemoryAlreadyRegistered == err)))
          //CUresult err = cuMemHostRegister( ptr, size, CU_MEMHOSTREGISTER_PORTABLE );
          //if ((err != CUDA_SUCCESS) && (err != CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED))
          {
            printf("***[%s]: cudaHostRegister error: %i\n", __func__, err);
            req.err = EALREADY;
          }
        }
        else if (req.op == DEVICE_UNREGISTER_REQUEST)
        {
          err = cudaHostUnregister(req.ptr);
          if (!( (cudaSuccess == err) || (cudaErrorHostMemoryNotRegistered == err)))
          {
            printf("***[%s]: cudaHostUnregister error: %i\n", __func__, err);
            req.err = EALREADY;
          }
        }
      }

      if (req.callback !=0)
      {
        xkblas_io_status_t istream = {0, req.err };
        req.callback(istream, 0, req.arg0, req.arg1, req.arg2);
      }

      assert(0 == pthread_mutex_lock(&rrl->lock));

      /* request lock */
      assert(0 == pthread_mutex_lock(&rrl->req[index].lock));
      request_state_t state = rrl->req[index].state;
      rrl->req[index].err = req.err;
      rrl->req[index].state = REQUEST_DONE;
      if (state == REQUEST_WAIT)
        assert(0 == pthread_cond_signal( &rrl->req[index].cond ));
      assert(0 == pthread_mutex_unlock( &rrl->req[index].lock));
      assert( rrl->posr % XKBLAS_MAX_REGLIST == index );
      ++rrl->posr;
      /* always broadcast to waiters on all requests: may be optimized */
      assert(0 == pthread_cond_broadcast( &rrl->cond_waitall ));
    }
    /* if queue empty and plugin dinitialized then exit */
    else if (!INITIALIZED) break;
  }
  assert(0 == pthread_mutex_unlock(&rrl->lock));
  assert(0 == xkblas_thread_unbind(kthread));
  return 0;
}



/* The purpose of this function is to increase istream->ok_p
   but whithout calling callback
 */
static int cuda_stream_advance_pending(
    xkblas_device_t* dev,
    xkblas_stream_t* istream,
    int blocking
)
{

  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)dev;
  cudaError_t res;
  //cudaSetDevice(CUDA_DEVICE_ID[device->inherited.device_id]);

  xkblas_stream_cuda_t* stream = (xkblas_stream_cuda_t*)istream;
  if (xkblas_io_stream_emptypending(istream))
    return 0;

#if CONFIG_USE_EVENT
  if (blocking)
  {
    res = cudaStreamSynchronize( stream->stream );
    __check_error(res);
    res = cudaStreamSynchronize( stream->stream_low );
    __check_error(res);
    istream->ok_p = istream->pos_wp;
    return 0;
  }

  size_t len_p = xkblas_io_stream_sizepending(istream);

#if 0 // best
  int queue_max[4];
  queue_max[XKBLAS_STREAM_TYPE_H2D]  = ctx->conf.cuda_conc_kernel;
  queue_max[XKBLAS_STREAM_TYPE_KERN] = ctx->conf.cuda_conc_kernel;
  queue_max[XKBLAS_STREAM_TYPE_D2H]  = ctx->conf.cuda_conc_kernel;
  queue_max[XKBLAS_STREAM_TYPE_D2D]  = ctx->conf.cuda_conc_kernel;
  if ((len_p >1) && (len_p>= queue_max[istream->type]))
  {
    int shift = 0; //(istream->type == XKBLAS_STREAM_TYPE_KERN ? 0: len_p/2-1);
    int idx = (istream->ok_p + shift)% istream->count;
    res = hipEventSynchronize(stream->end_events[idx]);
  }
#endif//if 0

  /* istream->ok_p is past the last ok pending request: test from ok_p to pos_wp */
  int cnt;
  uint64_t istream_okp = istream->ok_p;
  int prev_istreamokp = istream_okp-1;
  while (istream_okp < istream->pos_wp)
  {
    int idx = istream_okp % istream->count;
    xkblas_stream_instruction_t* op = &istream->pending[idx];
    res = cudaSuccess;
    switch (op->type)
    {
      case XKBLAS_STREAM_INSTR_TYPE_KERN:
      case XKBLAS_STREAM_INSTR_TYPE_COPY_H2H:
      case XKBLAS_STREAM_INSTR_TYPE_COPY_H2D:
      case XKBLAS_STREAM_INSTR_TYPE_COPY_D2H:
      case XKBLAS_STREAM_INSTR_TYPE_COPY_D2D:
        for (int cnt=0; cnt<1; ++cnt)
        {
          res = cudaEventQuery( stream->end_events[idx] );
          assert((res == cudaErrorNotReady)  || (res == cudaSuccess));
          if (res == cudaErrorNotReady)
            //goto break_label;
            sched_yield();
          else {
#if XKBLAS_USE_TRACELIB==1
            float delay; /* ms */
            res = cudaEventElapsedTime ( &delay, stream->start_events[idx], stream->end_events[idx] );
            if (res != cudaSuccess) {
              printf("   invalid Cuda event state at: %d non fifo order ?\n", idx );
              delay = 0;
              assert(0);
            }
            if (op->type != XKBLAS_STREAM_INSTR_TYPE_KERN)
            {
              XKBLAS_EVENT_PUSH2( &xkblas_self_context()->kproc, XKBLAS_EVT_OFFLOAD_CPY,
                 2 /* end */, op->inst.c_io.reserved, (uint64_t)(1000000.0*delay));
//printf("Delay CPY: %lu\n", (uint64_t)(1000000.0*delay));
            }
            else
            {
              XKBLAS_EVENT_PUSH2( &xkblas_self_context()->kproc, XKBLAS_EVT_OFFLOAD_KERN,
                 2 /* end */, op->inst.k_io.reserved, (uint64_t)(1000000.0*delay));
//printf("Delay KERNEL: %lu\n", (uint64_t)(1000000.0*delay));
            }
#endif
            if (prev_istreamokp+1 == istream_okp) ++prev_istreamokp;
          }
        }

#if LOG_DBG
        printf("%s:: instruction pos:%i, instr '%s' ok\n", __func__, istream_okp,  name_io[op->type]);
#endif

      case XKBLAS_STREAM_INSTR_TYPE_END:
      case XKBLAS_STREAM_INSTR_TYPE_BARRIER:
      case XKBLAS_STREAM_INSTR_TYPE_NOP:
        ++istream_okp;
        break;

      default:
        fprintf(stderr, "%i:: bad instruction type at pos:%li\n", device->inherited.ld->idx, istream_okp);
        assert(0);
        break;
    }
  }
break_label:
  /* all events have been tested, test the prev_istreamokp has been incremented */
  istream_okp = istream->ok_p;
  if (prev_istreamokp != istream_okp-1)
  {
    istream->ok_p = prev_istreamokp+1;
    return 0;
  }
  return EINPROGRESS;

#elif CONFIG_SYNCHRONOUS_COPY && CONFIG_SYNCHRONOUS_KERNEL
  /* if synchronous call then advance automatically at the end of the operation */
  ;
#else
  /* if do not use event */
  if (blocking)
  {
    res = cudaStreamSynchronize( stream->stream );
    __check_error(res);
    res = cudaStreamSynchronize( stream->stream_low );
    __check_error(res);
    istream->ok_p = istream->pos_wp;
    return 0;
  }
  else
  {
    res = cudaStreamQuery( stream->stream );
    if (res == cudaErrorNotReady)
    {
      return EINPROGRESS;
    }
    __check_error(res);
    res = cudaStreamQuery( stream->stream_low );
    if (res == cudaErrorNotReady)
    {
      return EINPROGRESS;
    }
    __check_error(res);

    istream->ok_p = istream->pos_wp;
  }
#endif
}



/*
 */
static int cuda_stream_process_pending(
    xkblas_device_t* device,
    xkblas_stream_t* istream,
    int blocking
)
{
  xkblas_stream_cuda_t* stream = (xkblas_stream_cuda_t*)istream;

  do {
    cuda_stream_advance_pending(device, istream, blocking );


    xkblas_io_status_t status = {0,0};

    /* call callback functions */
    for (uint64_t pos = istream->pos_rp; pos<istream->ok_p; ++pos)
    {
      int idx = pos % istream->count;
      xkblas_stream_instruction_t* op = &istream->pending[idx];
      switch (op->type)
      {
        case XKBLAS_STREAM_INSTR_TYPE_KERN:
        case XKBLAS_STREAM_INSTR_TYPE_COPY_H2H:
        case XKBLAS_STREAM_INSTR_TYPE_COPY_H2D:
        case XKBLAS_STREAM_INSTR_TYPE_COPY_D2H:
        case XKBLAS_STREAM_INSTR_TYPE_COPY_D2D:
        case XKBLAS_STREAM_INSTR_TYPE_END:
        case XKBLAS_STREAM_INSTR_TYPE_BARRIER:
        {
          if ((op->type >= XKBLAS_STREAM_INSTR_TYPE_COPY_H2H) && (op->type <= XKBLAS_STREAM_INSTR_TYPE_COPY_D2D))
          {
            status.bytes = xkblas_memory_view_size(op->inst.c_io.view_src);
          }


          if (op->inst.callback.func)
            op->inst.callback.func(status, istream, op->inst.callback.arg[0], op->inst.callback.arg[1], op->inst.callback.arg[2]);

          op->type = XKBLAS_STREAM_INSTR_TYPE_NOP;
        }

        case XKBLAS_STREAM_INSTR_TYPE_NOP:
          /* commit index for the next event */
          ++istream->pos_rp;
          //return 0;
          break;

        default:
          fprintf(stderr, "%i:: bad instruction type at pos:%li\n", device->ld->idx, pos);
          assert(0);
          break;
      }
    }
  } while (0);
  return 0;
}


/* Return the source device to send data to device in the destination memory 'dev'.
 */
static uint16_t cuda_get_source(
    xkblas_device_memory_t* dev,
    uint16_t lid0,
    XKBLAS_MEMORY_VALUE_TYPE valid_bit,
    XKBLAS_MEMORY_VALUE_TYPE xfer_bit
)
{
  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)device->inherited.device;
  int device_id_dest = CUDA_DEVICE_ID[device->inherited.device->device_id];
  uint16_t lid_dest = xkblas_memory_asid_get_lid(device->inherited.asid);
  uint16_t lid_src;
  assert((valid_bit !=0) || (xfer_bit !=0));

  /* return the device with the higher affinity rank with lid_dest
     - does not consider the last performance rank
   */
#if XKBLAS_USE_TOPO_D2D
  for (int rank = 0; rank < cuda_count_perfrank-1; ++rank)
  {
    if (valid_bit !=0)
      lid_src = XKBLAS_MEMORY_FFS( valid_bit & device->affinity[rank] );
      //lid_src = _xkblas_get_random_bit1(valid_bit & device->affinity[rank], &device->inherited.ctxt->seed);
    else
      lid_src = XKBLAS_MEMORY_FFS( xfer_bit & device->affinity[rank]);
      //lid_src = _xkblas_get_random_bit1(xfer_bit & device->affinity[rank], &device->inherited.ctxt->seed);
    if (lid_src !=0)
    {
      --lid_src;
      assert(lid_src < XKBLAS_MEMORY_MAX_NODES);
      return lid_src;
    }
  }
#endif
  /* first return a random valid data, else an xfer data */
  if (valid_bit !=0)
  {
    uint16_t retval = _xkblas_get_random_bit1(valid_bit, &device->inherited.ctxt->seed)-1;
    //printf("RndBit(%i).1=%i\n", valid_bit, retval );
    return retval;
  }
  if (xfer_bit !=0)
  {
    uint16_t retval = _xkblas_get_random_bit1(xfer_bit, &device->inherited.ctxt->seed)-1;
    //printf("RndBiti(%i).2=%i\n", xfer_bit, retval );
    return retval;
  }
  return (uint16_t)-1;
}


#if XKBLAS_HAVE_IO_THREADS
static int xkblas_cuda_io_thread( xkblas_device_cuda_t* device, xkblas_stream_type_t type )
{
  cudaError_t res;
  res = cudaSetDevice(CUDA_DEVICE_ID[device->inherited.device_id]);
  assert( res == cudaSuccess );

  /* initialize all IO cuda stream within the current context */
  xkblas_offload_stream_t* const stream = &device->inherited.stream;
  for (unsigned int i=0; i< stream->count[type]; ++i)
    xkblas_cuda_init_cuda_stream( (xkblas_stream_cuda_t*)stream->istream[type][i], type, stream->istream[type][i]->count );

  /* infinite loop */
  while (!device->inherited.finalize)
  {
    int count = 0;
    xkblas_offload_stream_process_instruction( stream, type );
    for (unsigned int i=0; i< stream->count[type]; ++i)
    {
      uint64_t old_ok_p = stream->istream[type][i]->ok_p;
      cuda_stream_advance_pending( &device->inherited, stream->istream[type][i], 0 );
      if (old_ok_p != stream->istream[type][i]->ok_p) ++count;
    }
    if (count ==0) sched_yield();
  }
}


static void* xkblas_cuda_H2D_io_thread( void* arg )
{
  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)arg;
  /* for debug */
  thread_type = 1;

  xkblas_cuda_io_thread(device, XKBLAS_STREAM_TYPE_H2D);
  return 0;
}

static void* xkblas_cuda_D2H_io_thread( void* arg )
{
  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)arg;
  /* for debug */
  thread_type = 2;

  xkblas_cuda_io_thread(device, XKBLAS_STREAM_TYPE_D2H);
  return 0;
}
#endif


/* Update pthread_attr_t with the CPUset to start the thread that manages the device
   Return ENOTSUP is hwloc is not available or some internal error occurs.
*/
static int xkblas_set_cpuset(cpu_set_t* schedset, int device_id)
{
}



/*
*/
static const char *
XKBLAS_DRIVER_ENTRYPOINT(get_name)(void)
{
  return "cuda";
}


/*
*/
static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_flags)(void)
{
  return 0;
}


/*
*/
static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_type)(void)
{
  return XKBLAS_PROC_TYPE_CUDA;
}


/*
*/
static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_number)(void)
{
  assert(INITIALIZED == true);
  return DEVICES_USED;
}


/*
*/



/*
*/
static int
XKBLAS_DRIVER_ENTRYPOINT(init)(void)
{
  cudaError_t res;
  xkblas_cuda_plugin_lock();
  if(INITIALIZED == true){
      xkblas_cuda_plugin_unlock();
      return 0;
  }

  int device_count;
  res = cudaGetDeviceCount(&device_count);
  __check_error(res);

  xkblas_device_list = (xkblas_device_cuda_t**)calloc( device_count, sizeof(xkblas_device_cuda_t) );
  xkblas_context_t * ctx = xkblas_context_get();
  ctx->conf.sys_ngpus = device_count;

  if (ctx->conf.ngpus == (uint8_t)-1)
  {
    ctx->conf.ngpus = device_count;
    ctx->conf.gpu_set = (1<<device_count)-1;
  }

  /* bad number of GPUS ? */
  if (ctx->conf.ngpus > device_count)
  {
    printf("[%s] too many GPUs requested: %i. Use system default count: %i\n",__func__, ctx->conf.ngpus, device_count);
    ctx->conf.ngpus = device_count;
  }

  DEVICES_USED = ctx->conf.ngpus;
  CUDA_DEVICE_ID = (int*)malloc( ctx->conf.ngpus*sizeof(int) );
  int gpuset = ctx->conf.gpu_set;
  for (int i=0; i<ctx->conf.ngpus; ++i)
  {
    int idx = __builtin_ffs((unsigned int)gpuset);
    assert( idx != 0);
    --idx;
    gpuset &= ~(1<<idx);
    CUDA_DEVICE_ID[i] = idx;
    //fprintf(stdout,"[%s] take GPU id:%2i= device_id:%i\n", __FUNCTION__, i, idx);
  }

  DEVICES_USED = ctx->conf.ngpus;
  INITIALIZED = true;
  xkblas_cuda_plugin_unlock();
#if _DRIVER_DEBUG
  fprintf(stdout, "cuda:%s: cuda init with %d devices\n", __FUNCTION__, DEVICES_USED);
#endif

#if XKBLAS_CUDA_USE_NVLINK_TOPO
  _xkblas_get_gpu_topo();
#endif

  int nrrl = getenv("XKBLAS_RRL_SIZE") ? atoi(getenv("XKBLAS_RRL_SIZE")) : 1;
  if (nrrl <=0) nrrl = 1;
  if (nrrl >= ctx->conf.ngpus) nrrl = ctx->conf.ngpus;
  //printf("*** NEW: #rrl=%i\n", nrrl);
  all_rrl_size =nrrl;
  all_rrl = (host_register_queue_t*)malloc(all_rrl_size* sizeof(host_register_queue_t));
  for (int i=0; i<nrrl; ++i)
  {
    xkblas_cuda_init_reqreg_list(&all_rrl[i]);
    int err = pthread_create(&all_rrl[i].thread, 0, xkblas_cuda_register_thread, (void*)(uintptr_t)i );
    assert(err ==0);
  }

  return 0;
}


/*
*/
static void
XKBLAS_DRIVER_ENTRYPOINT(finalize)(void)
{
  xkblas_cuda_plugin_lock();
  if (INITIALIZED == true)
  {
    INITIALIZED = false;
#if _DRIVER_DEBUG
    fprintf(stdout, "cuda:%s: cuda finalize\n", __FUNCTION__);
#endif
  }
  xkblas_cuda_plugin_unlock();

  /* */
  void* tmp;
  for (int i=0; i<all_rrl_size; ++i)
  {
    assert(0 == pthread_cond_signal( &all_rrl[i].cond ));
    assert(0 == pthread_join( all_rrl[i].thread, &tmp ));
  }
  free( all_rrl );
  all_rrl_size = 0;
  all_rrl = 0;

  free(CUDA_DEVICE_ID);
  CUDA_DEVICE_ID = 0;

#if XKBLAS_USE_HWLOC
  hwloc_topology_destroy(topology);
#endif

  free(  xkblas_device_list );
  xkblas_device_list = 0;
}


static uint64_t post_request(
    host_register_request_op_t op,
    void* ptr, size_t size,
    xkblas_stream_instruction_callback_t callback,
    void* arg0, void* arg1, void* arg2
)
{
  if (ptr ==0) return (uint64_t)-1;

  /* Hash function from self_thread to get its request register list */
  int hash = xkblas_hash_ulong( pthread_self() ) % all_rrl_size;
  host_register_queue_t* rrl = &all_rrl[hash];
  assert(0 == pthread_mutex_lock( &rrl->lock ));
  while (rrl->posw - rrl->posr >= XKBLAS_MAX_REGLIST)
    pthread_cond_wait( &rrl->cond_waitall, &rrl->lock );
  uint64_t index = rrl->posw;
  ++rrl->posw;
  int idx = index % XKBLAS_MAX_REGLIST;
  rrl->req[idx].op   = op;
  rrl->req[idx].state= REQUEST_POST;
  rrl->req[idx].size = size;
  rrl->req[idx].callback  = callback;
  rrl->req[idx].arg0 = arg0;
  rrl->req[idx].arg1 = arg1;
  rrl->req[idx].arg2 = arg2;
  rrl->req[idx].ptr  = ptr;
  /* signal daemon thread */
  assert(0 == pthread_cond_signal( &rrl->cond ));
  assert(0 == pthread_mutex_unlock( &rrl->lock ));
  return index;
}

/*
*/
static
uint64_t XKBLAS_DRIVER_ENTRYPOINT(host_register)(
    void* ptr, size_t size,
    xkblas_stream_instruction_callback_t callback,
    void* arg0, void* arg1, void* arg2
)
{
  if (ptr ==0) return (uint64_t)-1;
  return post_request( DEVICE_REGISTER_REQUEST, ptr, size, callback, arg0, arg1, arg2 );
}

/*
*/
static
uint64_t XKBLAS_DRIVER_ENTRYPOINT(host_unregister)(
    void* ptr, size_t size,
    xkblas_stream_instruction_callback_t callback,
    void* arg0, void* arg1, void* arg2
)
{
  if (ptr ==0) return (uint64_t)-1;
  return post_request( DEVICE_UNREGISTER_REQUEST, ptr, size, callback, arg0, arg1, arg2 );
}


/*
*/
static
int XKBLAS_DRIVER_ENTRYPOINT(host_register_testwait)(
    uint64_t index,
    int flag
)
{
  /* Hash function from the thread id to request register list */
  int hash = xkblas_hash_ulong( pthread_self() ) % all_rrl_size;
  host_register_queue_t* rrl = &all_rrl[hash];

  if ((flag != 2) && ((index == (uint64_t)-1) || (index >= rrl->posw)))
    return EINVAL;

  switch (flag)
  {
    case 0: /* test: do not lock if test is true, else may it is a false negative */
      if (index <= rrl->posr) return 0;
      return EINPROGRESS;

    case 1: /* wait */
      /*TODO: cannot compute the time between T0 of the post_request and now if the index < rrl->posr
        because it means that requests have been proceed but we lost t0 stored in the request struct.
        One solution: sum t0, sum twait here and at the end returns sum twait - sum t0 as the sum of the delay.
        One case should be take into account: is post without corresponding wait=> each wait should account for all request between rrl->posr until the current index !.
        Other solution: keep the list of waiting requests index increment on wait_request.
      */
      assert(0 == pthread_mutex_lock( &rrl->lock ));
      if (index >= rrl->posr)
      {
        int idx = index % XKBLAS_MAX_REGLIST;
        assert(0 == pthread_mutex_lock( &rrl->req[idx].lock ));
        assert(0 == pthread_mutex_unlock( &rrl->lock ));
        assert((rrl->req[idx].state == REQUEST_POST) || (rrl->req[idx].state == REQUEST_DONE));
        if (rrl->req[idx].state == REQUEST_POST)
        {
          rrl->req[idx].state = REQUEST_WAIT;
          assert(0 == pthread_cond_wait( &rrl->req[idx].cond, &rrl->req[idx].lock ));
          assert( rrl->req[idx].state == REQUEST_DONE );
          assert(0 == pthread_mutex_unlock( &rrl->req[idx].lock ));
        }
        return 0;
      }
      else {
        assert(0 == pthread_mutex_unlock( &rrl->lock ));
        return 0;
      }

    case 2: /* wait all */
    {
      assert(0 == pthread_mutex_lock( &rrl->lock ));
      uint64_t pos = rrl->posw;
      while (pos > rrl->posr)
        assert(0 == pthread_cond_wait( &rrl->cond_waitall, &rrl->lock ));
      assert(0 == pthread_mutex_unlock( &rrl->lock ));
      return 0;
    }
    default:
      return EINVAL;
  }
  return 0;
}


/*
*/

/*
*/


/*
*/



/*
*/
static const char* XKBLAS_DRIVER_ENTRYPOINT(device_info)(xkblas_device_t* dev)
{
  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)dev;
  static char buffer[256];
  static char buf1[16];
  static char buf2[16];
  static char buf3[16];
  buf1[10] = 0;
  buf2[10] = 0;
  buf3[10] = 0;
  _print_mask(buf1, 10, device->affinity[0]);
  _print_mask(buf2, 10, device->affinity[1]);
  _print_mask(buf3, 10, device->affinity[2]);
  snprintf(buffer, 256, "%s, cuda device: %i, pci: %02x:%02x, %i async engine(s), %.2f (GB), cache limit %.2f (GB), affinity: %s,%s,%s",
    device->prop.name,
    device->inherited.device_id,
    device->prop.pciBusID,
    device->prop.pciDeviceID,
    device->prop.async_engines,
    ((double)device->inherited.mem_total)/1024.0/1024.0/1024.0,
    ((double)device->inherited.mem_limit)/1024.0/1024.0/1024.0,
    buf1, buf2, buf3
  );
  return buffer;
}


/*
*/
static void
XKBLAS_DRIVER_ENTRYPOINT(device_finalize)(xkblas_device_t* dev)
{

  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)dev;
  assert(INITIALIZED == true);

  xkblas_offload_stream_destroy(&device->inherited.stream);
  assert(0== pthread_mutex_destroy(&device->inherited.pipe_lock));

  device->inherited.state = XKBLAS_DEVICE_STATE_FINALIZED;
}


/*
*/

/*
*/
static int
XKBLAS_DRIVER_ENTRYPOINT(device_detach)(xkblas_device_t* dev)
{
  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)dev;
  assert(INITIALIZED == true);

  if (device->save_device_id >=0)
  {
      cudaError_t res;
      res = cudaSetDevice( device->save_device_id );
      __check_error(res);
      device->save_device_id =-1;
  }

  return 0;
}


/*
*/
static void*
XKBLAS_DRIVER_ENTRYPOINT(get_gpublas_handle)(xkblas_device_t* dev)
{
  xkblas_device_cuda_t* device = (xkblas_device_cuda_t*)dev;
#if XKBLAS_USE_PERSTREAM_BLASHANDLE==0
  return (void*)(uintptr_t)device->handle;
#else
  return 0;
#endif
}
#endif /* 0 */

static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    int device_count = 0;
    __check_error(cudaGetDeviceCount(&device_count));
    return (unsigned int)device_count;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(init)(void)
{
    if (INITIALIZED)
        return 0;

    memset(CUDA_DEVICE_ID, -1, sizeof(CUDA_DEVICE_ID));

    XKBLAS_MUTEX_LOCK(DRIVER_MUTEX);
    {
        if (INITIALIZED)
        {
            XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);
            return 0;
        }

        # pragma message(TODO "What is the point of 'gpuset' ? Keep it ? or rely on 'CUDA_VISIBLE_DEVICES' instead ?")
        unsigned int ndevices = XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)();
        xkblas_context_t * ctx = xkblas_context_get();
        uint32_t gpuset = ctx->conf.gpu_set;
        for (int i = 0; i < ndevices ; ++i)
        {
            int idx = __builtin_ffs((unsigned int)gpuset);
            assert(idx != 0);
            --idx;
            gpuset &= ~(1<<idx);
            __set_device_cuda_id(i, idx);
        }

        INITIALIZED = true;
    }
    XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);

    hwloc_topology_init(&TOPOLOGY);
    hwloc_topology_load(TOPOLOGY);

#if XKBLAS_CUDA_USE_NVLINK_TOPO
    _xkblas_get_gpu_topo();
#endif

    # pragma message(TODO "What is RRL ? Register memory (for pinning)")
    # if 0
    int nrrl = getenv("XKBLAS_RRL_SIZE") ? atoi(getenv("XKBLAS_RRL_SIZE")) : 1;
    if (nrrl <= 0)
        nrrl = 1;
    if (nrrl >= ctx->conf.ngpus)
        nrrl = ctx->conf.ngpus;

    all_rrl_size = nrrl;
    all_rrl = (host_register_queue_t*)malloc(all_rrl_size* sizeof(host_register_queue_t));
    for (int i=0; i<nrrl; ++i)
    {
        xkblas_cuda_init_reqreg_list(&all_rrl[i]);
        int err = pthread_create(&all_rrl[i].thread, 0, xkblas_cuda_register_thread, (void*)(uintptr_t)i );
        assert(err ==0);
    }
    # endif

    return 0;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(finalize)(void)
{
    if (!INITIALIZED)
    {
        XKBLAS_MUTEX_LOCK(DRIVER_MUTEX);
        {
            if (!INITIALIZED)
                XKBLAS_FATAL("Finalize CUDA driver before initializing...");
        }
        XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);
    }

    assert(INITIALIZED);
    INITIALIZED = 0;
    hwloc_topology_destroy(TOPOLOGY);
}

static const char *
XKBLAS_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "CUDA";
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_set_cpuset)(cpu_set_t * schedset, int device_id)
{
    if (schedset == NULL)
        return EINVAL;

    assert(device_id >= 0);
    assert(device_id < XKBLAS_DEVICES_MAX);
    assert(__get_device_cuda_id(device_id) != -1);

    CPU_ZERO(schedset);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    int err = hwloc_cudart_get_device_cpuset(TOPOLOGY, __get_device_cuda_id(device_id), cpuset);
    if (err == 0)
    {
        err = hwloc_cpuset_to_glibc_sched_affinity(TOPOLOGY, cpuset, schedset, sizeof(cpu_set_t));
        assert(err == 0);
        if (err)
            err = ENOTSUP;
    }
    else
        XKBLAS_WARN("Could not get a 'cpuset' for CUDA device %d, falling back to glibc...", device_id);

    hwloc_bitmap_free(cpuset);
    return err;
}

static xkblas_device_t *
XKBLAS_DRIVER_ENTRYPOINT(device_create)(xkblas_driver_t * driver, int device_id)
{
    assert(INITIALIZED);
    assert(device_id >= 0 && device_id < XKBLAS_DEVICES_MAX);

    xkblas_device_cuda_t * device = DEVICES + device_id;
    device->save_device_id = -1;

    return (xkblas_device_t *) device;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(device_init)(int device_id)
{
    assert(INITIALIZED);

    xkblas_device_cuda_t * device = __get_device_cuda(device_id);
    # pragma message(TODO "Check device lifecycle: must be created here")

    struct cudaDeviceProp prop;
    cudaError_t res;
    res = cudaSetDevice(__get_device_cuda_id(device_id));
    __check_error(res);

    res = cudaGetDeviceProperties(&prop, __get_device_cuda_id(device_id));
    __check_error(res);

    device->prop.pciBusID = prop.pciBusID;
    device->prop.pciDeviceID = prop.pciDeviceID;
    device->prop.concurrent = prop.concurrentKernels;
    memset(device->prop.name, 0, 64*sizeof(char));
    strncpy(device->prop.name, prop.name, 64);

    /* memory device */
    device->mem_total = prop.totalGlobalMem;
    device->size_alloc = 0;
    device->size_free = 0;
    device->free_mem = 0;
    xkblas_context_t * ctx = xkblas_context_get();
    device->mem_limit = (size_t)((double)ctx->conf.cuda_cache_limit
            * (double)(device->free_mem-180UL*1024UL*1024UL));

    { /* work memory allocation, maybe smarter to do it in xkblas_device_init */
        size_t free;
        size_t total;
        res = cudaMemGetInfo(&free, &total);
        __check_error(res);

        /* allocate 90% of free memory, into a new chunk */
        size_t target = free * 0.9;
        xkblas_alloc_chunk_t* chunk0 = (xkblas_alloc_chunk_t*) malloc( sizeof(xkblas_alloc_chunk_t) );
        res = cudaMalloc( (void**) &(chunk0->device_ptr), target );
        __check_error(res);

        chunk0->size = target;
        chunk0->state = FREE_STATE;
        chunk0->prev = NULL;
        chunk0->next = NULL;
        chunk0->freelink = NULL;
        device->inherited.memdev.free_chunk_list = chunk0;
        device->inherited.memdev.memory_allocated = 1;
    }



    if (getenv("XKBLAS_NO_GPUALLOCATOR"))
        XKBLAS_ERROR("XKBLAS_NO_GPUALLOCATOR but code do not compile for this option");

    //device->inherited.memdev.f_alloc = cuda_alloc;
    //device->inherited.memdev.f_free = cuda_free;

    # pragma message(TODO "Implement missing interfaces")
    # if 0
    device->inherited.memdev.f_copy = cuda_copy;
    device->inherited.memdev.f_memsync = cuda_memsync;
    device->inherited.memdev.f_get_mem_info = cuda_get_mem_info;
    device->inherited.memdev.f_get_free_mem = cuda_get_free_mem;
    {
        size_t free;
        size_t total;
        res = cudaMemGetInfo(&free, &total);
        __check_error(res);

        device->inherited.free_mem = (size_t)free;
    }
    device->inherited.memdev.f_get_source = cuda_get_source;

    device->inherited.stream.f_stream_free = cuda_stream_free;
    device->inherited.stream.f_stream_alloc = cuda_stream_alloc;
    device->inherited.stream.f_stream_process_pending = cuda_stream_process_pending;
    device->inherited.stream.f_stream_decode_ioinstruction = cuda_stream_decode_ioinstruction;
    # endif
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_destroy)(xkblas_device_t * device)
{
    device->state = XKBLAS_DEVICE_STATE_DESTROY;
    free(device);
    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_attach)(int device_id)
{
    assert(INITIALIZED);

    xkblas_device_cuda_t * device = __get_device_cuda(device_id);
    assert(device->save_device_id == -1);

    cudaError_t res;
    res = cudaGetDevice(&device->save_device_id);
    __check_error(res);

    res = cudaSetDevice(__get_device_cuda_id(device_id));
    __check_error(res);

    return 0;
}

/* Called on all devices of the driver after they have been initialized */
static int
XKBLAS_DRIVER_ENTRYPOINT(device_commit)(int device_id)
{
    /* all other devices 'peer' context have been initialized, enable peer */
    xkblas_device_cuda_t * device = __get_device_cuda(device_id);

    for (int i = 0 ; i < XKBLAS_DEVICES_MAX ; ++i)
    {
        if (i != device_id)
        {
            xkblas_device_cuda_t * odevice = __get_device_cuda(i);
            if (odevice->inherited.state != XKBLAS_DEVICE_STATE_INIT)
                continue ;

            int device1 = __get_device_cuda_id(device_id);
            int device2 = __get_device_cuda_id(i);

            int access;
            cudaError_t res;
            res = cudaDeviceCanAccessPeer(&access, device1, device2);
            __check_error(res);

            if (access)
            {
                res = cudaDeviceEnablePeerAccess(device2, 0);
                if ((res == cudaSuccess) || (res ==cudaErrorPeerAccessAlreadyEnabled))
                {
                    # pragma message(TODO "Do we still need devices affinity ? -> Yes, to move closest data replicate")
                    # if 0
                    int rank = cuda_perf_topo[device1*cuda_device_count+device2];
                    assert(rank !=0);
                    if (cuda_perf_device[ device1*cuda_count_perfrank+ rank] & (1<<device2))
                    {
                        device->affinity[rank-1] |= (1UL<<xkblas_memory_asid_get_lid(xkblas_device_list[j]->inherited.memdev.asid));
                        ld->affinity[rank-1] |= (1UL<<xkblas_memory_asid_get_lid(xkblas_device_list[j]->inherited.memdev.asid));
                    }
                    # endif
                }
            }
        }
        /* add device with itself */
        else
        {
            # pragma message(TODO "Do we still need devices affinity ? -> Yes, to move closest data replicate")
            # if 0
            device->affinity[0] |= (1UL<<xkblas_memory_asid_get_lid(xkblas_device_list[j]->inherited.memdev.asid));
            ld->affinity[0] |= (1UL<<xkblas_memory_asid_get_lid(xkblas_device_list[j]->inherited.memdev.asid));
            # endif
        }
    }
    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(stream_instruction_decode)(
    xkblas_stream_t * istream,
    xkblas_stream_instruction_t * instr
) {
    xkblas_stream_cuda_t * stream = (xkblas_stream_cuda_t *) istream;
    assert(stream);

    switch (instr->type)
    {
        case (XKBLAS_STREAM_INSTR_TYPE_NOP):
        {
            return 0;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
        {
            return ENOSYS;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_BARRIER):
        {
            cudaError_t res = cudaStreamSynchronize(stream->cu.handle);
            assert(res == cudaSuccess);
            return 0;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_KERN):
        {
            xkblas_stream_instruction_kernel_t * op = &instr->kern;

            task_kernel_param_t param = { .task = op->task, .handle = stream->cu.blas.handle };
            xkblas_kernel_launch(XKBLAS_DRIVER_CUDA, &param);

            # pragma message(TODO "Add support for end event records")

            int wp = istream->pending.pos.w % istream->pending.capacity;
            cudaError_t err = cudaEventRecord(stream->cu.events.end[wp], stream->cu.handle);
            assert(err == cudaSuccess);

            return EINPROGRESS;

        } /* XKBLAS_STREAM_INSTR_TYPE_KERN */

        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
        {
            return ENOSYS;
        }

        default:
            return EINVAL;
    }

    /* unreachable code */
    XKBLAS_FATAL("Unreachable code");


            # if 0
        case XKBLAS_STREAM_INSTR_TYPE_NOP:
            return 0;

        case XKBLAS_STREAM_INSTR_TYPE_COPY_H2H:
        case XKBLAS_STREAM_INSTR_TYPE_COPY_H2D:
        case XKBLAS_STREAM_INSTR_TYPE_COPY_D2H:
        case XKBLAS_STREAM_INSTR_TYPE_COPY_D2D:
            {
#if XKBLAS_HAVE_IO_THREADS
                assert( (thread_type == 1) || (thread_type == 2) );
#endif

                if (instr->type == XKBLAS_STREAM_INSTR_TYPE_COPY_D2D)
                    stream = &stream->stream_low;
                else
                    stream = &stream->stream;
                assert(*stream !=0);

#if 0
                /* todo in an efficient way: implicit synchro between host_register_async and here */
                /* wait end of pining operation if any */
                while (rrl->posw != rrl->reg_sig)
                    xkblas_slowdown_cpu();
#endif

                struct xkblas_io_copy* op = &instr->inst.c_io;

                /* switch among view_src type (1D, 2D or 3D).
                   May be some redistribution may be implemented here ?
                   */
                size_t size = xkblas_memory_view_size(op->view_src);
                assert( size == xkblas_memory_view_size(op->view_dest));
                assert( size == xkblas_memory_view_size(op->view_dest));
                type = op->view_src->type;
                assert( type == op->view_dest->type);
                uint8_t storage = op->view_src->storage;
                assert( storage == op->view_dest->storage);
                int test = xkblas_memory_view_iscontiguous(op->view_src) && xkblas_memory_view_iscontiguous(op->view_dest);

                if (test)
                {
                    type = XKBLAS_MEMORY_VIEW_1D;
                }
                assert( test || (instr->type != XKBLAS_STREAM_INSTR_TYPE_COPY_D2D) );

                void* src  = xkblas_memory_view2pointer((void*)op->src, op->view_src);
                void* dest = xkblas_memory_view2pointer((void*)op->dest, op->view_dest);

                XKBLAS_EVENT_PUSH1( &xkblas_self_context()->kproc, XKBLAS_EVT_OFFLOAD_CPY,
                        1 /* begin */, op->reserved );
                uint64_t delay = xkblas_get_elapsedns();
                switch (type)
                {
                    case XKBLAS_MEMORY_VIEW_1D:
                        {
                            XKBLAS_DEBUG("%s: instr '%s' 1D data\n", __FUNCTION__, name_io[instr->type]);
                            //printf("%f: instr '%s' 1D data\n", xkblas_get_elapsedtime(), name_io[instr->type]);
                            switch (instr->type)
                            {
                                case XKBLAS_STREAM_INSTR_TYPE_COPY_H2H:
                                    memcpy( dest, src, size );
                                    delay = xkblas_get_elapsedns()-delay;
                                    XKBLAS_EVENT_PUSH2( &xkblas_self_context()->kproc, XKBLAS_EVT_OFFLOAD_CPY,
                                            2 /* end */, op->reserved, delay );
                                    res = 0;
                                    break;
                                case XKBLAS_STREAM_INSTR_TYPE_COPY_H2D:
#if 0// XKBLAS_DEBUG
                                    _xkblas_lock_print();
                                    printf("%x:: Memcpy1D H2D %p %p %i %p\n", pthread_self(), dest, src, size, *stream);
                                    _xkblas_unlock_print();
#endif
                                    res = cudaMemcpyAsync( dest,
                                            src,
                                            size,
                                            cudaMemcpyHostToDevice,
                                            *stream);
                                    break;
                                case XKBLAS_STREAM_INSTR_TYPE_COPY_D2H:
#if 0// XKBLAS_DEBUG
                                    _xkblas_lock_print();
                                    printf("%x:: Memcpy1D D2H %p %p %i %p\n", pthread_self(), dest, src, size, *stream);
                                    _xkblas_unlock_print();
#endif
                                    res = cudaMemcpyAsync( dest,
                                            src,
                                            size,
                                            cudaMemcpyDeviceToHost,
                                            *stream);
                                    break;

                                case XKBLAS_STREAM_INSTR_TYPE_COPY_D2D:
#if 0// XKBLAS_DEBUG
                                    _xkblas_lock_print();
                                    printf("%x:: Memcpy1D D2D: %i -> %i:: dest: %p, src: %p, size: %i, stream: %p\n", pthread_self(), 1+op->dev_src->device->device_id, 1+op->dev_dest->device->device_id, dest, src, size, *stream);
                                    _xkblas_unlock_print();
#endif
                                    res = cudaMemcpyPeerAsync( dest,
                                            CUDA_DEVICE_ID[op->dev_dest->device->device_id],
                                            src,
                                            CUDA_DEVICE_ID[op->dev_src->device->device_id],
                                            size,
                                            *stream);
                                    break;
                                default:
                                    assert(0);
                            };
                            __check_error(res);
                        } break;

                    case XKBLAS_MEMORY_VIEW_2D:
                        {
                            size_t width, height, dpitch, spitch;
                            if (storage == XKBLAS_MEMORY_STORAGE_ROWMAJOR)
                            {
                                width  = op->view_dest->size[1] * op->view_dest->wordsize;
                                height = op->view_dest->size[0];
                            }
                            else if (storage == XKBLAS_MEMORY_STORAGE_COLMAJOR)
                            {
                                width  = op->view_dest->size[0] * op->view_dest->wordsize;
                                height = op->view_dest->size[1];
                            } else {
                                xkblas_abort( __LINE__, __FILE__, "Invalid storage");
                            }
                            dpitch = op->view_dest->ld * op->view_dest->wordsize;
                            spitch = op->view_src->ld * op->view_src->wordsize;

                            switch (instr->type)
                            {
                                case XKBLAS_STREAM_INSTR_TYPE_COPY_H2H:
#if 0// XKBLAS_DEBUG
                                    _xkblas_lock_print();
                                    printf("%x:: Memcpy2D H2H %p %p %i %p\n", pthread_self(), dest, src, size, *stream);
                                    _xkblas_unlock_print();
#endif
                                    res = cudaMemcpy2DAsync ( dest, dpitch, src, spitch, width, height, cudaMemcpyHostToHost, *stream );
                                    break;
                                case XKBLAS_STREAM_INSTR_TYPE_COPY_H2D:
#if 0// XKBLAS_DEBUG
                                    _xkblas_lock_print();
                                    printf("%x:: Memcpy2D H2D: %i -> %i:: dest: %p, src: %p, size: %i, stream: %p\n", pthread_self(), op->dev_src->device->device_id, 1+op->dev_dest->device->device_id, dest, src, size, *stream);
                                    _xkblas_unlock_print();
#endif
                                    res = cudaMemcpy2DAsync ( dest, dpitch, src, spitch, width, height, cudaMemcpyHostToDevice, *stream );
                                    break;
                                case XKBLAS_STREAM_INSTR_TYPE_COPY_D2H:
#if 0// XKBLAS_DEBUG
                                    _xkblas_lock_print();
                                    printf("%x:: Memcpy2D D2H: %i -> %i:: dest: %p, src: %p, size: %i, stream: %p\n", pthread_self(), 1+op->dev_src->device->device_id, op->dev_dest->device->device_id, dest, src, size, *stream);
                                    _xkblas_unlock_print();
#endif
                                    res = cudaMemcpy2DAsync ( dest, dpitch, src, spitch, width, height, cudaMemcpyDeviceToHost, *stream );
                                    break;
                                case XKBLAS_STREAM_INSTR_TYPE_COPY_D2D:
#if 0// XKBLAS_DEBUG
                                    _xkblas_lock_print();
                                    printf("%x:: Memcpy2D D2D: %i -> %i:: dest: %p, src: %p, size: %i, stream: %p\n", pthread_self(), 1+op->dev_src->device->device_id, 1+op->dev_dest->device->device_id, dest, src, size, *stream);
                                    _xkblas_unlock_print();
#endif
                                    res = cudaMemcpy2DAsync ( dest, dpitch, src, spitch, width, height, cudaMemcpyDeviceToDevice, *stream );
                                    break;
                                default:
                                    assert(0);
                            };
                        } break;

                    case XKBLAS_MEMORY_VIEW_3D:
                    default:
                        assert(false);
                        break;
                };

                res = cudaEventRecord( stream->end_events[ istream->pos_wp % istream->count ], *stream );
                __check_error(res);
                XKBLAS_DEBUG("%s: stream %p instr '%s' src:%p, dest:%p size:%zu\n", __FUNCTION__,
                        (void*)*stream,
                        name_io[instr->type],
                        (void*)src,
                        (void*)dest,
                        size
                        );
            } break;

        case XKBLAS_STREAM_INSTR_TYPE_BARRIER:
            res = cudaStreamSynchronize( stream->stream );
            assert(res == cudaSuccess);
            res = cudaStreamSynchronize( stream->stream_low );
            assert(res == cudaSuccess);
            ++istream->ok_p;
            break;

        case XKBLAS_STREAM_INSTR_TYPE_KERN:
            {
#if XKBLAS_HAVE_IO_THREADS
                assert( thread_type == 0 );
#endif
                /* same as cublas */
                stream = &stream->stream;
                struct xkblas_io_kernel* op = &instr->inst.k_io;
                XKBLAS_DEBUG("%s: instr '%s' exec task:%p, stream: %p\n", __FUNCTION__,
                        name_io[instr->type],
                        op->task,
                        (void*)*stream
                        );
                XKBLAS_EVENT_PUSH1( &xkblas_self_context()->kproc, XKBLAS_EVT_OFFLOAD_KERN,
                        1 /* begin */, op->reserved );
#  if CONFIG_USE_EVENT
                res = cudaEventRecord(stream->start_events[ istream->pos_wp % istream->count ], *stream );
                assert(res == cudaSuccess);
#  endif
                xkblas_offload_device_execute_task(
                    &device->inherited,
                    op->task,
                    stream->handle
                );
            }
    }
    # endif
}


static xkblas_stream_t *
XKBLAS_DRIVER_ENTRYPOINT(stream_create)(
    xkblas_stream_type_t type,
    unsigned int capacity
) {
    cudaError_t res;
    assert(INITIALIZED == true);

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkblas_stream_cuda_t) + capacity * sizeof(cudaEvent_t));
    assert(mem);

    xkblas_stream_cuda_t * stream = (xkblas_stream_cuda_t *) mem;

    /*************************/
    /* init xkblas stream */
    /*************************/
    xkblas_stream_init((xkblas_stream_t *) stream, type, capacity, XKBLAS_DRIVER_ENTRYPOINT(stream_instruction_decode));

    /*************************/
    /* do cuda specific init */
    /*************************/

    /* events */
    stream->cu.events.end = (cudaEvent_t *) (mem + sizeof(xkblas_stream_cuda_t));
    stream->cu.events.capacity = capacity;

    cudaError_t err;
    for (int i = 0 ; i < capacity ; ++i)
    {
        err = cudaEventCreateWithFlags(stream->cu.events.end + i, cudaEventDisableTiming);
        __check_error(err);
    }

    /* streams */
    int leastPriority, greatestPriority;
    err = cudaDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);
    __check_error(err);

    err = cudaStreamCreateWithPriority(&stream->cu.handle, cudaStreamNonBlocking, greatestPriority);
    __check_error(err);

    if (type == XKBLAS_STREAM_TYPE_KERN)
    {
        cublasStatus_t cres = cublasCreate(&stream->cu.blas.handle);
        assert(cres == CUBLAS_STATUS_SUCCESS);
        cres = cublasSetStream(stream->cu.blas.handle, stream->cu.handle);
        assert(cres == CUBLAS_STATUS_SUCCESS);
    }
    else
    {
        stream->cu.blas.handle = 0;
    }

    return (xkblas_stream_t *) stream;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(stream_delete)(
    xkblas_stream_t * istream
) {
    xkblas_stream_cuda_t * stream = (xkblas_stream_cuda_t *) istream;
    if (stream->cu.blas.handle)
        cublasDestroy(stream->cu.blas.handle);
    cudaStreamDestroy(stream->cu.handle);
    free(stream);
}

void
XKBLAS_DRIVER_ENTRYPOINT(get_driver)(xkblas_driver_t * driver)
{
    # define EP(func) driver->f_##func = XKBLAS_DRIVER_ENTRYPOINT(func)

    EP(init);
    EP(finalize);
    EP(get_name);
    EP(get_ndevices_max);
    EP(device_set_cpuset);
    EP(device_create);
    EP(device_destroy);
    EP(device_init);
    EP(device_attach);
    EP(device_commit);

    EP(stream_create);
    EP(stream_delete);

    #if 0

    EP(get_flags);
    EP(get_type);
    EP(host_register);
    EP(host_register_testwait);
    EP(host_unregister);

    EP(device_info);
    EP(device_finalize);
    EP(device_detach);
    EP(get_gpublas_handle);

    #endif

    # undef EP
}
