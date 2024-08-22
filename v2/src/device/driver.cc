# include "min-max.h"
# include "device/device.h"
# include "device/driver.h"
# include "logger/logger.h"
# include "sync/spinlock.h"

# include <cassert>
# include <cstring>
# include <cerrno>

# pragma message(TODO "Implement host driver ?")

static void
xkblas_driver_init(xkblas_drivers_t * drivers, uint8_t driver_id, uint8_t ngpus)
{
    xkblas_driver_t * driver = drivers->list + driver_id;

    XKBLAS_INFO("Loading driver '%s'", driver->f_get_name());
    assert(driver->f_init);

    if (driver->f_init())
        return ;

    # pragma message(TODO "Currently, the only devices supported are GPUs")
    assert(driver->f_get_ndevices_max);
    int n_devices_max = driver->f_get_ndevices_max();
    int n_devices = MIN(ngpus, n_devices_max);
    // XKBLAS_INFO("using %d devices out of %d available", n_devices, n_devices_max);
    if (n_devices < 1)
        return ;
    driver->ndevices_targeted = n_devices;

    # pragma message(TODO "Move that to the 'Thread' interfaces")
    cpu_set_t save_schedset;
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);

    for (int i = 0; i < n_devices; ++i)
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        cpu_set_t schedset;
        assert(driver->f_device_set_cpuset);
        int err = driver->f_device_set_cpuset(&schedset, i);
        if (err)
        {
            XKBLAS_WARN("Invalid cpuset returned for device %d - using default cpuset", i);
        }
        else
        {
            // move the current thread to the device cpu set
            err = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &schedset);
            if (err)
            {
                XKBLAS_ERROR("Invalid cpuset returned by the driver for device %d", i);
                --driver->ndevices_targeted;
                continue ;
            }
        }

        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &schedset);
        for (int i=0; i<10; ++i) sched_yield();

        // start the device thread
        xkblas_driver_device_thread_arg_t * arg = (xkblas_driver_device_thread_arg_t *) malloc(sizeof(xkblas_driver_device_thread_arg_t));
        arg->drivers = drivers;
        arg->driver_id = driver_id;
        arg->driver_device_id = i;

        pthread_t thread;
        err = pthread_create(&thread, &attr, xkblas_device_thread_main, arg);
        if (err)
        {
            XKBLAS_ERROR("could not create a thread for the device %d", i);
            --driver->ndevices_targeted;
            continue ;
        }
    }

    // move back the current thread to its initial cpu set
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);
}

void
xkblas_drivers_init(xkblas_drivers_t * drivers, uint8_t ngpus)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    void (*loaders[XKBLAS_DRIVER_MAX])(xkblas_driver_t *);
    memset(loaders, 0, sizeof(loaders));

    extern void XKBLAS_DRIVER_ENTRYPOINT(get_host_driver)(xkblas_driver_t *);
    loaders[XKBLAS_DRIVER_HOST] = XKBLAS_DRIVER_ENTRYPOINT(get_host_driver);

# if USE_CUDA
    extern void XKBLAS_DRIVER_ENTRYPOINT(get_cuda_driver)(xkblas_driver_t *);
    loaders[XKBLAS_DRIVER_CUDA] = XKBLAS_DRIVER_ENTRYPOINT(get_cuda_driver);
    XKBLAS_DEBUG("using cuda");
    # else
    XKBLAS_DEBUG("not using cuda");
# endif /* USE_CUDA */

    uint8_t i;
    for (i = 0 ; i < XKBLAS_DRIVER_MAX ; ++i)
    {
        void (*loader)(xkblas_driver_t *) = loaders[i];
        if (loader)
        {
            loader(drivers->list + i);
            xkblas_driver_init(drivers, i, ngpus);
            if (drivers->devices.n == ngpus)
                break ;
        }
    }

    /* wait all threads for each devices of each driver */
    int total_devices = 0;
    for (i = 0 ; i < XKBLAS_DRIVER_MAX ; ++i)
    {
        xkblas_driver_t * driver = drivers->list + i;
        while (driver->ndevices_commited < driver->ndevices_targeted)
            mem_pause();
        total_devices += driver->ndevices_targeted;
    }

    if (total_devices == 0)
        XKBLAS_FATAL("No devices found :-(");

    XKBLAS_INFO("Enabled %d devices (with %d requested)", total_devices, ngpus);
    assert(total_devices < ngpus);
}

void
xkblas_drivers_deinit(xkblas_drivers_t * drivers)
{
    # pragma message(TODO "Implement driver_deinit - synchronize all devices threads")
}

// Warning: this is called by a ThreadProducer - to enqueue a task in a ThreadWorker
void
xkblas_drivers_enqueue(xkblas_drivers_t * drivers, Task * task)
{
    assert(task->state.value == TASK_STATE_READY);

    // Find the worker to offload the task
    uint8_t device_id = task->targetted_device_id;

    // if an ocr parameter is set, retrieve the device accordingly
    if (task->ocr_access_index < drivers->devices.n)
    {
        assert(task->ocr_access_index >= 0);
        XKBLAS_WARN("OCR feature is not implemented");
        // TODO
    }

    // targetted device and OCR failed, fallback to round robin
    if (device_id >= drivers->devices.n)
    {
        do {
            device_id = drivers->devices.round_robin_device_id.fetch_add(1, std::memory_order_relaxed);
            device_id = (device_id + 1) % drivers->devices.n;
        } while (drivers->devices.list[device_id] == nullptr);
    }

    // we found the thread
    assert(device_id >= 0 && device_id < drivers->devices.n);
    ThreadWorker * worker = drivers->devices.list[device_id]->thread;
    if (worker == NULL)
    {
        XKBLAS_ERROR("Trying to enqueue a task to an uninitialized worker %d", device_id);
        return ;
    }

    // XKBLAS_DEBUG("Enqueuing task %p to device %d", task, device_id);
    worker->push(task);
}

xkblas_device_t *
xkblas_get_device_host(xkblas_drivers_t * drivers)
{
    assert(drivers->devices.n);
    return drivers->devices.list[0];
}
