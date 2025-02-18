/* ************************************************************************** */
/*                                                                            */
/*   driver_cuda.cc                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/18 20:57:58 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_CUDA_ ## N

# include <xkrt/runtime.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/driver-cuda.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-cublas.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>

# include <cuda_runtime.h>
# include <cublas_v2.h>         // TODO : should cublas be part of xkrt's driver ? or as part of xkblas ?
# include <hwloc.h>
# include <hwloc/cuda.h>
# include <hwloc/cudart.h>
# include <hwloc/glibc-sched.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>

typedef struct  xkrt_device_cuda_t
{
    xkrt_device_t inherited;

    /* affinity[i] - j-th bit is set to '1' if this device has an affinity 'i' with 'j' (the lowest affinity, the better perf) */
    xkrt_device_global_id_bitfield_t * affinity;

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

}               xkrt_device_cuda_t;

/* number of used device for this run */
static xkrt_device_cuda_t DEVICES[XKRT_DEVICES_MAX];

static inline xkrt_device_t *
__get_device(int device_driver_id)
{
    return (xkrt_device_t *) (DEVICES + device_driver_id);
}

static inline xkrt_device_cuda_t *
__get_device_cuda(int device_driver_id)
{
    return (xkrt_device_cuda_t *) __get_device(device_driver_id);
}

static
uint64_t cuda_get_free_mem(int device_driver_id)
{
    CU_SAFE_CALL(cudaSetDevice(device_driver_id));

    uint64_t free, total;
    CU_SAFE_CALL(cudaMemGetInfo(&free, &total));

    xkrt_device_cuda_t * device = __get_device_cuda(device_driver_id);
    device->free_mem = (size_t)free;
    return device->free_mem;
}

static unsigned int
XKRT_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    int device_count = 0;
    CU_SAFE_CALL(cudaGetDeviceCount(&device_count));
    return (unsigned int)device_count;
}

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
    CU_SAFE_CALL(cudaGetDeviceCount(&cuda_device_count));
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

                CU_SAFE_CALL(
                    cudaDeviceGetP2PAttribute(
                        &access_supported,
                        cudaDevP2PAttrAccessSupported,
                        i, j
                    )
                );

                if (access_supported)
                {
                    CU_SAFE_CALL(
                        cudaDeviceGetP2PAttribute(
                            &perf_rank,
                            cudaDevP2PAttrPerformanceRank,
                            i, j
                        )
                    );

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
XKRT_DRIVER_ENTRYPOINT(init)(unsigned int ngpus)
{
    int ndevices_max;
    cudaError_t err = cudaGetDeviceCount(&ndevices_max);
    if (err)
        return 1;

    assert(ngpus < XKRT_DEVICES_MAX);
    for (int i = 0 ; i < ngpus ; ++i)
    {
        xkrt_device_cuda_t * device = __get_device_cuda(i);
        assert(device);
        device->inherited.state = XKRT_DEVICE_STATE_DEALLOCATED;
    }

    __get_gpu_topo();

    return 0;
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{
}

static const char *
XKRT_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "CUDA";
}

static int
XKRT_DRIVER_ENTRYPOINT(device_set_cpuset)(hwloc_topology_t topology, cpu_set_t * schedset, int device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < XKRT_DEVICES_MAX);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    HWLOC_SAFE_CALL(hwloc_cudart_get_device_cpuset(topology, device_driver_id, cpuset));

    CPU_ZERO(schedset);
    HWLOC_SAFE_CALL(hwloc_cpuset_to_glibc_sched_affinity(topology, cpuset, schedset, sizeof(cpu_set_t)));

    hwloc_bitmap_free(cpuset);

    return 0;
}

static xkrt_device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(xkrt_driver_t * driver, int device_driver_id)
{
    assert(device_driver_id >= 0 && device_driver_id < XKRT_DEVICES_MAX);

    xkrt_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);
    assert(device->inherited.state == XKRT_DEVICE_STATE_DEALLOCATED);

    return (xkrt_device_t *) device;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_init)(int device_driver_id)
{
    xkrt_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);
    assert(device->inherited.state == XKRT_DEVICE_STATE_CREATE);

    struct cudaDeviceProp prop;
    CU_SAFE_CALL(cudaSetDevice(device_driver_id));
    CU_SAFE_CALL(cudaGetDeviceProperties(&prop, device_driver_id));

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
}

static void *
XKRT_DRIVER_ENTRYPOINT(memory_alloc)(int device_driver_id, const size_t size)
{
    (void) device_driver_id;

    /* allocate memory into an initial chunk */
    void * device_ptr;
    CU_SAFE_CALL(cudaMalloc((void **) &device_ptr, size));
    return device_ptr;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_info)(int device_driver_id, size_t * total)
{
    (void) device_driver_id;
    size_t free;
    CU_SAFE_CALL(cudaMemGetInfo(&free, total));
}

static int
XKRT_DRIVER_ENTRYPOINT(device_destroy)(xkrt_device_t * device)
{
    free(device);
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_attach)(int device_driver_id)
{
    xkrt_device_cuda_t * device = __get_device_cuda(device_driver_id);
    CU_SAFE_CALL(cudaSetDevice(device_driver_id));
    return 0;
}

# if 0

/**
 * @params
 *      'dst_global_id' is where to send the data
 *      'valid'         is a bitmask of 'device_global_id' where '1' means the device holds valid data
 *  @return
 *      the source device to use for a valid transfer
 */
static xkrt_device_global_id_t
XKRT_DRIVER_ENTRYPOINT(get_source)(
    xkrt_device_global_id_t dst_global_id,
    xkrt_device_global_id_bitfield_t bitfield
) {
    # pragma message(TODO "Improve this heuristic, naive currently")

    assert(bitfield);

    /* retrieve dst device */
    xkrt_device_cuda_t * device = (xkrt_device_cuda_t *) xkrt_device_get(dst_global_id);
    assert(device);

    /* fast way out: good on that device already */
    if (bitfield & (1 << dst_global_id))
        return dst_global_id;

    /* lowest rank <=> best performance - find a device for P2P transfer with most perf */
    for (int rank = 0 ; rank < cuda_count_perfrank -1 ; ++rank)
    {
        /* get valid devices for this affinity */
        const xkrt_device_global_id_bitfield_t mask = bitfield & device->affinity[rank];
        if (mask == 0)
            continue ;

        /* return a random device with this affinity */
        return (xkrt_device_global_id_t) (__random_set_bit(mask) - 1);
    }

    /* no nvlink, get any random device */
    return (xkrt_device_global_id_t) (__random_set_bit(bitfield) - 1);
}

# endif

/* Called for each device of the driver once they all have been initialized */
static int
XKRT_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id)
{
# ifndef NDEBUG
    int cu_devid = -1;
    cudaGetDevice(&cu_devid);
    assert(cu_devid == device_driver_id);
# endif /* NDEBUG */

    xkrt_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);
    assert(device->inherited.state == XKRT_DEVICE_STATE_INIT);

    const uint64_t size = sizeof(xkrt_device_global_id_bitfield_t) * cuda_count_perfrank;
    device->affinity = (xkrt_device_global_id_bitfield_t *) malloc(size);
    memset(device->affinity, 0, size);

    /* all other devices have been initialized, enable peer */
    for (int other_device_driver_id = 0 ; other_device_driver_id < XKRT_DEVICES_MAX ; ++other_device_driver_id)
    {
        xkrt_device_cuda_t * other_device = __get_device_cuda(other_device_driver_id);
        assert(other_device);
        if (other_device->inherited.state < XKRT_DEVICE_STATE_INIT)
            continue ;

        /* add device with itself */
        if (device_driver_id == other_device_driver_id)
        {
            device->affinity[0] |= (xkrt_device_global_id_bitfield_t) (1UL << other_device->inherited.global_id);
        }
        else
        {
            int access;
            CU_SAFE_CALL(cudaDeviceCanAccessPeer(&access, device_driver_id, other_device_driver_id));

            if (access)
            {
                cudaError_t res = cudaDeviceEnablePeerAccess(other_device_driver_id, 0);
                if ((res == cudaSuccess) || (res == cudaErrorPeerAccessAlreadyEnabled))
                {
                    int rank = cuda_perf_topo[device_driver_id*cuda_device_count+other_device_driver_id];
                    assert(rank);
                    if (cuda_perf_device[device_driver_id*cuda_count_perfrank+rank] & (1UL << other_device_driver_id))
                    {
                        device->affinity[rank - 1] |= (xkrt_device_global_id_bitfield_t) (1UL << other_device_driver_id);
                    }
                }
                else
                {
                    LOGGER_WARN("Could not enable peer from %d to %d", device->inherited.global_id, other_device->inherited.global_id);
                }
            }
            else
            {
                LOGGER_WARN("GPU peer from %d to %d is not possible", device->inherited.global_id, other_device->inherited.global_id);
            }
        }
    }

    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(memory_register)(
    void * ptr,
    uint64_t size
) {
    int err = cudaHostRegister(ptr, size, cudaHostRegisterPortable);
    return err;
}

static int
XKRT_DRIVER_ENTRYPOINT(memory_unregister)(
    void * ptr,
    uint64_t size
) {
    (void) size;
    int err = cudaHostUnregister(ptr);
    return err;
}

static int
cuda_stream_instructions_launch(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    xkrt_stream_cuda_t * stream = (xkrt_stream_cuda_t *) istream;
    assert(stream);

    cudaEvent_t event = stream->cu.events.buffer[idx];

    // get transfer type
    cudaMemcpyKind kind;
    cudaStream_t handle;

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
        {
            kind = cudaMemcpyHostToDevice;
            handle = stream->cu.handle.high;
            break ;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
        {
            kind = cudaMemcpyDeviceToHost;
            handle = stream->cu.handle.high;
            break ;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
        {
            kind = cudaMemcpyDeviceToDevice;
            handle = stream->cu.handle.low;
            break ;
        }

        default:
        {
            LOGGER_FATAL("instr->type invalid");
            break ;
        }
    }

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
        {
                  void * dst    = (      void *) instr->copy.D1.dst_device_addr;
            const void * src    = (const void *) instr->copy.D1.src_device_addr;
            const size_t count  = instr->copy.D1.size;
            assert(count > 0);

            LOGGER_DEBUG("cudaMemcpyAsync(dst=%p, src=%p, count=%zu, kind=%s",
                    dst, src, count, (kind == cudaMemcpyDeviceToDevice) ? "D2D" : (kind == cudaMemcpyDeviceToHost) ? "D2H" : (kind == cudaMemcpyHostToDevice) ? "H2D" : "?");
            CU_SAFE_CALL(cudaMemcpyAsync(dst, src, count, kind, handle));
            CU_SAFE_CALL(cudaEventRecord(event, handle));

            return EINPROGRESS;
        }
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
        {
                  void * dst = (      void *) instr->copy.D2.dst_device_view.addr;
            const void * src = (const void *) instr->copy.D2.src_device_view.addr;

            size_t dpitch = instr->copy.D2.dst_device_view.ld * instr->copy.D2.sizeof_type;
            size_t spitch = instr->copy.D2.src_device_view.ld * instr->copy.D2.sizeof_type;

            // assume col major for cuda - if not, need to do some shit here
            size_t width  = instr->copy.D2.m * instr->copy.D2.sizeof_type;
            size_t height = instr->copy.D2.n;
            assert(width >= 0);
            assert(height >= 0);

            LOGGER_DEBUG("cudaMemcpy2DAsync(dst=%p, dpitch=%zu, src=%p, spitch=%zu, width=%zu, height=%zu, kind=%s",
                    dst, dpitch, src, spitch, width, height, (kind == cudaMemcpyDeviceToDevice) ? "D2D" : (kind == cudaMemcpyDeviceToHost) ? "D2H" : (kind == cudaMemcpyHostToDevice) ? "H2D" : "?");
            CU_SAFE_CALL(cudaMemcpy2DAsync(dst, dpitch, src, spitch, width, height, kind, handle));
            CU_SAFE_CALL(cudaEventRecord(event, handle));

            return EINPROGRESS;
        }

        default:
            return EINVAL;
    }

    /* unreachable code */
    LOGGER_FATAL("Unreachable code");
}

static inline int
cuda_stream_instructions_wait(
    xkrt_stream_t * istream
) {
    xkrt_stream_cuda_t * stream = (xkrt_stream_cuda_t *) istream;
    assert(stream);

    CU_SAFE_CALL(cudaStreamSynchronize(stream->cu.handle.high));
    CU_SAFE_CALL(cudaStreamSynchronize(stream->cu.handle.low));

    return 0;
}

static inline int
cuda_stream_instructions_progress(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    xkrt_stream_cuda_t * stream = (xkrt_stream_cuda_t *) istream;
    assert(stream);

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_KERN):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
        {
            /* poll events */
            for (int i = 0 ; i < 16 ; ++i)
            {
                CU_SAFE_CALL(cudaGetLastError());
                cudaError_t res = cudaEventQuery(stream->cu.events.buffer[idx]);
                if (res == cudaErrorNotReady)
                    sched_yield();
                else if (res == cudaSuccess)
                    return 0;
                else
                    LOGGER_FATAL("Error querying event");
            }
            break ;
        }

        default:
            LOGGER_FATAL("Wrong instruction");
    }

    return EINPROGRESS;
}

static xkrt_stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    xkrt_device_t * device,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity
) {
    assert(device);

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkrt_stream_cuda_t) + capacity * sizeof(cudaEvent_t));
    assert(mem);

    xkrt_stream_cuda_t * stream = (xkrt_stream_cuda_t *) mem;

    /*************************/
    /* init xkrt stream */
    /*************************/
    xkrt_stream_init(
        (xkrt_stream_t *) stream,
        type,
        capacity,
        cuda_stream_instructions_launch,
        cuda_stream_instructions_progress,
        cuda_stream_instructions_wait
    );

    /*************************/
    /* do cuda specific init */
    /*************************/

    /* events */
    stream->cu.events.buffer = (cudaEvent_t *) (stream + 1);
    stream->cu.events.capacity = capacity;

    cudaError_t err;
    for (int i = 0 ; i < capacity ; ++i)
        CU_SAFE_CALL(cudaEventCreateWithFlags(stream->cu.events.buffer + i, cudaEventDisableTiming));

    /* streams */
    int leastPriority, greatestPriority;
    CU_SAFE_CALL(cudaDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority));
    CU_SAFE_CALL(cudaStreamCreateWithPriority(&stream->cu.handle.high, cudaStreamNonBlocking, greatestPriority));
    CU_SAFE_CALL(cudaStreamCreateWithPriority(&stream->cu.handle.low, cudaStreamNonBlocking, leastPriority));

    if (type == XKRT_STREAM_TYPE_KERN)
    {
        CUBLAS_SAFE_CALL(cublasCreate(&stream->cu.blas.handle));
        CUBLAS_SAFE_CALL(cublasSetStream(stream->cu.blas.handle, stream->cu.handle.high));
    }
    else
    {
        stream->cu.blas.handle = 0;
    }

    return (xkrt_stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    xkrt_stream_cuda_t * stream = (xkrt_stream_cuda_t *) istream;
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
XKRT_DRIVER_ENTRYPOINT(device_info)(int device_driver_id)
{
    static char buffer[512];

    xkrt_device_cuda_t * device = __get_device_cuda(device_driver_id);
    assert(device);

    snprintf(buffer, 256, "%s, cuda device: %i, pci: %02x:%02x, %.2f (GB)",
        device->prop.name,
        device->inherited.global_id,
        device->prop.pciBusID,
        device->prop.pciDeviceID,
        ((double)device->mem_total)/1e9
    );
    return buffer;
}

void
XKRT_DRIVER_ENTRYPOINT(get_driver)(xkrt_driver_t * driver)
{
    # define EP(func) driver->f_##func = XKRT_DRIVER_ENTRYPOINT(func)

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

    EP(memory_alloc);
    EP(memory_info);
    EP(memory_register);
    EP(memory_unregister);

    EP(stream_create);
    EP(stream_delete);

    # undef EP
}
