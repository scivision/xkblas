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

# include <xkrt/memory-tree.hpp>
# include <xkrt/runtime.h>
# include <xkrt/driver/device.h>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/driver/thread-producer.hpp>
# include <xkrt/driver/thread-worker.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mem.h>
# include <xkrt/stats/stats.h>

# include <cassert>
# include <cstring>
# include <cerrno>

# pragma message(TODO "Move these initializer into class member functions")

///////////////////////
//  MEMORY ALLOCATOR //
///////////////////////

void
xkrt_device_memory_reset(xkrt_device_t * device)
{
    # pragma message(TODO "This is leaking")
    xkrt_area_chunk_t * chunk0 = (xkrt_area_chunk_t *) malloc(sizeof(xkrt_area_chunk_t));
    assert(chunk0);
    memcpy(chunk0, &(device->memdev.chunk0), sizeof(xkrt_area_chunk_t));
    device->memdev.free_chunk_list = chunk0;

    XKRT_STATS_INCR(device->stats.memory.freed, device->stats.memory.allocated.currently);
    XKRT_STATS_SET(device->stats.memory.allocated.currently, 0);
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

void
xkrt_memory_deallocate(
    xkrt_device_t * device,
    xkrt_area_chunk_t * chunk
) {
    bool delete_chunk = false;

    XKRT_MUTEX_LOCK(device->memdev.lock);
    {
        chunk->state = XKRT_ALLOC_CHUNK_STATE_FREE;
        chunk->use_counter = 0;

        /* can we merge chunk into next_chunk ? */
        xkrt_area_chunk_t * next_chunk = chunk->next;
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

        xkrt_area_chunk_t * prev_chunk = chunk->prev;
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

    XKRT_STATS_INCR(device->stats.memory.freed, chunk->size);
    XKRT_STATS_DECR(device->stats.memory.allocated.currently, chunk->size);

    if (delete_chunk)
        free(chunk);
}

xkrt_area_chunk_t *
xkrt_memory_allocate(
    xkrt_device_t * device,
    size_t size
) {
    /* adapted from xkrt_memory_alloc */

    /* align data */
    size = (size + 7UL) & ~7UL;

    XKRT_MUTEX_LOCK(device->memdev.lock);

    /* best fit strategy */
    xkrt_area_chunk_t * curr = device->memdev.free_chunk_list;
    xkrt_area_chunk_t * prevfree = NULL;
    size_t min_size = 0;
    xkrt_area_chunk_t * min_size_curr = NULL;
    xkrt_area_chunk_t * min_size_prevfree = NULL;

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
        xkrt_area_chunk_t * remainder = (xkrt_area_chunk_t *) malloc(sizeof(xkrt_area_chunk_t));
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

    if (curr)
    {
        XKRT_STATS_INCR(device->stats.memory.allocated.total,       size);
        XKRT_STATS_INCR(device->stats.memory.allocated.currently,   size);
    }

    return curr;
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

/////////////////////////
//  DEVICE PROGRESSION //
/////////////////////////

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
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    Task * task
) {
    assert(task->wc == 0);
    assert(task->state.value == TASK_STATE_READY);

    # ifndef NDEBUG
    LOGGER_INFO("Scheduling task `%s` of format `%d` on device %d",
            task->label, task->fmtid, device->global_id);
    # endif /* NDEBUG */

    /* increase task 'fetching' counter so it does not get ready early
     * (eg before we processed all accesses bellow) */
    task->fetching();

    /* for each access */
    assert(task->naccesses <= TASK_MAX_ACCESSES);
    for (int i = 0 ; i < task->naccesses ; ++i)
    {
        Access * access = task->accesses + i;
        assert(access);

        MemoryCoherencyController * memcontroller = runtime->get_or_insert_memory_controller(access->host_view.ld, access->host_view.sizeof_type);
        assert(memcontroller);

        // TODO " pass task
        memcontroller->fetch(task, access, device->global_id);
    }

    /* fetch, return 'TASK_STATE_DATA_FETCHED' if the data got fetched already */
    if (task->fetched() == TASK_STATE_DATA_FETCHED)
    {
        /* all data has been fetched, the task kernel is ready for execution */
        xkrt_device_task_execute(runtime, device, task);
    }
    else
    {
        /* task will be launched in a callback while all accesses were fetched */
    }
}

static inline void
xkrt_device_progress(xkrt_device_t * device)
{
    int err = xkrt_device_poll(device);
    assert((err == 0) || (err == EINPROGRESS));
}

/* main loop for the thread responsible the passed device */
static inline int
xkrt_device_thread_main_loop(
    xkrt_runtime_t * runtime,
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

        xkrt_device_progress(device);
        if (task)
            xkrt_device_prepare_task(runtime, device, task);
    }

    return EINTR;
}

///////////
//  MAIN //
///////////

/* Main entry thread created per device */
void
xkrt_device_thread_main(void * vruntime, xkrt_driver_type_t driver_type, uint8_t device_driver_id)
{
    # pragma message(TODO "Implement device thread")

    xkrt_runtime_t * runtime = (xkrt_runtime_t *) vruntime;
    xkrt_driver_t * driver = runtime->drivers.list + driver_type;

    unsigned int cpu, node;
    getcpu(&cpu, &node);

    // create device
    assert(driver->f_device_create);
    xkrt_device_t * device = driver->f_device_create(driver, device_driver_id);
    assert(device);

    device->state       = XKRT_DEVICE_STATE_CREATE;
    device->driver_type = driver_type;
    device->driver_id   = device_driver_id;
    device->global_id   = runtime->drivers.devices.n.fetch_add(1, std::memory_order_seq_cst);

    runtime->drivers.devices.list[device->global_id] = device;

    // register worker thread
    ThreadWorker::init();
    device->thread = ThreadWorker::self();
    assert(device->thread);

    // init device

    /* initialize by the driver */
    driver->f_device_init(device->driver_id);

    /* init offloader */
    device->offloader.init(&(runtime->conf.device.offloader), driver->f_stream_create);

    /* attach current thread to the device */
    if (driver->f_device_attach)
        if (driver->f_device_attach(device->driver_id))
            LOGGER_FATAL("Could not attach to device %d of driver %s", device->driver_id, driver->f_get_name());

    /* init memory */
    XKRT_MUTEX_INIT(device->memdev.lock);

    /* get total memory and allocate chunk0 */
    assert(driver->f_memory_info);
    size_t total;
    driver->f_memory_info(device->driver_id, &total);
    const size_t size = (size_t) ((double)total * (double)(runtime->conf.device.gpu_mem_percent / 100.0));

    assert(driver->f_memory_alloc);
    const void * device_ptr = driver->f_memory_alloc(device->driver_id, size);
    xkrt_device_memory_set_chunk0(device, (uintptr_t) device_ptr, size);

    assert(device->state == XKRT_DEVICE_STATE_CREATE);
    device->state = XKRT_DEVICE_STATE_INIT;

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
    int err = xkrt_device_thread_main_loop(runtime, device);
    assert((err==0) || (err==EINTR));

    # pragma message(TODO "Implement proper device deinitialization")
}

////////////////////
// Task execution //
////////////////////

static void
xkrt_device_task_executed_callback(
    const void * args[XKRT_CALLBACK_ARGS_MAX]
) {
    xkrt_runtime_t * runtime = (xkrt_runtime_t *) args[0];
    assert(runtime);

    Task * task = (Task *) args[1];
    assert(task);

    runtime->complete(task);
}

/**
 * Must be called once all task accessed were fetched, to queue the task kernel for execution
 *  - driver - the driver to use for executing the kernel
 *  - device - the device to use for executing the kernel
 *  - task   - the task
 */
void
xkrt_device_task_execute(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    Task * task
) {
    # ifndef NDEBUG
    LOGGER_INFO("Task `%s` is ready for kernel execution", task->label);
    # endif /* NDEBUG */

    /* running an empty task */
    if (task->fmtid == TASK_FORMAT_NULL)
    {
        runtime->complete(task);
    }
    else
    {
        /* retrieve task format */
        task_format_t * format = task_format_get(&(runtime->task_formats), task->fmtid);
        assert(format);

        // convert device driver type to task format target
        task_format_target_t targetfmt;
        switch (device->driver_type)
        {
            # define CASE(X)                            \
                case (XKRT_DRIVER_TYPE_##X):            \
                    targetfmt = TASK_FORMAT_TARGET_##X; \
                    break ;

            CASE(HOST)
            CASE(CUDA)
            CASE(ZE)

            default:
                LOGGER_FATAL("Invalid device driver type");
        }

        if (format->f[targetfmt] == NULL)
            targetfmt = TASK_FORMAT_TARGET_HOST;

        if (format->f[targetfmt] == NULL)
            LOGGER_FATAL("Task got scheduled but its format has no valid function");

        /* running a host task */
        if (targetfmt == TASK_FORMAT_TARGET_HOST)
        {
            format->f[targetfmt](NULL, task);
            runtime->complete(task);
        }
        /* running a device task */
        else
        {
            /* the callback will be called asynchronously in the
             * driver on kernel completion test success */
            xkrt_callback_t callback;
            callback.func    = xkrt_device_task_executed_callback;
            callback.args[0] = runtime;
            callback.args[1] = task;
            assert(XKRT_CALLBACK_ARGS_MAX >= 2);

            /* submit kernel launch instruction */
            xkrt_device_stream_instruction_submit_kernel(device, format->f[targetfmt], task, callback);

            if (device->thread == ThreadWorker::self()) // TODO : explain why this
                device->offloader.launch_ready_instructions(XKRT_STREAM_TYPE_KERN);
            /* else kernel launch will be called asynchronously */
        }
    }
}

static inline void
xkrt_device_wait(xkrt_device_t * device)
{
    LOGGER_DEBUG("Waiting for device %d...", device->global_id);
    device->offloader.progress_pending_instructions(XKRT_STREAM_TYPE_ALL, true);
}

///////////////
// Utilities //
///////////////

xkrt_driver_t *
xkrt_runtime_t::driver_get(
    const xkrt_driver_type_t type
) {
    assert(type >= 0);
    assert(type < XKRT_DRIVER_TYPE_MAX);
    return this->drivers.list + type;
}

xkrt_device_t *
xkrt_runtime_t::device_get(
    const xkrt_device_global_id_t device_global_id
) {
    assert(device_global_id >= 0);
    assert(device_global_id < this->drivers.devices.n);
    return this->drivers.devices.list[device_global_id];
}

MemoryCoherencyController *
xkrt_runtime_t::get_or_insert_memory_controller(
    const size_t ld,
    const size_t sizeof_type
) {

    /* find previous memtree for that ld */
    for (MemoryCoherencyController * memcontroller : this->memcontrollers)
    {
        MemoryTree * memtree = (MemoryTree *) memcontroller;
        if (memtree->ld == ld && memtree->sizeof_type == sizeof_type)
            return memcontroller;
    }

    LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
            ld, sizeof_type, this->conf.merge_transfers ? "true" : "false");

    /* if not found, create a new memtree */
    MemoryCoherencyController * memcontroller = new MemoryTree(this, ld, sizeof_type, this->conf.merge_transfers);
    assert(memcontroller);;
    this->memcontrollers.push_back(memcontroller);

    return memcontroller;
}

xkrt_area_chunk_t *
xkrt_runtime_t::memory_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return xkrt_memory_allocate(device, size);
}

void
xkrt_runtime_t::memory_deallocate(
    const xkrt_device_global_id_t device_global_id,
    xkrt_area_chunk_t * chunk
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return xkrt_memory_deallocate(device, chunk);

}

void
xkrt_runtime_t::submit_copy(
    const xkrt_device_global_id_t   device_global_id,
    const memory_view_t           & host_view,
    const xkrt_device_global_id_t   dst_device_global_id,
    const memory_replicate_view_t & dst_device_view,
    const xkrt_device_global_id_t   src_device_global_id,
    const memory_replicate_view_t & src_device_view,
    const xkrt_callback_t         & callback
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_device_stream_instruction_submit_copy(device, host_view, dst_device_global_id, dst_device_view, src_device_global_id, src_device_view, callback);
}

void
xkrt_runtime_t::task_execute(
    Task * task,
    const xkrt_device_global_id_t device_global_id
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_device_task_execute(this, device, task);
}

/////////////////////////////////////////////

static inline void
enqueue(void * vargs, Task * task)
{
    xkrt_runtime_submit_task((xkrt_runtime_t *) vargs, task);
}

void
xkrt_runtime_t::commit(Task * task)
{
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    thread->commit<enqueue>(this, task);
}

void
xkrt_runtime_t::complete(Task * task)
{
    assert(task);

    # ifndef NDEBUG
    LOGGER_INFO("task `%s` completed", task->label);
    # endif /* NDEBUG */

    ThreadWorker * thread = ThreadWorker::self();
    assert(thread);

    thread->complete<enqueue>(this, task);
}
