# include "xkblas-context.h"
# include "conf/conf.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/stream.hpp"
# include "logger/logger.h"
# include "logger/todo.h"
# include "scheduler/thread-worker.hpp"
# include "sync/mem.h"

# include <cassert>
# include <cstring>
# include <cerrno>

# pragma message(TODO "Move these initializer into class member functions")

void
xkblas_device_stream_init(xkblas_device_t * device, Stream * stream, unsigned int capacity)
{
    # pragma message(TODO "wtf is this madness ? Lacking knowledge on the rest of the code, leaving stream initialization for now")

    # if 0
    unsigned int cnt = 0;
    unsigned int prefix[XKBLAS_IO_STREAM_ALL+1];

    prefix[XKBLAS_IO_STREAM_H2D] = 0;
    cnt += (stream->count[XKBLAS_IO_STREAM_H2D]  = XKBLAS_CONF.cuda_conc_h2d);
    prefix[XKBLAS_IO_STREAM_D2H] = cnt;
    cnt += (stream->count[XKBLAS_IO_STREAM_D2H]  = XKBLAS_CONF.cuda_conc_d2h);
    prefix[XKBLAS_IO_STREAM_D2D] = cnt;
    cnt += (stream->count[XKBLAS_IO_STREAM_D2D]  = XKBLAS_CONF.cuda_conc_d2d);
    prefix[XKBLAS_IO_STREAM_KERN] = cnt;
    cnt += (stream->count[XKBLAS_IO_STREAM_KERN] = XKBLAS_CONF.cuda_conc_stream_kernel);
    prefix[XKBLAS_IO_STREAM_KERN+1] = cnt;

    stream->next[XKBLAS_IO_STREAM_D2H]  = 0;
    stream->next[XKBLAS_IO_STREAM_H2D]  = 0;
    stream->next[XKBLAS_IO_STREAM_D2D]  = 0;
    stream->next[XKBLAS_IO_STREAM_KERN] = 0;

    xkblas_io_stream_t** ios;
    stream->ios = ios = (xkblas_io_stream_t **) malloc(sizeof(xkblas_io_stream_t*) * cnt );
    assert( stream->ios[0]!= 0 );
    stream->ios[XKBLAS_IO_STREAM_H2D]  = stream->ios[0]+prefix[XKBLAS_IO_STREAM_H2D];
    stream->ios[XKBLAS_IO_STREAM_D2H]  = stream->ios[0]+prefix[XKBLAS_IO_STREAM_D2H];
    stream->ios[XKBLAS_IO_STREAM_D2D]  = stream->ios[0]+prefix[XKBLAS_IO_STREAM_D2D];
    stream->ios[XKBLAS_IO_STREAM_KERN] = stream->ios[0]+prefix[XKBLAS_IO_STREAM_KERN];

    for (unsigned int i = 0; i < cnt; ++i)
    {
        xkblas_io_stream_type_t type =
            i < prefix[XKBLAS_IO_STREAM_D2H] ? XKBLAS_IO_STREAM_H2D :
            i < prefix[XKBLAS_IO_STREAM_D2D] ? XKBLAS_IO_STREAM_D2H :
            i < prefix[XKBLAS_IO_STREAM_KERN] ? XKBLAS_IO_STREAM_D2D : XKBLAS_IO_STREAM_KERN
            ;
        ios[i]  = stream->f_stream_alloc( device, type, capacity );
        ios[i]->sid = i;
        assert( ios[i] != 0 );
        ios[i]->stream = s;
        //printf("%i:: init stream %i type: %s\n", device->ld->ldid, i, 
        //    type == XKBLAS_IO_STREAM_H2D ? "H2D" : type == XKBLAS_IO_STREAM_KERN ? "kern": type == XKBLAS_IO_STREAM_D2H ? "D2H" : type == XKBLAS_IO_STREAM_D2D ? "D2D" : "<NOTYPE>" );
        assert( 0 == _xkblas_offload_iostream_init( stream->ios[i], type, capacity )); 
    }
    # endif
}

static void
xkblas_device_commit(xkblas_driver_t * driver, xkblas_device_t * device, int driver_device_id)
{
    assert(driver->f_device_commit);
    int err = driver->f_device_commit(driver_device_id);
    if (err)
        XKBLAS_FATAL("Commit fail device %d of driver %s", driver_device_id, driver->f_get_name());

    # if 0
    assert(0 == pthread_mutex_lock(&device->lock));
    assert(0 == pthread_cond_signal(&device->cond_sleep));
    assert(0 == pthread_mutex_unlock(&device->lock));
    # endif

    assert(device->state == XKBLAS_DEVICE_STATE_INIT);
    device->state = XKBLAS_DEVICE_STATE_COMMIT;
}

static void
xkblas_device_init(xkblas_driver_t * driver, xkblas_device_t * device, int driver_device_id)
{
    device->driver_device_id = driver_device_id;

    # if 0
    device->tid = 0;
    device->spawn_count = 0;
    device->exec_count = 0;
    device->finalize = false;
    assert(0 == pthread_mutex_init(&device->lock, 0));
    assert(0 == pthread_cond_init(&device->cond, 0));
    assert(0 == pthread_cond_init(&device->cond_sleep, 0));
    device->issleeping = 0;
    device->request.op = XKBLAS_DEVICEOP_NOP;
    device->request.arg = 0;
    device->request.counter = 0;

    device->cnt_push = 0;
    # endif

    driver->f_device_init(driver_device_id);

    # if 0
    assert(0== pthread_mutex_init(&device->pipe_lock, 0));

    device->p_write   = 0; /* next position where to write new task */
    device->p_ready   = 0; /* position of the next task to insert into the kernel submission stream */
    device->p_finish  = 0; /* position of the next task to erase from the pipeline */
    device->pipe_size = XKBLAS_CONF.cuda_conc_kernel;
    device->pipeline  = (Task**)malloc(sizeof(Task*)*device->pipe_size);
    for (int i=0; i<device->pipe_size; ++i)
        device->pipeline[i] = 0;

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

    xkblas_device_stream_init(device, &device->stream, XKBLAS_STREAM_CAPACITY);

    # if 0
    assert(0 == pthread_mutex_lock(&device->lock));
    assert(0 == pthread_cond_signal(&device->cond_sleep));
    assert(0 == pthread_mutex_unlock(&device->lock));
    # endif

    if (driver->f_device_attach(driver_device_id))
        XKBLAS_FATAL("Could not attach to device %d of driver %s", driver_device_id, driver->f_get_name());

    assert(device->state == XKBLAS_DEVICE_STATE_CREATE);
    device->state = XKBLAS_DEVICE_STATE_INIT;
}

static xkblas_device_t *
xkblas_device_create(
    xkblas_drivers_t * drivers,
    uint8_t driver_id,
    int driver_device_id
) {
    if (drivers->devices.n == XKBLAS_DEVICES_MAX)
        XKBLAS_FATAL("Too many devices. Increase 'XKBLAS_DEVICES_MAX' and recompile Xkblas");

    int global_device_id = drivers->devices.n++;
    xkblas_driver_t * driver = drivers->list + driver_id;
    xkblas_device_t * device = driver->f_device_create(driver, driver_device_id);
    assert(device);
    drivers->devices.list[global_device_id] = device;

    pthread_mutex_init(&device->sleep.lock, 0);
    pthread_cond_init (&device->sleep.cond, 0);

    device->request.op      = XKBLAS_DEVICEOP_NOP;
    device->request.arg     = 0;
    device->request.counter = NULL;
    device->request.err     = 0;

    device->state = XKBLAS_DEVICE_STATE_CREATE;

    // register worker thread, using a nasty global variable here :-(
    ThreadWorker::init();
    ThreadWorker * thread = ThreadWorker::get();
    assert(thread);

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    xkblas_scheduler_register(&context->scheduler, thread, global_device_id);

    return device;
}

static inline void
__device_sleep(xkblas_device_t * device)
{
    XKBLAS_DEBUG("Sleeping device %p", device);
    pthread_mutex_lock(&device->sleep.lock);
    {
        assert(device->state == XKBLAS_DEVICE_STATE_RUNNING);
        device->state = XKBLAS_DEVICE_STATE_SLEEPING;
        while (device->state == XKBLAS_DEVICE_STATE_SLEEPING)
            pthread_cond_wait(&device->sleep.cond, &device->sleep.lock);
    }
    pthread_mutex_unlock(&device->sleep.lock);
    XKBLAS_DEBUG("Slept device %p", device);
}

static inline void
__device_wakeup(xkblas_device_t * device)
{
    XKBLAS_DEBUG("Waking up device %p", device);
    pthread_mutex_lock(&device->sleep.lock);
    if (device->state == XKBLAS_DEVICE_STATE_SLEEPING)
    {
        device->state = XKBLAS_DEVICE_STATE_RUNNING;
        pthread_cond_signal(&device->sleep.cond);
    }
    pthread_mutex_unlock(&device->sleep.lock);
    XKBLAS_DEBUG("Woke up device %p", device);
}

static inline void
xkblas_device_progress(
    xkblas_driver_t * driver,
    xkblas_device_t * device
) {
    switch (device->request.op)
    {
        case (XKBLAS_DEVICEOP_NOP):
        {
            // TODO
            break ;
        }

        case (XKBLAS_DEVICEOP_REPLY):
        {
            // TODO
            break ;
        }

        case (XKBLAS_DEVICEOP_WRITEBACK):
        {
            // TODO
            break ;
        }

        case (XKBLAS_DEVICEOP_WRITEBACK_WAIT):
        {
            // TODO
            break ;
        }

        case (XKBLAS_DEVICEOP_MEMSYNC):
        {
            // TODO
            break ;
        }

        case (XKBLAS_DEVICEOP_INVALIDATE_CACHES):
        {
            // TODO
            break ;
        }

        default:
        {
            XKBLAS_FATAL("Invalid request op code");
            break ;
        }
    }
}

static inline void
xkblas_device_prepare_task(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    Task * task
) {
    // TODO
}


/* main loop for the thread responsible the passed device */
static inline int
xkblas_device_thread_main_loop(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    int driver_device_id
) {
    // thread ready for execution
    assert(device->state == XKBLAS_DEVICE_STATE_COMMIT);
    device->state = XKBLAS_DEVICE_STATE_RUNNING;

    # pragma message(TODO "do we really need this mem_barrier here?")
    mem_barrier();

    ThreadWorker * thread = ThreadWorker::get();

    while (device->state == XKBLAS_DEVICE_STATE_RUNNING)
    {
        // If there is no tasks and streams are empty, sleep the thread
        Task * task;
        while ((task = thread->pop()) == NULL &&
                device->stream.is_empty(XKBLAS_IO_STREAM_ALL) &&
                device->request.op == XKBLAS_DEVICEOP_NOP)
            __device_sleep(device);

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
    XKBLAS_INFO("Starting thread for %s device (driver=%d) on cpu %d of node %d",
            driver->f_get_name(), driver_device_id, cpu, node);

    xkblas_device_t * device = xkblas_device_create(drivers, driver_id, driver_device_id);
    xkblas_device_init(driver, device, driver_device_id);

    // wait for all devices of that driver to be in the 'init' state
    ++driver->ndevices_inited;
    while (driver->ndevices_inited < driver->ndevices_targeted)
        mem_pause();

    // can now commit my device
    xkblas_device_commit(driver, device, driver_device_id);
    ++driver->ndevices_commited;

    /* infinite loop with the device context */
    int err = xkblas_device_thread_main_loop(driver, device, driver_device_id);
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


