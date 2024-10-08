# include "xkblas-context.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/stream.h"
# include "device/stream-instruction-submit.h"
# include "logger/logger.h"
# include "logger/todo.h"
# include "device/thread-worker.hpp"
# include "sync/mem.h"

# include <cassert>
# include <cstring>
# include <cerrno>

# pragma message(TODO "Move these initializer into class member functions")


///////////////////////
//  MEMORY ALLOCATOR //
///////////////////////

static inline void
xkblas_device_memory_reset(xkblas_device_t * device)
{
    # pragma message(TODO "This is leaking")
    xkblas_alloc_chunk_t * chunk0 = (xkblas_alloc_chunk_t *) malloc(sizeof(xkblas_alloc_chunk_t));
    assert(chunk0);
    memcpy(chunk0, &(device->memdev.chunk0), sizeof(xkblas_alloc_chunk_t));
    device->memdev.free_chunk_list = chunk0;
}

static inline void
xkblas_device_init_memory(xkblas_device_t * device)
{
    assert(device->memdev.chunk0.device_ptr);
    assert(device->memdev.chunk0.size);

    device->memdev.chunk0.state = XKBLAS_ALLOC_CHUNK_STATE_FREE;
    device->memdev.chunk0.prev  = NULL;
    device->memdev.chunk0.next  = NULL;

    XKBLAS_MUTEX_INIT(device->memdev.lock);
    xkblas_device_memory_reset(device);
}

/**
 * Allocate memory on device from list of free chunk
 * It may fail and return NULL
 */
static inline xkblas_alloc_chunk_t *
xkblas_try_allocate_on_device(
    xkblas_device_t * device,
    size_t size
) {
    /* adapted from kaapi_memory_alloc */

    /* align data */
    size = (size + 7UL) & ~7UL;

    XKBLAS_MUTEX_LOCK(device->memdev.lock);

    /* best fit strategy */
    xkblas_alloc_chunk_t * curr = device->memdev.free_chunk_list;
    xkblas_alloc_chunk_t * prevfree = NULL;
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
    if ((curr != NULL) && (min_size - size >= (size_t)(0.5*(double)size)))
    {
        size_t curr_size = curr->size;
        xkblas_alloc_chunk_t * remainder = (xkblas_alloc_chunk_t *) malloc(sizeof(xkblas_alloc_chunk_t));
        remainder->device_ptr = size + curr->device_ptr;
        remainder->size       = (curr_size - size);
        remainder->state      = XKBLAS_ALLOC_CHUNK_STATE_FREE;

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

    if (curr != NULL)
    {
        if (prevfree) prevfree->freelink = curr->freelink;
        else device->memdev.free_chunk_list = curr->freelink;
        curr->state &= ~XKBLAS_ALLOC_CHUNK_STATE_FREE;
        curr->freelink = 0;
    }

    XKBLAS_MUTEX_UNLOCK( device->memdev.lock );

    return curr;
}

# pragma message(TODO "Implement xkblas_evict_memory_from_device")
static inline size_t
xkblas_evict_memory_from_device(
    xkblas_device_t * device,
    size_t size
) {
    /* reference code: kaapi_memory_cache_evict_fromlist  */


    return ENOMEM;
}

xkblas_alloc_chunk_t *
xkblas_memory_allocate(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    size_t size
) {

    int retry_cnt = 0;

    do {

        xkblas_alloc_chunk_t * chunk = xkblas_try_allocate_on_device(device, size);
        if (chunk)
            return chunk;

        XKBLAS_FATAL("GPU IS OUT OF MEMORY!");

        // TODO : polling is risky here, because it may take a lock on the
        // memory tree, and 'xkblas_memory_allocate' is called within a
        // memory-tree lock => double-lock deadlock

        // xkblas_device_poll(device);
        xkblas_evict_memory_from_device(device, size);

    } while (++retry_cnt < 32);

    return NULL;
}

void
xkblas_memory_deallocate_all(void)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    for (xkblas_device_global_id_t device_global_id = 0 ;
            device_global_id < context->drivers.devices.n ;
            ++device_global_id)
    {
        xkblas_device_t * device = xkblas_device_get(device_global_id);
        assert(device);

        xkblas_device_memory_reset(device);
    }
}

///////////////////////////
// DEVICE INITIALIZATION //
///////////////////////////

static void
xkblas_device_commit(
    xkblas_driver_t * driver,
    xkblas_device_t * device
) {
    assert(driver->f_device_commit);
    int err = driver->f_device_commit(device->driver_id);
    if (err)
        XKBLAS_FATAL("Commit fail device %d of driver %s", device->driver_id, driver->f_get_name());

    assert(device->state == XKBLAS_DEVICE_STATE_INIT);
    device->state = XKBLAS_DEVICE_STATE_COMMIT;
}

static void
xkblas_device_init(
    xkblas_driver_t * driver,
    xkblas_device_t * device
) {
    driver->f_device_init(device->driver_id);

    xkblas_context_t * context = xkblas_context_get();
    device->offloader.init(&(context->conf.device.offloader), driver->f_stream_create);

    /* initialize device memory management */
    xkblas_device_init_memory(device);

    /* attach current thread to the device */
    if (driver->f_device_attach(device->driver_id))
        XKBLAS_FATAL("Could not attach to device %d of driver %s", device->driver_id, driver->f_get_name());

    assert(device->state == XKBLAS_DEVICE_STATE_CREATE);
    device->state = XKBLAS_DEVICE_STATE_INIT;
}

static xkblas_device_t *
xkblas_device_create(
    xkblas_drivers_t * drivers,
    uint8_t driver_id,
    uint8_t driver_device_id
) {
    if (drivers->devices.n == XKBLAS_DEVICES_MAX)
        XKBLAS_FATAL("Too many devices. Increase 'XKBLAS_DEVICES_MAX' and recompile Xkblas");

    xkblas_driver_t * driver = drivers->list + driver_id;
    xkblas_device_t * device = driver->f_device_create(driver, driver_device_id);
    assert(device);

    device->state           = XKBLAS_DEVICE_STATE_CREATE;
    device->driver_id       = driver_device_id;
    device->global_id       = drivers->devices.n++;

    drivers->devices.list[device->global_id] = device;

    // register worker thread, using a nasty global variable here :-(
    ThreadWorker::init();
    device->thread = ThreadWorker::self();
    assert(device->thread);

    return device;
}

/////////////////////////
//  DEVICE PROGRESSION //
/////////////////////////
int
xkblas_device_poll(xkblas_device_t * device)
{
    int err = 0;
    assert(ThreadWorker::self() == device->thread);

    err = device->offloader.launch_ready_instructions(XKBLAS_STREAM_TYPE_ALL);
    assert( (err == 0) || (err == EINPROGRESS));

    err = device->offloader.progress_pending_instructions(XKBLAS_STREAM_TYPE_ALL, false);
    assert( (err == 0) || (err == EINPROGRESS));

    return err;
}

/* Return 1 iff device may accept new runing offloaded task  */
static inline int
xkblas_device_accept_new_task(xkblas_device_t * device)
{
    # pragma message(TODO "Add conditions here, like the number of kernel in-flight")
    assert(device);
    return 1;
}

static inline void
xkblas_device_prepare_task(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    Task * task
) {
    assert(task->wc == 0);
    assert(task->state.value == TASK_STATE_READY);

    # ifndef NDEBUG
    XKBLAS_INFO("Scheduling task `%s` of format `%d` on device %d - driver `%s`",
            task->label, task->fmtid, device->global_id, driver->f_get_name());
    # endif /* NDEBUG */

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    /* fetch, return 'TASK_STATE_DATA_FETCHED' if the data got fetched early */
    if (context->memtree.fetch(driver, device, task) == TASK_STATE_DATA_FETCHED)
    {
        /* all data has been fetched, the task kernel is ready for execution */
        xkblas_device_task_execute(driver, device, task);
    }
    else
    {
        /* task will be launched in a callback while all accesses were fetched */
    }
}

static inline void
xkblas_device_progress(
    xkblas_driver_t * driver,
    xkblas_device_t * device
) {
    int err = xkblas_device_poll(device);
    assert((err == 0) || (err == EINPROGRESS));
}

/* main loop for the thread responsible the passed device */
static inline int
xkblas_device_thread_main_loop(
    xkblas_driver_t * driver,
    xkblas_device_t * device
) {
    assert(ThreadWorker::self() == device->thread);
    assert(device->state == XKBLAS_DEVICE_STATE_COMMIT);
    device->state = XKBLAS_DEVICE_STATE_RUNNING;

    # pragma message(TODO "do we really need this mem_barrier here?")
    mem_barrier();

    ThreadWorker * worker = ThreadWorker::self();
    while (device->state == XKBLAS_DEVICE_STATE_RUNNING)
    {
        # pragma message(TODO "Per-device thread currently actively polling, the pause/resume mechanism is suspicious")
        # if 1
        # pragma message(TODO "'device->offloader.is_empty' is called with no lock, while inner lists are modifed under locks, is this a problem ?")
        // If there is no tasks and streams are empty, sleep the thread
        Task * task;
        while ((task = worker->pop()) == NULL && device->offloader.is_empty(XKBLAS_STREAM_TYPE_ALL))
            worker->pause();

        XKBLAS_DEBUG("Thread of device %d of driver %s is working, task=%p, offloader.is_empty()=%d",
                device->global_id, driver->f_get_name(), task, device->offloader.is_empty(XKBLAS_STREAM_TYPE_ALL));
        # else
        Task * task = worker->pop();
        # endif

        xkblas_device_progress(driver, device);
        if (task)
            xkblas_device_prepare_task(driver, device, task);
    }

    return EINTR;
}

///////////
//  MAIN //
///////////

/* Main entry thread created per device */
void *
xkblas_device_thread_main(void * a)
{
    # pragma message(TODO "Implement device thread")

    xkblas_driver_device_thread_arg_t * arg = (xkblas_driver_device_thread_arg_t *) a;
    xkblas_drivers_t * drivers  = arg->drivers;
    uint8_t driver_id = arg->driver_id;
    uint8_t driver_device_id = arg->driver_device_id;
    free(arg);

    xkblas_driver_t * driver = drivers->list + driver_id;
    unsigned int cpu, node;
    getcpu(&cpu, &node);

    /* init the device */
    xkblas_device_t * device = xkblas_device_create(drivers, driver_id, driver_device_id);
    xkblas_device_init(driver, device);

    XKBLAS_INFO("Starting thread for %s device (device_driver_id=%d, device_global_id=%d) on cpu %d of node %d",
            driver->f_get_name(), driver_device_id, device->global_id, cpu, node);

    // wait for all devices of that driver to be in the 'init' state
    ++driver->ndevices_inited;
    while (driver->ndevices_inited < driver->ndevices_targeted)
        mem_pause();

    // can now commit my device
    xkblas_device_commit(driver, device);
    ++driver->ndevices_commited;

    /* infinite loop with the device context */
    int err = xkblas_device_thread_main_loop(driver, device);
    assert((err==0) || (err==EINTR));

    # pragma message(TODO "Implement proper device deinitialization")
    # if 0

    /* */
#if XKBLAS_SLEEP_DEVICETHREAD
    xkblas_fifo_register_waiter( device->ld->queue, (void (*)(void *))xkblas_offload_device_wakeup, device );
#else
    xkblas_fifo_register_waiter( device->ld->queue, 0, 0);
#endif

    /* infinite loop with the device context */
    int err = xkblas_sched_idle_offload(thread, _xkblas_device_finalize, device);
    assert((err==0)||(err==eintr));

    /* thread is stopped */
    assert(0 == pthread_mutex_lock(&device->lock));
    device->state = XKBLAS_DEVICE_STATE_STOPPED;
    assert(0 == pthread_cond_signal(&device->cond_sleep));
    assert(0 == pthread_mutex_unlock(&device->lock));

    assert(0 == pthread_mutex_lock(&device->lock));
    _xkblas_offload_device_finalize(device);
    xkblas_offload_device_pop( device );
    xkblas_localitydomain_destroy(device->ld);
    device->state = XKBLAS_DEVICE_STATE_FINALIZED;

    if (err != EINTR)
    {
        XKBLAS_FATAL("device %d/%p abort with natural interrup\n", device->device_id, (void*)device);
    }
    assert(0 == pthread_mutex_unlock(&device->lock));
    xkblas_thread_unbind(thread);
    _xkblas_self_context = 0;
    device->state = XKBLAS_DEVICE_STATE_DESTROYED;

# endif

    return NULL;
}
