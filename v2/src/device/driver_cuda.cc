# define XKBLAS_DRIVER_ENTRYPOINT(N) XKBLAS_DRIVER_TYPE_CUDA_ ## N

# include "xkblas-context.h"
# include "conf/conf.h"
# include "device/cublas-helper.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/stream.h"
# include "logger/logger.h"
# include "sync/mutex.h"

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

        struct {
            cudaStream_t high;
            cudaStream_t low;
        } handle;

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

    uint64_t * affinity;

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
    if (err != cudaSuccess && err != cudaErrorNotReady)
    {
        const char * errstr = cudaGetErrorName(err);
        XKBLAS_ERROR("cuCheckError() error: %s (%i)", errstr, err);
        return 1;
    }
    return 0;
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

static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    int device_count = 0;
    __check_error(cudaGetDeviceCount(&device_count));
    return (unsigned int)device_count;
}

// NVLINK TOPOLOGY

/* cuda_perf_topo[i,j] returns the perfRank of the communication link between
   device.
   cuda_perf_device[d][i] for i=0,..,cuda_count_perfrank-1 is the mask of device
   for which the device d has link with performance i.
*/

static int cuda_device_count;
static int cuda_perf_topo[XKBLAS_DEVICES_MAX][XKBLAS_DEVICES_MAX];
static int cuda_count_perfrank;
static uint64_t * cuda_perf_device;
static int* cuda_routing_table;

static void
__get_gpu_topo(void)
{
    __check_error(cudaGetDeviceCount(&cuda_device_count));
    if (cuda_device_count == 0)
        return;

    int min_perf = 0;
    int max_perf = 0;

    // Enumerates Device <-> Device links and store perfRank
    for (int i = 0; i < cuda_device_count; i++)
    {
        for (int j = 0; j < cuda_device_count; j++)
        {
            if (i == j)
            {
                cuda_perf_topo[i][j] = 0;
                continue ;
            }
            else
            {
                int perfRank = 0;
                int accessSupported = 0;

                cudaError_t res = cudaDeviceGetP2PAttribute(
                                    &accessSupported,
                                    cudaDevP2PAttrAccessSupported,
                                    i,
                                    j
                );
                __check_error(res);
                if (accessSupported)
                {
                    res = cudaDeviceGetP2PAttribute(
                            &perfRank,
                            cudaDevP2PAttrPerformanceRank,
                            i,
                            j
                    );
                    __check_error(res);
                    cuda_perf_topo[i][j] = 1 + perfRank;

                    if (perfRank < max_perf)
                        max_perf = perfRank;
                    if (perfRank > min_perf)
                        min_perf = perfRank;
                }
                else
                {
                    /* should be higher than previous value: computed after */
                    cuda_perf_topo[i][j] = -1;
                }
            }
        }
    }

    #pragma message(TODO "Not sure to get all the logic here")

    /* if there is no link, set to the minmum perf */
    ++min_perf;
    for (int i = 0 ; i < cuda_device_count ; ++i)
    {
        for (int j = 0 ; j < cuda_device_count ; ++j)
        {
            if (cuda_perf_topo[i][j] == -1)
            {
                cuda_perf_topo[i][j] = min_perf + 1;    // TODO : this +1 is suspicious to me
                XKBLAS_INFO("Cuda GPU : no peer access from %d to %d", i, j);
            }
        }
    }

    cuda_count_perfrank = min_perf - max_perf + 2;
    size_t size = cuda_device_count * cuda_count_perfrank * sizeof(uint64_t);
    cuda_perf_device = (uint64_t *) malloc( size );
    assert(cuda_perf_device);
    memset(cuda_perf_device, 0, size);

    for (int i = 0 ; i < cuda_device_count ; ++i)
    {
        for (int j = 0 ; j < cuda_device_count ; ++j)
        {
            int rank = cuda_perf_topo[i][j];
            assert(0 <= i * cuda_device_count + rank );
            assert(i * cuda_count_perfrank + rank <= cuda_device_count * cuda_count_perfrank);
            cuda_perf_device[i * cuda_count_perfrank + rank] |= (1UL << j);
            XKBLAS_INFO("Cuda GPU %d -> %d with perf = %d", i, j, rank);
        }
    }
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
        xkblas_context_t * ctx = xkblas_context_get();
        uint32_t ngpus = MIN(XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)(), ctx->conf.ngpus);
        uint32_t gpuset = ctx->conf.gpu_set;
        for (int i = 0; i < ngpus ; ++i)
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
    __get_gpu_topo();

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
# ifndef NDEBUG
    int cu_devid = -1;
    cudaGetDevice(&cu_devid);
    assert(cu_devid == __get_device_cuda_id(device_id));
# endif /* NDEBUG */

    xkblas_device_cuda_t * device = __get_device_cuda(device_id);
    assert(device);

    const uint64_t perfrank = cuda_count_perfrank - 1;
    const uint64_t size = sizeof(uint64_t) * perfrank;
    device->affinity = (uint64_t *) malloc(sizeof(uint64_t) * perfrank);
    memset(device->affinity, 0, size);

    /* all other devices have been initialized, enable peer */
    int i = __get_device_cuda_id(device_id);
    for (int dev = 0 ; dev < XKBLAS_DEVICES_MAX ; ++dev)
    {
        int j = __get_device_cuda_id(dev);

        /* add device with itself */
        if (i == j)
        {
            # if 0
            device->affinity[0] |= (1UL<<xkblas_memory_asid_get_lid(xkblas_device_list[j]->inherited.memdev.asid));
            ld->affinity[0] |= (1UL<<xkblas_memory_asid_get_lid(xkblas_device_list[j]->inherited.memdev.asid));
            # endif
        }
        else
        {
            xkblas_device_cuda_t * odevice = __get_device_cuda(j);
            if (odevice->inherited.state != XKBLAS_DEVICE_STATE_INIT)
                continue ;

            int access;
            cudaError_t res;
            res = cudaDeviceCanAccessPeer(&access, i, j);
            __check_error(res);

            if (access)
            {
                res = cudaDeviceEnablePeerAccess(j, 0);
                if ((res == cudaSuccess) || (res == cudaErrorPeerAccessAlreadyEnabled))
                {
                    int rank = cuda_perf_topo[i][j];
                    assert(rank);
                    if (cuda_perf_device[i * cuda_count_perfrank + rank] & (1 << j))
                    {
                        # if 0
                        device->affinity[rank-1] |= (1UL <<xkblas_memory_asid_get_lid(xkblas_device_list[j]->inherited.memdev.asid));
                        # endif
                    }
                }
                else
                {
                    XKBLAS_WARN("Could not enable peer from %d to %d", i, j);
                }
            }
            else
            {
                XKBLAS_WARN("GPU peer from %d to %d is not possible", i, j);
            }
        }
    }

    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(stream_instruction_launch)(
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

        case (XKBLAS_STREAM_INSTR_TYPE_BARRIER):
        {
            cudaError_t res;
            res = cudaStreamSynchronize(stream->cu.handle.high);
            assert(res == cudaSuccess);
            res = cudaStreamSynchronize(stream->cu.handle.low);
            assert(res == cudaSuccess);
            ++istream->ok_p;
            return 0;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_KERN):
        {
            assert(istream->type == XKBLAS_STREAM_TYPE_KERN);
            assert(stream->cu.handle.high);
            assert(stream->cu.blas.handle);

            xkblas_stream_instruction_kernel_t * op = &instr->kern;
            task_kernel_param_t param = { .task = op->task, .handle = stream->cu.blas.handle };
            xkblas_kernel_launch(XKBLAS_DRIVER_TYPE_CUDA, &param);

            # pragma message(TODO "Add support for end event records")

            uint32_t wp = istream->pending.pos.w % istream->pending.capacity;
            assert(stream->cu.events.capacity == istream->pending.capacity);
            cudaError_t err = cudaEventRecord(stream->cu.events.end[wp], stream->cu.handle.high);
            assert(err == cudaSuccess);

            return EINPROGRESS;

        } /* XKBLAS_STREAM_INSTR_TYPE_KERN */

        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
        {
            void * dst          = (void *) instr->copy.dst_device_view.addr;
            size_t dpitch       = instr->copy.dst_device_view.ld * instr->copy.host_view.sizeof_type;
            const void * src    = (const void *) instr->copy.src_device_view.addr;
            size_t spitch       = instr->copy.src_device_view.ld * instr->copy.host_view.sizeof_type;
            size_t width        = instr->copy.host_view.bs_n * instr->copy.host_view.sizeof_type;
            size_t height       = instr->copy.host_view.bs_m;
            cudaMemcpyKind kind;
            cudaStream_t handle;

            switch (instr->type)
            {
                case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
                {
                    kind = cudaMemcpyHostToDevice;
                    handle = stream->cu.handle.high;
                    break ;
                }

                case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
                {
                    kind = cudaMemcpyDeviceToHost;
                    handle = stream->cu.handle.high;
                    break ;
                }

                case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
                {
                    kind = cudaMemcpyDeviceToDevice;
                    handle = stream->cu.handle.low;
                    break ;
                }

                case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
                    return ENOSYS;

                default:
                {
                    XKBLAS_FATAL("instr->type got modified, something went really wrong");
                    break ;
                }
            }

            XKBLAS_WARN("cudaMemcpy2DAsync(dst=%p, dpitch=%d, src=%p, spitch=%d, width=%d, height=%d, kind=%s", dst, dpitch, src, spitch, width, height, (kind == cudaMemcpyDeviceToDevice) ? "D2D" : (kind == cudaMemcpyDeviceToHost) ? "D2H" : (kind == cudaMemcpyHostToDevice) ? "H2D" : "?");
            cudaError_t err = cudaMemcpy2DAsync(dst, dpitch, src, spitch, width, height, kind, handle);
            __check_error(err);
            err = cudaEventRecord(stream->cu.events.end[istream->pending.pos.w % istream->pending.capacity], handle);
            __check_error(err);

            return EINPROGRESS;
        }

        default:
            return EINVAL;
    }

    /* unreachable code */
    XKBLAS_FATAL("Unreachable code");
}

/* increase ok_p without calling callback */
static inline int
cuda_stream_instructions_progress(
    xkblas_stream_t * istream,
    int blocking
) {
    assert(istream);

    xkblas_stream_cuda_t * stream = (xkblas_stream_cuda_t *) istream;

    /* no pending instructions */
    if (istream->pending.pos.r == istream->pending.pos.w)
        return 0;

    if (blocking)
    {
        cudaError_t err;
        err = cudaStreamSynchronize(stream->cu.handle.high);
        __check_error(err);
        err = cudaStreamSynchronize(stream->cu.handle.low);
        __check_error(err);
        istream->ok_p = istream->pending.pos.w;
        return 0;
    }

    /* istream->ok_p is past the last ok pending request: test from ok_p to pos_wp */
    uint64_t     size = istream->pending.pos.w - istream->pending.pos.r;
    uint64_t      okp = istream->ok_p;
     int64_t prev_okp = okp - 1;

    while (okp < istream->pending.pos.w)
    {
        int idx = okp % istream->pending.capacity;
        xkblas_stream_instruction_t * instr = istream->pending.instr + idx;
        cudaError_t res = cudaSuccess;

        switch (instr->type)
        {
            case (XKBLAS_STREAM_INSTR_TYPE_KERN):
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
            {
                /* poll events */
                for (int i = 0 ; i < 1 ; ++i)
                {
                    res = cudaEventQuery(stream->cu.events.end[idx]);
                    assert(res == cudaErrorNotReady || res == cudaSuccess);
                    __check_error(res);

                    # pragma message(TODO "Why pthread_yield here ?")
                    if (res == cudaErrorNotReady)
                    {
                        XKBLAS_DEBUG("Not ready, yielding");
                        pthread_yield();
                    }
                    else
                    {
                        assert(res == cudaSuccess);
                        if (prev_okp + 1 == okp)
                            ++prev_okp;
                    }
                }
            } /* intentionally fallthrough */

            case (XKBLAS_STREAM_INSTR_TYPE_NOP):
            case (XKBLAS_STREAM_INSTR_TYPE_BARRIER):
            {
                ++okp;
                break ;
            }

            default:
            {
                XKBLAS_FATAL("Wrong instruction");
            }
        }
    }

    /* all events have been tested, test the prev_okp has been incremented */
    okp = istream->ok_p;
    if (prev_okp != okp - 1)
    {
        istream->ok_p = prev_okp + 1;
        return 0;
    }

    return EINPROGRESS;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    xkblas_stream_t * istream,
    int blocking
) {
    int err = cuda_stream_instructions_progress(istream, blocking);
    assert(err == 0 || err == EINPROGRESS);

    for (int p = istream->pending.pos.r ; p < istream->ok_p ; ++p)
    {
        int idx = p % istream->pending.capacity;
        xkblas_stream_instruction_t * instr = istream->pending.instr + idx;
        assert(instr);

        switch (instr->type)
        {
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
            case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
            case (XKBLAS_STREAM_INSTR_TYPE_BARRIER):
            case (XKBLAS_STREAM_INSTR_TYPE_KERN):
            {
                if (instr->callback.func)
                    instr->callback.func(instr->callback.args);

            } /* intentionally fallthrough the next case */

            case (XKBLAS_STREAM_INSTR_TYPE_NOP):
            {
                ++istream->pending.pos.r;
                break ;
            }

            default:
            {
                /* unreachable code */
                XKBLAS_FATAL("Unreachable code - instr->type=%d", instr->type);
                return EINVAL;
            }
        }
    }

    return 0;
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
    xkblas_stream_init(
        (xkblas_stream_t *) stream,
        type,
        capacity,
        XKBLAS_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKBLAS_DRIVER_ENTRYPOINT(stream_instructions_progress)
    );

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

    err = cudaStreamCreateWithPriority(&stream->cu.handle.high, cudaStreamNonBlocking, greatestPriority);
    __check_error(err);

    err = cudaStreamCreateWithPriority(&stream->cu.handle.low, cudaStreamNonBlocking, leastPriority);
    __check_error(err);

    if (type == XKBLAS_STREAM_TYPE_KERN)
    {
        cublasStatus_t cres = cublasCreate(&stream->cu.blas.handle);
        xkblas_cublas_status_check(cres);
        assert(cres == CUBLAS_STATUS_SUCCESS);

        cres = cublasSetStream(stream->cu.blas.handle, stream->cu.handle.high);
        xkblas_cublas_status_check(cres);
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
    cudaStreamDestroy(stream->cu.handle.high);
    cudaStreamDestroy(stream->cu.handle.low);
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
