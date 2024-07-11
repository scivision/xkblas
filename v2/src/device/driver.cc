# include "min-max.h"
# include "conf/conf.h"
# include "device/device.h"
# include "device/driver.h"
# include "logger/logger.h"
# include "sync/spinlock.h"

# include <cassert>
# include <cstring>
# include <cerrno>

# pragma message(TODO "Implement host driver ?")

// Drivers
# define XKBLAS_DRIVER_CUDA     0
# define XKBLAS_DRIVER_MAX      1
# define XKBLAS_DRIVER_GPU      XKBLAS_DRIVER_CUDA
# define XKBLAS_DRIVER_DEFAULT  XKBLAS_DRIVER_GPU
static xkblas_driver_t DRIVERS[XKBLAS_DRIVER_MAX];

static void
xkblas_driver_init(xkblas_driver_t * driver)
{
    XKBLAS_INFO("Loading driver '%s'", driver->f_get_name());
    assert(driver->f_init);

    if (driver->f_init())
        return ;

    # pragma message(TODO "Currently, the only devices supported are GPUs")
    assert(driver->f_get_ndevices_max);
    int n_devices_max = driver->f_get_ndevices_max();
    int n_devices = MIN(XKBLAS_CONF.ngpus, n_devices_max);
    XKBLAS_INFO("using %d devices out of %d available", n_devices, n_devices_max);
    if (n_devices < 1)
        return ;
    driver->ndevices_targeted = n_devices;

    # pragma message(TODO "Move that to the 'Thread' interfaces")
    cpu_set_t save_schedset;
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);

    for (int i = 0; i < n_devices; ++i)
    {
        cpu_set_t schedset;
        assert(driver->f_device_set_cpuset);
        int err = driver->f_device_set_cpuset(&schedset, i);
        if (err)
        {
            XKBLAS_ERROR("cannot use device %d", i);
            --driver->ndevices_targeted;
            continue ;
        }

        // move the current thread to the device cpu set
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        err = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &schedset);
        if (err)
        {
            XKBLAS_ERROR("invalid cpu_set returned by the driver for device %d", i);
            --driver->ndevices_targeted;
            continue ;
        }

        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &schedset);
        for (int i=0; i<10; ++i) sched_yield();

        // start the device thread
        xkblas_driver_thread_arg_t * arg = (xkblas_driver_thread_arg_t *) malloc(sizeof(xkblas_driver_thread_arg_t));
        arg->driver = driver;
        arg->driver_device_id = i;

        pthread_t thread;
        err = pthread_create(&thread, &attr, xkblas_device_thread_main, arg);
        assert(err ==0);
    }

    // move back the current thread to its initial cpu set
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);
}

void
xkblas_drivers_init(void)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in CUDA driver")

    extern void XKBLAS_DRIVER_ENTRYPOINT(get_cuda_driver)(xkblas_driver_t *);

    void (*loaders[XKBLAS_DRIVER_MAX])(xkblas_driver_t *);
    loaders[XKBLAS_DRIVER_CUDA] = XKBLAS_DRIVER_ENTRYPOINT(get_cuda_driver);

    for (int i = 0 ; i < XKBLAS_DRIVER_MAX ; ++i)
    {
        xkblas_driver_t * driver = DRIVERS + i;
        loaders[i](driver);
        xkblas_driver_init(driver);
    }

    /* wait all threads for each devices of each driver */
    int total_devices = 0;
    for (int i = 0 ; i < XKBLAS_DRIVER_MAX ; ++i)
    {
        xkblas_driver_t * driver = DRIVERS + i;
        while (driver->ndevices_commited < driver->ndevices_targeted)
            mem_pause();
        total_devices += driver->ndevices_targeted;
    }

    XKBLAS_INFO("Enabled %d devices (with %d requested)", total_devices, XKBLAS_CONF.ngpus);
    assert(total_devices < XKBLAS_CONF.ngpus);
}

void
xkblas_drivers_deinit(void)
{
    # pragma message(TODO "Implement driver_deinit - synchronize all devices threads")

    XKBLAS_INFO("Infinite loop... CTRL+C to exit");
    while (1)
        sleep(1);
}
