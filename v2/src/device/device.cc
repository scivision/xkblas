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

static void
xkblas_device_commit(
    xkblas_driver_t * driver,
    xkblas_device_t * device
) {
    assert(driver->f_device_commit);
    int err = driver->f_device_commit(device->driver_id);
    if (err)
        XKBLAS_FATAL("Commit fail device %d of driver %s", device->driver_id, driver->f_get_name());

    # if 0
    assert(0 == pthread_mutex_lock(&device->lock));
    assert(0 == pthread_cond_signal(&device->cond_sleep));
    assert(0 == pthread_mutex_unlock(&device->lock));
    # endif

    assert(device->state == XKBLAS_DEVICE_STATE_INIT);
    device->state = XKBLAS_DEVICE_STATE_COMMIT;
}

static void
xkblas_device_init(
    xkblas_driver_t * driver,
    xkblas_device_t * device
) {
    # if 0
    device->tid = 0;
    device->spawn_count = 0;
    device->exec_count = 0;
    device->finalize = false;
    assert(0 == pthread_mutex_init(&device->lock, 0));
    assert(0 == pthread_cond_init(&device->cond, 0));
    assert(0 == pthread_cond_init(&device->cond_sleep, 0));
    device->issleeping = 0;
    device->request.op = XKBLAS_DEVICE_REQUEST_TYPE_NOP;
    device->request.arg = 0;
    device->request.counter = 0;

    device->cnt_push = 0;
    # endif

    driver->f_device_init(device->driver_id);

    # if 0
    device->time_tasks = 0.0;
    device->exectasks  = 0;
    device->flops_exectasks = 0.0;
    device->data_exectasks = 0.0;
    device->submittasks = 0;
    device->flops_submittasks= 0.0;
    device->data_submittasks = 0.0;

    device->cnt_pending = 0;
    device->cnt_ready = 0;
    device->cnt_exec = 0;
    # endif

    xkblas_context_t * context = xkblas_context_get();
    device->offloader.init(&(context->conf.device.offloader), driver->f_stream_create);

    /* initialize device memory management */
    XKBLAS_MUTEX_INIT( device->memdev.mem_lock );

    # if 0
    assert(0 == pthread_mutex_lock(&device->lock));
    assert(0 == pthread_cond_signal(&device->cond_sleep));
    assert(0 == pthread_mutex_unlock(&device->lock));
    # endif

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

    // 'prepare_execute:' label

# if 0
    // TODO : this part of the code had been changed quite a bit, check performances impact
    xkblas_device_poll(device);
    while (!xkblas_device_accept_new_task(device))
    {
        xkblas_device_poll(device);
        int err = device->offloader.progress_pending_instructions(XKBLAS_STREAM_TYPE_D2D, true);   // Romain: why wait on D2D ? (inherited from kaapi)
        assert(err == 0);
        xkblas_device_poll(device);
    }
# endif

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
    // TODO : implement 'kaapi_sched_idle_offload' switch case

    # pragma message(TODO "Do we really need all that 'device request' stuff ? Just poll streams for now")

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
        # if 0
        # pragma message(TODO "'device->offloader.is_empty' is called with no lock, while inner lists are modifed under locks, is this a problem ?")
        // If there is no tasks and streams are empty, sleep the thread
        Task * task;
        while ((task = worker->pop()) == NULL &&
                device->offloader.is_empty(XKBLAS_STREAM_TYPE_ALL) &&
                device->request.type == XKBLAS_DEVICE_REQUEST_TYPE_NOP)
            worker->pause();    // TODO : bad design, 'worker' must be 'ThreadWorker::self()' !!

        XKBLAS_DEBUG("Thread of device %d of driver %s is working, task=%p, offloader.is_empty()=%d",
                device->global_id, driver->f_get_name(), task, device->offloader.is_empty(XKBLAS_STREAM_TYPE_ALL));
        # else
        Task * task = worker->pop();
        # endif

        if (task)
            xkblas_device_prepare_task(driver, device, task);
        else
            xkblas_device_progress(driver, device);
    }

    return EINTR;
}

/* Main entry thread created per device */
void *
xkblas_device_thread_main(void * a)
{
    # pragma message(TODO "Implement device thread")

    xkblas_driver_device_thread_arg_t * arg = (xkblas_driver_device_thread_arg_t *) a;
    xkblas_drivers_t * drivers  = arg->drivers;
    uint8_t driver_id = arg->driver_id;
    int driver_device_id = arg->driver_device_id;
    free(arg);

    xkblas_driver_t * driver = drivers->list + driver_id;
    unsigned int cpu, node;
    getcpu(&cpu, &node);

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
