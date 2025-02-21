/* ************************************************************************** */
/*                                                                            */
/*   runtime-sched.cc                                                         */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 01:12:01 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

// TODO : split this file, its a trashbin atm

# include <xkrt/memory-tree.hpp>
# include <xkrt/runtime.h>
# include <xkrt/driver/device.hpp>
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
        while ((task = worker->pop()) == NULL && device->offloader_streams_are_empty(XKRT_STREAM_TYPE_ALL))
            worker->pause();

        device->offloader_poll();
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

    // initialize device attributes
    device->state       = XKRT_DEVICE_STATE_CREATE;
    device->driver_type = driver_type;
    device->driver_id   = device_driver_id;
    device->global_id   = runtime->drivers.devices.n.fetch_add(1, std::memory_order_seq_cst);
    device->conf        = &(runtime->conf.device);
    XKRT_MUTEX_INIT(device->area.lock);

    // register worker thread
    ThreadWorker::init();
    device->thread = ThreadWorker::self();
    assert(device->thread);

    // register affinity
    xkrt_runtime_t::thread_getaffinity(device->thread->cpuset);

    // register device to the driver list
    runtime->drivers.devices.list[device->global_id] = device;

    // init device by the driver
    driver->f_device_init(device->driver_id);
    LOGGER_INFO("%s", driver->f_device_info(device_driver_id));

    // initialize offloader
    device->offloader_init(driver->f_stream_create);

    // attach current thread to the device (cuda state machine...)
    if (driver->f_device_attach)
        if (driver->f_device_attach(device->driver_id))
            LOGGER_FATAL("Could not attach to device %d of driver %s", device->driver_id, driver->f_get_name());

    /* init memory */

    /* get total memory and allocate chunk0 */
    assert(driver->f_memory_device_info);
    xkrt_device_memory_info_t info;
    driver->f_memory_device_info(device->driver_id, &info);
    const size_t size = (size_t) ((double)info.capacity * (double)(runtime->conf.device.gpu_mem_percent / 100.0));

    assert(driver->f_memory_device_allocate);
    const void * device_ptr = driver->f_memory_device_allocate(device->driver_id, size);
    device->memory_set_chunk0((uintptr_t) device_ptr, size);

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

    runtime->task_complete(task);
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
        runtime->task_complete(task);
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
            CASE(CL)

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
            ((void (*)(Task *)) format->f[targetfmt])(task);
            runtime->task_complete(task);
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
            device->offloader_stream_instruction_submit_kernel(
                (void (*)(void *, void *, xkrt_stream_instruction_counter_t)) format->f[targetfmt],
                task,
                callback
            );

            if (device->thread == ThreadWorker::self()) // TODO : explain why this
                device->offloader_stream_instructions_launch(XKRT_STREAM_TYPE_KERN);
            /* else kernel launch will be called asynchronously */
        }
    }
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
    return device->memory_allocate(size);
}

void
xkrt_runtime_t::memory_deallocate(
    const xkrt_device_global_id_t device_global_id,
    xkrt_area_chunk_t * chunk
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_deallocate(chunk);
}

void
xkrt_runtime_t::memory_deallocate_all(
    const xkrt_device_global_id_t device_global_id
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_reset();
}

void
xkrt_runtime_t::memory_info(
    const xkrt_device_global_id_t device_global_id,
    xkrt_device_memory_info_t * info
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    assert(driver->f_memory_device_info);
    driver->f_memory_device_info(device->driver_id, info);
}

void *
xkrt_runtime_t::memory_host_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_host_allocate)
        return driver->f_memory_host_allocate(device->driver_id, size);
    else
    {
        LOGGER_WARN("Driver `%s` does not implement memory_alloc_host", driver->f_get_name());
        return malloc(size);
    }
}

void
xkrt_runtime_t::memory_host_deallocate(
    const xkrt_device_global_id_t device_global_id,
    void * mem,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_host_deallocate)
        driver->f_memory_host_deallocate(device->driver_id, mem, size);
    else
    {
        LOGGER_WARN("Driver `%s` does not implement memory_dealloc_host", driver->f_get_name());
        free(mem);
    }
}

void
xkrt_runtime_t::copy(
    const xkrt_device_global_id_t   device_global_id,
    const memory_view_t           & host_view,
    const xkrt_device_global_id_t   dst_device_global_id,
    const memory_replicate_view_t & dst_device_view,
    const xkrt_device_global_id_t   src_device_global_id,
    const memory_replicate_view_t & src_device_view,
    const xkrt_callback_t         & callback
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    device->offloader_stream_instruction_submit_copy<memory_view_t, memory_replicate_view_t>(
        host_view,
        dst_device_global_id,
        dst_device_view,
        src_device_global_id,
        src_device_view,
        callback
    );
}

void
xkrt_runtime_t::copy(
    const xkrt_device_global_id_t   device_global_id,
    const size_t                    size,
    const xkrt_device_global_id_t   dst_device_global_id,
    const uintptr_t                 dst_device_addr,
    const xkrt_device_global_id_t   src_device_global_id,
    const uintptr_t                 src_device_addr,
    const xkrt_callback_t         & callback
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    device->offloader_stream_instruction_submit_copy<size_t, uintptr_t>(
        size,
        dst_device_global_id,
        dst_device_addr,
        src_device_global_id,
        src_device_addr,
        callback
    );
}

void
xkrt_runtime_t::wait_device(xkrt_device_global_id_t device_global_id)
{
    const xkrt_device_t * device = this->device_get(device_global_id);
    while (!device->offloader_streams_are_empty(XKRT_STREAM_TYPE_ALL))
        usleep(5);
}

//////////
// TASK //
//////////

void
xkrt_runtime_t::task_submit(
    Task * task,
    const xkrt_device_global_id_t device_global_id
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_device_task_execute(this, device, task);
}

static inline void
enqueue(void * vargs, Task * task)
{
    xkrt_runtime_submit_task((xkrt_runtime_t *) vargs, task);
}

void
xkrt_runtime_t::task_commit(Task * task)
{
    ThreadProducer * thread = ThreadProducer::self();
    assert(thread);

    thread->commit<enqueue>(this, task);
}

void
xkrt_runtime_t::task_complete(Task * task)
{
    assert(task);

    # ifndef NDEBUG
    LOGGER_INFO("task `%s` completed", task->label);
    # endif /* NDEBUG */

    ThreadWorker * thread = ThreadWorker::self();
    assert(thread);

    thread->complete<enqueue>(this, task);
}
