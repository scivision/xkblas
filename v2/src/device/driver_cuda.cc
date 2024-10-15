# define XKBLAS_DRIVER_ENTRYPOINT(N) XKBLAS_DRIVER_TYPE_CUDA_ ## N

# include "xkblas-context.h"
# include "conf/conf.h"
# include "device/cublas-helper.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/stream.h"
# include "logger/logger.h"
# include "sync/bits.h"
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
            xkblas_stream_instruction_counter_t capacity;
        } events;

        struct {
            cublasHandle_t handle;
        } blas;

    } cu;
}               xkblas_stream_cuda_t;

typedef struct  xkblas_device_cuda_t
{
    xkblas_device_t inherited;

    /* affinity[i] - j-th bit is set to '1' if this device has an affinity 'i' with 'j' (the lowest affinity, the better perf) */
    xkblas_device_global_id_bitfield_t * affinity;

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
        char name[64];      /* GPU name */
    } prop;

}               xkblas_device_cuda_t;

/* number of used device for this run */
static xkblas_device_cuda_t DEVICES[XKBLAS_DEVICES_MAX];

static inline xkblas_device_t *
__get_device(int device_driver_id)
{
    return (xkblas_device_t *) (DEVICES + device_driver_id);
}

static inline xkblas_device_cuda_t *
__get_device_cuda(int device_driver_id)
{
    return (xkblas_device_cuda_t *) __get_device(device_driver_id);
}

/* Convert xkblas driver device id (in [0..ngpus-1]) to the cuda driver device id (in [0, INT_MAX[) */
static int DEVICE_CUDA_ID[XKBLAS_DEVICES_MAX];

static inline void
__set_device_cuda_id(int device_driver_id, int device_cuda_id)
{
    DEVICE_CUDA_ID[device_driver_id] = device_cuda_id;
}

static inline int
__get_device_cuda_id(int device_driver_id)
{
    return DEVICE_CUDA_ID[device_driver_id];
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
        XKBLAS_FATAL("cuCheckError() error: %s (%i)", errstr, err);
        return 1;
    }
    return 0;
}

static
uint64_t cuda_get_free_mem(int device_driver_id)
{
    cudaError_t res = cudaSetDevice(__get_device_cuda_id(device_driver_id));
    __check_error(res);

    uint64_t free, total;
    res = cudaMemGetInfo(&free, &total);
    __check_error(res);

    xkblas_device_cuda_t * device = __get_device_cuda(device_driver_id);
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

/* cuda_perf_topo[i,j] returns the perf_rank of the communication link between
   device.
   cuda_perf_device[d][i] for i=0,..,cuda_count_perfrank-1 is the mask of device
   for which the device d has link with performance i.
*/

static int          cuda_device_count   = 0;
static int *        cuda_perf_topo      = nullptr;
static int          cuda_count_perfrank = 0;
static uint64_t *   cuda_perf_device    = 0;

static void
__get_gpu_topo(void)
{
    __check_error(cudaGetDeviceCount(&cuda_device_count));
    if (cuda_device_count == 0)
        return;

    int min_perf = 0;
    int max_perf = 0;

    cuda_perf_topo = (int *) malloc(sizeof(int) * cuda_device_count * cuda_device_count);
    assert(cuda_perf_topo);

    // Enumerates Device <-> Device links and store perf_rank
    for (int i = 0; i < cuda_device_count; ++i)
    {
        for (int j = 0; j < cuda_device_count; ++j)
        {
            if (i == j)
            {
                cuda_perf_topo[i*cuda_device_count+j] = 0;
            }
            else
            {
                int perf_rank = 0;
                int access_supported = 0;

                cudaError_t res = cudaDeviceGetP2PAttribute(
                                    &access_supported,
                                    cudaDevP2PAttrAccessSupported,
                                    i, j
                );
                __check_error(res);
                if (access_supported)
                {
                    res = cudaDeviceGetP2PAttribute(
                            &perf_rank,
                            cudaDevP2PAttrPerformanceRank,
                            i, j
                    );
                    __check_error(res);

                    cuda_perf_topo[i*cuda_device_count+j] = 1 + perf_rank;
                    max_perf = MAX(perf_rank, max_perf);
                    min_perf = MIN(perf_rank, min_perf);
                }
                else
                {
                    /* should be higher than previous value: computed after */
                    cuda_perf_topo[i*cuda_device_count+j] = -1;
                }
            }
        }
    }

    #pragma message(TODO "Not sure to get all the logic here")

    /* if there is no link, set to the minmum perf */
    ++min_perf;
    for (int i = 0 ; i < cuda_device_count*cuda_device_count ; ++i)
    {
        if (cuda_perf_topo[i] == -1)
            cuda_perf_topo[i] = min_perf + 1;
    }

    cuda_count_perfrank = min_perf - max_perf + 2;
    size_t size = cuda_device_count * cuda_count_perfrank * sizeof(uint64_t);
    cuda_perf_device = (uint64_t *) malloc(size);
    assert(cuda_perf_device);
    memset(cuda_perf_device, 0, size);

    for (int device_cuda_id = 0 ; device_cuda_id < cuda_device_count ; ++device_cuda_id)
    {
        for (int other_device_cuda_id = 0 ; other_device_cuda_id < cuda_device_count ; ++other_device_cuda_id)
        {
            int rank = cuda_perf_topo[device_cuda_id*cuda_device_count+other_device_cuda_id];
            assert(0 <= device_cuda_id * cuda_device_count   + rank);
            assert(     device_cuda_id * cuda_count_perfrank + rank <= cuda_device_count * cuda_count_perfrank);

            cuda_perf_device[device_cuda_id * cuda_count_perfrank + rank] |= (1UL << other_device_cuda_id);
        }
    }
}

static int
XKBLAS_DRIVER_ENTRYPOINT(init)(void)
{
    if (INITIALIZED)
        return 0;

    XKBLAS_MUTEX_LOCK(DRIVER_MUTEX);
    {
        if (INITIALIZED)
        {
            XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);
            return 0;
        }
        INITIALIZED = true;

        memset(DEVICE_CUDA_ID, -1, sizeof(DEVICE_CUDA_ID));

        for (int i = 0 ; i < XKBLAS_DEVICES_MAX ; ++i)
        {
            xkblas_device_cuda_t * device = __get_device_cuda(i);
            assert(device);
            device->inherited.state = XKBLAS_DEVICE_STATE_DEALLOCATED;
        }

        # pragma message(TODO "What is the point of 'gpuset' ? Keep it ? or rely on 'CUDA_VISIBLE_DEVICES' instead ?")
        xkblas_context_t * context = xkblas_context_get();
        uint32_t ngpus = MIN(XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)(), context->conf.ngpus);
        uint32_t gpuset = context->conf.gpu_set;
        for (int i = 0; i < ngpus ; ++i)
        {
            int idx = __builtin_ffs((unsigned int)gpuset);
            assert(idx != 0);
            --idx;
            gpuset &= ~(1<<idx);
            __set_device_cuda_id(i, idx);
        }

        hwloc_topology_init(&TOPOLOGY);
        hwloc_topology_load(TOPOLOGY);
        __get_gpu_topo();
    }
    XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);

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
XKBLAS_DRIVER_ENTRYPOINT(device_set_cpuset)(cpu_set_t * schedset, int device_driver_id)
{
    if (schedset == NULL)
        return EINVAL;

    assert(device_driver_id >= 0);
    assert(device_driver_id < XKBLAS_DEVICES_MAX);
    assert(__get_device_cuda_id(device_driver_id) != -1);

    CPU_ZERO(schedset);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    int err = hwloc_cudart_get_device_cpuset(TOPOLOGY, __get_device_cuda_id(device_driver_id), cpuset);
    if (err == 0)
    {
        err = hwloc_cpuset_to_glibc_sched_affinity(TOPOLOGY, cpuset, schedset, sizeof(cpu_set_t));
        assert(err == 0);
        if (err)
            err = ENOTSUP;
    }
    else
        XKBLAS_WARN("Could not get a 'cpuset' for CUDA device %d, falling back to glibc...", device_driver_id);

    hwloc_bitmap_free(cpuset);
    return err;
}

static xkblas_device_t *
XKBLAS_DRIVER_ENTRYPOINT(device_create)(xkblas_driver_t * driver, int device_driver_id)
{
    assert(INITIALIZED);
    assert(device_driver_id >= 0 && device_driver_id < XKBLAS_DEVICES_MAX);

    xkblas_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);
    assert(device->inherited.state == XKBLAS_DEVICE_STATE_DEALLOCATED);

    return (xkblas_device_t *) device;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(device_init)(int device_driver_id)
{
    assert(INITIALIZED);

    xkblas_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);
    assert(device->inherited.state == XKBLAS_DEVICE_STATE_CREATE);

    struct cudaDeviceProp prop;
    cudaError_t res;
    res = cudaSetDevice(__get_device_cuda_id(device_driver_id));
    __check_error(res);

    res = cudaGetDeviceProperties(&prop, __get_device_cuda_id(device_driver_id));
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

    /* work memory allocation */
    size_t free, total;
    res = cudaMemGetInfo(&free, &total);
    __check_error(res);

    /* allocate memory into an initial chunk */
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    const size_t size = (size_t) ((double)free * (double)(context->conf.gpu_mem_percent / 100.0));
    uintptr_t device_ptr;
    res = cudaMalloc((void **) &device_ptr, size);
    __check_error(res);
    xkblas_device_memory_set_chunk0(&(device->inherited), device_ptr, size);
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_destroy)(xkblas_device_t * device)
{
    device->state = XKBLAS_DEVICE_STATE_DESTROY;
    free(device);
    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_attach)(int device_driver_id)
{
    assert(INITIALIZED);

    xkblas_device_cuda_t * device = __get_device_cuda(device_driver_id);

    cudaError_t res = cudaSetDevice(__get_device_cuda_id(device_driver_id));
    __check_error(res);

    return 0;
}

/**
 * @params
 *      'dst_global_id' is where to send the data
 *      'valid'         is a bitmask of 'device_global_id' where '1' means the device holds valid data
 *  @return
 *      the source device to use for a valid transfer
 */
static xkblas_device_global_id_t
XKBLAS_DRIVER_ENTRYPOINT(get_source)(
    xkblas_device_global_id_t dst_global_id,
    xkblas_device_global_id_bitfield_t bitfield
) {
    # pragma message(TODO "Improve this heuristic, naive currently")

    assert(bitfield);

    /* retrieve dst device */
    xkblas_device_cuda_t * device = (xkblas_device_cuda_t *) xkblas_device_get(dst_global_id);
    assert(device);

    /* fast way out: good on that device already */
    if (bitfield & (1 << dst_global_id))
        return dst_global_id;

    /* lowest rank <=> best performance - find a device for P2P transfer with most perf */
    for (int rank = 0 ; rank < cuda_count_perfrank -1 ; ++rank)
    {
        /* get valid devices for this affinity */
        const xkblas_device_global_id_bitfield_t mask = bitfield & device->affinity[rank];
        if (mask == 0)
            continue ;

        /* return a random device with this affinity */
        return __random_set_bit(mask) - 1;
    }

    /* no nvlink, get any random device */
    return __random_set_bit(bitfield) - 1;
}

/* Called on all devices of the driver after they have been initialized */
static int
XKBLAS_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id)
{
# ifndef NDEBUG
    int cu_devid = -1;
    cudaGetDevice(&cu_devid);
    assert(cu_devid == __get_device_cuda_id(device_driver_id));
# endif /* NDEBUG */

    xkblas_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);
    assert(device->inherited.state == XKBLAS_DEVICE_STATE_INIT);

    int device_cuda_id = __get_device_cuda_id(device_driver_id);

    const uint64_t perfrank = cuda_count_perfrank - 1;
    const uint64_t size = sizeof(uint64_t) * perfrank;
    device->affinity = (xkblas_device_global_id_bitfield_t *) malloc(sizeof(xkblas_device_global_id_bitfield_t) * perfrank);
    memset(device->affinity, 0, size);

    /* all other devices have been initialized, enable peer */
    for (int other_device_driver_id = 0 ; other_device_driver_id < XKBLAS_DEVICES_MAX ; ++other_device_driver_id)
    {
        xkblas_device_cuda_t * other_device = __get_device_cuda(other_device_driver_id);
        assert(other_device);
        if (other_device->inherited.state < XKBLAS_DEVICE_STATE_INIT)
            continue ;

        int other_device_cuda_id = __get_device_cuda_id(other_device_driver_id);

        /* add device with itself */
        if (device_cuda_id == other_device_cuda_id)
        {
            device->affinity[0] |= (xkblas_device_global_id_bitfield_t) (1UL << other_device->inherited.global_id);
        }
        else
        {
            int access;
            cudaError_t res = cudaDeviceCanAccessPeer(&access, device_cuda_id, other_device_cuda_id);
            __check_error(res);

            if (access)
            {
                res = cudaDeviceEnablePeerAccess(other_device_cuda_id, 0);
                if ((res == cudaSuccess) || (res == cudaErrorPeerAccessAlreadyEnabled))
                {
                    int rank = cuda_perf_topo[device_cuda_id*cuda_device_count+other_device_cuda_id];
                    assert(rank);
                    if (cuda_perf_device[device_cuda_id*cuda_count_perfrank+rank] & (1UL << other_device_cuda_id))
                    {
                        device->affinity[rank - 1] |= (xkblas_device_global_id_bitfield_t) (1UL << other_device_cuda_id);
                    }
                }
                else
                {
                    XKBLAS_WARN("Could not enable peer from %d to %d", device->inherited.global_id, other_device->inherited.global_id);
                }
            }
            else
            {
                XKBLAS_WARN("GPU peer from %d to %d is not possible", device->inherited.global_id, other_device->inherited.global_id);
            }
        }
    }

    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(memory_register)(
    void * ptr,
    uint64_t size
) {
    cudaError_t err = cudaHostRegister(ptr, size, cudaHostRegisterPortable);
    __check_error(err);
    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(memory_unregister)(
    void * ptr,
    uint64_t size
) {
    (void) size;

    cudaError_t err = cudaHostUnregister(ptr);
    __check_error(err);
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
            task_launcher_t launcher = {
                .task   = op->task,
                .target = XKBLAS_DRIVER_TYPE_CUDA,
                .handle = stream->cu.blas.handle
            };
            xkblas_task_launch(&launcher);

            # pragma message(TODO "Add support for end event records")

            xkblas_stream_instruction_counter_t wp = istream->pending.pos.w % istream->pending.capacity;
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

            // assume col major for cuda - if not, need to do some shit here
            assert(instr->copy.host_view.order == MATRIX_COLMAJOR);
            size_t width  = instr->copy.host_view.m * instr->copy.host_view.sizeof_type;
            size_t height = instr->copy.host_view.n;
            assert(width >= 0);
            assert(height >= 0);

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

            XKBLAS_INFO("cudaMemcpy2DAsync(dst=%p, dpitch=%d, src=%p, spitch=%d, width=%d, height=%d, kind=%s",
                    dst, dpitch, src, spitch, width, height, (kind == cudaMemcpyDeviceToDevice) ? "D2D" : (kind == cudaMemcpyDeviceToHost) ? "D2H" : (kind == cudaMemcpyHostToDevice) ? "H2D" : "?");
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
    xkblas_stream_instruction_counter_t     size = istream->pending.pos.w - istream->pending.pos.r;
    xkblas_stream_instruction_counter_t      okp = istream->ok_p;
    xkblas_stream_instruction_counter_t prev_okp = okp - 1;

    while (okp < istream->pending.pos.w)
    {
        const xkblas_stream_instruction_counter_t idx = okp % istream->pending.capacity;
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
                for (int i = 0 ; i < 16 ; ++i)
                {
                    __check_error(cudaGetLastError());

                    res = cudaEventQuery(stream->cu.events.end[idx]);
                    assert(res == cudaErrorNotReady || res == cudaSuccess);
                    __check_error(res);

                    # pragma message(TODO "Why pthread_yield here ?")
                    if (res == cudaErrorNotReady)
                    {
                        // XKBLAS_DEBUG("Not ready, yielding");
                        pthread_yield();
                    }
                    else
                    {
                        assert(res == cudaSuccess);
                        if (prev_okp + 1 == okp)
                            ++prev_okp;
                        break ;
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

    for (xkblas_stream_instruction_counter_t p = istream->pending.pos.r ; p < istream->ok_p ; ++p)
    {
        const xkblas_stream_instruction_counter_t idx = p % istream->pending.capacity;
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
    xkblas_stream_instruction_counter_t capacity
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

static inline void
_print_mask(char * buffer, ssize_t size, uint64_t v)
{
    for (int i = 0; i < size; ++i)
        buffer[size-1-i] = (v & (1ULL<<i)) ? '1' : '0';
}


const char *
XKBLAS_DRIVER_ENTRYPOINT(device_info)(int device_driver_id)
{
    static char buffer[256];
    static char buf1[16];
    static char buf2[16];
    static char buf3[16];

    xkblas_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);

    buf1[10] = 0;
    buf2[10] = 0;
    buf3[10] = 0;
    _print_mask(buf1, 10, device->affinity[0]);
    _print_mask(buf2, 10, device->affinity[1]);
    _print_mask(buf3, 10, device->affinity[2]);
    snprintf(buffer, 256, "%s, cuda device: %i, pci: %02x:%02x, %.2f (GB), affinity: %s,%s,%s",
        device->prop.name,
        device->inherited.global_id,
        device->prop.pciBusID,
        device->prop.pciDeviceID,
        ((double)device->mem_total)/1024.0/1024.0/1024.0,
        buf1, buf2, buf3
    );
    return buffer;
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
    EP(device_info);

    EP(memory_register);
    EP(memory_unregister);

    EP(stream_create);
    EP(stream_delete);

    EP(get_source);

    # undef EP
}
