/* ************************************************************************** */
/*                                                                            */
/*   device.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:56:36 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/device/device.h>
# include <xkrt/device/driver.h>
# include <xkrt/device/stream-instruction-submit.h>
# include <xkrt/device/stream.h>
# include <xkrt/device/thread-worker.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/runtime.h>
# include <xkrt/sync/mem.h>

# include <cassert>
# include <cstring>
# include <cerrno>

# pragma message(TODO "Move these initializer into class member functions")


///////////////////////
//  MEMORY ALLOCATOR //
///////////////////////

static inline void
xkrt_device_memory_reset(xkrt_device_t * device)
{
    # pragma message(TODO "This is leaking")
    xkrt_alloc_chunk_t * chunk0 = (xkrt_alloc_chunk_t *) malloc(sizeof(xkrt_alloc_chunk_t));
    assert(chunk0);
    memcpy(chunk0, &(device->memdev.chunk0), sizeof(xkrt_alloc_chunk_t));
    device->memdev.free_chunk_list = chunk0;

    # if USE_STATS
    xkrt_stats_t * stats = xkrt_stats_get();
    stats->memory.freed += stats->memory.allocated.currently;
    stats->memory.allocated.currently = 0;
    # endif /* USE_STATS */
}

void
xkrt_device_memory_set_chunk0(
    xkrt_device_t * device,
    uintptr_t device_ptr,
    size_t size
) {
    device->memdev.chunk0.device_ptr    = device_ptr;
    device->memdev.chunk0.size          = size;
    device->memdev.chunk0.state         = XKRT_ALLOC_CHUNK_STATE_FREE;
    device->memdev.chunk0.prev          = NULL;
    device->memdev.chunk0.next          = NULL;
    device->memdev.chunk0.freelink      = NULL;
    device->memdev.chunk0.use_counter   = 0;

    xkrt_device_memory_reset(device);
}

static inline void
xkrt_device_init_memory(xkrt_device_t * device)
{
    XKRT_MUTEX_INIT(device->memdev.lock);
    xkrt_device_memory_set_chunk0(device, 0, 0);
}

void
xkrt_memory_deallocate(
    xkrt_device_t * device,
    xkrt_alloc_chunk_t * chunk
) {

    bool delete_chunk = false;

    XKRT_MUTEX_LOCK(device->memdev.lock);
    {
        chunk->state = XKRT_ALLOC_CHUNK_STATE_FREE;
        chunk->use_counter = 0;

        /* can we merge chunk into next_chunk ? */
        xkrt_alloc_chunk_t * next_chunk = chunk->next;
        if (next_chunk && next_chunk->state == XKRT_ALLOC_CHUNK_STATE_FREE)
        {
            next_chunk->prev = chunk->prev;
            if (chunk->prev)
                chunk->prev->next = next_chunk;
            next_chunk->size += chunk->size;
            assert(next_chunk->device_ptr > chunk->device_ptr);
            next_chunk->device_ptr = chunk->device_ptr;
            delete_chunk = true;
        }

        xkrt_alloc_chunk_t * prev_chunk = chunk->prev;
        if (prev_chunk)
        {
            /*  if prev_chunk is a free chunk and 'delete_chunk' is true,
             *  then we have to merge prev and next */
            if (prev_chunk->state == XKRT_ALLOC_CHUNK_STATE_FREE)
            {
                if (delete_chunk)
                {
                    assert(prev_chunk->device_ptr < chunk->device_ptr);
                    assert(prev_chunk->device_ptr < next_chunk->device_ptr);

                    prev_chunk->size += next_chunk->size;
                    prev_chunk->next = next_chunk->next;
                    if (next_chunk->next)
                        next_chunk->next->prev = prev_chunk;
                    prev_chunk->freelink = next_chunk->freelink;
                    free(next_chunk);
                }
                else
                {
                    /* merge chunk into prev_chunk */
                    assert(prev_chunk->device_ptr < chunk->device_ptr);
                    prev_chunk->next = chunk->next;
                    if (chunk->next)
                        chunk->next->prev = prev_chunk;
                    prev_chunk->size += chunk->size;
                    delete_chunk = true;
                }
            }
            else if (!delete_chunk)
            {
                /* free_chunk_list is ordered by increasing adress: search form prev the previous bloc */
                while (prev_chunk && prev_chunk->state != XKRT_ALLOC_CHUNK_STATE_FREE)
                    prev_chunk = prev_chunk->prev;

                if (!prev_chunk)
                {
                    chunk->freelink = device->memdev.free_chunk_list;
                    device->memdev.free_chunk_list = chunk;
                }
                else
                {
                    chunk->freelink = prev_chunk->freelink;
                    prev_chunk->freelink = chunk;
                }
            }
        }
        else if (!delete_chunk)
        {
            chunk->freelink = device->memdev.free_chunk_list;
            device->memdev.free_chunk_list = chunk;
        }
    }
    XKRT_MUTEX_UNLOCK(device->memdev.lock);

    # if USE_STATS
    xkrt_stats_t * stats = xkrt_stats_get();
    stats->memory.freed += chunk->size;
    stats->memory.allocated.currently -= chunk->size;
    # endif /* USE_STATS */

    if (delete_chunk)
        free(chunk);
}

xkrt_alloc_chunk_t *
xkrt_memory_allocate(
    xkrt_driver_t * driver,
    xkrt_device_t * device,
    size_t size
) {
    /* adapted from xkrt_memory_alloc */

    /* align data */
    size = (size + 7UL) & ~7UL;

    XKRT_MUTEX_LOCK(device->memdev.lock);

    /* best fit strategy */
    xkrt_alloc_chunk_t * curr = device->memdev.free_chunk_list;
    xkrt_alloc_chunk_t * prevfree = NULL;
    size_t min_size = 0;
    xkrt_alloc_chunk_t * min_size_curr = NULL;
    xkrt_alloc_chunk_t * min_size_prevfree = NULL;

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
        xkrt_alloc_chunk_t * remainder = (xkrt_alloc_chunk_t *) malloc(sizeof(xkrt_alloc_chunk_t));
        remainder->device_ptr   = size + curr->device_ptr;
        remainder->size         = (curr_size - size);
        remainder->state        = XKRT_ALLOC_CHUNK_STATE_FREE;
        remainder->use_counter  = 0;
        remainder->prev         = curr;
        remainder->next         = curr->next;
        remainder->freelink     = curr->freelink;

        /* link remainder segment after curr */
        if (curr->next)
            curr->next->prev = remainder;
        curr->next = remainder;
        curr->size = size;
        curr->freelink = remainder;
    }

    if (curr != NULL)
    {
        if (prevfree)
            prevfree->freelink = curr->freelink;
        else
            device->memdev.free_chunk_list = curr->freelink;
        curr->state = XKRT_ALLOC_CHUNK_STATE_ALLOCATED;
        curr->freelink = NULL;
    }

    XKRT_MUTEX_UNLOCK( device->memdev.lock );

    # if USE_STATS
    if (curr)
    {
        xkrt_stats_t * stats = xkrt_stats_get();
        stats->memory.allocated.total     += size;
        stats->memory.allocated.currently += size;
    }
    # endif /* USE_STATS */

    return curr;
}

void
xkrt_memory_deallocate_all(void)
{
    xkrt_runtime_t * context = xkrt_runtime_get();
    assert(context);

    for (xkrt_device_global_id_t device_global_id = 0 ;
            device_global_id < context->drivers.devices.n ;
            ++device_global_id)
    {
        xkrt_device_t * device = xkrt_device_get(device_global_id);
        assert(device);

        // device memory
        xkrt_device_memory_reset(device);

        // worker thread memory
        ThreadWorker * worker = device->thread;
        assert(worker);
        worker->deallocate_all();
    }
}

///////////////////////////
// DEVICE INITIALIZATION //
///////////////////////////

static void
xkrt_device_commit(
    xkrt_driver_t * driver,
    xkrt_device_t * device
) {
    assert(driver->f_device_commit);
    int err = driver->f_device_commit(device->driver_id);
    if (err)
        LOGGER_FATAL("Commit fail device %d of driver %s", device->driver_id, driver->f_get_name());

    assert(device->state == XKRT_DEVICE_STATE_INIT);
    device->state = XKRT_DEVICE_STATE_COMMIT;
}

static void
xkrt_device_init(
    xkrt_driver_t * driver,
    xkrt_device_t * device
) {
    /* initialize device memory management */
    xkrt_device_init_memory(device);

    /* initialize by the driver */
    driver->f_device_init(device->driver_id);

    /* init offloader */
    xkrt_runtime_t * context = xkrt_runtime_get();
    device->offloader.init(&(context->conf.device.offloader), driver->f_stream_create);

    /* attach current thread to the device */
    if (driver->f_device_attach && driver->f_device_attach(device->driver_id))
        LOGGER_FATAL("Could not attach to device %d of driver %s", device->driver_id, driver->f_get_name());

    assert(device->state == XKRT_DEVICE_STATE_CREATE);
    device->state = XKRT_DEVICE_STATE_INIT;
}

static xkrt_device_t *
xkrt_device_create(
    xkrt_drivers_t * drivers,
    uint8_t driver_id,
    uint8_t device_driver_id
) {
    if (drivers->devices.n == XKRT_DEVICES_MAX)
        LOGGER_FATAL("Too many devices. Increase 'XKRT_DEVICES_MAX' and recompile Xkblas");

    xkrt_driver_t * driver = drivers->list + driver_id;
    assert(driver->f_device_create);

    xkrt_device_t * device = driver->f_device_create(driver, device_driver_id);
    assert(device);

    device->state     = XKRT_DEVICE_STATE_CREATE;
    device->driver_id = device_driver_id;
    device->global_id = drivers->devices.n.fetch_add(1, std::memory_order_seq_cst);

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
xkrt_device_poll(xkrt_device_t * device)
{
    int err = 0;
    assert(ThreadWorker::self() == device->thread);

    err = device->offloader.launch_ready_instructions(XKRT_STREAM_TYPE_ALL);
    assert( (err == 0) || (err == EINPROGRESS));

    err = device->offloader.progress_pending_instructions(XKRT_STREAM_TYPE_ALL, false);
    assert( (err == 0) || (err == EINPROGRESS));

    return err;
}

/* Return 1 iff device may accept new runing offloaded task  */
static inline int
xkrt_device_accept_new_task(xkrt_device_t * device)
{
    # pragma message(TODO "Add conditions here, like the number of kernel in-flight")
    assert(device);
    return 1;
}

static inline void
xkrt_device_prepare_task(
    xkrt_driver_t * driver,
    xkrt_device_t * device,
    Task * task
) {
    assert(task->wc == 0);
    assert(task->state.value == TASK_STATE_READY);

    # ifndef NDEBUG
    LOGGER_INFO("Scheduling task `%s` of format `%d` on device %d - driver `%s`",
            task->label, task->fmtid, device->global_id, driver->f_get_name());
    # endif /* NDEBUG */

    xkrt_runtime_t * context = xkrt_runtime_get();
    assert(context);

    /* increase task 'fetching' counter so it does not get ready early
     * (eg before we processed all accesses bellow) */
    task->fetching();

    /* for each access */
    assert(task->naccesses <= TASK_MAX_ACCESSES);
    for (int i = 0 ; i < task->naccesses ; ++i)
    {
        Access * access = task->accesses + i;
        assert(access);

        MemoryTree * memtree = context->get_memory_tree(access->host_view.ld, access->host_view.sizeof_type);
        assert(memtree);

        memtree->fetch_access(driver, device, task, access);
    }

    /* fetch, return 'TASK_STATE_DATA_FETCHED' if the data got fetched already */
    if (task->fetched() == TASK_STATE_DATA_FETCHED)
    {
        /* all data has been fetched, the task kernel is ready for execution */
        xkrt_device_task_execute(device, task);
    }
    else
    {
        /* task will be launched in a callback while all accesses were fetched */
    }
}

static inline void
xkrt_device_progress(
    xkrt_driver_t * driver,
    xkrt_device_t * device
) {
    int err = xkrt_device_poll(device);
    assert((err == 0) || (err == EINPROGRESS));
}

/* main loop for the thread responsible the passed device */
static inline int
xkrt_device_thread_main_loop(
    xkrt_driver_t * driver,
    xkrt_device_t * device
) {
    assert(ThreadWorker::self() == device->thread);
    assert(device->state == XKRT_DEVICE_STATE_COMMIT);
    device->state = XKRT_DEVICE_STATE_RUNNING;

    # pragma message(TODO "do we really need this mem_barrier here?")
    mem_barrier();

    ThreadWorker * worker = ThreadWorker::self();
    while (device->state == XKRT_DEVICE_STATE_RUNNING)
    {
        Task * task;
        while ((task = worker->pop()) == NULL && device->offloader.is_empty(XKRT_STREAM_TYPE_ALL))
            worker->pause();

        xkrt_device_progress(driver, device);
        if (task)
            xkrt_device_prepare_task(driver, device, task);
    }

    return EINTR;
}

///////////
//  MAIN //
///////////

/* Main entry thread created per device */
void *
xkrt_device_thread_main(void * a)
{
    # pragma message(TODO "Implement device thread")

    xkrt_driver_device_thread_arg_t * arg = (xkrt_driver_device_thread_arg_t *) a;
    xkrt_drivers_t * drivers  = arg->drivers;
    uint8_t driver_id = arg->driver_id;
    uint8_t device_driver_id = arg->device_driver_id;
    free(arg);

    xkrt_driver_t * driver = drivers->list + driver_id;
    unsigned int cpu, node;
    getcpu(&cpu, &node);

    /* init the device */
    xkrt_device_t * device = xkrt_device_create(drivers, driver_id, device_driver_id);
    xkrt_device_init(driver, device);

    LOGGER_INFO("Starting thread for %s device (device_driver_id=%d, device_global_id=%d) on cpu %d of node %d",
            driver->f_get_name(), device_driver_id, device->global_id, cpu, node);

    // wait for all devices of that driver to be in the 'init' state
    ++driver->ndevices_inited;
    while (driver->ndevices_inited < driver->ndevices_targeted)
        mem_pause();

    // can now commit my device
    xkrt_device_commit(driver, device);
    ++driver->ndevices_commited;

    LOGGER_INFO("%s", driver->f_device_info(device_driver_id));

    /* infinite loop with the device context */
    int err = xkrt_device_thread_main_loop(driver, device);
    assert((err==0) || (err==EINTR));

    # pragma message(TODO "Implement proper device deinitialization")
    # if 0

    /* */
#if XKRT_SLEEP_DEVICETHREAD
    xkrt_fifo_register_waiter( device->ld->queue, (void (*)(void *))xkrt_offload_device_wakeup, device );
#else
    xkrt_fifo_register_waiter( device->ld->queue, 0, 0);
#endif

    /* infinite loop with the device context */
    int err = xkrt_sched_idle_offload(thread, _xkrt_device_finalize, device);
    assert((err==0)||(err==eintr));

    /* thread is stopped */
    assert(0 == pthread_mutex_lock(&device->lock));
    device->state = XKRT_DEVICE_STATE_STOPPED;
    assert(0 == pthread_cond_signal(&device->cond_sleep));
    assert(0 == pthread_mutex_unlock(&device->lock));

    assert(0 == pthread_mutex_lock(&device->lock));
    _xkrt_offload_device_finalize(device);
    xkrt_offload_device_pop( device );
    xkrt_localitydomain_destroy(device->ld);
    device->state = XKRT_DEVICE_STATE_FINALIZED;

    if (err != EINTR)
    {
        LOGGER_FATAL("device %d/%p abort with natural interrup\n", device->device_id, (void*)device);
    }
    assert(0 == pthread_mutex_unlock(&device->lock));
    xkrt_thread_unbind(thread);
    _xkrt_self_context = 0;
    device->state = XKRT_DEVICE_STATE_DESTROYED;

# endif

    return NULL;
}
