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
        cl_context context;
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

static void xkrt_cl_pfn_notify(
    const char * errinfo,
    const void * private_info,
    size_t cb,
    void * user_data
) {
    LOGGER_ERROR("CL Error occured `%s`", errinfo);
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
        // retrieve cl device ids
        cl_device_id * devices = cl_devices + cl_n_devices;
        cl_uint ndevices = ngpus - cl_n_devices;
        int err = clGetDeviceIDs(cl_platforms[i], CL_DEVICE_TYPE_GPU, ndevices, devices, &ndevices);
        if (err == CL_DEVICE_NOT_FOUND)
            continue ;
        CL_SAFE_CALL(err);
        assert(0 <= ndevices);
        assert(ndevices <= ngpus - cl_n_devices);

        // create a context for all these platform devices
        err;
        const cl_context_properties properties[] = {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)cl_platforms[i],
            0 // end of properties
        };
        cl_context context = clCreateContext(properties, ndevices, devices, xkrt_cl_pfn_notify, NULL, &err);
        CL_SAFE_CALL(err);

        for (cl_uint j = 0; j < ndevices ; ++j)
        {
            xkrt_device_cl_t * device = DEVICES + cl_n_devices;
            device->cl.id = devices[j];
            device->cl.context = context;
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

    // nothing to do

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

    char name[64];
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_NAME, sizeof(name), name, NULL));

    char vendor[64];
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_VENDOR, sizeof(vendor), vendor, NULL));

    cl_ulong max_mem_alloc_size;
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_mem_alloc_size, NULL));

    snprintf(
        buffer,
        sizeof(buffer),
        "XKRT device %d - OpenCL device %d - named %s of vendor %s - max-mem-alloc-size=%.2lfGB",
        device_driver_id,
        device->cl.id,
        name,
        vendor,
        max_mem_alloc_size / 1e9
    );
    return buffer;
}

////////////
// STREAM //
////////////

static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;
    assert(stream);

    assert(istream->is_locked());
    const xkrt_stream_instruction_counter_t wp = istream->pending.pos.w % istream->pending.capacity;
    cl_event event = stream->cl.events[wp];

    switch (instr->type)
    {
        case (XKRT_STREAM_INSTR_TYPE_KERN):
        {
            assert(istream->type == XKRT_STREAM_TYPE_KERN);

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
            size_t dst_pitch    = instr->copy.dst_device_view.ld * instr->copy.host_view.sizeof_type;
            const void * src    = (const void *) instr->copy.src_device_view.addr;
            size_t src_pitch    = instr->copy.src_device_view.ld * instr->copy.host_view.sizeof_type;

            // assume col major - if not, need to do some shit here
            assert(instr->copy.host_view.order == MATRIX_COLMAJOR);
            const size_t width  = instr->copy.host_view.m * instr->copy.host_view.sizeof_type;
            const size_t height = instr->copy.host_view.n;
            assert(width >= 0);
            assert(height >= 0);

            switch (instr->type)
            {
                case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
                {
                    // TODO : how to send memory from 'dst' that points on the device ?
                    // this code is wrong
                    int err;
                    cl_mem dst_buffer = clCreateBuffer(stream->cl.context, CL_MEM_READ_WRITE, width*height, dst, &err);
                    CL_SAFE_CALL(err);

                    cl_bool blocking_write = 0;

                    const size_t dst_origin[] = {0, 0, 0};
                    const size_t src_origin[] = {0, 0, 0};
                    const size_t region[]     = {width, height, 1};

                    const size_t dst_slice_pitch = 0;
                    const size_t src_slice_pitch = 0;

                    cl_uint num_events_in_wait_list = 0;
                    const cl_event * event_wait_list = NULL;

                    CL_SAFE_CALL(
                        clEnqueueWriteBufferRect(
                            stream->cl.queue,
                            dst_buffer,
                            blocking_write,
                            dst_origin,
                            src_origin,
                            region,
                            dst_pitch,
                            dst_slice_pitch,
                            src_pitch,
                            src_slice_pitch,
                            src,
                            num_events_in_wait_list,
                            event_wait_list,
                           &event
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
    assert(stream);

    CL_SAFE_CALL(clFinish(stream->cl.queue));
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
            LOGGER_FATAL("IMPL ME");
            # if 0
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
            # endif

            return EINPROGRESS;
        }

        default:
            LOGGER_FATAL("Wrong instruction");
    }

    return EINPROGRESS;
}


static xkrt_stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    xkrt_device_t * idevice,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity
) {
    assert(idevice);

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkrt_stream_cl_t) + sizeof(cl_event) * capacity);
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

    // TODO : no control over the queue type with OpenCL
    (void) type;

    // create a queue
    const cl_queue_properties properties[] = {
        CL_QUEUE_PROPERTIES,
        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,    // not sure why we must specify 'CL_QUEUE_ON_DEVICE' ?
        CL_QUEUE_SIZE,
        CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,                       // default parameter
        0                                                               // end of properties
    };
    int err;
    stream->cl.queue = clCreateCommandQueueWithProperties(device->cl.context, device->cl.id, 0, &err);
    CL_SAFE_CALL(err);

    // create events
    stream->cl.events = (cl_event *) (stream + 1);
    for (xkrt_stream_instruction_counter_t i = 0 ; i < capacity ; ++i)
    {
        int err;
        stream->cl.events[i] = clCreateUserEvent(device->cl.context, &err);
        CL_SAFE_CALL(err);
    }

    // save context for later buffer use
    stream->cl.context = device->cl.context;

    return (xkrt_stream_t *) stream;
}

static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    xkrt_stream_cl_t * stream = (xkrt_stream_cl_t *) istream;

    for (xkrt_stream_instruction_counter_t i = 0 ; i < istream->pending.capacity ; ++i)
        CL_SAFE_CALL(clReleaseEvent(stream->cl.events[i]));

    CL_SAFE_CALL(clReleaseCommandQueue(stream->cl.queue));

    free(stream);
}

////////////
// MEMORY //
////////////

static void *
XKRT_DRIVER_ENTRYPOINT(memory_alloc)(int device_driver_id, const size_t size)
{
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);
    int err;
    void * ptr = clCreateBuffer(device->cl.context, CL_MEM_READ_WRITE, size, NULL, &err);
    CL_SAFE_CALL(err);
    return ptr;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_info)(int device_driver_id, size_t * total)
{
    xkrt_device_cl_t * device = device_cl_get(device_driver_id);

    cl_ulong max_mem_alloc_size;
    CL_SAFE_CALL(clGetDeviceInfo(device->cl.id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_mem_alloc_size, NULL));
    *total = (size_t) max_mem_alloc_size;
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
    // EP(device_attach);   // no "state machine" with opencl, no need to attach to a device
    EP(device_commit);
    EP(device_info);

    EP(memory_alloc);
    EP(memory_info);

    EP(stream_create);
    EP(stream_delete);

# if 0
    // EP(memory_register);
    // EP(memory_unregister);

    // EP(get_source);

    # undef EP

# endif /* 0 */
}
