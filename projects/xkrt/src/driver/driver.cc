/* ************************************************************************** */
/*                                                                            */
/*   driver.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/18 14:54:33 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt-support.h>
# include <xkrt/driver/driver.h>
# include <xkrt/logger/logger.h>
# include <xkrt/min-max.h>
# include <xkrt/sync/spinlock.h>

# include <cassert>
# include <cstring>
# include <cerrno>
# include <climits>

typedef struct  xkrt_driver_device_thread_arg_t
{
    xkrt_driver_type_t driver_type;
    uint8_t device_driver_type;
    void (*routine)(void * vargs, xkrt_driver_type_t driver_type, uint8_t device_driver_type);
    void * vargs;
}               xkrt_driver_device_thread_arg_t;

static void *
trampoline(void * vargs)
{
    xkrt_driver_device_thread_arg_t * args = (xkrt_driver_device_thread_arg_t *) vargs;
    args->routine(args->vargs, args->driver_type, args->device_driver_type);
    free(args);
    return NULL;
}

static inline void
bindto(cpu_set_t * cpuset)
{
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), cpuset);
    for (int ii=0; ii<10; ++ii) sched_yield();
}

static void
xkrt_driver_init(
    xkrt_drivers_t * drivers,
    uint8_t ngpus,
    void (*routine)(void * vargs, xkrt_driver_type_t driver_type, uint8_t device_driver_type),
    void * vargs,
    xkrt_driver_type_t driver_type
) {
    xkrt_driver_t * driver = drivers->list + driver_type;

    const char * driver_name = driver->f_get_name ? driver->f_get_name() : "(null)";
    LOGGER_INFO("Loading driver `%s`", driver_name);

    if (driver->f_init == NULL || driver->f_init(ngpus))
    {
        LOGGER_WARN("Failed to load");
        return ;
    }

    assert(driver->f_get_ndevices_max);
    unsigned int n_devices_max = driver->f_get_ndevices_max();
    LOGGER_DEBUG("Found %u devices", n_devices_max);

    unsigned int n_devices = MIN(ngpus, n_devices_max);
    if (n_devices < 1)
        return ;
    driver->ndevices_targeted = n_devices;

    # pragma message(TODO "Move that to the 'Thread' interfaces")
    cpu_set_t save_schedset;
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);

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
        assert(driver->f_device_set_cpuset);
        int err = driver->f_device_set_cpuset(topology, &schedset, i);
        if (err)
        {
            LOGGER_WARN("Invalid cpuset returned for device %d - using default cpuset", i);
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

        bindto(&schedset);

        // start the device thread
        xkrt_driver_device_thread_arg_t * arg = (xkrt_driver_device_thread_arg_t *) malloc(sizeof(xkrt_driver_device_thread_arg_t));
        assert(arg);
        arg->routine = routine;
        arg->vargs = vargs;
        arg->driver_type = driver_type;
        arg->device_driver_type = i;

        pthread_t thread;
        err = pthread_create(&thread, &attr, trampoline, arg);
        if (err)
        {
            LOGGER_ERROR("could not create a thread for the device %d", i);
            --driver->ndevices_targeted;
            continue ;
        }
    }

    // move back the current thread to its initial cpu set
    bindto(&save_schedset);

    hwloc_topology_destroy(topology);
}

/* initialize drivers and create 1 thread per gpu starting on the passed routine */
void
xkrt_drivers_init(
    xkrt_drivers_t * drivers,
    uint8_t ngpus,
    void (*routine)(void * args, xkrt_driver_type_t driver_type, uint8_t device_driver_type),
    void * args
) {
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // SET MEMBERS
    memset(drivers->list, 0, sizeof(drivers->list));
    memset(drivers->devices.list, 0, sizeof(drivers->devices.list));
    drivers->devices.n = 0;
    drivers->devices.round_robin_device_global_id = 0;

    // LOAD DRIVERS
    void (*loaders[XKRT_DRIVER_TYPE_MAX])(xkrt_driver_t *);
    memset(loaders, 0, sizeof(loaders));

# if XKRT_SUPPORT_HOST
    extern void XKRT_DRIVER_HOST_get_driver(xkrt_driver_t *);
    loaders[XKRT_DRIVER_TYPE_HOST] = XKRT_DRIVER_HOST_get_driver;
# endif /* XKRT_SUPPORT_HOST */

# if XKRT_SUPPORT_CUDA
    extern void XKRT_DRIVER_TYPE_CUDA_get_driver(xkrt_driver_t *);
    loaders[XKRT_DRIVER_TYPE_CUDA] = XKRT_DRIVER_TYPE_CUDA_get_driver;
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_ZE
    extern void XKRT_DRIVER_TYPE_ZE_get_driver(xkrt_driver_t *);
    loaders[XKRT_DRIVER_TYPE_ZE] = XKRT_DRIVER_TYPE_ZE_get_driver;
# endif /* XKRT_SUPPORT_ZE */

# if XKRT_SUPPORT_CL
    extern void XKRT_DRIVER_TYPE_CL_get_driver(xkrt_driver_t *);
    loaders[XKRT_DRIVER_TYPE_CL] = XKRT_DRIVER_TYPE_CL_get_driver;
# endif /* XKRT_SUPPORT_CL */

    uint8_t total_gpus = 0;
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        void (*loader)(xkrt_driver_t *) = loaders[driver_type];
        if (loader)
        {
            loader(drivers->list + driver_type);
            xkrt_driver_init(drivers, ngpus - total_gpus, routine, args, (xkrt_driver_type_t) driver_type);
            total_gpus += drivers->devices.n;
            assert(total_gpus <= ngpus);
            if (total_gpus == ngpus)
                break ;
        }
    }

    /* wait each thread of each device of each driver to start */
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        xkrt_driver_t * driver = drivers->list + driver_type;
        while (driver->ndevices_commited < driver->ndevices_targeted)
            mem_pause();
    }

    // DEBUG OUTPUT
    if (drivers->devices.n == 0 && ngpus != 0)
        LOGGER_WARN("No devices found :-(");

    LOGGER_INFO("Enabled %d devices (with %d requested)", drivers->devices.n.load(), ngpus);
    assert(drivers->devices.n <= ngpus);
}

void
xkrt_drivers_deinit(xkrt_drivers_t * drivers)
{
    # pragma message(TODO "Implement driver_deinit - synchronize all devices threads")
}


extern "C"
int
xkrt_support_driver(xkrt_driver_type_t driver_type)
{
    switch (driver_type)
    {
        case (XKRT_DRIVER_TYPE_HOST):
            return XKRT_SUPPORT_HOST;

        case (XKRT_DRIVER_TYPE_CUDA):
            return XKRT_SUPPORT_CUDA;

        case (XKRT_DRIVER_TYPE_ZE):
            return XKRT_SUPPORT_ZE;

        case (XKRT_DRIVER_TYPE_CL):
            return XKRT_SUPPORT_CL;

        default:
            return 0;
    }
}
