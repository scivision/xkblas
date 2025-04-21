/* ************************************************************************** */
/*                                                                            */
/*   driver.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 23:53:52 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/support.h>
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

/* initialize drivers and create 1 thread per gpu starting on the passed routine */
void
xkrt_drivers_init(xkrt_runtime_t * runtime)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // PARAMETERS
    unsigned int ndevices_requested  = runtime->conf.device.ngpus + 1;                      // host device + ngpus
    int nthreads_per_device = runtime->conf.device.offloader.nthreads_per_device;
    int drivers_mask        = runtime->conf.drivers_mask | (1 << XKRT_DRIVER_TYPE_HOST);    // always force host driver
    assert(ndevices_requested < XKRT_DEVICES_MAX);

    // SET MEMBERS
    memset(runtime->drivers.list, 0, sizeof(runtime->drivers.list));
    memset(runtime->drivers.devices.list, 0, sizeof(runtime->drivers.devices.list));
    runtime->drivers.devices.next_id = 1;   // host device is always 0
    runtime->drivers.devices.n = 0;
    runtime->drivers.devices.round_robin_device_global_id = 0;

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

    # if 0
    typedef struct  xkrt_thread_device_args_t
    {
        xkrt_driver_type_t driver_type;
        int driver_device_id;
    }               xkrt_thread_device_args_t;
    # endif

    // number of devices
    uint8_t ndevices = 0;

    // ALLOCATE DEVICE THREAD PLACES
    xkrt_thread_place_t * places = (xkrt_thread_place_t *) malloc(sizeof(xkrt_thread_place_t) * ndevices_requested);
    assert(places);

    // ARGS PASSED TO FORKED THREADS
    xkrt_device_team_args_t args = {
        .runtime = runtime,
        .devices = {},
        .ndevices = 0
    };

    // FOR EACH DRIVER
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX && ndevices < ndevices_requested ; ++driver_type)
    {
        // if the driver is enabled
        xkrt_driver_t * (*creator)(void) = creators[driver_type];
        if (drivers_mask & (1 << driver_type) && creator)
        {
            // create it
            xkrt_driver_t * driver = creator();
            runtime->drivers.list[driver_type] = driver;
            if (driver == NULL)
                continue ;
            driver->type = (xkrt_driver_type_t) driver_type;

            // load devices
            const char * driver_name = driver->f_get_name ? driver->f_get_name() : "(null)";
            LOGGER_INFO("Loading driver `%s`", driver_name);

            driver->ndevices_commited = 0;
            if (driver->f_init == NULL || driver->f_init(ndevices_requested - ndevices))
            {
                LOGGER_WARN("Failed to load");
                return ;
            }

            assert(driver->f_get_ndevices_max);
            unsigned int ndevices_max = driver->f_get_ndevices_max();
            LOGGER_DEBUG("Driver has up to %u devices", ndevices_max);

            unsigned int ndevices_for_driver = MIN(ndevices_requested - ndevices, ndevices_max);
            assert(ndevices_for_driver);

            // get cpuset for the device
            for (uint8_t i = 0; i < ndevices_for_driver && ndevices < ndevices_requested; ++i)
            {
                assert(driver->f_device_cpuset);
                int err = driver->f_device_cpuset(runtime->topology, &places[ndevices], i);
                if (err)
                    LOGGER_WARN("Invalid cpuset returned for device %d - using default cpuset", i);
                else
                {
                    // TODO : save device info and pass it to forked thread
                    args.devices[ndevices].driver_type      = (xkrt_driver_type_t) driver_type;
                    args.devices[ndevices].device_driver_id = i;
                    ++ndevices;
                }
            }
        }
        // driver disabled or couldnt init
        else
            runtime->drivers.list[driver_type] = NULL;
    }
    assert(ndevices <= ndevices_requested);

    // DEBUG OUTPUT
    if (ndevices == 0)
    {
        LOGGER_WARN("No devices found :-(");
        return ;
    }
    LOGGER_INFO("Found %d devices (with %d requested)", ndevices, ndevices_requested);

    // TODO : wait for all threads to start
    runtime->drivers.devices.team.desc.routine              = xkrt_device_thread_main;
    runtime->drivers.devices.team.desc.args                 = &args;
    runtime->drivers.devices.team.desc.nthreads             = ndevices * nthreads_per_device;
    runtime->drivers.devices.team.desc.binding.mode         = XKRT_TEAM_BINDING_MODE_COMPACT;
    runtime->drivers.devices.team.desc.binding.places       = XKRT_TEAM_BINDING_PLACES_EXPLICIT;
    runtime->drivers.devices.team.desc.binding.places_list  = places;
    runtime->drivers.devices.team.desc.binding.nplaces      = ndevices;
    runtime->drivers.devices.team.desc.binding.flags        = XKRT_TEAM_BINDING_FLAG_NONE;

    // prepare the barrier for the devices team
    pthread_barrier_t * barrier = &runtime->drivers.devices.barrier;
    args.ndevices = ndevices;
    if (pthread_barrier_init(barrier, NULL, runtime->drivers.devices.team.desc.nthreads + 1))
        LOGGER_FATAL("Couldnt initialized pthread_barrier_t");

    // create a team of thread
    runtime->team_create(&runtime->drivers.devices.team);

    // wait for all devices to be created
    pthread_barrier_wait(barrier);    // init
    pthread_barrier_wait(barrier);    // commit
    pthread_barrier_wait(barrier);    // offloader streams
}

void
xkrt_drivers_deinit(xkrt_runtime_t * runtime)
{
    // notify each thread to stop
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime->drivers.devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = runtime->drivers.devices.list[device_global_id];
        assert(device);
        device->state = XKRT_DEVICE_STATE_STOP;

        int nthreads = device->nthreads.load(std::memory_order_acq_rel);
        for (int i = 0 ; i < nthreads ; ++i)
            device->threads[i]->wakeup();
    }

    // wait for threads stream deletion
    pthread_barrier_wait(&runtime->drivers.devices.barrier);

    // wait for main thread driver deinitialization
    pthread_barrier_wait(&runtime->drivers.devices.barrier);

    // can destroy the barrier now
    pthread_barrier_destroy(&runtime->drivers.devices.barrier);

    // join threads
    runtime->team_join(&runtime->drivers.devices.team);
    free(runtime->drivers.devices.team.desc.binding.places_list);

    // finalize each driver
    for (uint8_t driver_type = 0 ; driver_type < XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        xkrt_driver_t * driver = runtime->drivers.list[driver_type];
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
