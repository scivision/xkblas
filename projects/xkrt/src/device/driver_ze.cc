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

# include <xkrt/hwloc.h>
# include <xkrt/runtime.h>
# include <xkrt/conf/conf.h>
# include <xkrt/device/cublas-helper.h>
# include <xkrt/device/device.h>
# include <xkrt/device/driver.h>
# include <xkrt/device/stream.h>
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

static ze_driver_handle_t  * ze_drivers_handle = NULL;
static uint32_t ze_n_drivers = 0;

static ze_device_handle_t ** ze_devices_handle_per_driver   = NULL;
static ze_device_handle_t  * ze_devices_handle              = NULL;

static uint32_t ze_n_devices = 0;

static int
XKRT_DRIVER_ENTRYPOINT(init)(void)
{
    // zeInit got deprecated, use other ifdef depending on version but cba
    // intel mess, so only deprecated is implemented atm
    # if 1

    const ze_init_flags_t flags = ZE_INIT_FLAG_GPU_ONLY;
    const ze_result_t r = zeInit(flags);

    if (r != ZE_RESULT_SUCCESS)
        return 1;

    ZE_SAFE_CALL(zeDriverGet(&ze_n_drivers, NULL));
    ze_drivers_handle = (ze_driver_handle_t *) malloc(sizeof(ze_driver_handle_t));
    assert(ze_drivers_handle);
    ZE_SAFE_CALL(zeDriverGet(&ze_n_drivers, ze_drivers_handle));

    ze_devices_handle_per_driver = (ze_device_handle_t **) malloc(sizeof(ze_device_handle_t *) * ze_n_drivers);

    for (unsigned int i = 0 ; i < ze_n_drivers ; ++i)
    {
        uint32_t ndevices = 0;
        ZE_SAFE_CALL(zeDeviceGet(ze_drivers_handle[i], &ndevices, NULL));
        ze_devices_handle_per_driver[i] = (ze_device_handle_t *) malloc(sizeof(ze_device_handle_t) * ndevices);
        assert(ze_devices_handle_per_driver[i]);
        ZE_SAFE_CALL(zeDeviceGet(ze_drivers_handle[i], &ndevices, ze_devices_handle_per_driver[i]));
        ze_n_devices += ndevices;
    }

    uint32_t offset = 0;
    ze_devices_handle = (ze_device_handle_t *) malloc(sizeof(ze_device_handle_t) * ze_n_devices);
    assert(ze_devices_handle);
    for (unsigned int i = 0 ; i < ze_n_drivers ; ++i)
    {
        uint32_t ndevices = 0;
        ZE_SAFE_CALL(zeDeviceGet(ze_drivers_handle[i], &ndevices, NULL));
        ZE_SAFE_CALL(zeDeviceGet(ze_drivers_handle[i], &ndevices, ze_devices_handle + offset));
        offset += ndevices;
    }

    # else
    ze_init_driver_type_desc_t desc = {ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC};
    desc.pNext = nullptr;
    desc.driverType = ZE_INIT_FLAG_GPU_ONLY;
    uint32_t driverCount = 0;
    const ze_result_t r = zeInitDrivers(&driverCount, nullptr, &desc);
    # endif

    return 0;
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
XKRT_DRIVER_ENTRYPOINT(device_set_cpuset)(cpu_set_t * schedset, int device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < ze_n_devices);

    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    HWLOC_SAFE_CALL(hwloc_levelzero_get_device_cpuset(XKRT_HWLOC_TOPOLOGY, ze_devices_handle[device_driver_id], cpuset));
    CPU_ZERO(schedset);
    HWLOC_SAFE_CALL(hwloc_cpuset_to_glibc_sched_affinity(XKRT_HWLOC_TOPOLOGY, cpuset, schedset, sizeof(cpu_set_t)));

    hwloc_bitmap_free(cpuset);
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

    // EP(device_create);
    // EP(device_destroy);
    // EP(device_init);
    // EP(device_attach);
    // EP(device_commit);
    // EP(device_info);

    // EP(memory_register);
    // EP(memory_unregister);

    // EP(stream_create);
    // EP(stream_delete);

    // EP(get_source);

    # undef EP
}
