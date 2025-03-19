/* ************************************************************************** */
/*                                                                            */
/*   driver.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/17 22:30:54 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt-support.h>
# include <xkrt/runtime.h>
# include <xkrt/driver/driver.h>
# include <xkrt/logger/logger.h>
# include <xkrt/min-max.h>
# include <xkrt/sync/spinlock.h>
# include <xkrt/thread/thread.h>

# include <cassert>
# include <cstring>
# include <cerrno>
# include <climits>

typedef struct  xkrt_driver_device_thread_arg_t
{
    xkrt_thread_t thread;
    xkrt_driver_type_t driver_type;
    uint8_t device_driver_id;
    void (*routine)(void * vargs, xkrt_thread_t * thread, xkrt_driver_type_t driver_type, uint8_t device_driver_id);
    void * vargs;
}               xkrt_driver_device_thread_arg_t;

static void *
trampoline(void * vargs)
{
    xkrt_driver_device_thread_arg_t * args = (xkrt_driver_device_thread_arg_t *) vargs;
    assert(args);

    // launch thread
    args->routine(args->vargs, &args->thread, args->driver_type, args->device_driver_id);

    // cannot free args here, because the main thread may `join` onto `args->thread->pthread` - TODO: fix me
    // free(args);

    return NULL;
}

static void
xkrt_driver_init(
    xkrt_drivers_t * drivers,
    int ndevices,
    void (*routine)(void * vargs, xkrt_thread_t * thread, xkrt_driver_type_t driver_type, uint8_t device_driver_id),
    void * vargs,
    xkrt_driver_type_t driver_type
) {
    xkrt_driver_t * driver = drivers->list[driver_type];
    assert(driver);

    driver->type = driver_type;
    driver->ndevices_targeted = 0;
    driver->ndevices_inited = 0;
    driver->ndevices_commited = 0;

    const char * driver_name = driver->f_get_name ? driver->f_get_name() : "(null)";
    LOGGER_INFO("Loading driver `%s`", driver_name);

    if (driver->f_init == NULL || driver->f_init(ndevices))
    {
        LOGGER_WARN("Failed to load");
        return ;
    }

    assert(driver->f_get_ndevices_max);
    unsigned int n_devices_max = driver->f_get_ndevices_max();
    LOGGER_DEBUG("Found %u devices", n_devices_max);

    unsigned int n_devices = MIN(ndevices, n_devices_max);

    driver->ndevices_targeted = n_devices;
    driver->ndevices_inited   = 0;
    driver->ndevices_commited = 0;

    if (n_devices < 1)
        return ;

    # pragma message(TODO "Move that to the 'Thread' interfaces")
    cpu_set_t save_schedset;
    xkrt_runtime_t::thread_getaffinity(save_schedset);

    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    /* init each device of that driver */
    for (uint8_t i = 0; i < n_devices; ++i)
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        // move the current thread to the device cpu set
        cpu_set_t schedset;
        assert(driver->f_device_cpuset);
        int err = driver->f_device_cpuset(topology, &schedset, i);
        if (err)
        {
            LOGGER_WARN("Invalid cpuset returned for device %d - using default cpuset", i);
            --driver->ndevices_targeted;
        }
        else
        {
            err = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &schedset);
            if (err)
            {
                LOGGER_ERROR("Invalid cpuset returned by the driver for device %d", i);
                --driver->ndevices_targeted;
                continue ;
            }
        }

        // bind the current thread before allocating
        xkrt_runtime_t::thread_setaffinity(schedset);

        // start the device thread
        xkrt_driver_device_thread_arg_t * arg = (xkrt_driver_device_thread_arg_t *) malloc(sizeof(xkrt_driver_device_thread_arg_t));
        assert(arg);
        new (&arg->thread) xkrt_thread_t(-1);
        arg->driver_type = driver_type;
        arg->device_driver_id = i;
        arg->routine = routine;
        arg->vargs = vargs;

        err = pthread_create(&arg->thread.pthread, &attr, trampoline, arg);
        if (err)
        {
            LOGGER_ERROR("could not create a thread for the device %d", i);
            --driver->ndevices_targeted;
            continue ;
        }
    }

    // move back the current thread to its initial cpu set
    xkrt_runtime_t::thread_setaffinity(save_schedset);

    hwloc_topology_destroy(topology);
}

/* initialize drivers and create 1 thread per gpu starting on the passed routine */
void
xkrt_drivers_init(
    xkrt_drivers_t * drivers,
    int ndevices,
    int drivers_mask,
    void (*routine)(void * args, xkrt_thread_t * thread, xkrt_driver_type_t driver_type, uint8_t device_driver_id),
    void * args
) {
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // SET MEMBERS
    memset(drivers->list, 0, sizeof(drivers->list));
    memset(drivers->devices.list, 0, sizeof(drivers->devices.list));
    drivers->devices.n = 0;
    drivers->devices.round_robin_device_global_id = 0;

    // LOAD DRIVERS
    xkrt_driver_t * (*creators[XKRT_DRIVER_TYPE_MAX])(void);
    memset(creators, 0, sizeof(creators));

    extern xkrt_driver_t * XKRT_DRIVER_TYPE_HOST_create_driver(void);
    creators[XKRT_DRIVER_TYPE_HOST] = XKRT_DRIVER_TYPE_HOST_create_driver;
    static_assert(XKRT_DRIVER_TYPE_HOST == 0);
    static_assert(HOST_DEVICE_GLOBAL_ID == 0);

# if XKRT_SUPPORT_CUDA
    extern xkrt_driver_t * XKRT_DRIVER_TYPE_CU_create_driver(void);
    creators[XKRT_DRIVER_TYPE_CUDA] = XKRT_DRIVER_TYPE_CU_create_driver;
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_ZE
    extern xkrt_driver_t * XKRT_DRIVER_TYPE_ZE_create_driver(void);
    creators[XKRT_DRIVER_TYPE_ZE] = XKRT_DRIVER_TYPE_ZE_create_driver;
# endif /* XKRT_SUPPORT_ZE */

# if XKRT_SUPPORT_CL
    extern xkrt_driver_t * XKRT_DRIVER_TYPE_CL_create_driver(void);
    creators[XKRT_DRIVER_TYPE_CL] = XKRT_DRIVER_TYPE_CL_create_driver;
# endif /* XKRT_SUPPORT_CL */

# if XKRT_SUPPORT_HIP
    extern xkrt_driver_t * XKRT_DRIVER_TYPE_HIP_create_driver(void);
    creators[XKRT_DRIVER_TYPE_HIP] = XKRT_DRIVER_TYPE_HIP_create_driver;
# endif /* XKRT_SUPPORT_HIP */

    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX && drivers->devices.n < ndevices ; ++driver_type)
    {
        // TODO : do not load if conf
        xkrt_driver_t * (*creator)(void) = creators[driver_type];
        if (drivers_mask & (1 << driver_type) && creator)
        {
            xkrt_driver_t * driver = creator();
            drivers->list[driver_type] = driver;

            xkrt_driver_init(drivers, ndevices - drivers->devices.n, routine, args, (xkrt_driver_type_t) driver_type);
            while (driver->ndevices_commited != driver->ndevices_targeted)
                mem_pause();

            assert(drivers->devices.n <= ndevices);
            if (drivers->devices.n == ndevices)
                break ;
        }
        else
            drivers->list[driver_type] = NULL;
    }

    // DEBUG OUTPUT
    if (drivers->devices.n == 0 && ndevices != 0)
        LOGGER_WARN("No devices found :-(");

    LOGGER_INFO("Enabled %d GPUs (with %d requested)", drivers->devices.n.load()-1, ndevices-1);
    assert(drivers->devices.n <= ndevices);
}

void
xkrt_drivers_deinit(xkrt_drivers_t * drivers)
{
    // notify each thread to stop
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < drivers->devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = drivers->devices.list[device_global_id];
        device->state = XKRT_DEVICE_STATE_STOP;
        device->thread->thread->wakeup();
    }

    // wait for each thread
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < drivers->devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = drivers->devices.list[device_global_id];
        pthread_join(device->thread->thread->pthread, NULL);
    }

    // finalize each driver
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        xkrt_driver_t * driver = drivers->list[driver_type];
        if (driver)
        {
            if (driver->f_finalize)
                driver->f_finalize();
            else
                LOGGER_WARN("Driver `%u` is missing `f_finalize`", driver_type);
        }
    }
}

extern "C"
int
xkrt_support_driver(xkrt_driver_type_t driver_type)
{
    switch (driver_type)
    {
        case (XKRT_DRIVER_TYPE_HOST):   return 1;
        case (XKRT_DRIVER_TYPE_CUDA):   return XKRT_SUPPORT_CUDA;
        case (XKRT_DRIVER_TYPE_ZE):     return XKRT_SUPPORT_ZE;
        case (XKRT_DRIVER_TYPE_CL):     return XKRT_SUPPORT_CL;
        default:                        return 0;
    }
}

extern "C"
xkrt_device_t *
xkrt_driver_device_get(xkrt_driver_t * driver, xkrt_device_global_id_t device_driver_id)
{
    assert(device_driver_id >= 0);
    assert(device_driver_id < driver->ndevices_commited);
    return driver->devices[device_driver_id];
}
