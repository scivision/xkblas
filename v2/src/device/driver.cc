# include "xkblas-context.h" // TODO : remove me
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
xkblas_driver_init(xkblas_drivers_t * drivers, uint8_t driver_id, uint8_t ngpus)
{
    xkblas_driver_t * driver = drivers->list + driver_id;

    XKBLAS_INFO("Loading driver '%s'", driver->f_get_name());
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
            XKBLAS_WARN("Invalid cpuset returned for device %d - using default cpuset", i);
        }
        else
        {
            err = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &schedset);
            if (err)
            {
                XKBLAS_ERROR("Invalid cpuset returned by the driver for device %d", i);
                --driver->ndevices_targeted;
                continue ;
            }
        }

        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &schedset);
        for (int ii=0; ii<10; ++ii) sched_yield();

        // start the device thread
        xkblas_driver_device_thread_arg_t * arg = (xkblas_driver_device_thread_arg_t *) malloc(sizeof(xkblas_driver_device_thread_arg_t));
        arg->drivers = drivers;
        arg->driver_id = driver_id;
        arg->device_driver_id = i;

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
    for (int i=0; i<10; ++i) sched_yield();
}

void
xkblas_drivers_init(xkblas_drivers_t * drivers, uint8_t ngpus)
{
    # pragma message(TODO "Dynamic driver loading not implemented (with dlopen). Only supporting built-in drivers")

    // SET MEMBERS
    memset(drivers->list, 0, sizeof(drivers->list));
    memset(drivers->devices.list, 0, sizeof(drivers->devices.list));
    drivers->devices.n = 0;
    drivers->devices.round_robin_device_global_id = 0;

    // LOAD DRIVERS
    void (*loaders[XKBLAS_DRIVER_TYPE_MAX])(xkblas_driver_t *);
    memset(loaders, 0, sizeof(loaders));

# if USE_CPU
    extern void XKBLAS_DRIVER_CPU_get_driver(xkblas_driver_t *);
    loaders[XKBLAS_DRIVER_CPU] = XKBLAS_DRIVER_CPU_get_driver;
# endif /* USE_CPU */

# if USE_CUDA
    extern void XKBLAS_DRIVER_TYPE_CUDA_get_driver(xkblas_driver_t *);
    loaders[XKBLAS_DRIVER_TYPE_CUDA] = XKBLAS_DRIVER_TYPE_CUDA_get_driver;
# endif /* USE_CUDA */

    uint8_t i;
    for (i = 0 ; i < XKBLAS_DRIVER_TYPE_MAX ; ++i)
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

    /* wait each thread of each device of each driver to start */
    int total_devices = 0;
    for (i = 0 ; i < XKBLAS_DRIVER_TYPE_MAX ; ++i)
    {
        xkblas_driver_t * driver = drivers->list + i;
        while (driver->ndevices_commited < driver->ndevices_targeted)
            mem_pause();
        total_devices += driver->ndevices_targeted;
    }

    // DEBUG OUTPUT
    if (total_devices == 0)
        XKBLAS_FATAL("No devices found :-(");

    XKBLAS_INFO("Enabled %d devices (with %d requested)", total_devices, ngpus);
    assert(total_devices <= ngpus);
}

void
xkblas_drivers_deinit(xkblas_drivers_t * drivers)
{
    # pragma message(TODO "Implement driver_deinit - synchronize all devices threads")
}

xkblas_device_t *
xkblas_get_device_host(xkblas_drivers_t * drivers)
{
    assert(drivers->devices.n);
    return drivers->devices.list[0];
}

int
xkblas_task_launch(task_launcher_t * launcher)
{
    assert(launcher);
    assert(launcher->task);
    assert(launcher->task->fmtid);
    assert(launcher->target >= 0 && launcher->target <= XKBLAS_DRIVER_TYPE_MAX);

    task_format_t * format = task_format_get(launcher->task->fmtid);
    assert(format);
    assert(format->f[launcher->target]);

    format->f[launcher->target](launcher);

    return 0;
}

/* callback after the task kernel executed */
static inline void
xkblas_device_task_executed(
    Task * task
) {
    assert(task);

    # if USE_STATS
    xkblas_stats_t * stats = xkblas_stats_get();
    ++stats->tasks.completed;
    # if 0  /* cannot detect completed kernels only, depends on the 'format'
               which does not have that info atm */
    if (task->fmtid != TASK_FORMAT_NULL)
        ++stats->kernels.completed;
    # else
    stats->kernels.completed = -1;
    # endif
    # endif /* USE_STATS */

    ThreadWorker * thread = ThreadWorker::self();
    assert(thread);

    thread->complete(task);
}

static void
xkblas_device_task_executed_callback(
    const void * args[XKBLAS_CALLBACK_ARGS_MAX]
) {
    assert(args[0]);
    xkblas_device_task_executed((Task *) args[0]);
}

/**
 * Must be called once all task accessed were fetched, to queue the task kernel for execution
 *  - driver - the driver to use for executing the kernel
 *  - device - the device to use for executing the kernel
 *  - task   - the task
 */
void
xkblas_device_task_execute(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
               Task * task
) {
    assert(XKBLAS_CALLBACK_ARGS_MAX >= 1);

    # if USE_STATS
    xkblas_stats_t * stats = xkblas_stats_get();
    ++stats->tasks.launched;
    # endif /* USE_STATS */

    /* running an empty task */
    if (task->fmtid == TASK_FORMAT_NULL)
    {
        xkblas_device_task_executed(task);
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
            xkblas_task_launch(&launcher);
            xkblas_device_task_executed(task);
        }
        /* running a device task */
        else
        {
            assert(format->target == TASK_FORMAT_TARGET_DRIVER);

            xkblas_callback_t callback;
            callback.func    = xkblas_device_task_executed_callback;
            callback.args[0] = task;

            xkblas_stream_instruction_submit_kernel(driver, device, task, callback);
            device->offloader.launch_ready_instructions(XKBLAS_STREAM_TYPE_KERN);

            /* launch will be called asynchronously in the driver */

            /* the 'executed' callback will be called asynchronously in the driver on kernel completion test success */
        }
    }
}

static inline void
xkblas_device_wait(xkblas_device_t * device)
{
    XKBLAS_DEBUG("Waiting for device %d...", device->global_id);
    device->offloader.progress_pending_instructions(XKBLAS_STREAM_TYPE_ALL, true);
}

xkblas_driver_t *
xkblas_driver_get(xkblas_driver_type_t type)
{
    xkblas_context_t * context = xkblas_context_get();
    return context->drivers.list + XKBLAS_DRIVER_TYPE_CUDA;
}

xkblas_device_t *
xkblas_device_get(int device_global_id)
{
    xkblas_context_t * context = xkblas_context_get();
    return context->drivers.devices.list[device_global_id];
}
