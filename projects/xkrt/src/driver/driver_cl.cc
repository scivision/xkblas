/* ************************************************************************** */
/*                                                                            */
/*   driver_cl.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:57:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

/* see https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html */

# define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_CL_ ## N

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/driver/cublas-helper.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>

# include <xkrt/driver/driver-cl.h>
# include <xkrt/logger/logger-cl.h>

# include <CL/cl.h>
# include <hwloc/opencl.h>
# include <hwloc/glibc-sched.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>
# include <functional>

// plateforms
static cl_platform_id cl_platforms[XKRT_DEVICES_MAX];
static cl_device_id cl_devices[XKRT_DEVICES_MAX];

// devices
typedef struct  xkrt_device_cl_t
{
    xkrt_device_t inherited;

    struct {
        cl_device_id id;
    } cl;
}               xkrt_device_cl_t;


static xkrt_device_cl_t DEVICES[XKRT_DEVICES_MAX];
static cl_uint cl_n_devices = 0;

static xkrt_device_cl_t *
device_cl_get(int device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < cl_n_devices);
    return DEVICES + device_driver_id;
}

static int
XKRT_DRIVER_ENTRYPOINT(init)(unsigned int ngpus)
{
    assert(0 < ngpus);
    assert(ngpus <= XKRT_DEVICES_MAX);

    // get all drivers
    cl_uint cl_n_platforms = ngpus; // i believe cl ensure at least 1 device per platform ?
    CL_SAFE_CALL(clGetPlatformIDs(cl_n_platforms, cl_platforms, &cl_n_platforms));
    assert(0 <= cl_n_platforms);
    assert(cl_n_platforms <= ngpus);

    for (cl_uint i = 0; i < cl_n_platforms; ++i)
    {
        cl_device_id * devices = cl_devices + cl_n_devices;
        cl_uint ndevices = ngpus - cl_n_devices;
        int err = clGetDeviceIDs(cl_platforms[i], CL_DEVICE_TYPE_GPU, ndevices, devices, &ndevices);
        if (err == CL_DEVICE_NOT_FOUND)
            continue ;
        CL_SAFE_CALL(err);

        assert(0 <= ndevices);
        assert(ndevices <= ngpus - cl_n_devices);

        for (cl_uint j = 0; j < ndevices ; ++j)
        {
            xkrt_device_cl_t * device = DEVICES + cl_n_devices;
            device->cl.id = devices[j];
            ++cl_n_devices;
        }
    }

    return 0;
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{

}

static const char *
XKRT_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "CL";
}

static unsigned int
XKRT_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return cl_n_devices;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_set_cpuset)(hwloc_topology_t topology, cpu_set_t * schedset, int device_driver_id)
{
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    HWLOC_SAFE_CALL(hwloc_opencl_get_device_cpuset(topology, device->cl.id, cpuset));
    CPU_ZERO(schedset);
    HWLOC_SAFE_CALL(hwloc_cpuset_to_glibc_sched_affinity(topology, cpuset, schedset, sizeof(cpu_set_t)));

    hwloc_bitmap_free(cpuset);
    return 0;
}

static xkrt_device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(xkrt_driver_t * driver, int device_driver_id)
{
    assert(device_driver_id >= 0 && device_driver_id < XKRT_DEVICES_MAX);

    xkrt_device_cl_t * device = device_cl_get(device_driver_id);
    assert(device->inherited.state == XKRT_DEVICE_STATE_DEALLOCATED);

    # if 0
    // query number of command queue groups
    CL_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device->cl_device, &device->ncommandqueuegroups, NULL));
    assert(device->ncommandqueuegroups);

    // query each group
    device->cl_command_queue_group_properties = (cl_command_queue_group_properties_t *) malloc(sizeof(cl_command_queue_group_properties_t) * device->ncommandqueuegroups);
    assert(device->cl_command_queue_group_properties);
    CL_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device->cl_device, &device->ncommandqueuegroups, device->cl_command_queue_group_properties));

    device->cl_command_queue_group_used = new std::atomic<cl_uint>[device->ncommandqueuegroups];
    assert(device->cl_command_queue_group_used);
    for (cl_uint i = 0 ; i < device->ncommandqueuegroups ; ++i)
        device->cl_command_queue_group_used[i].store(0);
    # endif

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

const char *
XKRT_DRIVER_ENTRYPOINT(device_info)(int device_driver_id)
{
    static char buffer[512];

    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    snprintf(
        buffer,
        sizeof(buffer),
        "XKRT device %d - OpenCL device %d",
        device_driver_id,
        device->cl.id
    );
    return buffer;
}

# if 0

// TODO : can be make this member of a 'xkrt_driver_cl_t' ?  most likely yes,
// but cuda state machine would make it hard to maintain for cuda as well. Keep
// them as global variable for now, there should only be 1 instances of a
// driver right now

static cl_driver_handle_t   cl_drivers[XKRT_DEVICES_MAX];
static cl_context_handle_t  cl_contextes[XKRT_DEVICES_MAX];

typedef struct  xkrt_device_cl_t
{
    xkrt_device_t inherited;

    cl_driver_handle_t      cl_driver;
    cl_context_handle_t     cl_context;
    cl_device_handle_t      cl_device;
    cl_device_properties_t  cl_device_properties;

    // number of command queue group
    cl_uint ncommandqueuegroups;

    // per command queue group property
    cl_command_queue_group_properties_t * cl_command_queue_group_properties;

    // per command queue number of queue used
    std::atomic<cl_uint>               * cl_command_queue_group_used;

}               xkrt_device_cl_t;

static xkrt_device_cl_t DEVICES[XKRT_DEVICES_MAX];
static cl_uint cl_n_devices = 0;



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
    const cl_init_flags_t flags = CL_INIT_FLAG_GPU_ONLY;
    const cl_result_t r = zeInit(flags);
    # else
    cl_init_driver_type_desc_t desc = {CL_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.pNext = nullptr;
    desc.driverType = CL_INIT_FLAG_GPU_ONLY;
    cl_uint driverCount = 0;
    const cl_result_t r = zeInitDrivers(&driverCount, nullptr, &desc);
    # endif

    if (r != CL_RESULT_SUCCESS)
        return 1;

    // get all drivers
    cl_uint cl_n_drivers = ngpus; // i believe Intel API ensure at least 1 gpu per driver ?
    CL_SAFE_CALL(zeDriverGet(&cl_n_drivers, cl_drivers));

    // get all device handles per driver
    for (unsigned int cl_driver_id = 0 ; cl_driver_id < cl_n_drivers && cl_n_devices < ngpus ; ++cl_driver_id)
    {
        // Create context for driver
        cl_context_desc_t cl_context_desc = {
            .stype = CL_STRUCTURE_TYPE_CONTEXT_DESC,
            .pNext = NULL,
            .flags = CL_CONTEXT_FLAG_TBD
        };
        CL_SAFE_CALL(zeContextCreate(cl_drivers[cl_driver_id], &cl_context_desc, cl_contextes + cl_driver_id));

        // get devices handles
        cl_uint ndevices = ngpus - cl_n_devices;
        cl_device_handle_t cl_devices[XKRT_DEVICES_MAX];
        CL_SAFE_CALL(zeDeviceGet(cl_drivers[cl_driver_id], &ndevices, cl_devices));

        for (unsigned int i = 0 ; i < ndevices ; ++i)
        {
            cl_device_handle_t cl_device = cl_devices[i];

            # if SUBDEVICE_TO_XKRT_DEVICE
            cl_device_handle_t cl_subdevices[XKRT_DEVICES_MAX];
            cl_uint nsubdevices = ngpus - cl_n_devices;
            zeDeviceGetSubDevices(cl_device, &nsubdevices, cl_subdevices);

            for (unsigned int j = 0; j < nsubdevices ; ++j)
            {
                cl_device_handle_t cl_device = cl_subdevices[j];
            # endif

                xkrt_device_cl_t * device = DEVICES + cl_n_devices;

                // save handles
                device->cl_context = cl_contextes[cl_driver_id];
                device->cl_driver  = cl_drivers[cl_driver_id];
                device->cl_device  = cl_device;

                // get subdevice properties
                device->cl_device_properties.stype = CL_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                CL_SAFE_CALL(zeDeviceGetProperties(device->cl_device, &device->cl_device_properties));

                if (++cl_n_devices == ngpus)
                    return 0;

            # if SUBDEVICE_TO_XKRT_DEVICE
            }
            # endif
        }
    }

    return 0;
}


////////////////
// STREAM //
/////////////

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;
    assert(stream);

    assert(istream->is_locked());
    const xkrt_stream_instruction_counter_t wp = istream->pending.pos.w % istream->pending.capacity;
    cl_event_handle_t cl_event_handle = stream->ze.events.list[wp];

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_KERN):
        {
            assert(istream->type == XKRT_STREAM_TYPE_KERN);
            assert(stream->ze.command.list);

            xkrt_stream_instruction_kernel_t * op = &instr->kern;
            op->launch(istream, op->vargs);

            return EINPROGRESS;

        } /* XKRT_STREAM_INSTR_TYPE_KERN */

        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D):
        {
            void * dst          = (void *) instr->copy.dst_device_view.addr;
            sicl_t dst_pitch    = instr->copy.dst_device_view.ld * instr->copy.host_view.sizeof_type;
            const void * src    = (const void *) instr->copy.src_device_view.addr;
            sicl_t src_pitch    = instr->copy.src_device_view.ld * instr->copy.host_view.sizeof_type;

            // assume col major for ze - if not, need to do some shit here
            assert(instr->copy.host_view.order == MATRIX_COLMAJOR);
            const sicl_t width  = instr->copy.host_view.m * instr->copy.host_view.sizeof_type;
            const sicl_t height = instr->copy.host_view.n;
            assert(width >= 0);
            assert(height >= 0);

            switch (instr->type)
            {
                case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
                {
                    const cl_uint dst_slice_pitch = 0;
                    const cl_copy_region_t dst_region = {
                        .originX = 0,
                        .originY = 0,
                        .originZ = 0,
                        .width   = (cl_uint) width,
                        .height  = (cl_uint) height,
                        .depth   = 0
                    };

                    const cl_uint src_slice_pitch = 0;
                    const cl_copy_region_t src_region = {
                        .originX = 0,
                        .originY = 0,
                        .originZ = 0,
                        .width   = (cl_uint) width,
                        .height  = (cl_uint) height,
                        .depth   = 0
                    };

                    const cl_uint num_wait_events = 0;
                    cl_event_handle_t * wait_events = NULL;

                    CL_SAFE_CALL(
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
                            cl_event_handle,
                            num_wait_events,
                            wait_events
                        )
                    );

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
XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)(
    xkrt_stream_t * istream
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;

    const uint64_t timeout = 0;
    CL_SAFE_CALL(zeCommandListHostSynchronize(stream->ze.command.list, timeout));
    return 0;
}

static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx

) {
    assert(istream);

    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_KERN):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D):
        {
            cl_result_t res;

            /* poll event */
            for (int i = 0 ; i < 16 ; ++i)
            {
                res = zeEventQueryStatus(stream->ze.events.list[idx]);
                if (res == CL_RESULT_NOT_READY)
                    sched_yield();
                else if (res == CL_RESULT_SUCCESS)
                    return res;
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
static inline cl_uint
device_command_queue_group_next(
    xkrt_device_cl_t * device,
    const cl_command_queue_group_property_flag_t & flag
) {
    cl_uint ordinal_with_least_queues = UINT32_MAX;
    cl_uint min_queues = UINT32_MAX;

    for (cl_uint i = 0; i < device->ncommandqueuegroups; ++i)
    {
        cl_command_queue_group_properties_t * properties = device->cl_command_queue_group_properties + i;
        if (f((const cl_command_queue_group_property_flag_t &) properties->flags, flag))
        {
            const cl_uint used = device->cl_command_queue_group_used[i].load();
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

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkrt_stream_cl_t));
    assert(mem);

    xkrt_stream_init(
        (xkrt_stream_t *) mem,
        type,
        capacity,
        XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress),
        XKRT_DRIVER_ENTRYPOINT(stream_instructions_wait)
    );

    xkrt_device_cl_t * device = (xkrt_device_cl_t *) idevice;
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) mem;

    // convert xkrt stream type to a command queue group flag
    cl_command_queue_group_property_flag_t flag;
    switch (type)
    {
        case (XKRT_STREAM_TYPE_H2D):
        case (XKRT_STREAM_TYPE_D2H):
        case (XKRT_STREAM_TYPE_D2D):
        {
            flag = CL_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
            break ;
        }

        case (XKRT_STREAM_TYPE_KERN):
        {
            flag = CL_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE;
            break ;
        }

        default:
            LOGGER_FATAL("Unknown stream type");
    }

    // create command queue
    cl_uint ordinal = device_command_queue_group_next<cl_command_queue_group_property_flag_t, f_equals>(device, flag);
    if (ordinal == UINT32_MAX)
    {
        ordinal = device_command_queue_group_next<cl_command_queue_group_property_flag_t, f_and>(device, flag);
        if (ordinal == UINT32_MAX)
            LOGGER_FATAL("No command queue group available for stream");
    }

    // retrieve group properties
    const cl_command_queue_group_properties_t * properties = device->cl_command_queue_group_properties + ordinal;
    cl_uint index = device->cl_command_queue_group_used[ordinal].fetch_add(1) % properties->numQueues;

    // get the next command queue index to use in the group
    const cl_command_queue_desc_t cl_command_queue_desc = {
        .stype      = CL_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext      = NULL,
        .ordinal    = ordinal,
        .index      = index,
        .flags      = CL_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,      // TODO : i think we want explicit ?
        .mode       = CL_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        .priority   = CL_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW    // TODO : maybe do one queue for each priority in 'device->cl_device_properties.maxCommandQueuePriority'
    };
    # if 0
    CL_SAFE_CALL(
        zeCommandQueueCreate(
            device->cl_context,
            device->cl_device,
           &cl_command_queue_desc,
           &stream->ze.command.queue
        )
    );
    # endif

    // create command list
    # if 0
    cl_command_list_desc_t cl_command_list_desc = {
        .stype = CL_STRUCTURE_TYPE_COMMAND_LIST_DESC,
        .pNext = NULL,
        .commandQueueGroupOrdinal = ordinal,
        .flags = CL_COMMAND_LIST_FLAG_RELAXED_ORDERING | CL_COMMAND_LIST_FLAG_MAXIMICL_THROUGHPUT
    };
    # endif
    CL_SAFE_CALL(
        zeCommandListCreateImmediate(
            device->cl_context,
            device->cl_device,
           &cl_command_queue_desc,
           &stream->ze.command.list
        )
    );

    // create event pool and events
    const cl_event_pool_desc_t cl_event_pool_desc = {
        .stype  = CL_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext  = NULL,
        .flags  = CL_EVENT_POOL_FLAG_HOST_VISIBLE,
        .count  = capacity
    };
    const cl_uint ndevices = 1;
    CL_SAFE_CALL(zeEventPoolCreate(device->cl_context, &cl_event_pool_desc, ndevices, &device->cl_device, &stream->ze.events.pool));

    cl_event_desc_t event_desc = {
        .stype  = CL_STRUCTURE_TYPE_EVENT_DESC,
        .signal = CL_EVENT_SCOPE_FLAG_HOST,
        .wait   = CL_EVENT_SCOPE_FLAG_HOST
    };
    stream->ze.events.list = (cl_event_handle_t *) malloc(sizeof(cl_event_handle_t) * capacity);
    assert(stream->ze.events.list);
    for (xkrt_stream_instruction_counter_t i = 0 ; i < capacity ; ++i)
        CL_SAFE_CALL(zeEventCreate(stream->ze.events.pool, &event_desc, stream->ze.events.list + i));

    return (xkrt_stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;
    free(stream);
}

////////////
// MEMORY //
////////////

static void *
XKRT_DRIVER_ENTRYPOINT(memory_alloc)(int device_driver_id, const sicl_t size)
{
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    # if 0
    const cl_device_mem_alloc_desc_t device_desc = {
        .stype = CL_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES,
        .pNext = NULL,
        .flags = 0,
        .ordinal = 0
    };
    const sicl_t alignment = 8;
    void * device_ptr = NULL;
    CL_SAFE_CALL(zeMemAllocDevice(device->cl_context, &device_desc, size, alignment, device->cl_device, &device_ptr));
    CL_SAFE_CALL(zeContextMakeMemoryResident(device->cl_context, device->cl_device, device_ptr, size));
    # else

    // Query page size for our allocation
    sicl_t pagesize;
    CL_SAFE_CALL(
        zeVirtualMemQueryPageSize(
            device->cl_context,
            device->cl_device,
            size,
           &pagesize
        )
    );

    // Align size and reserve virtual address space.
    const sicl_t reserve_size = size + (pagesize - (size % pagesize));
    void * device_ptr = NULL;
    CL_SAFE_CALL(
        zeVirtualMemReserve(
            device->cl_context,
            NULL,
            reserve_size,
           &device_ptr
        )
    );
    assert(device_ptr);

    // Create physical memory
    cl_physical_mem_desc_t cl_physical_mem_desc = {
        .stype = CL_STRUCTURE_TYPE_PHYSICAL_MEM_DESC,
        .pNext = NULL,
        .flags = CL_PHYSICAL_MEM_FLAG_TBD,
        .size  = reserve_size
    };
    cl_physical_mem_handle_t cl_physical_mem_handle;
    CL_SAFE_CALL(
        zePhysicalMemCreate(
            device->cl_context,
            device->cl_device,
           &cl_physical_mem_desc,
           &cl_physical_mem_handle
        )
    );

    // Map virtual to physical memory
    const cl_memory_access_attribute_t cl_memory_access_attribute = CL_MEMORY_ACCESS_ATTRIBUTE_READWRITE;
    const sicl_t offset = 0;
    CL_SAFE_CALL(
        zeVirtualMemMap(
            device->cl_context,
            device_ptr,
            size,
            cl_physical_mem_handle,
            offset,
            cl_memory_access_attribute
        )
    );
    # endif

    CL_SAFE_CALL(
        zeContextMakeMemoryResident(
            device->cl_context,
            device->cl_device,
            device_ptr, size
        )
    );

    return device_ptr;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_info)(int device_driver_id, sicl_t * total)
{
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);
    *total = device->cl_device_properties.maxMemAllocSize;
}

# endif /* 0 */

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

# if 0
    EP(memory_alloc);
    EP(memory_info);
    // EP(memory_register);
    // EP(memory_unregister);

    EP(stream_create);
    EP(stream_delete);

    // EP(get_source);

    # undef EP

# endif /* 0 */
}
