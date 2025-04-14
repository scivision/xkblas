/* ************************************************************************** */
/*                                                                            */
/*   driver_ze.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 16:54:02 by Romain PEREIRA            \_)     (_/    */
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
# include <xkrt/logger/metric.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>
# include <xkrt/xkrt-support.h>

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

static xkrt_device_ze_t * DEVICES;
static uint32_t ze_n_devices = 0;

static xkrt_device_ze_t *
device_ze_get(int device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < ze_n_devices);
    return DEVICES + device_driver_id;
}

// see `ZE_FLAT_DEVICE_HIERARCHY` env variable
static int
XKRT_DRIVER_ENTRYPOINT(init)(unsigned int ndevices_requested)
{
    assert(0 < ndevices_requested);
    assert(ndevices_requested <= XKRT_DEVICES_MAX);

    DEVICES = (xkrt_device_ze_t *) calloc(XKRT_DEVICES_MAX, sizeof(xkrt_device_ze_t));
    assert(DEVICES);

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
    uint32_t ze_n_drivers = 0;
    ZE_SAFE_CALL(zeDriverGet(&ze_n_drivers, NULL));
    assert(ze_n_drivers < sizeof(ze_drivers) / sizeof(*ze_drivers));
    ZE_SAFE_CALL(zeDriverGet(&ze_n_drivers, ze_drivers));

    # if XKRT_SUPPORT_ZES
    zes_init_flags_t zes_flags = ZES_INIT_FLAG_PLACEHOLDER;
    ZE_SAFE_CALL(zesInit(zes_flags));

    uint32_t zes_n_drivers;
    ZE_SAFE_CALL(zesDriverGet(&zes_n_drivers, NULL));
    assert(ze_n_drivers == zes_n_drivers);

    zes_driver_handle_t zes_drivers[XKRT_DEVICES_MAX];
    ZE_SAFE_CALL(zesDriverGet(&zes_n_drivers, zes_drivers));
    # endif /* XKRT_SUPPORT_ZES */

    // get all device handles per driver
    for (unsigned int ze_driver_id = 0 ; ze_driver_id < ze_n_drivers && ze_n_devices < ndevices_requested ; ++ze_driver_id)
    {
        // get the driver
        ze_driver_handle_t ze_driver = ze_drivers[ze_driver_id];

        // Create context for driver
        ze_context_desc_t ze_context_desc = {
            .stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC,
            .pNext = NULL,
            .flags = ZE_CONTEXT_FLAG_TBD
        };
        ZE_SAFE_CALL(zeContextCreate(ze_driver, &ze_context_desc, ze_contextes + ze_driver_id));

        // get devices handles
        uint32_t ndevices = ndevices_requested - ze_n_devices;
        ze_device_handle_t ze_devices[XKRT_DEVICES_MAX];
        ZE_SAFE_CALL(zeDeviceGet(ze_driver, &ndevices, ze_devices));

        # if XKRT_SUPPORT_ZES
        zes_driver_handle_t zes_driver = zes_drivers[ze_driver_id];

        uint32_t zes_n_devices;
        ZE_SAFE_CALL(zesDeviceGet(zes_driver, &zes_n_devices, NULL));
        assert(ze_n_devices == zes_n_devices);

        zes_device_handle_t zes_devices[XKRT_DEVICES_MAX];
        ZE_SAFE_CALL(zesDeviceGet(zes_driver, &zes_n_devices, zes_devices));
        # endif /* XKRT_SUPPORT_ZES */

        // sycl interop
        # if XKRT_SUPPORT_SYCL
        sycl::platform platform = sycl::ext::oneapi::level_zero::make_platform((pi_native_handle) ze_driver);
        # endif /* XKRT_SUPPORT_SYCL */

        for (unsigned int i = 0 ; i < ndevices ; ++i)
        {
            ze_device_handle_t ze_device = ze_devices[i];

            // save handles
            xkrt_device_ze_t * device = DEVICES + ze_n_devices;
            device->ze.context = ze_contextes[ze_driver_id];
            device->ze.driver  = ze_drivers[ze_driver_id];
            device->ze.device  = ze_device;

            // get subdevice properties
            device->ze.device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            ZE_SAFE_CALL(zeDeviceGetProperties(device->ze.device, &device->ze.device_properties));

            // get memory properties
            device->ze.memory.pcount = XKRT_DEVICE_MEMORIES_MAX;
            ZE_SAFE_CALL(zeDeviceGetMemoryProperties(device->ze.device, &device->ze.memory.pcount, device->ze.memory.properties));

            # if XKRT_SUPPORT_SYCL
            // sycl interop
            device->sycl.device = sycl::ext::oneapi::level_zero::make_device(platform, (pi_native_handle) ze_device);

            std::vector<sycl::device> sycl_devices(1);
            sycl_devices[0] = device->sycl.device;
            device->sycl.context = sycl::ext::oneapi::level_zero::make_context(sycl_devices, (pi_native_handle)device->ze.context, 1);
            # endif /* XKRT_SUPPORT_SYCL */

            # if XKRT_SUPPORT_ZES
            zes_device_handle_t zes_device = zes_devices[i];
            uint32_t zes_pwr_handle_count;
            ZE_SAFE_CALL(zesDeviceEnumPowerDomains(zes_device, &zes_pwr_handle_count, NULL));
            assert(zes_pwr_handle_count == 1);
            ZE_SAFE_CALL(zesDeviceEnumPowerDomains(zes_device, &zes_pwr_handle_count, &device->zes.pwr.handle));
            # endif /* XKRT_SUPPORT_ZES */

            if (++ze_n_devices == ndevices_requested)
                break ;
        }
    }

    return 0;
}

void
XKRT_DRIVER_ENTRYPOINT(device_info)(
    int device_driver_id,
    char * buffer,
    size_t size
) {
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);

    char uuid[2 + 2 * ZE_MAX_DEVICE_UUID_SIZE + 1];
    size_t pos = 0;
    pos += snprintf(uuid + pos, sizeof(uuid) - pos, "0x");
    for (int i = 0 ; i < ZE_MAX_DEVICE_UUID_SIZE ; ++i)
        pos += snprintf(uuid + pos, sizeof(uuid) - pos, "%X", device->ze.device_properties.uuid.id[i]);

    snprintf(
        buffer,
        size,
        "Level Zero device %2d - %s with %d slices of %d subslices of %d EUs of "
        "%d threads - %.2lfGB maximum alloc - core clock rate of %.2lfGHz - "
        "timer resolution of %luns - deviceId(pci)=%d - uuid=%s",
        device_driver_id,
        device->ze.device_properties.name,
        device->ze.device_properties.numSlices,
        device->ze.device_properties.numSubslicesPerSlice,
        device->ze.device_properties.numEUsPerSubslice,
        device->ze.device_properties.numThreadsPerEU,
        device->ze.device_properties.maxMemAllocSize / 1e9,
        device->ze.device_properties.coreClockRate / 1e3,
        device->ze.device_properties.timerResolution,
        device->ze.device_properties.deviceId,
        uuid
    );
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{
    // TODO : zeDeinit ?

    // get all device handles per driver
    for (int i = 0 ; i < XKRT_DEVICES_MAX && ze_contextes[i] ; ++i)
        ZE_SAFE_CALL(zeContextDestroy(ze_contextes[i]));

    free(DEVICES);
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
XKRT_DRIVER_ENTRYPOINT(device_cpuset)(hwloc_topology_t topology, cpu_set_t * schedset, int device_driver_id)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    HWLOC_SAFE_CALL(hwloc_levelzero_get_device_cpuset(topology, device->ze.device, cpuset));
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
    ZE_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device->ze.device, &device->ze.ncommandqueuegroups, NULL));
    assert(device->ze.ncommandqueuegroups);

    // query each group
    device->ze.command_queue_group_properties = (ze_command_queue_group_properties_t *) malloc(sizeof(ze_command_queue_group_properties_t) * device->ze.ncommandqueuegroups);
    assert(device->ze.command_queue_group_properties);
    ZE_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device->ze.device, &device->ze.ncommandqueuegroups, device->ze.command_queue_group_properties));

    device->ze.command_queue_group_used = new std::atomic<uint32_t>[device->ze.ncommandqueuegroups];
    assert(device->ze.command_queue_group_used);
    for (uint32_t i = 0 ; i < device->ze.ncommandqueuegroups ; ++i)
        device->ze.command_queue_group_used[i].store(0);

    return (xkrt_device_t *) device;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_init)(int device_driver_id)
{
    // TODO : move some stuff from driver init to here
    (void) device_driver_id;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_destroy)(int device_driver_id)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    delete [] device->ze.command_queue_group_used;
    return 0;
}

/* Called for each device of the driver once they all have been initialized */
static int
XKRT_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id, xkrt_device_global_id_bitfield_t * affinity)
{
    // TODO: Intel API `zeDeviceGetP2PProperties` currently does not have a property about P2P performances
    // so instead, simply hard-code affinity for now

    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    xkrt_device_global_id_t device_global_id = device->inherited.global_id;

    int rank = 0;
    affinity[rank++] = (1 << device_global_id);
# if SUBDEVICE_TO_XKRT_DEVICE
    xkrt_device_global_id_t subdevice_global_id = (device_global_id % 2 == 0) ? (device_global_id + 1) : (device_global_id - 1);
    affinity[rank++] =  (1 << subdevice_global_id);
    affinity[rank++] = (~affinity[0]) & (~affinity[1]);
# else
    affinity[rank++] = ~affinity[0];
# endif
    assert(rank <= XKRT_DEVICES_PERF_RANK_MAX);

    return 0;
}

////////////
// STREAM //
////////////

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;
    assert(stream);

    ze_event_handle_t ze_event_handle = stream->ze.events.list[idx];

    // TODO : try zeCommandListAppendEventReset and see if it reduces latency
    ZE_SAFE_CALL(zeEventHostReset(ze_event_handle));

    const uint32_t num_wait_events = 0;
    ze_event_handle_t * wait_events = NULL;

    int err = EINPROGRESS;

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
            break ;
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
                .depth   = 1
            };

            const uint32_t src_slice_pitch = 0;
            const ze_copy_region_t src_region = {
                .originX = 0,
                .originY = 0,
                .originZ = 0,
                .width   = (uint32_t) width,
                .height  = (uint32_t) height,
                .depth   = 1
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
            break ;
        }

        default:
            return EINVAL;
    }

    // TODO : how to use this ?
    // ZE_SAFE_CALL(zeCommandListAppendEventReset(stream->ze.command.list, ze_event_handle));

    return err;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)(
    xkrt_stream_t * istream
) {
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;

    const uint64_t timeout = UINT64_MAX;
    ZE_SAFE_CALL(zeCommandListHostSynchronize(stream->ze.command.list, timeout));
    return 0;
}


static int
XKRT_DRIVER_ENTRYPOINT(stream_suggest)(
    int device_driver_id,
    xkrt_stream_type_t stype
) {
    switch (stype)
    {
        case (XKRT_STREAM_TYPE_KERN):
            return 8;

        case (XKRT_STREAM_TYPE_H2D):
        case (XKRT_STREAM_TYPE_D2H):
        case (XKRT_STREAM_TYPE_D2D):
            return 4;

        default:
            return 1;
    }
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
                    ZE_SAFE_CALL(res);
            }

            return EINPROGRESS;
        }

        default:
            LOGGER_FATAL("Wrong instruction");
    }

    return EINPROGRESS;
}

# if 1
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

    for (uint32_t i = 0; i < device->ze.ncommandqueuegroups; ++i)
    {
        ze_command_queue_group_properties_t * properties = device->ze.command_queue_group_properties + i;
        if (f((const ze_command_queue_group_property_flag_t &) properties->flags, flag))
        {
            const uint32_t used = device->ze.command_queue_group_used[i].load();
            if (used < min_queues)
            {
                min_queues = used;
                ordinal_with_least_queues = i;
            }
        }
    }

    return ordinal_with_least_queues;
}
# endif

static xkrt_stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    xkrt_device_t * idevice,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity
) {
    assert(idevice);

    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) malloc(sizeof(xkrt_stream_ze_t));
    assert(stream);

    xkrt_stream_init(
        (xkrt_stream_t *) stream,
        type,
        capacity,
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)
    );

    xkrt_device_ze_t * device = (xkrt_device_ze_t *) idevice;
    stream->ze.device = device;

    // Round robin over copy engines

    # if 0
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

    uint32_t ordinal = device_command_queue_group_next<ze_command_queue_group_property_flag_t, f_equals>(device, flag);
    if (ordinal == UINT32_MAX)
    {
        ordinal = device_command_queue_group_next<ze_command_queue_group_property_flag_t, f_and>(device, flag);
        if (ordinal == UINT32_MAX)
            LOGGER_FATAL("No command queue group available for stream");
    }
    # else
    /* https://github.com/pmodels/mpich/blob/main/src/mpl/src/gpu/mpl_gpu_ze.c#L656-L660 */
    uint32_t ordinal;
    switch (type)
    {
        case (XKRT_STREAM_TYPE_KERN):
        {
            ordinal = 0;
            break ;
        }

        case (XKRT_STREAM_TYPE_H2D):
        case (XKRT_STREAM_TYPE_D2H):
        case (XKRT_STREAM_TYPE_D2D):
        {
            ordinal = 1;
            break ;
        }

        default:
            LOGGER_FATAL("Unknown stream type");
    }
    # endif

    // retrieve group properties
    const ze_command_queue_group_properties_t * properties = device->ze.command_queue_group_properties + ordinal;
    uint32_t index = device->ze.command_queue_group_used[ordinal].fetch_add(1) % properties->numQueues;

    // get the next command queue index to use in the group
    const ze_command_queue_desc_t ze_command_queue_desc = {
        .stype      = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext      = NULL,
        .ordinal    = ordinal,
        .index      = index,
        .flags      = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,
        .mode       = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        .priority   = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW
    };
    LOGGER_DEBUG("Creating stream of type `%4s` with (ordinal, index) = (%d, %d)", xkrt_stream_type_to_str(type), ordinal, index);

    # if 0 /* use a command list and command queue */
    ZE_SAFE_CALL(
        zeCommandQueueCreate(
            device->ze.context,
            device->ze.device,
           &ze_command_queue_desc,
           &stream->ze.command.queue
        )
    );

    // create command list
    ze_command_list_desc_t ze_command_list_desc = {
        .stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC,
        .pNext = NULL,
        .commandQueueGroupOrdinal = ordinal,
        .flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING | ZE_COMMAND_LIST_FLAG_MAXIMIZE_THROUGHPUT
    };
    # else /* use command list immediate */
    ZE_SAFE_CALL(
        zeCommandListCreateImmediate(
            device->ze.context,
            device->ze.device,
           &ze_command_queue_desc,
           &stream->ze.command.list
        )
    );

    # if XKRT_SUPPORT_SYCL
    sycl::property_list props = {}; /* how to convert `ze_command_queue_desc` to `sycl::property_list` ? */
    sycl::queue queue = sycl::ext::oneapi::level_zero::make_queue(
        device->sycl.context,
        device->sycl.device,
        (pi_native_handle) stream->ze.command.list,
        true,   /* immediate */
        true,   /* keep ownership */
        props
    );
    new (&stream->sycl.queue) sycl::queue(queue);
    # endif /* XKRT_SUPPORT_SYCL */

    # endif

    // create event pool and events
    const ze_event_pool_desc_t ze_event_pool_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext  = NULL,
        .flags  = ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        .count  = capacity
    };
    const uint32_t ndevices = 1;
    ZE_SAFE_CALL(zeEventPoolCreate(device->ze.context, &ze_event_pool_desc, ndevices, &device->ze.device, &stream->ze.events.pool));

    stream->ze.events.list = (ze_event_handle_t *) malloc(sizeof(ze_event_handle_t) * capacity);
    assert(stream->ze.events.list);
    for (xkrt_stream_instruction_counter_t i = 0 ; i < capacity ; ++i)
    {
        ze_event_desc_t event_desc = {
            .stype  = ZE_STRUCTURE_TYPE_EVENT_DESC,
            .pNext  = NULL,
            .index  = (uint32_t) i,
            .signal = ZE_EVENT_SCOPE_FLAG_HOST,
            .wait   = ZE_EVENT_SCOPE_FLAG_HOST
        };
        ZE_SAFE_CALL(zeEventCreate(stream->ze.events.pool, &event_desc, stream->ze.events.list + i));
    }

    return (xkrt_stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;
    ZE_SAFE_CALL(zeEventPoolDestroy(stream->ze.events.pool));
    ZE_SAFE_CALL(zeCommandListDestroy(stream->ze.command.list));
    free(stream);
}

////////////
// MEMORY //
////////////

static void *
XKRT_DRIVER_ENTRYPOINT(memory_device_allocate)(int device_driver_id, const size_t size, int area_idx)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);

    # if 1
    const ze_device_mem_alloc_desc_t device_desc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES,
        .pNext = NULL,
        .flags = 0,
        .ordinal = (uint32_t) area_idx // device memory ordinal (should be here where we see HBM/DRAM)
    };
    const size_t alignment = 4 * sizeof(double);
    void * device_ptr = NULL;
    ze_result_t res = zeMemAllocDevice(device->ze.context, &device_desc, size, alignment, device->ze.device, &device_ptr);
    # else

    // TODO : cannot select memory ordinal with virtual/physical memory API

    // Query page size for our allocation
    size_t pagesize;
    ZE_SAFE_CALL(
        zeVirtualMemQueryPageSize(
            device->ze.context,
            device->ze.device,
            size,
           &pagesize
        )
    );

    // Align size and reserve virtual address space.
    const size_t reserve_size = size + (pagesize - (size % pagesize));
    void * device_ptr = NULL;
    ZE_SAFE_CALL(
        zeVirtualMemReserve(
            device->ze.context,
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
            device->ze.context,
            device->ze.device,
           &ze_physical_mem_desc,
           &ze_physical_mem_handle
        )
    );

    // Map virtual to physical memory
    const ze_memory_access_attribute_t ze_memory_access_attribute = ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE;
    const size_t offset = 0;
    ZE_SAFE_CALL(
        zeVirtualMemMap(
            device->ze.context,
            device_ptr,
            size,
            ze_physical_mem_handle,
            offset,
            ze_memory_access_attribute
        )
    );
    # endif

    if (res == ZE_RESULT_SUCCESS)
    {
        res = zeContextMakeMemoryResident(device->ze.context, device->ze.device, device_ptr, size);
        if (res == ZE_RESULT_SUCCESS)
            return device_ptr;

        ZE_SAFE_CALL(zeMemFree(device->ze.context, device_ptr));
    }

    return NULL;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_deallocate)(int device_driver_id, void * ptr, const size_t size, int area_idx)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    ZE_SAFE_CALL(zeMemFree(device->ze.context, ptr));
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_info)(
    int device_driver_id,
    xkrt_device_memory_info_t info[XKRT_DEVICES_MAX],
    int * nmemories
) {
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    info->capacity = device->ze.device_properties.maxMemAllocSize;
    *nmemories = device->ze.memory.pcount;
    for (int i = 0 ; i < device->ze.memory.pcount && i < XKRT_DEVICE_MEMORIES_MAX ; ++i)
    {
        info[i].capacity = device->ze.memory.properties[i].totalSize;
        strncpy(info[i].name, device->ze.memory.properties[i].name, MIN(sizeof(device->ze.memory.properties[i].name), sizeof(info[i].name)));
    }
}

static void *
XKRT_DRIVER_ENTRYPOINT(memory_host_allocate)(
    int device_driver_id,
    uint64_t size
) {
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    const ze_host_mem_alloc_desc_t host_desc = {
        .stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC,
        .pNext = NULL,
        .flags = 0
        // .flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_CACHED | ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT | ZE_HOST_MEM_ALLOC_FLAG_BIAS_WRITE_COMBINED
    };
    void * ptr;
    ZE_SAFE_CALL(zeMemAllocHost(device->ze.context, &host_desc, size, 64, (void **) &ptr));
    return ptr;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_host_deallocate)(
    int device_driver_id,
    void * mem,
    uint64_t size
) {
    (void) size;

    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    ZE_SAFE_CALL(zeMemFree(device->ze.context, mem));
}

xkrt_driver_module_t
XKRT_DRIVER_ENTRYPOINT(module_load)(
    int device_driver_id,
    uint8_t * bin,
    size_t binsize,
    xkrt_driver_module_format_t format
) {
    ze_module_format_t ze_format;
    switch (format)
    {
        case (XKRT_DRIVER_MODULE_FORMAT_SPIRV):
        {
            ze_format = ZE_MODULE_FORMAT_IL_SPIRV;
            break ;
        }

        case (XKRT_DRIVER_MODULE_FORMAT_NATIVE):
        {
            ze_format = ZE_MODULE_FORMAT_NATIVE;
            break ;
        }

        default:
            LOGGER_FATAL("Unknown format");
    }
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    ze_module_desc_t desc = {
        .stype = ZE_STRUCTURE_TYPE_MODULE_DESC,
        .pNext = NULL,
        .format = ze_format,
        .inputSize = binsize,
        .pInputModule = bin,
        .pBuildFlags = NULL,
        .pConstants = NULL
    };

    // TODO : build log
    xkrt_driver_module_t module = NULL;
    ZE_SAFE_CALL(zeModuleCreate(device->ze.context, device->ze.device, &desc, (ze_module_handle_t *) &module, NULL));
    assert(module);

    return module;
}

# if XKRT_SUPPORT_ZES

void
XKRT_DRIVER_ENTRYPOINT(power_start)(int device_driver_id)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    assert(device);

    device->zes.pwr.t1 = xkrt_get_nanotime();
    ZE_SAFE_CALL(zesPowerGetEnergyCounter(device->zes.pwr.handle, &device->zes.pwr.e1));
}

void
XKRT_DRIVER_ENTRYPOINT(power_stop)(int device_driver_id, double * dt, double * P)
{
    xkrt_device_ze_t * device = device_ze_get(device_driver_id);
    assert(device);

    ZE_SAFE_CALL(zesPowerGetEnergyCounter(device->zes.pwr.handle, &device->zes.pwr.e2));
    device->zes.pwr.t2 = xkrt_get_nanotime();

    double uJ = (double) (device->zes.pwr.e2.energy - device->zes.pwr.e2.energy);
    double J = uJ / (double)1e6;
    double s = (device->zes.pwr.t2 - device->zes.pwr.t1) / (double) 1e9;

    if (dt)
        *dt = s;
    if (P)
        *P = J / s;
}

# endif /* XKRT_SUPPORT_ZES */

void
XKRT_DRIVER_ENTRYPOINT(module_unload)(
    xkrt_driver_module_t module
) {
    ZE_SAFE_CALL(zeModuleDestroy((ze_module_handle_t) module));
}

xkrt_driver_module_fn_t
XKRT_DRIVER_ENTRYPOINT(module_get_fn)(
    xkrt_driver_module_t module,
    const char * name
) {
    xkrt_driver_module_fn_t fn = NULL;
    ze_kernel_desc_t desc = {
        .stype = ZE_STRUCTURE_TYPE_KERNEL_DESC,
        .pNext = NULL,
        .flags = ZE_KERNEL_FLAG_FORCE_RESIDENCY,
        .pKernelName = name
    };
    ZE_SAFE_CALL(zeKernelCreate((ze_module_handle_t) module, &desc, (ze_kernel_handle_t *) &fn));
    assert(fn);
    return fn;
}

//////////////////////////
// Routine registration //
//////////////////////////
xkrt_driver_t *
XKRT_DRIVER_ENTRYPOINT(create_driver)(void)
{
    xkrt_driver_ze_t * driver = (xkrt_driver_ze_t *) calloc(1, sizeof(xkrt_driver_ze_t));
    assert(driver);

    # define REGISTER(func) driver->super.f_##func = XKRT_DRIVER_ENTRYPOINT(func)

    REGISTER(init);
    REGISTER(finalize);

    REGISTER(get_name);
    REGISTER(get_ndevices_max);

    REGISTER(device_create);
    REGISTER(device_init);
    REGISTER(device_commit);
    REGISTER(device_destroy);

    REGISTER(device_info);

    REGISTER(memory_device_info);
    REGISTER(memory_device_allocate);
    REGISTER(memory_device_deallocate);
    REGISTER(memory_host_allocate);
    REGISTER(memory_host_deallocate);
    // REGISTER(memory_host_register);
    // REGISTER(memory_host_unregister);
    // REGISTER(memory_unified_allocate);
    // REGISTER(memory_unified_deallocate);

    REGISTER(device_cpuset);

    REGISTER(stream_suggest);
    REGISTER(stream_create);
    REGISTER(stream_delete);

    REGISTER(module_load);
    REGISTER(module_unload);
    REGISTER(module_get_fn);

    # if XKRT_SUPPORT_ZES
    REGISTER(power_start);
    REGISTER(power_stop);
    # endif /* XKRT_SUPPORT_ZES */

    # undef REGISTER

    return (xkrt_driver_t *) driver;
}
