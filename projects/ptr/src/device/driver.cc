/* ************************************************************************** */
/*                                                                            */
/*   driver.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "runtime.h" // TODO : remove me
# include "min-max.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/stream-instruction-submit.h"
# include "logger/logger.h"
# include "sync/spinlock.h"

# include <cassert>
# include <cstring>
# include <cerrno>
# include <climits>

# pragma message(TODO "Implement host driver ?")

static void
ptr_driver_init(ptr_drivers_t * drivers, uint8_t driver_id, uint8_t ngpus)
{
    ptr_driver_t * driver = drivers->list + driver_id;

    LOGGER_INFO("Loading driver '%s'", driver->f_get_name());
    assert(driver->f_init);

    if (driver->f_init())
        return ;

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
        ptr_driver_device_thread_arg_t * arg = (ptr_driver_device_thread_arg_t *) malloc(sizeof(ptr_driver_device_thread_arg_t));
        arg->drivers = drivers;
        arg->driver_id = driver_id;
        arg->device_driver_id = i;

        pthread_t thread;
        err = pthread_create(&thread, &attr, ptr_device_thread_main, arg);
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
ptr_drivers_init(ptr_drivers_t * drivers, uint8_t ngpus)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // SET MEMBERS
    memset(drivers->list, 0, sizeof(drivers->list));
    memset(drivers->devices.list, 0, sizeof(drivers->devices.list));
    drivers->devices.n = 0;
    drivers->devices.round_robin_device_global_id = 0;

    // LOAD DRIVERS
    void (*loaders[PTR_DRIVER_TYPE_MAX])(ptr_driver_t *);
    memset(loaders, 0, sizeof(loaders));

# if USE_CPU
    extern void PTR_DRIVER_CPU_get_driver(ptr_driver_t *);
    loaders[PTR_DRIVER_CPU] = PTR_DRIVER_CPU_get_driver;
# endif /* USE_CPU */

# if USE_CUDA
    extern void PTR_DRIVER_TYPE_CUDA_get_driver(ptr_driver_t *);
    loaders[PTR_DRIVER_TYPE_CUDA] = PTR_DRIVER_TYPE_CUDA_get_driver;
# endif /* USE_CUDA */

    uint8_t i;
    for (i = 0 ; i < PTR_DRIVER_TYPE_MAX && drivers->devices.n < ngpus ; ++i)
    {
        void (*loader)(ptr_driver_t *) = loaders[i];
        if (loader)
        {
            loader(drivers->list + i);
            ptr_driver_init(drivers, i, ngpus);
            if (drivers->devices.n == ngpus)
                break ;
        }
    }

    /* wait each thread of each device of each driver to start */
    for (i = 0 ; i < PTR_DRIVER_TYPE_MAX ; ++i)
    {
        ptr_driver_t * driver = drivers->list + i;
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
ptr_drivers_deinit(ptr_drivers_t * drivers)
{
    # pragma message(TODO "Implement driver_deinit - synchronize all devices threads")
}

ptr_device_t *
ptr_get_device_host(ptr_drivers_t * drivers)
{
    assert(drivers->devices.n);
    return drivers->devices.list[0];
}

int
ptr_task_launch(task_launcher_t * launcher)
{
    assert(launcher);
    assert(launcher->task);
    assert(launcher->task->fmtid);
    assert(launcher->target >= 0 && launcher->target <= PTR_DRIVER_TYPE_MAX);

    task_format_t * format = task_format_get(launcher->task->fmtid);
    assert(format);
    assert(format->f[launcher->target]);

    format->f[launcher->target](launcher);

    return 0;
}

/* callback after the task kernel executed */
static inline void
ptr_device_task_executed(
    Task * task
) {
    assert(task);

    ThreadWorker * thread = ThreadWorker::self();
    assert(thread);

    thread->complete(task);
}

static void
ptr_device_task_executed_callback(
    const void * args[PTR_CALLBACK_ARGS_MAX]
) {
    assert(args[0]);
    ptr_device_task_executed((Task *) args[0]);
}

/**
 * Must be called once all task accessed were fetched, to queue the task kernel for execution
 *  - driver - the driver to use for executing the kernel
 *  - device - the device to use for executing the kernel
 *  - task   - the task
 */
void
ptr_device_task_execute(
    ptr_device_t * device,
               Task * task
) {
    assert(PTR_CALLBACK_ARGS_MAX >= 1);

    /* running an empty task */
    if (task->fmtid == TASK_FORMAT_NULL)
    {
        ptr_device_task_executed(task);
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
            ptr_task_launch(&launcher);
            ptr_device_task_executed(task);
        }
        /* running a device task */
        else
        {
            assert(format->target == TASK_FORMAT_TARGET_DRIVER);

            ptr_callback_t callback;
            callback.func    = ptr_device_task_executed_callback;
            callback.args[0] = task;

            ptr_stream_instruction_submit_kernel(device, task, callback);
            if (device->thread == ThreadWorker::self())
                device->offloader.launch_ready_instructions(PTR_STREAM_TYPE_KERN);

            /* kernel launch will be called asynchronously in the driver */
            /* the 'executed' callback will be called asynchronously in the
             * driver on kernel completion test success */
        }
    }
}

static inline void
ptr_device_wait(ptr_device_t * device)
{
    LOGGER_DEBUG("Waiting for device %d...", device->global_id);
    device->offloader.progress_pending_instructions(PTR_STREAM_TYPE_ALL, true);
}

ptr_driver_t *
ptr_driver_get(ptr_driver_type_t type)
{
    ptr_context_t * context = ptr_context_get();
    return context->drivers.list + PTR_DRIVER_TYPE_CUDA;
}

ptr_device_t *
ptr_device_get(int device_global_id)
{
    ptr_context_t * context = ptr_context_get();
    return context->drivers.devices.list[device_global_id];
}
