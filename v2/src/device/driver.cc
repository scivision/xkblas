# include "min-max.h"
# include "conf/conf.h"
# include "device/driver/driver.h"
# include "logger/logger.h"

# include <cassert>
# include <cstring>
# include <cerrno>

# pragma message(TODO "Implement host driver ?")

// Drivers
# define XKBLAS_DRIVER_CUDA     0
# define XKBLAS_DRIVER_MAX      1
# define XKBLAS_DRIVER_GPU      XKBLAS_DRIVER_CUDA
# define XKBLAS_DRIVER_DEFAULT  XKBLAS_DRIVER_GPU
static xkblas_driver_t DRIVERS[XKBLAS_DRIVER_MAX];

// Devices
static xkblas_device_t DEVICES[XKBLAS_DEVICES_MAX];
static uint8_t DEVICES_USED = 0;

/* Main entry thread created per device */
static void *
xkblas_device_thread_main(void * a)
{
    # pragma message(TODO "Implement device thread")

    xkblas_driver_thread_arg_t * arg = (xkblas_driver_thread_arg_t *) a;
    xkblas_driver_t * driver = arg->driver;
    xkblas_device_t * device = DEVICES + arg->device_id;

    unsigned int cpu, node;
    getcpu(&cpu, &node);
    XKBLAS_INFO("Starting thread for %s device %d on cpu %d of node %d", driver->f_get_name(), arg->device_id, cpu, node);

    driver->f_device_create(driver, device, arg->device_id);


    # if 0

    _xkblas_offload_config_data_field_device(driver, device);

    /* register the new device in global table */
    xkblas_offload_devices[arg->global_device_id] = device;

    /* recopy thread id */
    device->tid = arg->tid;

    /* basic initialisation */
    xkblas_offload_device_init(device, arg->ld);

    XKBLAS_INFO("device_id:%i, thread:%p, initialized @:%X\n",device->device_id, device->tid, device);

    /* release the thread argument */
    free(a);

    device->state = XKBLAS_DEVICE_STATE_INIT;
    xkblas_offload_device_push( device );

    xkblas_thread_t* thread = xkblas_thread_bind(device->driver->f_get_type(),0);
    assert( thread !=0);
    xkblas_context_t* ctxt = xkblas_thread2context(thread);
    device->ctxt = ctxt;
    ctxt->device = device;
    ctxt->ld = device->ld;
    _xkblas_self_context = ctxt;

    XKBLAS_ATOMIC_INCR(&driver->ndevices);
    xkblas_mem_barrier();

    /* we need to wait all threads of the driver before doing commit */
    int ndevices = driver->f_get_number();

    while (XKBLAS_ATOMIC_READ(&driver->ndevices) < ndevices)
        xkblas_slowdown_cpu();

    xkblas_offload_device_commit(device);
    assert( device->state == XKBLAS_DEVICE_STATE_COMMIT);

    /* thread ready for execution */
    device->state = XKBLAS_DEVICE_STATE_START;
    XKBLAS_ATOMIC_INCR(&driver->ndevices_commit);

    xkblas_mem_barrier();
    XKBLAS_INFO("device_id:%i, thread:%p, commited @:%X\n",device->device_id, device->tid, device);

    /* */
#if XKBLAS_SLEEP_DEVICETHREAD
    xkblas_fifo_register_waiter( device->ld->queue, (void (*)(void *))xkblas_offload_device_wakeup, device );
#else
    xkblas_fifo_register_waiter( device->ld->queue, 0, 0);
#endif

    /* infinite loop with the device context */
    int err = xkblas_sched_idle_offload(thread, _xkblas_device_finalize, device);
    assert((err==0)||(err==EINTR));

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

static void
xkblas_driver_init(xkblas_driver_t * driver)
{
    XKBLAS_INFO("Loading driver '%s'", driver->f_get_name());
    assert(driver->f_init);

    if (driver->f_init())
        return ;

    # pragma message(TODO "Currently, the only devices supported are GPUs")
    assert(driver->f_get_ndevices_max);
    int n_devices_max = driver->f_get_ndevices_max();
    int n_devices = MIN(XKBLAS_CONF.ngpus, n_devices_max);
    XKBLAS_INFO("using %d devices out of %d available", n_devices, n_devices_max);
    if (n_devices < 1)
        return ;
    memset(DEVICES + DEVICES_USED, 0, n_devices * sizeof(xkblas_device_t));

    # pragma message(TODO "Move that to the 'Thread' interfaces")
    cpu_set_t save_schedset;
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);

    for (int i = 0; i < n_devices; ++i)
    {
        cpu_set_t schedset;
        assert(driver->f_get_ndevices_max);
        int err = driver->f_device_set_cpuset(&schedset, i);
        if (err)
        {
            XKBLAS_ERROR("cannot use device %d", i);
            continue ;
        }

        // move the current thread to the device cpu set
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        err = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &schedset);
        if (err)
        {
            XKBLAS_ERROR("invalid cpu_set returned by the driver for device %d", i);
            continue ;
        }
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &schedset);
        for (int i=0; i<10; ++i) sched_yield();

        xkblas_driver_thread_arg_t * arg = (xkblas_driver_thread_arg_t *) malloc(sizeof(xkblas_driver_thread_arg_t));
        arg->driver = driver;
        arg->device_id = i;
        arg->global_device_id = DEVICES_USED;
        arg->tid = 0;
        arg->ld = 0;

        # pragma message(TODO "Implement locality domain")
        # if 0
        int type = driver->type;
        switch (type) {
            case XKBLAS_DRIVER_HOST:
                break;
            case XKBLAS_DRIVER_CUDA:
                arg->ld = (xkblas_localitydomain_t *) malloc(sizeof(xkblas_localitydomain_t));
                xkblas_localitydomain_init(arg->ld, 0);
                xkblas_localitydomain_attach(XKBLAS_LD_GPU, 0, arg->ld);
                break;
            default:
                abort();
        };
        # endif

        err = pthread_create(&arg->tid, &attr, xkblas_device_thread_main, arg);
        assert(err ==0);
        ++DEVICES_USED;
    }

    // move back the current thread to its initial cpu set
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &save_schedset);
}

void
xkblas_drivers_init(void)
{
    XKBLAS_NOT_IMPLEMENTED_WARN("Dynamic driver loading not implemented. Only supporting built-in CUDA driver");

    extern void XKBLAS_DRIVER_ENTRYPOINT(get_cuda_driver)(xkblas_driver_t *);

    void (*loaders[XKBLAS_DRIVER_MAX])(xkblas_driver_t *);
    loaders[XKBLAS_DRIVER_CUDA] = XKBLAS_DRIVER_ENTRYPOINT(get_cuda_driver);

    memset(DRIVERS, 0, sizeof(DRIVERS));
    for (int i = 0 ; i < XKBLAS_DRIVER_MAX ; ++i)
    {
        xkblas_driver_t * driver = DRIVERS + i;
        loaders[i](driver);
        xkblas_driver_init(driver);
    }

    if (XKBLAS_CONF.ngpus > DEVICES_USED)
        XKBLAS_WARN("Requested %d GPUs but only found %d", XKBLAS_CONF.ngpus, DEVICES_USED);
}

void
xkblas_drivers_deinit(void)
{
    # pragma message(TODO "Implement driver_deinit - synchronize all devices threads")
    sleep(2);
}
