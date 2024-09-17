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

    for (int i = 0; i < n_devices; ++i)
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
    drivers->devices.round_robin_device_id = 0;

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

///////////////////////
//  MEMORY ALLOCATOR //
///////////////////////

/**
 * Allocate memory on device from list of free chunk
 * It may fail and return NULL
 */
static inline void *
xkblas_try_allocate_on_device(
    xkblas_device_t * device,
    size_t size
) {
    /* adapted from kaapi_memory_alloc */

    /* align data */
    size = (size + 7UL) & ~7UL;

    XKBLAS_MUTEX_LOCK(device->memdev.mem_lock);
    if (device->memdev.memory_allocated == 0)
    {
        /* Device is host, should be used only in debug mode */
        XKBLAS_DEBUG("Allocate memory on host device (global id %d) - it may not be cleaned\n", device->global_id);
        XKBLAS_MUTEX_UNLOCK( device->memdev.mem_lock );
        return malloc(size);
    }

    xkblas_alloc_chunk * curr = device->memdev.free_chunk_list;
    xkblas_alloc_chunk * prevfree = NULL;
    size_t min_size = 0;
    xkblas_alloc_chunk_t * min_size_curr = NULL;
    xkblas_alloc_chunk_t * min_size_prevfree = NULL;
    while (curr)
    {
        size_t curr_size = curr->size;
        if (curr_size >= size)
    	{
    	    // TODO : check original code, seems it does not check the min...
	        min_size = curr_size;
	        min_size_curr = curr;
	        min_size_prevfree = prevfree;
	    }
	    prevfree = curr;
	    curr = curr->freelink;
    }

    curr = min_size_curr;
    prevfree = min_size_prevfree;

    /* split chunk */
    if((curr != NULL) && (min_size - size >= 0.5*size))
    {
        size_t curr_size = curr->size;
        xkblas_alloc_chunk_t * remainder = (xkblas_alloc_chunk_t *) malloc(sizeof(xkblas_alloc_chunk_t));
        remainder->device_ptr = size + curr->device_ptr;
        remainder->size       = (curr_size - size);
        remainder->state      = FREE_STATE;

        /* link remainder segment after curr */
        remainder->prev       = curr;
        remainder->next       = curr->next;
        if (curr->next) curr->next->prev = remainder;
        curr->next            = remainder;
        curr->size            = size;

        /* link freelist */
        remainder->freelink = curr->freelink;
        curr->freelink = remainder;
    }

    if( curr != NULL )
    {
        if (prevfree) prevfree->freelink = curr->freelink;
        else device->memdev.free_chunk_list = curr->freelink;
        curr->state &= ~FREE_STATE;
        curr->freelink = 0;
    }

    XKBLAS_MUTEX_UNLOCK( device->memdev.mem_lock );

    return (curr != NULL) ? ((void*) curr->device_ptr) : NULL;
}

static inline size_t
xkblas_evict_memory_from_device(
    xkblas_device_t * device,
    size_t size
) {
    /* reference code: kaapi_memory_cache_evict_fromlist  */
    // TODO implement eviction strategy
    # pragma message(TODO "Implement xkblas_evict_memory_from_device")
    XKBLAS_FATAL( "Try to evict data from global device %d, function not implemented - it may create an infite loop\n", device->global_id);
    return ENOMEM;
}

void *
xkblas_memory_allocate(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    size_t size
) {
    void * ptr = xkblas_try_allocate_on_device(device, size);

    // TODO : memory eviction
    while (ptr == NULL)
    {
        /* No memory found, we need to evict */
        int err = xkblas_evict_memory_from_device(device, size);

        /* eviction success, new space available */
        if (err == 0)
            ptr = xkblas_try_allocate_on_device(device, size);
        /* eviction failed ..., wait for some tasks to finish */
        else if (err == ENOMEM || ptr == NULL)
            xkblas_device_poll(device);

        // TODO what if the memory is never available ? infinit loop
    }

    return ptr;
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
    if (task->fmtid != TASK_FORMAT_NULL)
        ++stats->kernels.completed;
    # endif /* USE_STATS */

    # ifndef NDEBUG
    XKBLAS_WARN("Task `%s` completed", task->label);
    # endif /* NDEBUG */

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
    if (task->fmtid != TASK_FORMAT_NULL)
        ++stats->kernels.launched;
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
    xkblas_context_t * ctx = xkblas_context_get();
    return ctx->drivers.list + XKBLAS_DRIVER_TYPE_CUDA;
}

xkblas_device_t *
xkblas_device_get(int device_global_id)
{
    xkblas_context_t * ctx = xkblas_context_get();
    return ctx->drivers.devices.list[device_global_id];
}

