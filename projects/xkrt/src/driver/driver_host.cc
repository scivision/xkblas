/* ************************************************************************** */
/*                                                                            */
/*   driver_host.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/10 17:00:08 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 19:14:53 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# define XKRT_DRIVER_ENTRYPOINT(N) XKRT_DRIVER_TYPE_HOST_ ## N

# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/mutex.h>

# include <hwloc.h>
# include <hwloc/glibc-sched.h>
# include <sys/sysinfo.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>
# include <functional>

static int
XKRT_DRIVER_ENTRYPOINT(init)(
    unsigned int ndevices,
    bool use_p2p
) {
    (void) ndevices;
    return 0;
}

void
XKRT_DRIVER_ENTRYPOINT(device_info)(
    int device_driver_id,
    char * buffer,
    size_t size
) {
    (void) device_driver_id;

    // Initialize and load topology
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    // Get the first PU (Processing Unit) and move up to the package (CPU)
    hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PACKAGE, 0);
    if (obj && obj->name)
        snprintf(buffer, size, "%s", obj->name);
    else
        snprintf(buffer, size, "Unknown CPU");

    // Destroy topology
    hwloc_topology_destroy(topology);
}

static void
XKRT_DRIVER_ENTRYPOINT(finalize)(void)
{
}

static const char *
XKRT_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "HOST";
}

static unsigned int
XKRT_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return 1;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_cpuset)(
    hwloc_topology_t topology,
    cpu_set_t * schedset,
    int device_driver_id
) {
    (void) topology;
    assert(device_driver_id == 0);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), schedset);
    return 0;
}

static xkrt_device_t *
XKRT_DRIVER_ENTRYPOINT(device_create)(xkrt_driver_t * driver, int device_driver_id)
{
    (void) driver;
    assert(device_driver_id == 0);
    static xkrt_device_t device;
    return &device;
}

static void
XKRT_DRIVER_ENTRYPOINT(device_init)(int device_driver_id)
{
    (void) device_driver_id;
}

static int
XKRT_DRIVER_ENTRYPOINT(device_destroy)(int device_driver_id)
{
    (void) device_driver_id;
    return 0;
}

/* Called for each device of the driver once they all have been initialized */
static int
XKRT_DRIVER_ENTRYPOINT(device_commit)(int device_driver_id, xkrt_device_global_id_bitfield_t * affinity)
{
    (void) device_driver_id;
    (void) affinity;
    return 0;
}

////////////
// STREAM //
////////////

# if 0
static int
XKRT_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    (void) istream;
    (void) instr;
    (void) idx;
    return 0;
}
# endif

static int
XKRT_DRIVER_ENTRYPOINT(stream_suggest)(
    int device_driver_id,
    xkrt_stream_type_t stype
) {
    (void) stype;
    assert(device_driver_id == 0);
    return 0;
}

# if 0
static int
XKRT_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    xkrt_stream_t * istream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    (void)istream;
    (void)instr;
    (void)idx;
    assert(0);
    return EINPROGRESS;
}
# endif

# if 0
static xkrt_stream_t *
XKRT_DRIVER_ENTRYPOINT(stream_create)(
    xkrt_device_t * idevice,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity
) {
    (void)idevice;
    (void)type;
    (void)capacity;
    assert(0);
    return NULL;
}
# endif

# if 0
static void
XKRT_DRIVER_ENTRYPOINT(stream_delete)(
    xkrt_stream_t * istream
) {
    (void)istream;
    assert(0);
}
# endif

////////////
// MEMORY //
////////////

# if 0
static void *
XKRT_DRIVER_ENTRYPOINT(memory_device_allocate)(int device_driver_id, const size_t size, int area_idx)
{
    (void)device_driver_id;
    (void)size;
    (void)area_idx;
    return NULL;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_deallocate)(int device_driver_id, void * ptr, const size_t size, int area_idx)
{
    (void)device_driver_id;
    (void)ptr;
    (void)size;
    (void)area_idx;
}
# endif

static void
XKRT_DRIVER_ENTRYPOINT(memory_device_info)(
    int device_driver_id,
    xkrt_device_memory_info_t info[XKRT_DEVICE_MEMORIES_MAX],
    int * nmemories
) {
    (void)device_driver_id;
    assert(device_driver_id == 0);

    struct sysinfo sinfo;

    if (sysinfo(&sinfo) == 0)
    {
        const int i = 0;
        strncpy(info[i].name, "RAM", sizeof(info[i].name));
        info[i].used     = sinfo.totalram - sinfo.freeram;
        info[i].capacity = sinfo.totalram;
        *nmemories = 1;
    }
    else
    {
        *nmemories = 0;
    }
}

# if 0
static void *
XKRT_DRIVER_ENTRYPOINT(memory_host_allocate)(
    int device_driver_id,
    uint64_t size
) {
    (void)device_driver_id;
    (void)size;
    return NULL;
}

static void
XKRT_DRIVER_ENTRYPOINT(memory_host_deallocate)(
    int device_driver_id,
    void * mem,
    uint64_t size
) {
    (void)device_driver_id;
    (void)mem;
    (void)size;
}

xkrt_driver_module_t
XKRT_DRIVER_ENTRYPOINT(module_load)(
    int device_driver_id,
    uint8_t * bin,
    size_t binsize,
    xkrt_driver_module_format_t format
) {
    (void)device_driver_id;
    (void)bin;
    (void)binsize;
    (void)format;
    return NULL;
}

void
XKRT_DRIVER_ENTRYPOINT(module_unload)(
    xkrt_driver_module_t module
) {
    (void)module;
}

xkrt_driver_module_fn_t
XKRT_DRIVER_ENTRYPOINT(module_get_fn)(
    xkrt_driver_module_t module,
    const char * name
) {
    (void)module;
    (void)name;
    return NULL;
}
# endif

//////////////////////////
// Routine registration //
//////////////////////////
xkrt_driver_t *
XKRT_DRIVER_ENTRYPOINT(create_driver)(void)
{
    xkrt_driver_t * driver = (xkrt_driver_t *) calloc(1, sizeof(xkrt_driver_t));
    assert(driver);

    # define REGISTER(func) driver->f_##func = XKRT_DRIVER_ENTRYPOINT(func)

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
 // REGISTER(memory_device_allocate);
 // REGISTER(memory_device_deallocate);
 // REGISTER(memory_host_allocate);
 // REGISTER(memory_host_deallocate);
 // REGISTER(memory_host_register);
 // REGISTER(memory_host_unregister);
 // REGISTER(memory_unified_allocate);
 // REGISTER(memory_unified_deallocate);

    REGISTER(device_cpuset);

    REGISTER(stream_suggest);
 // REGISTER(stream_create);
 // REGISTER(stream_delete);

 // REGISTER(module_load);
 // REGISTER(module_unload);
 // REGISTER(module_get_fn);

    # undef REGISTER

    return (xkrt_driver_t *) driver;
}
