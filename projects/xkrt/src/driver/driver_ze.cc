/* ************************************************************************** */
/*                                                                            */
/*   driver_ze.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/18 21:47:53 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

/* see https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html */

# define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_ZE_ ## N

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver-ze.h>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/logger-ze.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>

# include <ze_api.h>
# include <hwloc/levelzero.h>
# include <hwloc/glibc-sched.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>
# include <functional>

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
    ze_device_properties_t  ze_device_properties;

    // number of command queue group
    uint32_t ncommandqueuegroups;

    // per command queue group property
    ze_command_queue_group_properties_t * ze_command_queue_group_properties;

    // per command queue number of queue used
    std::atomic<uint32_t>               * ze_command_queue_group_used;

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

// set to '1' if Intel's subdevices should be mapped to a single xkrt device
// if '0', then Intel's devices are mapped
# define SUBDEVICE_TO_XKRT_DEVICE 1

static int
XKRT_DRIVER_ENTRYPOINT(init)(unsigned int ngpus)
{
    assert(0 < ngpus);
    assert(ngpus <= XKRT_DEVICES_MAX);

    # pragma message(TODO "We initialize all Intel drivers and devices here. Maybe make this a bit more lazy")

    // zeInit got deprecated, use other ifdef depending on version
    # if 1
    const ze_init_flags_t flags = ZE_INIT_FLAG_GPU_ONLY;
    const ze_result_t r = zeInit(flags);
    # else
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.pNext = nullptr;
    desc.driverType = ZE_INIT_FLAG_GPU_ONLY;
    uint32_t driverCount = 0;
    const ze_result_t r = zeInitDrivers(&driverCount, nullptr, &desc);
    # endif

    if (r != ZE_RESULT_SUCCESS)
        return 1;

    // get all drivers
    uint32_t ze_n_drivers = ngpus; // i believe Intel API ensure at least 1 gpu per driver ?
    ZE_SAFE_CALL(zeDriverGet(&ze_n_drivers, ze_drivers));

    // get all device handles per driver
    for (unsigned int ze_driver_id = 0 ; ze_driver_id < ze_n_drivers && ze_n_devices < ngpus ; ++ze_driver_id)
    {
        // Create context for driver
        ze_context_desc_t ze_context_desc = {
            .stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC,
            .pNext = NULL,
            .flags = ZE_CONTEXT_FLAG_TBD
        };
        ZE_SAFE_CALL(zeContextCreate(ze_drivers[ze_driver_id], &ze_context_desc, ze_contextes + ze_driver_id));

        // get devices handles
        uint32_t ndevices = ngpus - ze_n_devices;
        ze_device_handle_t ze_devices[XKRT_DEVICES_MAX];
        ZE_SAFE_CALL(zeDeviceGet(ze_drivers[ze_driver_id], &ndevices, ze_devices));

        for (unsigned int i = 0 ; i < ndevices ; ++i)
        {
            ze_device_handle_t ze_device = ze_devices[i];

            # if SUBDEVICE_TO_XKRT_DEVICE
            ze_device_handle_t ze_subdevices[XKRT_DEVICES_MAX];
            uint32_t nsubdevices = ngpus - ze_n_devices;
            zeDeviceGetSubDevices(ze_device, &nsubdevices, ze_subdevices);

            for (unsigned int j = 0; j < nsubdevices ; ++j)
            {
                ze_device_handle_t ze_device = ze_subdevices[j];
            # endif

                xkrt_device_ze_t * device = DEVICES + ze_n_devices;

                // save handles
                device->ze_context = ze_contextes[ze_driver_id];
                device->ze_driver  = ze_drivers[ze_driver_id];
                device->ze_device  = ze_device;

                // get subdevice properties
                device->ze_device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                ZE_SAFE_CALL(zeDeviceGetProperties(device->ze_device, &device->ze_device_properties));

                if (++ze_n_devices == ngpus)
                    return 0;

            # if SUBDEVICE_TO_XKRT_DEVICE
            }
            # endif
        }
    }

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
        device->ze_device_properties.name,
        device->ze_device_properties.numSlices,
        device->ze_device_properties.numSubslicesPerSlice,
        device->ze_device_properties.numEUsPerSubslice,
        device->ze_device_properties.numThreadsPerEU,
        device->ze_device_properties.maxMemAllocSize / 1e9,
        device->ze_device_properties.coreClockRate / 1e3,
        device->ze_device_properties.timerResolution
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

    // query number of command queue groups
    ZE_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device->ze_device, &device->ncommandqueuegroups, NULL));
    assert(device->ncommandqueuegroups);

    // query each group
    device->ze_command_queue_group_properties = (ze_command_queue_group_properties_t *) malloc(sizeof(ze_command_queue_group_properties_t) * device->ncommandqueuegroups);
    assert(device->ze_command_queue_group_properties);
    ZE_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device->ze_device, &device->ncommandqueuegroups, device->ze_command_queue_group_properties));

    device->ze_command_queue_group_used = new std::atomic<uint32_t>[device->ncommandqueuegroups];
    assert(device->ze_command_queue_group_used);
    for (uint32_t i = 0 ; i < device->ncommandqueuegroups ; ++i)
        device->ze_command_queue_group_used[i].store(0);

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

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;
    assert(stream);

    ze_event_handle_t ze_event_handle = stream->ze.events.list[idx];

    const uint32_t num_wait_events = 0;
    ze_event_handle_t * wait_events = NULL;

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
        {
                  void * dst    = (      void *) instr->copy.D1.dst_device_addr;
            const void * src    = (const void *) instr->copy.D1.src_device_addr;
            const size_t size   = instr->copy.D1.size;
            ZE_SAFE_CALL(
                zeCommandListAppendMemoryCopy(
                    stream->ze.command.list,
                    dst,
                    src,
                    size,
                    ze_event_handle,
                    num_wait_events,
                    wait_events
                )
            );
            return EINPROGRESS;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
        {
                  void * dst    = (      void *) instr->copy.D2.dst_device_view.addr;
            const void * src    = (const void *) instr->copy.D2.src_device_view.addr;

            const size_t dst_pitch = instr->copy.D2.dst_device_view.ld * instr->copy.D2.sizeof_type;
            const size_t src_pitch = instr->copy.D2.src_device_view.ld * instr->copy.D2.sizeof_type;

            const size_t width  = instr->copy.D2.m * instr->copy.D2.sizeof_type;
            const size_t height = instr->copy.D2.n;

            const uint32_t dst_slice_pitch = 0;
            const ze_copy_region_t dst_region = {
                .originX = 0,
                .originY = 0,
                .originZ = 0,
                .width   = (uint32_t) width,
                .height  = (uint32_t) height,
                .depth   = 0
            };

            const uint32_t src_slice_pitch = 0;
            const ze_copy_region_t src_region = {
                .originX = 0,
                .originY = 0,
                .originZ = 0,
                .width   = (uint32_t) width,
                .height  = (uint32_t) height,
                .depth   = 0
            };

            ZE_SAFE_CALL(
                zeCommandListAppendMemoryCopyRegion(
                    stream->ze.command.list,
                    dst,
                   &dst_region,
                    dst_pitch,
                    dst_slice_pitch,
                    src,
                   &src_region,
                    src_pitch,
                    src_slice_pitch,
                    ze_event_handle,
                    num_wait_events,
                    wait_events
                )
            );

            return EINPROGRESS;
        }

        default:
            return EINVAL;
    }

    /* unreachable code */
    LOGGER_FATAL("Unreachable code");
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)(
    xkrt_stream_t * istream
) {
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;

    const uint64_t timeout = 0;
    ZE_SAFE_CALL(zeCommandListHostSynchronize(stream->ze.command.list, timeout));
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx

) {
    assert(istream);

    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;

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
            ze_result_t res;

            /* poll event */
            for (int i = 0 ; i < 16 ; ++i)
            {
                res = zeEventQueryStatus(stream->ze.events.list[idx]);
                if (res == ZE_RESULT_NOT_READY)
                    sched_yield();
                else if (res == ZE_RESULT_SUCCESS)
                    return 0;
                else
                    LOGGER_FATAL("Error querying event");
            }

            return EINPROGRESS;
        }

        default:
            LOGGER_FATAL("Wrong instruction");
    }

    return EINPROGRESS;
}

template<typename T>
static inline bool
f_equals(
    const T & x,
    const T & y
) {
    return x == y ? true : false;
}

template<typename T>
static inline bool
f_and(
    const T & x,
    const T & y
) {
    return x & y ? true : false;
}

// return the next command queue group to use
template <typename T, bool (*f)(const T & x, const T & y)>
static inline uint32_t
device_command_queue_group_next(
    xkrt_device_ze_t * device,
    const ze_command_queue_group_property_flag_t & flag
) {
    uint32_t ordinal_with_least_queues = UINT32_MAX;
    uint32_t min_queues = UINT32_MAX;

    for (uint32_t i = 0; i < device->ncommandqueuegroups; ++i)
    {
        ze_command_queue_group_properties_t * properties = device->ze_command_queue_group_properties + i;
        if (f((const ze_command_queue_group_property_flag_t &) properties->flags, flag))
        {
            const uint32_t used = device->ze_command_queue_group_used[i].load();
            if (used < min_queues)
            {
                min_queues = used;
                ordinal_with_least_queues = i;
            }
        }
    }

    return ordinal_with_least_queues;
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

    xkrt_stream_init(
        (xkrt_stream_t *) mem,
        type,
        capacity,
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)
    );

    xkrt_device_ze_t * device = (xkrt_device_ze_t *) idevice;
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) mem;

    // convert xkrt stream type to a command queue group flag
    ze_command_queue_group_property_flag_t flag;
    switch (type)
    {
        case (XKRT_STREAM_TYPE_H2D):
        case (XKRT_STREAM_TYPE_D2H):
        case (XKRT_STREAM_TYPE_D2D):
        {
            flag = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
            break ;
        }

        case (XKRT_STREAM_TYPE_KERN):
        {
            flag = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE;
            break ;
        }

        default:
            LOGGER_FATAL("Unknown stream type");
    }

    // create command queue
    uint32_t ordinal = device_command_queue_group_next<ze_command_queue_group_property_flag_t, f_equals>(device, flag);
    if (ordinal == UINT32_MAX)
    {
        ordinal = device_command_queue_group_next<ze_command_queue_group_property_flag_t, f_and>(device, flag);
        if (ordinal == UINT32_MAX)
            LOGGER_FATAL("No command queue group available for stream");
    }

    // retrieve group properties
    const ze_command_queue_group_properties_t * properties = device->ze_command_queue_group_properties + ordinal;
    uint32_t index = device->ze_command_queue_group_used[ordinal].fetch_add(1) % properties->numQueues;

    // get the next command queue index to use in the group
    const ze_command_queue_desc_t ze_command_queue_desc = {
        .stype      = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext      = NULL,
        .ordinal    = ordinal,
        .index      = index,
        .flags      = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,      // TODO : i think we want explicit ?
        .mode       = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        .priority   = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW    // TODO : maybe do one queue for each priority in 'device->ze_device_properties.maxCommandQueuePriority'
    };
    # if 0
    ZE_SAFE_CALL(
        zeCommandQueueCreate(
            device->ze_context,
            device->ze_device,
           &ze_command_queue_desc,
           &stream->ze.command.queue
        )
    );
    # endif

    // create command list
    # if 0
    ze_command_list_desc_t ze_command_list_desc = {
        .stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC,
        .pNext = NULL,
        .commandQueueGroupOrdinal = ordinal,
        .flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING | ZE_COMMAND_LIST_FLAG_MAXIMIZE_THROUGHPUT
    };
    # endif
    ZE_SAFE_CALL(
        zeCommandListCreateImmediate(
            device->ze_context,
            device->ze_device,
           &ze_command_queue_desc,
           &stream->ze.command.list
        )
    );

    // create event pool and events
    const ze_event_pool_desc_t ze_event_pool_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext  = NULL,
        .flags  = ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        .count  = capacity
    };
    const uint32_t ndevices = 1;
    ZE_SAFE_CALL(zeEventPoolCreate(device->ze_context, &ze_event_pool_desc, ndevices, &device->ze_device, &stream->ze.events.pool));

    ze_event_desc_t event_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_DESC,
        .signal = ZE_EVENT_SCOPE_FLAG_HOST,
        .wait   = ZE_EVENT_SCOPE_FLAG_HOST
    };
    stream->ze.events.list = (ze_event_handle_t *) malloc(sizeof(ze_event_handle_t) * capacity);
    assert(stream->ze.events.list);
    for (xkrt_stream_instruction_counter_t i = 0 ; i < capacity ; ++i)
        ZE_SAFE_CALL(zeEventCreate(stream->ze.events.pool, &event_desc, stream->ze.events.list + i));

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

    # if 0
    const ze_device_mem_alloc_desc_t device_desc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES,
        .pNext = NULL,
        .flags = 0,
        .ordinal = 0
    };
    const size_t alignment = 8;
    void * device_ptr = NULL;
    ZE_SAFE_CALL(zeMemAllocDevice(device->ze_context, &device_desc, size, alignment, device->ze_device, &device_ptr));
    ZE_SAFE_CALL(zeContextMakeMemoryResident(device->ze_context, device->ze_device, device_ptr, size));
    # else

    // Query page size for our allocation
    size_t pagesize;
    ZE_SAFE_CALL(
        zeVirtualMemQueryPageSize(
            device->ze_context,
            device->ze_device,
            size,
           &pagesize
        )
    );

    // Align size and reserve virtual address space.
    const size_t reserve_size = size + (pagesize - (size % pagesize));
    void * device_ptr = NULL;
    ZE_SAFE_CALL(
        zeVirtualMemReserve(
            device->ze_context,
            NULL,
            reserve_size,
           &device_ptr
        )
    );
    assert(device_ptr);

    // Create physical memory
    ze_physical_mem_desc_t ze_physical_mem_desc = {
        .stype = ZE_STRUCTURE_TYPE_PHYSICAL_MEM_DESC,
        .pNext = NULL,
        .flags = ZE_PHYSICAL_MEM_FLAG_TBD,
        .size  = reserve_size
    };
    ze_physical_mem_handle_t ze_physical_mem_handle;
    ZE_SAFE_CALL(
        zePhysicalMemCreate(
            device->ze_context,
            device->ze_device,
           &ze_physical_mem_desc,
           &ze_physical_mem_handle
        )
    );

    // Map virtual to physical memory
    const ze_memory_access_attribute_t ze_memory_access_attribute = ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE;
    const size_t offset = 0;
    ZE_SAFE_CALL(
        zeVirtualMemMap(
            device->ze_context,
            device_ptr,
            size,
            ze_physical_mem_handle,
            offset,
            ze_memory_access_attribute
        )
    );
    # endif

    ZE_SAFE_CALL(
        zeContextMakeMemoryResident(
            device->ze_context,
            device->ze_device,
            device_ptr, size
        )
    );

    return device_ptr;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_info)(int device_driver_id, size_t * total)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    *total = device->ze_device_properties.maxMemAllocSize;
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

// TODO : if we were to impl specific kernelsm thats how you launch

# if 0
    // retrieve event handle
    const xkrt_stream_instruction_counter_t wp = stream->super.pending.pos.w % stream->super.pending.capacity;
    ze_event_handle_t ze_event_handle = stream->ze.events.list[wp];

    // retrieve the kernel
    const ze_kernel_desc_t kernel_desc = {
        .stype = ZE_STRUCTURE_TYPE_KERNEL_DESC,
        .pNext = nullptr,
        .flags = ZE_KERNEL_FLAG_EXPLICIT_RESIDENCY, // TODO : what do we want here ?
        .pKernelName = "GEMM"                       // TODO : what do we want here ?
    };
    ze_kernel_handle_t ze_kernel_handle = NULL;
    ZE_SAFE_CALL(zeKernelCreate(module, &ze_kernel_desc, &ze_kernel_handle));

    // setup the launch grid
    uint32_t gsx, gsy, gsz; // group sizes
    const size_t sx = args->m;  // TODO : what size is this ?
    const size_t sy = args->n;  // TODO : what size is this ?
    const size_t sz = 0;        // TODO : what size is this ?
    ZE_SAFE_CALL(
        zeKernelSuggestGroupSize(
            kernel,
            sx, sy, sz,
            &gsx, &gsy, &gsz
        )
    );
    assert(sz == 1);                                // TODO : not sure about this
    assert((size % sx == 0) && (size % sy) == 0);   // TODO : not sure this is necessary

    const ze_group_count_t ze_group_count = {
        .groupCountX = size / sx,   // TODO
        .groupCountY = size / sy,   // TODO
        .groupCountZ = 1            // TODO
    };

    // launch kernel
    ZE_SAFE_CALL(
        zeCommandListAppendLaunchKernel(
            stream->ze.command.list,
            ze_kernel_handle,
           &ze_group_count,
            ze_event_handle,
            0, /* numWaitEvents */
            NULL /* phWaitEvents */
        )
    );
# endif

