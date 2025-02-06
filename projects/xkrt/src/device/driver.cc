/* ************************************************************************** */
/*                                                                            */
/*   driver.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:01:53 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/device/device.h>
# include <xkrt/device/driver.h>
# include <xkrt/device/stream-instruction-submit.h>
# include <xkrt/logger/logger.h>
# include <xkrt/min-max.h>
# include <xkrt/sync/spinlock.h>

# include <cassert>
# include <cstring>
# include <cerrno>
# include <climits>

# pragma message(TODO "Implement host driver ?")

static void
xkrt_driver_init(xkrt_drivers_t * drivers, uint8_t driver_id, uint8_t ngpus)
{
    xkrt_driver_t * driver = drivers->list + driver_id;

    assert(driver->f_init);

    const char * driver_name = driver->f_get_name();
    if (driver->f_init())
    {
        LOGGER_INFO("Failed to load driver `%s`", driver_name);
        return ;
    }
    LOGGER_INFO("Loading driver `%s`", driver_name);

    assert(driver->f_get_ndevices_max);
    int n_devices_max = driver->f_get_ndevices_max();
    int n_devices = MIN(ngpus, n_devices_max);
    if (n_devices < 1)
        return ;
    driver->ndevices_targeted = n_devices;

    # pragma message(TODO "Move that to the 'Thread' interfaces")
    cpu_set_t save_schedset;
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);

    /* init each device of that driver */
    for (uint8_t i = 0; i < n_devices; ++i)
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        // move the current thread to the device cpu set
        cpu_set_t schedset;
        assert(driver->f_device_set_cpuset);
        int err = driver->f_device_set_cpuset(&schedset, i);
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

        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &schedset);
        for (int ii=0; ii<10; ++ii) sched_yield();

        // start the device thread
        xkrt_driver_device_thread_arg_t * arg = (xkrt_driver_device_thread_arg_t *) malloc(sizeof(xkrt_driver_device_thread_arg_t));
        arg->drivers = drivers;
        arg->driver_id = driver_id;
        arg->device_driver_id = i;

        pthread_t thread;
        err = pthread_create(&thread, &attr, xkrt_device_thread_main, arg);
        if (err)
        {
            LOGGER_ERROR("could not create a thread for the device %d", i);
            --driver->ndevices_targeted;
            continue ;
        }
    }

    // move back the current thread to its initial cpu set
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);
    for (int i=0; i<10; ++i) sched_yield();
}

void
xkrt_drivers_init(xkrt_drivers_t * drivers, uint8_t ngpus)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // SET MEMBERS
    memset(drivers->list, 0, sizeof(drivers->list));
    memset(drivers->devices.list, 0, sizeof(drivers->devices.list));
    drivers->devices.n = 0;
    drivers->devices.round_robin_device_global_id = 0;

    // LOAD DRIVERS
    void (*loaders[XKRT_DRIVER_TYPE_MAX])(xkrt_driver_t *);
    memset(loaders, 0, sizeof(loaders));

# if USE_CPU
    extern void XKRT_DRIVER_CPU_get_driver(xkrt_driver_t *);
    loaders[XKRT_DRIVER_CPU] = XKRT_DRIVER_CPU_get_driver;
# endif /* USE_CPU */

# if USE_CUDA
    extern void XKRT_DRIVER_TYPE_CUDA_get_driver(xkrt_driver_t *);
    loaders[XKRT_DRIVER_TYPE_CUDA] = XKRT_DRIVER_TYPE_CUDA_get_driver;
# endif /* USE_CUDA */

    uint8_t i;
    for (i = 0 ; i < XKRT_DRIVER_TYPE_MAX && drivers->devices.n < ngpus ; ++i)
    {
        void (*loader)(xkrt_driver_t *) = loaders[i];
        if (loader)
        {
            loader(drivers->list + i);
            xkrt_driver_init(drivers, i, ngpus);
            if (drivers->devices.n == ngpus)
                break ;
        }
    }

    /* wait each thread of each device of each driver to start */
    for (i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
    {
        xkrt_driver_t * driver = drivers->list + i;
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

xkrt_device_t *
xkrt_get_device_host(xkrt_drivers_t * drivers)
{
    assert(drivers->devices.n);
    return drivers->devices.list[0];
}

int
xkrt_task_launch(task_launcher_t * launcher)
{
    assert(launcher);
    assert(launcher->task);
    assert(launcher->task->fmtid);
    assert(launcher->target >= 0 && launcher->target <= XKRT_DRIVER_TYPE_MAX);

    task_format_t * format = task_format_get(launcher->task->fmtid);
    assert(format);
    assert(format->f[launcher->target]);

    format->f[launcher->target](launcher);

    return 0;
}

/* callback after the task kernel executed */
static inline void
xkrt_device_task_executed(
    Task * task
) {
    assert(task);

    ThreadWorker * thread = ThreadWorker::self();
    assert(thread);

    thread->complete(task);
}

static void
xkrt_device_task_executed_callback(
    const void * args[XKRT_CALLBACK_ARGS_MAX]
) {
    assert(args[0]);
    xkrt_device_task_executed((Task *) args[0]);
}

/**
 * Must be called once all task accessed were fetched, to queue the task kernel for execution
 *  - driver - the driver to use for executing the kernel
 *  - device - the device to use for executing the kernel
 *  - task   - the task
 */
void
xkrt_device_task_execute(
    xkrt_device_t * device,
               Task * task
) {
    assert(XKRT_CALLBACK_ARGS_MAX >= 1);

    /* running an empty task */
    if (task->fmtid == TASK_FORMAT_NULL)
    {
        xkrt_device_task_executed(task);
    }
    else
    {
        /* retrieve task format */
        task_format_t * format = task_format_get(task->fmtid);
        assert(format);

        /* running a host task */
        if (format->target == TASK_FORMAT_TARGET_HOST)
        {
            task_launcher_t launcher = {
                .task   = task,
                .handle = NULL
            };
            xkrt_task_launch(&launcher);
            xkrt_device_task_executed(task);
        }
        /* running a device task */
        else
        {
            assert(format->target == TASK_FORMAT_TARGET_DRIVER);

            xkrt_callback_t callback;
            callback.func    = xkrt_device_task_executed_callback;
            callback.args[0] = task;

            xkrt_stream_instruction_submit_kernel(device, task, callback);
            if (device->thread == ThreadWorker::self())
                device->offloader.launch_ready_instructions(XKRT_STREAM_TYPE_KERN);

            /* kernel launch will be called asynchronously in the driver */
            /* the 'executed' callback will be called asynchronously in the
             * driver on kernel completion test success */
        }
    }
}

static inline void
xkrt_device_wait(xkrt_device_t * device)
{
    LOGGER_DEBUG("Waiting for device %d...", device->global_id);
    device->offloader.progress_pending_instructions(XKRT_STREAM_TYPE_ALL, true);
}

xkrt_driver_t *
xkrt_driver_get(xkrt_driver_type_t type)
{
    xkrt_runtime_t * context = xkrt_runtime_get();
    return context->drivers.list + XKRT_DRIVER_TYPE_CUDA;
}

xkrt_device_t *
xkrt_device_get(int device_global_id)
{
    xkrt_runtime_t * context = xkrt_runtime_get();
    return context->drivers.devices.list[device_global_id];
}
