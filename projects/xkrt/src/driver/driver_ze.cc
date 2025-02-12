/* ************************************************************************** */
/*                                                                            */
/*   driver_level_zero.cc                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:57:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

/* see https://oneapi-src.github.io/level-zero-spec/level-zero/1.12.15/ */

# define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_ZE_ ## N

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/driver/cublas-helper.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/logger.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>

# include <ze_api.h>
# include <hwloc/levelzero.h>
# include <hwloc/glibc-sched.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>

// TODO : can be make this member of a 'xkrt_driver_ze_t' ?  most likely yes,
// but cuda state machine would make it hard to maintain for cuda as well. Keep
// them as global variable for now, there should only be 1 instances of a
// driver right now

static ze_driver_handle_t   ze_drivers[XKRT_DEVICES_MAX];
static ze_context_handle_t  ze_contextes[XKRT_DEVICES_MAX];

typedef struct  xkrt_device_ze_t
{
    xkrt_device_t inherited;

    ze_driver_handle_t      ze_driver;
    ze_context_handle_t     ze_context;
    ze_device_handle_t      ze_device;
    ze_device_properties_t  ze_properties;

}               xkrt_device_ze_t;

static xkrt_device_ze_t DEVICES[XKRT_DEVICES_MAX];
static uint32_t ze_n_devices = 0;

static xkrt_device_ze_t *
device_ze_get(int device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < ze_n_devices);
    return DEVICES + device_driver_id;
}

static int
XKRT_DRIVER_ENTRYPOINT(init)(void)
{
    // zeInit got deprecated, use other ifdef depending on version but cba
    // intel mess, so only deprecated is implemented atm
    # if 1

    # pragma message(TODO "We initialize all Intel drivers and devices here. Maybe make this a bit more lazy")

    const ze_init_flags_t flags = ZE_INIT_FLAG_GPU_ONLY;
    const ze_result_t r = zeInit(flags);

    if (r != ZE_RESULT_SUCCESS)
        return 1;

    // get all drivers
    uint32_t ze_n_drivers = XKRT_DEVICES_MAX; // i believe Intel API ndriver <= ndevices ?
    ZE_SAFE_CALL(zeDriverGet(&ze_n_drivers, ze_drivers));

    // get all device handles per driver
    for (unsigned int ze_driver_id = 0 ; ze_driver_id < ze_n_drivers && ze_n_devices < XKRT_DEVICES_MAX ; ++ze_driver_id)
    {
        // Create context for driver
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ZE_SAFE_CALL(zeContextCreate(ze_drivers[ze_driver_id], &desc, ze_contextes + ze_driver_id));

        // get devices handles
        uint32_t ndevices = XKRT_DEVICES_MAX - ze_n_devices;
        ze_device_handle_t ze_devices[XKRT_DEVICES_MAX];
        ZE_SAFE_CALL(zeDeviceGet(ze_drivers[ze_driver_id], &ndevices, ze_devices));
        assert(ze_n_devices + ndevices <= XKRT_DEVICES_MAX);

        // ensure we do not overflow if assertions are off
        if (ze_n_devices + ndevices > XKRT_DEVICES_MAX)
            ndevices = XKRT_DEVICES_MAX - ze_n_devices;

        // setup xkrt devices
        for (unsigned int ze_driver_device_id = 0 ; ze_driver_device_id < ndevices ; ++ze_driver_device_id)
        {
            unsigned int device_driver_id = ze_n_devices + ze_driver_device_id;

            xkrt_device_ze_t * device = DEVICES + device_driver_id;
            device->ze_driver = ze_drivers[ze_driver_id];
            device->ze_device = ze_devices[ze_driver_device_id];

            // get device properties
            device->ze_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            ZE_SAFE_CALL(zeDeviceGetProperties(device->ze_device, &device->ze_properties));

            // set context
            device->ze_context = ze_contextes[ze_driver_id];
        }

        ze_n_devices += ndevices;
    }
    assert(ze_n_devices <= XKRT_DEVICES_MAX);

    # else
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.pNext = nullptr;
    desc.driverType = ZE_INIT_FLAG_GPU_ONLY;
    uint32_t driverCount = 0;
    const ze_result_t r = zeInitDrivers(&driverCount, nullptr, &desc);
    # endif

    return 0;
}

const char *
XKRT_DRIVER_ENTRYPOINT(device_info)(int device_driver_id)
{
    static char buffer[512];

    xkrt_device_ze_t * device = device_ze_get(device_driver_id);

    snprintf(
        buffer,
        sizeof(buffer),
        "Level Zero device %d - %s with %d slices of %d subslices of %d EUs of "
        "%d threads - %.2lfGB maximum alloc - core clock rate of %.2lfGHz - "
        "timer resolution of %luns",
        device_driver_id,
        device->ze_properties.name,
        device->ze_properties.numSlices,
        device->ze_properties.numSubslicesPerSlice,
        device->ze_properties.numEUsPerSubslice,
        device->ze_properties.numThreadsPerEU,
        device->ze_properties.maxMemAllocSize / 1e9,
        device->ze_properties.coreClockRate / 1e3,
        device->ze_properties.timerResolution
    );
    return buffer;
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{

}

static const char *
XKRT_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "ZE";
}

static unsigned int
XKRT_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return ze_n_devices;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_set_cpuset)(hwloc_topology_t topology, cpu_set_t * schedset, int device_driver_id)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    HWLOC_SAFE_CALL(hwloc_levelzero_get_device_cpuset(topology, device->ze_device, cpuset));
    CPU_ZERO(schedset);
    HWLOC_SAFE_CALL(hwloc_cpuset_to_glibc_sched_affinity(topology, cpuset, schedset, sizeof(cpu_set_t)));

    hwloc_bitmap_free(cpuset);
    return 0;
}

static xkrt_device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(xkrt_driver_t * driver, int device_driver_id)
{
    assert(device_driver_id >= 0 && device_driver_id < XKRT_DEVICES_MAX);

    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    assert(device->inherited.state == XKRT_DEVICE_STATE_DEALLOCATED);

    return (xkrt_device_t *) device;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_init)(int device_driver_id)
{
    // TODO : move some stuff from driver init to here
    (void) device_driver_id;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_destroy)(xkrt_device_t * device)
{
    free(device);
    return 0;
}


/* Called for each device of the driver once they all have been initialized */
static int
XKRT_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id)
{
    return 0;
}

////////////////
// STREAM //
/////////////

typedef struct  xkrt_stream_ze_t
{
    xkrt_stream_t super;

    // TODO : do we want to share command lists between streams as the Intel
    // API allow it ? I am not seeing it a practical use case here
    struct {
        struct {
            ze_command_list_handle_t list;
            ze_command_queue_handle_t queue;
        } command;
    } ze;
}               xkrt_stream_ze_t;

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr
) {
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;
    assert(stream);

    assert(istream->is_locked());

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_BARRIER):
        {
            return 0;
        }

        case (XKRT_STREAM_INSTR_TYPE_KERN):
        {
            return EINPROGRESS;

        } /* XKRT_STREAM_INSTR_TYPE_KERN */

        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D):
        {
            void * dst          = (void *) instr->copy.dst_device_view.addr;
            size_t dpitch       = instr->copy.dst_device_view.ld * instr->copy.host_view.sizeof_type;
            const void * src    = (const void *) instr->copy.src_device_view.addr;
            size_t spitch       = instr->copy.src_device_view.ld * instr->copy.host_view.sizeof_type;

            // assume col major for ze - if not, need to do some shit here
            assert(instr->copy.host_view.order == MATRIX_COLMAJOR);
            const size_t width  = instr->copy.host_view.m * instr->copy.host_view.sizeof_type;
            const size_t height = instr->copy.host_view.n;
            assert(width >= 0);
            assert(height >= 0);

            switch (instr->type)
            {
                case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
                {
                    break ;
                }

                case (XKRT_STREAM_INSTR_TYPE_COPY_D2H):
                {
                    break ;
                }

                case (XKRT_STREAM_INSTR_TYPE_COPY_D2D):
                {
                    break ;
                }

                case (XKRT_STREAM_INSTR_TYPE_COPY_H2H):
                    return ENOSYS;

                default:
                {
                    LOGGER_FATAL("instr->type got modified, something went really wrong");
                    break ;
                }
            }

            return EINPROGRESS;
        }

        default:
            return EINVAL;
    }

    /* unreachable code */
    LOGGER_FATAL("Unreachable code");
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    xkrt_stream_t * istream,
    int blocking
) {
    assert(istream);

    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;

    LOGGER_FATAL("IMPL ME");

    # if 0
    if (blocking)
    {
        CUDA_SAFE_CALL(cudaStreamSynchronize(stream->cu.handle.high));
        CUDA_SAFE_CALL(cudaStreamSynchronize(stream->cu.handle.low));
        istream->ok_p = istream->pending.pos.w;
        return 0;
    }
    # endif

    return 0;
}

static xkrt_stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    xkrt_device_t * idevice,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity
) {
    assert(idevice);

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkrt_stream_ze_t));
    assert(mem);

    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) mem;

    /************************/
    /* init ptr stream      */
    /************************/
    xkrt_stream_init(
        (xkrt_stream_t *) stream,
        type,
        capacity,
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)
    );

    LOGGER_FATAL("IMPL ME");
    xkrt_device_ze_t * device = (xkrt_device_ze_t *) idevice;
    ze_command_list_desc_t desc;
    ze_command_list_handle_t handle;
    ZE_SAFE_CALL(zeCommandListCreate(device->ze_context, device->ze_device, &desc, &handle));

    # if 0
    /***********************/
    /* do ze specific init */
    /***********************/

    /* events */
    stream->cu.events.end = (zeEvent_t *) (mem + sizeof(xkrt_stream_ze_t));
    stream->cu.events.capacity = capacity;

    zeError_t err;
    for (int i = 0 ; i < capacity ; ++i)
        CUDA_SAFE_CALL(zeEventCreateWithFlags(stream->cu.events.end + i, zeEventDisableTiming));

    /* streams */
    int leastPriority, greatestPriority;
    CUDA_SAFE_CALL(zeDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority));
    CUDA_SAFE_CALL(zeStreamCreateWithPriority(&stream->cu.handle.high, zeStreamNonBlocking, greatestPriority));
    CUDA_SAFE_CALL(zeStreamCreateWithPriority(&stream->cu.handle.low, zeStreamNonBlocking, leastPriority));

    if (type == XKRT_STREAM_TYPE_KERN)
    {
        cublasStatus_t cres = cublasCreate(&stream->cu.blas.handle);
        xkrt_cublas_status_check(cres);
        assert(cres == CUBLAS_STATUS_SUCCESS);

        cres = cublasSetStream(stream->cu.blas.handle, stream->cu.handle.high);
        xkrt_cublas_status_check(cres);
        assert(cres == CUBLAS_STATUS_SUCCESS);
    }
    else
    {
        stream->cu.blas.handle = 0;
    }
    # endif

    return (xkrt_stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;
    free(stream);
}

////////////
// MEMORY //
////////////

static void *
XKRT_DRIVER_ENTRYPOINT(memory_alloc)(int device_driver_id, const size_t size)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);

    // Query page size for our allocation
    size_t pagesize;
    ZE_SAFE_CALL(zeVirtualMemQueryPageSize(device->ze_context, device->ze_device, size, &pagesize));

    // Align size and reserve virtual address space.
    size_t reserve_size = size + (pagesize - (size % pagesize));

    void * device_ptr = nullptr;
    ZE_SAFE_CALL(zeVirtualMemReserve(device->ze_context, nullptr, reserve_size, &device_ptr));
    assert(device_ptr);

    return device_ptr;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_info)(int device_driver_id, size_t * total)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    *total = device->ze_properties.maxMemAllocSize;
}

//////////////////////////
// Routine registration //
//////////////////////////
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
    // EP(device_attach);   // no "state machine" with level zero, no need to attach to a device
    EP(device_commit);
    EP(device_info);

    EP(memory_alloc);
    EP(memory_info);
    // EP(memory_register);
    // EP(memory_unregister);

    EP(stream_create);
    EP(stream_delete);

    // EP(get_source);

    # undef EP
}
