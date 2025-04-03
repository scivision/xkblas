/* ************************************************************************** */
/*                                                                            */
/*   runtime-sched.cc                                                         */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 07:13:31 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

// TODO : split this file, its a trashbin atm

# include <xkrt/memory-tree.hpp>
# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/bits-to-str.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mem.h>
# include <xkrt/stats/stats.h>
# include <xkrt/task/task.hpp>

# include <cassert>
# include <cstring>
# include <cerrno>

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif /* _GNU_SOURCE */
# include <sched.h> /* getcpu */

# pragma message(TODO "Move these initializer into class member functions")

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

inline void
xkrt_device_prepare_task(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    xkrt_device_global_id_t device_global_id,
    task_t * task
) {
    assert((device == NULL && device_global_id == HOST_DEVICE_GLOBAL_ID) || (device && device->global_id == device_global_id));
    assert(  task->state.value == TASK_STATE_READY);

    LOGGER_DEBUG("Preparing task `%s` of format `%d` on device %d",
            task->label, task->fmtid, device_global_id);

    if (task->flags & TASK_FLAG_DEPENDENT)
    {
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        assert(TASK_DEP_INFO(task)->wc == 0);

        /* increase task 'fetching' counter so it does not get ready early
         * (eg before we processed all accesses bellow) */
        __task_fetching(1, task);

        /* for each access */
        assert(dep->ac <= TASK_MAX_ACCESSES);
        access_t * accesses = TASK_ACCESSES(task);
        for (int i = 0 ; i < dep->ac ; ++i)
        {
            access_t * access = accesses + i;

            MemoryCoherencyController * memcontroller = runtime->get_or_insert_memory_controller(access->host_view.ld, access->host_view.sizeof_type);
            assert(memcontroller);

            memcontroller->fetch(task, access, device_global_id);
        }

        /* decrease the task 'fetching' counter to detect early-fetch completion */
        __task_fetched(1, task, xkrt_device_task_execute, runtime, device);
        /* else the task will be launched in a callback while all accesses were fetched */
    }
    else
    {
        xkrt_device_task_execute(runtime, device, task);
    }
}

/* main loop for the thread responsible the passed device */
static inline int
xkrt_device_thread_main_loop(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    xkrt_thread_t * thread,
    uint8_t device_tid
) {
    assert(thread == xkrt_thread_t::get_tls());

    while (device->state == XKRT_DEVICE_STATE_COMMIT)
    {
        task_t * task;
        while ((task = thread->deque.pop()) == NULL &&
                device->offloader_streams_are_empty(device_tid, XKRT_STREAM_TYPE_ALL) &&
                device->state == XKRT_DEVICE_STATE_COMMIT)
            thread->pause();

        if (device->state != XKRT_DEVICE_STATE_COMMIT)
        {
            assert(device->state == XKRT_DEVICE_STATE_STOP);
            break ;
        }

        if (!device->offloader_streams_are_empty(device_tid, XKRT_STREAM_TYPE_ALL))
            device->offloader_poll(device_tid);

        if (task)
            xkrt_device_prepare_task(runtime, device, device->global_id, task);
    }

    return EINTR;
}

///////////
//  MAIN //
///////////

/* Main entry thread created per device */
void *
xkrt_device_thread_main(
    xkrt_team_t * team,
    xkrt_thread_t * thread
) {
    // xkrt_thread_t * thread, xkrt_driver_type_t driver_type, uint8_t device_driver_id)
    # pragma message(TODO "Implement device thread")

    // unpack args
    xkrt_device_team_args_t * args = (xkrt_device_team_args_t *) team->desc.args;
    assert(args);

    // get the device id of that thread in the args->devices list
    int id = thread->tid % args->ndevices;

    // unpack args runtime
    xkrt_runtime_t * runtime        = args->runtime;
    xkrt_driver_type_t driver_type  = args->devices[id].driver_type;
    uint8_t device_driver_id        = args->devices[id].device_driver_id;

    // get the driver
    xkrt_driver_t * driver = runtime->driver_get(driver_type);
    int is_device_main_thread = thread->tid < args->ndevices;

    // create device
    if (is_device_main_thread)
    {
        assert(driver->f_device_create);

        // create the device
        xkrt_device_t * device = driver->f_device_create(driver, device_driver_id);
        assert(device);

        // initialize device attributes
        device->state       = XKRT_DEVICE_STATE_CREATE;
        device->driver_type = driver_type;
        device->driver_id   = device_driver_id;
        device->global_id   = runtime->drivers.devices.n.fetch_add(1, std::memory_order_seq_cst);
        device->conf        = &(runtime->conf.device);

        // register device to the global list
        runtime->drivers.devices.list[device->global_id] = device;

        // register device to the driver list
        driver->devices[device_driver_id] = device;

        // init device by the driver
        driver->f_device_init(device->driver_id);
        LOGGER_INFO("%s", driver->f_device_info(device_driver_id));

        /* get total memory and allocate chunk0 */
        if (driver->f_memory_device_info)
        {
            driver->f_memory_device_info(device->driver_id, device->memories, &device->nmemories);
            assert(device->nmemories > 0);
            for (int i = 0 ; i < device->nmemories ; ++i)
            {
                xkrt_device_memory_info_t * info = device->memories + i;
                LOGGER_INFO("Found memory `%s` of capacity %zuGB", info->name, info->capacity/(size_t)1e9);

                XKRT_MUTEX_INIT(info->area.lock);
                const size_t size = (size_t) ((double)info->capacity * (double)(runtime->conf.device.gpu_mem_percent / 100.0));

                assert(driver->f_memory_device_allocate);
                const void * device_ptr = driver->f_memory_device_allocate(device->driver_id, size, i);
                device->memory_set_chunk0((uintptr_t) device_ptr, size, i);
            }
        }

        assert(device->state == XKRT_DEVICE_STATE_CREATE);
        device->state = XKRT_DEVICE_STATE_INIT;
    }

    // wait for all devices to be in the 'init' state and for all threads to join
    pthread_barrier_wait(&runtime->drivers.devices.barrier);

    // register the device thread
    xkrt_device_t * device = driver->devices[device_driver_id];
    assert(device);
    uint8_t device_tid = device->nthreads.fetch_add(1, std::memory_order_relaxed);
    device->threads[device_tid] = thread;

    // print thread
    unsigned int cpu, node;
    getcpu(&cpu, &node);
    LOGGER_INFO("Starting thread for %s device (device_driver_id=%d, device_global_id=%d) on cpu %d of node %d",
            driver->f_get_name(), device_driver_id, device->global_id, cpu, node);

    // 'commit' all devices
    if (is_device_main_thread)
    {
        // commit
        assert(driver->f_device_commit);
        xkrt_device_global_id_bitfield_t * affinity = &(runtime->router.affinity[device->global_id][0]);
        memset(affinity, 0, sizeof(runtime->router.affinity[device->global_id]));
        int err = driver->f_device_commit(device->driver_id, affinity);
        if (err)
            LOGGER_FATAL("Commit fail device %d of driver %s", device->driver_id, driver->f_get_name());
        assert(device->state == XKRT_DEVICE_STATE_INIT);
        device->state = XKRT_DEVICE_STATE_COMMIT;
        ++driver->ndevices_commited;

        // print affinity
        for (int i = 0 ; i < XKRT_DEVICES_PERF_RANK_MAX ; ++i)
        {
            xkrt_device_global_id_bitfield_t bf = affinity[i];
            int nbytes = sizeof(xkrt_device_global_id_bitfield_t);
            char buffer[8*nbytes + 1];
            xkrt_bits_to_str(buffer, (unsigned char *) &bf, nbytes);
            LOGGER_DEBUG("Device `%2u` affinity mask for perf `%2u` is `%s`", device->global_id, i, buffer);
        }

        // init offloader
        device->offloader_init(driver->f_stream_suggest);
    }

    // wait for all devices to be in the 'commit' state with the offloader init
    pthread_barrier_wait(&runtime->drivers.devices.barrier);

    // initialize offloader thread
    device->offloader_init_thread(device_tid, driver->f_stream_create);

    // wait for all threads to have streams initialized
    pthread_barrier_wait(&runtime->drivers.devices.barrier);
    // cannot use 'args->barrier' after this point

    /* infinite loop with the device context */
    int err = xkrt_device_thread_main_loop(runtime, device, thread, device_tid);
    assert((err==0) || (err==EINTR));

    // delete streams
    if (driver->f_stream_delete)
        for (uint8_t j = 0 ; j < XKRT_STREAM_TYPE_ALL ; ++j)
            for (int k = 0 ; k < device->count[j] ; ++k)
                driver->f_stream_delete(device->streams[device_tid][j][k]);

    // wait for all thread to delete their streams
    pthread_barrier_wait(&runtime->drivers.devices.barrier);

    /* deinitialize driver */
    if (is_device_main_thread)
    {
        // release memory
        if (driver->f_memory_device_deallocate)
        {
            for (int j = 0 ; j < device->nmemories ; ++j)
            {
                xkrt_area_t * area = &(device->memories[j].area);
                driver->f_memory_device_deallocate(device->driver_id, (void *) area->chunk0.ptr, area->chunk0.size, j);
            }
        }
        else
            LOGGER_WARN("Driver `%u` is missing `f_device_memory_deallocate`", driver_type);

        // delete device
        if (driver->f_device_destroy)
            driver->f_device_destroy(device->driver_id);
        else
            LOGGER_WARN("Driver `%u` is missing `f_device_destroy`", driver_type);
    }

    /* wait for all the main thread to deinit */
    pthread_barrier_wait(&runtime->drivers.devices.barrier);

    return NULL;
}

//////////////////////
// task_t execution //
//////////////////////

static void
xkrt_device_task_executed_callback(
    const void * args[XKRT_CALLBACK_ARGS_MAX]
) {
    xkrt_runtime_t * runtime = (xkrt_runtime_t *) args[0];
    assert(runtime);

    task_t * task = (task_t *) args[1];
    assert(task);

    __task_executed(task, xkrt_runtime_submit_task, runtime);
}

/**
 * Must be called once all task accesses were fetched, to queue the task kernel for execution
 *  - driver - the driver to use for executing the kernel
 *  - device - the device to use for executing the kernel
 *  - task   - the task
 */
void
xkrt_device_task_execute(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    task_t * task
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    task_t * current = thread->current_task;
    thread->current_task = task;

    /* running an empty task */
    if (task->fmtid == TASK_FORMAT_NULL)
    {
        __task_executed(task, xkrt_runtime_submit_task, runtime);
    }
    else if (device)
    {
        /* retrieve task format */
        task_format_t * format = task_format_get(&(runtime->formats.list), task->fmtid);
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
            LOGGER_FATAL("task_t got scheduled but its format has no valid function");

        /* running a host task */
        if (targetfmt == TASK_FORMAT_TARGET_HOST)
        {
            ((void (*)(task_t *)) format->f[targetfmt])(task);
            __task_executed(task, xkrt_runtime_submit_task, runtime);
        }
        /* running a device task */
        else
        {
            /* the task will complete in the callback called asynchronously on kernel completion */
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
        }
    }
    else
    {
        /* retrieve task format */
        task_format_t * format = task_format_get(&(runtime->formats.list), task->fmtid);
        if (format)
        {
            assert(format->f[TASK_FORMAT_TARGET_HOST]);
            ((void (*)(task_t *)) format->f[TASK_FORMAT_TARGET_HOST])(task);
        }
        __task_executed(task, xkrt_runtime_submit_task, runtime);
    }

    thread->current_task = current;
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
    return this->drivers.list[type];
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
    SPINLOCK_LOCK(this->memcontrollers_lock);

    /* find previous memtree for that ld */
    for (MemoryCoherencyController * memcontroller : this->memcontrollers)
    {
        MemoryTree * memtree = (MemoryTree *) memcontroller;
        if (memtree->ld == ld && memtree->sizeof_type == sizeof_type)
        {
            SPINLOCK_UNLOCK(this->memcontrollers_lock);
            return memcontroller;
        }
    }

    LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
            ld, sizeof_type, this->conf.merge_transfers ? "true" : "false");

    /* if not found, create a new memtree */
    MemoryCoherencyController * memcontroller = new MemoryTree(this, ld, sizeof_type, this->conf.merge_transfers);
    assert(memcontroller);
    this->memcontrollers.push_back(memcontroller);

    SPINLOCK_UNLOCK(this->memcontrollers_lock);

    return memcontroller;
}

xkrt_area_chunk_t *
xkrt_runtime_t::memory_device_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_allocate(size);
}

void
xkrt_runtime_t::memory_device_deallocate(
    const xkrt_device_global_id_t device_global_id,
    xkrt_area_chunk_t * chunk
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_deallocate(chunk);
}

void
xkrt_runtime_t::memory_device_deallocate_all(
    const xkrt_device_global_id_t device_global_id
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_reset();
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

void *
xkrt_runtime_t::memory_unified_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_unified_allocate)
        return driver->f_memory_unified_allocate(device->driver_id, size);
    else
    {
        LOGGER_FATAL("Driver `%s` does not implement memory_alloc_unified", driver->f_get_name());
    }
}

void
xkrt_runtime_t::memory_unified_deallocate(
    const xkrt_device_global_id_t device_global_id,
    void * mem,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_unified_deallocate)
        driver->f_memory_unified_deallocate(device->driver_id, mem, size);
    else
    {
        LOGGER_FATAL("Driver `%s` does not implement memory_dealloc_unified", driver->f_get_name());
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

//////////
// TASK //
//////////

void
xkrt_runtime_t::task_submit(
    const xkrt_device_global_id_t device_global_id,
    task_t * task
) {
    xkrt_device_t * device = (device_global_id == HOST_DEVICE_GLOBAL_ID) ? NULL : this->device_get(device_global_id);
    XKRT_STATS_INCR(this->stats.tasks[task->fmtid].submitted, 1);
    xkrt_device_task_execute(this, device, task);
}

/* submit a task to the given device */
void
xkrt_device_task_submit(
    xkrt_runtime_t * runtime,
    xkrt_device_global_id_t device_global_id,
    task_t * task
) {
    runtime->task_submit(device_global_id, task);
}

void
xkrt_runtime_t::task_commit(task_t * task)
{
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    thread->commit(task, xkrt_runtime_submit_task, this);
    XKRT_STATS_INCR(this->stats.tasks[task->fmtid].commited, 1);
}

void
xkrt_runtime_t::task_detachable_post(task_t * task)
{
    assert(task);
    assert(task->flags & TASK_FLAG_DETACHABLE);
    __task_detachable_post(task, xkrt_runtime_submit_task, this);
}

void
xkrt_runtime_t::task_complete(task_t * task)
{
    assert(task);
    assert(!(task->flags & TASK_FLAG_DETACHABLE));

    __task_complete(task, xkrt_runtime_submit_task, this);
    XKRT_STATS_INCR(this->stats.tasks[task->fmtid].completed, 1);
}

void
xkrt_team_thread_task_enqueue(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_thread_t * thread,
    task_t * task
) {
    thread->deque.push(task);
}

