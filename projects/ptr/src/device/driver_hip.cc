/* ************************************************************************** */
/*                                                                            */
/*   driver_hip.cc                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:45 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# define XKBLAS_DRIVER_ENTRYPOINT(N) XKBLAS_DRIVER_TYPE_CUDA_ ## N

# include "xkblas-context.h"
# include "conf/conf.h"
# include "device/cublas-helper.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/stream.h"
# include "logger/logger.h"
# include "sync/mutex.h"

# include <hip/hip_runtime.h>
# include <hipblas.h>
# include <hwloc.h>
# include <hwloc/hip/hip_runtime.h>
# include <hwloc/glibc-sched.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>

typedef struct  xkblas_stream_hip_t
{
    xkblas_stream_t super;

    struct {

        struct {
            hipStream_t high;
            hipStream_t low;
        } handle;

        struct {
            hipEvent_t * end;
            uint16_t capacity;
        } events;

        struct {
            hipblasHandle_t handle;
        } blas;

    } hip;
}               xkblas_stream_hip_t;

typedef struct  xkblas_device_hip_t
{
    xkblas_device_t inherited;
    int save_device_id;

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

}               xkblas_device_hip_t;

/* number of used device for this run */
static xkblas_device_hip_t DEVICES[XKBLAS_DEVICES_MAX];

static inline xkblas_device_t *
__get_device(int device_id)
{
    return (xkblas_device_t *) (DEVICES + device_id);
}

static inline xkblas_device_hip_t *
__get_device_hip(int device_id)
{
    return (xkblas_device_hip_t *) __get_device(device_id);
}

/* Convert xkblas driver device id (in [0..ngpus-1]) to the hip driver device id (in [0, INT_MAX[) */
static int CUDA_DEVICE_ID[XKBLAS_DEVICES_MAX];

static inline void
__set_device_hip_id(int device_id, int hip_device_id)
{
    CUDA_DEVICE_ID[device_id] = hip_device_id;
    // XKBLAS_DEBUG("driver device id = %d ; hip device id = %d", device_id, hip_device_id);
}

static inline int
__get_device_hip_id(int device_id)
{
    return CUDA_DEVICE_ID[device_id];
}

/* initialization synchronization */
static bool INITIALIZED = false;
static xkblas_mutex_t DRIVER_MUTEX = XKBLAS_MUTEX_INITIALIZER;

/* hwloc topology */
static hwloc_topology_t TOPOLOGY;

static int
__check_error(hipError_t err)
{
    if (err != hipSuccess && err != hipErrorNotReady)
    {
        const char * errstr = hipGetErrorName(err);
        XKBLAS_FATAL("cuCheckError() error: %s (%i)", errstr, err);
        return 1;
    }
    return 0;
}

static
uint64_t hip_get_free_mem(int device_id)
{
    hipError_t res = hipSetDevice(__get_device_hip_id(device_id));
    __check_error(res);

    uint64_t free, total;
    res = hipMemGetInfo(&free, &total);
    __check_error(res);

    xkblas_device_hip_t * device = __get_device_hip(device_id);
    device->free_mem = (size_t)free;
    return device->free_mem;
}

static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    int device_count = 0;
    __check_error(hipGetDeviceCount(&device_count));
    return (unsigned int)device_count;
}

// NVLINK TOPOLOGY

/* hip_perf_topo[i,j] returns the perfRank of the communication link between
   device.
   hip_perf_device[d][i] for i=0,..,hip_count_perfrank-1 is the mask of device
   for which the device d has link with performance i.
*/

static int hip_device_count;
static int hip_perf_topo[XKBLAS_DEVICES_MAX][XKBLAS_DEVICES_MAX];
static int hip_count_perfrank;
static uint64_t * hip_perf_device;
static int* hip_routing_table;

static void
__get_gpu_topo(void)
{
    __check_error(hipGetDeviceCount(&hip_device_count));
    if (hip_device_count == 0)
        return;

    int min_perf = 0;
    int max_perf = 0;

    // Enumerates Device <-> Device links and store perfRank
    for (int i = 0; i < hip_device_count; i++)
    {
        for (int j = 0; j < hip_device_count; j++)
        {
            if (i == j)
            {
                hip_perf_topo[i][j] = 0;
                continue ;
            }
            else
            {
                int perfRank = 0;
                int accessSupported = 0;

                hipError_t res = hipDeviceGetP2PAttribute(
                                    &accessSupported,
                                    hipDevP2PAttrAccessSupported,
                                    i,
                                    j
                );
                __check_error(res);
                if (accessSupported)
                {
                    res = hipDeviceGetP2PAttribute(
                            &perfRank,
                            hipDevP2PAttrPerformanceRank,
                            i,
                            j
                    );
                    __check_error(res);
                    hip_perf_topo[i][j] = 1 + perfRank;

                    if (perfRank < max_perf)
                        max_perf = perfRank;
                    if (perfRank > min_perf)
                        min_perf = perfRank;
                }
                else
                {
                    /* should be higher than previous value: computed after */
                    hip_perf_topo[i][j] = -1;
                }
            }
        }
    }

    #pragma message(TODO "Not sure to get all the logic here")

    /* if there is no link, set to the minmum perf */
    ++min_perf;
    for (int i = 0 ; i < hip_device_count ; ++i)
    {
        for (int j = 0 ; j < hip_device_count ; ++j)
        {
            if (hip_perf_topo[i][j] == -1)
            {
                hip_perf_topo[i][j] = min_perf + 1;    // TODO : this +1 is suspicious to me
                XKBLAS_INFO("Cuda GPU : no peer access from %d to %d", i, j);
            }
        }
    }

    hip_count_perfrank = min_perf - max_perf + 2;
    size_t size = hip_device_count * hip_count_perfrank * sizeof(uint64_t);
    hip_perf_device = (uint64_t *) malloc( size );
    assert(hip_perf_device);
    memset(hip_perf_device, 0, size);

    for (int i = 0 ; i < hip_device_count ; ++i)
    {
        for (int j = 0 ; j < hip_device_count ; ++j)
        {
            int rank = hip_perf_topo[i][j];
            assert(0 <= i * hip_device_count + rank );
            assert(i * hip_count_perfrank + rank <= hip_device_count * hip_count_perfrank);
            hip_perf_device[i * hip_count_perfrank + rank] |= (1UL << j);
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
            __set_device_hip_id(i, idx);
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
        xkblas_hip_init_reqreg_list(&all_rrl[i]);
        int err = pthread_create(&all_rrl[i].thread, 0, xkblas_hip_register_thread, (void*)(uintptr_t)i );
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
    assert(__get_device_hip_id(device_id) != -1);

    CPU_ZERO(schedset);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    int err = hwloc_hiprt_get_device_cpuset(TOPOLOGY, __get_device_hip_id(device_id), cpuset);
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

    xkblas_device_hip_t * device = DEVICES + device_id;
    device->save_device_id = -1;

    return (xkblas_device_t *) device;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(device_init)(int device_id)
{
    assert(INITIALIZED);

    xkblas_device_hip_t * device = __get_device_hip(device_id);
    # pragma message(TODO "Check device lifecycle: must be created here")

    struct hipDeviceProp_t prop;
    hipError_t res;
    res = hipSetDevice(__get_device_hip_id(device_id));
    __check_error(res);

    res = hipGetDeviceProperties(&prop, __get_device_hip_id(device_id));
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

    /* work memory allocation */
    size_t free, total;
    res = hipMemGetInfo(&free, &total);
    __check_error(res);

    /* allocate 90% of free memory, into a new chunk */
    const size_t size = (size_t) ((double)free * 0.9);
    // const size_t size = (size_t) ((double)free * 0.1);
    // const size_t size = (size_t) ((double)free * 0.01);
    uintptr_t device_ptr;
    res = hipMalloc((void **) &device_ptr, size);
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
XKBLAS_DRIVER_ENTRYPOINT(device_attach)(int device_id)
{
    assert(INITIALIZED);

    xkblas_device_hip_t * device = __get_device_hip(device_id);
    assert(device->save_device_id == -1);

    hipError_t res;
    res = hipGetDevice(&device->save_device_id);
    __check_error(res);

    res = hipSetDevice(__get_device_hip_id(device_id));
    __check_error(res);

    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(get_source)(int dst_global_id, int bitmask)
{
    # pragma message(TODO "Converting device 'global' to 'driver' id here - maybe create a mapping at initialization, instead of doing it everytime")

    // retrieve the 'driver' id of the 'dst_global_id' passed
    int dst_driver_id = -1;
    for( int driver_dev_id = 0; driver_dev_id < XKBLAS_DEVICES_MAX; driver_dev_id++ )
    {
         xkblas_device_hip_t* pdev = DEVICES + driver_dev_id;
         if( pdev->inherited.state == XKBLAS_DEVICE_STATE_CREATE )
             continue;
         if( pdev->inherited.global_id == dst_global_id )
         {
             dst_driver_id = pdev->inherited.driver_id;
             break;
         }
    }
    if( dst_driver_id == -1 )
    { // Can't found dst in the driver devices...
        return -1;
    }


    # pragma message(TODO "This loop implementation is O(n) with 'n' the number of devices - it can be O(m) with 'm' the number of distinct performances using the 'hip_perf_device' array - maybe reimplement it using 'hip_perf_device'")
    // get the valid device with the best connectivity
    int src = -1;
    int src_rank = INT_MAX;
    for( int driver_dev_id = 0; driver_dev_id < XKBLAS_DEVICES_MAX; driver_dev_id++ )
    {
         xkblas_device_hip_t* pdev = DEVICES + driver_dev_id;

         // 1 - check if the device exist/is initialized ?
         if( !(pdev->inherited.state == XKBLAS_DEVICE_STATE_COMMIT || pdev->inherited.state == XKBLAS_DEVICE_STATE_RUNNING) )
             continue; // Device is not is a valid state

         // 2 - check if the data is valid here
         if( ((1 << pdev->inherited.global_id) & bitmask) == 0 )
             continue; // Data is not valid on this device

         // 3 - check the rank
         int rank = hip_perf_topo[dst_driver_id][driver_dev_id];
         if( rank < src_rank )
         {
             src = pdev->inherited.global_id;
             src_rank = rank;
         }
    }
    return src_rank;
}

/* Called on all devices of the driver after they have been initialized */
static int
XKBLAS_DRIVER_ENTRYPOINT(device_commit)(int device_id)
{
# ifndef NDEBUG
    int cu_devid = -1;
    hipGetDevice(&cu_devid);
    assert(cu_devid == __get_device_hip_id(device_id));
# endif /* NDEBUG */

    xkblas_device_hip_t * device = __get_device_hip(device_id);
    assert(device);

    const uint64_t perfrank = hip_count_perfrank - 1;
    const uint64_t size = sizeof(uint64_t) * perfrank;
    device->affinity = (uint64_t *) malloc(sizeof(uint64_t) * perfrank);
    memset(device->affinity, 0, size);

    /* all other devices have been initialized, enable peer */
    int i = __get_device_hip_id(device_id);
    for (int dev = 0 ; dev < XKBLAS_DEVICES_MAX ; ++dev)
    {
        int j = __get_device_hip_id(dev);

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
            xkblas_device_hip_t * odevice = __get_device_hip(j);
            if (odevice->inherited.state != XKBLAS_DEVICE_STATE_INIT)
                continue ;

            int access;
            hipError_t res;
            res = hipDeviceCanAccessPeer(&access, i, j);
            __check_error(res);

            if (access)
            {
                res = hipDeviceEnablePeerAccess(j, 0);
                if ((res == hipSuccess) || (res == hipErrorPeerAccessAlreadyEnabled))
                {
                    int rank = hip_perf_topo[i][j];
                    assert(rank);
                    if (hip_perf_device[i * hip_count_perfrank + rank] & (1 << j))
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
XKBLAS_DRIVER_ENTRYPOINT(memory_register)(
    void * ptr,
    uint64_t size
) {
    hipError_t err = hipHostRegister(ptr, size, hipHostRegisterPortable);
    __check_error(err);
    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(memory_unregister)(
    void * ptr,
    uint64_t size
) {
    (void) size;

    hipError_t err = hipHostUnregister(ptr);
    __check_error(err);
    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkblas_stream_t * istream,
    xkblas_stream_instruction_t * instr
) {
    xkblas_stream_hip_t * stream = (xkblas_stream_hip_t *) istream;
    assert(stream);

    switch (instr->type)
    {
        case (XKBLAS_STREAM_INSTR_TYPE_NOP):
        {
            return 0;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_BARRIER):
        {
            hipError_t res;
            res = hipStreamSynchronize(stream->hip.handle.high);
            assert(res == hipSuccess);
            res = hipStreamSynchronize(stream->hip.handle.low);
            assert(res == hipSuccess);
            ++istream->ok_p;
            return 0;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_KERN):
        {
            assert(istream->type == XKBLAS_STREAM_TYPE_KERN);
            assert(stream->hip.handle.high);
            assert(stream->hip.blas.handle);

            xkblas_stream_instruction_kernel_t * op = &instr->kern;
            task_launcher_t launcher = {
                .task   = op->task,
                .target = XKBLAS_DRIVER_TYPE_CUDA,
                .handle = stream->hip.blas.handle
            };
            xkblas_task_launch(&launcher);

            # pragma message(TODO "Add support for end event records")

            uint32_t wp = istream->pending.pos.w % istream->pending.capacity;
            assert(stream->hip.events.capacity == istream->pending.capacity);
            hipError_t err = hipEventRecord(stream->hip.events.end[wp], stream->hip.handle.high);
            assert(err == hipSuccess);

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

            // assume col major for hip - if not, need to do some shit here
            assert(instr->copy.host_view.order == MATRIX_COLMAJOR);
            size_t width  = instr->copy.host_view.m * instr->copy.host_view.sizeof_type;
            size_t height = instr->copy.host_view.n;
            assert(width >= 0);
            assert(height >= 0);

            hipMemcpyKind kind;
            hipStream_t handle;

            switch (instr->type)
            {
                case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
                {
                    kind = hipMemcpyHostToDevice;
                    handle = stream->hip.handle.high;
                    break ;
                }

                case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
                {
                    kind = hipMemcpyDeviceToHost;
                    handle = stream->hip.handle.high;
                    break ;
                }

                case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
                {
                    kind = hipMemcpyDeviceToDevice;
                    handle = stream->hip.handle.low;
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

            XKBLAS_INFO("hipMemcpy2DAsync(dst=%p, dpitch=%d, src=%p, spitch=%d, width=%d, height=%d, kind=%s",
                    dst, dpitch, src, spitch, width, height, (kind == hipMemcpyDeviceToDevice) ? "D2D" : (kind == hipMemcpyDeviceToHost) ? "D2H" : (kind == hipMemcpyHostToDevice) ? "H2D" : "?");
            hipError_t err = hipMemcpy2DAsync(dst, dpitch, src, spitch, width, height, kind, handle);
            __check_error(err);
            err = hipEventRecord(stream->hip.events.end[istream->pending.pos.w % istream->pending.capacity], handle);
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
hip_stream_instructions_progress(
    xkblas_stream_t * istream,
    int blocking
) {
    assert(istream);

    xkblas_stream_hip_t * stream = (xkblas_stream_hip_t *) istream;

    /* no pending instructions */
    if (istream->pending.pos.r == istream->pending.pos.w)
        return 0;

    if (blocking)
    {
        hipError_t err;
        err = hipStreamSynchronize(stream->hip.handle.high);
        __check_error(err);
        err = hipStreamSynchronize(stream->hip.handle.low);
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
        uint16_t idx = (uint16_t) (okp % istream->pending.capacity);
        xkblas_stream_instruction_t * instr = istream->pending.instr + idx;
        hipError_t res = hipSuccess;

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
                    __check_error(hipGetLastError());

                    res = hipEventQuery(stream->hip.events.end[idx]);
                    assert(res == hipErrorNotReady || res == hipSuccess);
                    __check_error(res);

                    # pragma message(TODO "Why pthread_yield here ?")
                    if (res == hipErrorNotReady)
                    {
                        // XKBLAS_DEBUG("Not ready, yielding");
                        pthread_yield();
                    }
                    else
                    {
                        assert(res == hipSuccess);
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
    int err = hip_stream_instructions_progress(istream, blocking);
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
    uint16_t capacity
) {
    hipError_t res;
    assert(INITIALIZED == true);

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkblas_stream_hip_t) + capacity * sizeof(hipEvent_t));
    assert(mem);

    xkblas_stream_hip_t * stream = (xkblas_stream_hip_t *) mem;

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
    /* do hip specific init */
    /*************************/

    /* events */
    stream->hip.events.end = (hipEvent_t *) (mem + sizeof(xkblas_stream_hip_t));
    stream->hip.events.capacity = capacity;

    hipError_t err;
    for (int i = 0 ; i < capacity ; ++i)
    {
        err = hipEventCreateWithFlags(stream->hip.events.end + i, hipEventDisableTiming);
        __check_error(err);
    }

    /* streams */
    int leastPriority, greatestPriority;
    err = hipDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);
    __check_error(err);

    err = hipStreamCreateWithPriority(&stream->hip.handle.high, hipStreamNonBlocking, greatestPriority);
    __check_error(err);

    err = hipStreamCreateWithPriority(&stream->hip.handle.low, hipStreamNonBlocking, leastPriority);
    __check_error(err);

    if (type == XKBLAS_STREAM_TYPE_KERN)
    {
        hipblasStatus_t cres = hipblasCreate(&stream->hip.blas.handle);
        xkblas_cublas_status_check(cres);
        assert(cres == HIPBLAS_STATUS_SUCCESS);

        cres = hipblasSetStream(stream->hip.blas.handle, stream->hip.handle.high);
        xkblas_cublas_status_check(cres);
        assert(cres == HIPBLAS_STATUS_SUCCESS);
    }
    else
    {
        stream->hip.blas.handle = 0;
    }

    return (xkblas_stream_t *) stream;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(stream_delete)(
    xkblas_stream_t * istream
) {
    xkblas_stream_hip_t * stream = (xkblas_stream_hip_t *) istream;
    if (stream->hip.blas.handle)
        hipblasDestroy(stream->hip.blas.handle);
    hipStreamDestroy(stream->hip.handle.high);
    hipStreamDestroy(stream->hip.handle.low);
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

    EP(memory_register);
    EP(memory_unregister);

    EP(stream_create);
    EP(stream_delete);

    EP(get_source);

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
