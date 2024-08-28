# define XKBLAS_DRIVER_ENTRYPOINT(N) XKBLAS_DRIVER_HOST_ ## N

# include "xkblas-context.h"
# include "conf/conf.h"
# include "device/device.h"
# include "device/driver.h"
# include "logger/logger.h"
# include "sync/mutex.h"

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>

typedef struct  xkblas_device_host_t
{
    xkblas_device_t inherited;

}               xkblas_device_host_t;

/* the host devices (should be '1' - using >1 for debug purposes when no cuda/gpu available :-) */
# define N_HOST_DEVICE 1
static xkblas_device_host_t DEVICES[N_HOST_DEVICE];

/* initialization synchronization */
static bool INITIALIZED = false;
static xkblas_mutex_t DRIVER_MUTEX = XKBLAS_MUTEX_INITIALIZER;

static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return N_HOST_DEVICE;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(init)(void)
{
    if (INITIALIZED)
        return 0;

    XKBLAS_MUTEX_LOCK(DRIVER_MUTEX);
    {
        if (INITIALIZED)
        {
            XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);
            return 0;
        }

        // TODO : initialize here if needed

        INITIALIZED = true;
    }
    XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);

    return 0;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(finalize)(void)
{
    if (!INITIALIZED)
    {
        XKBLAS_MUTEX_LOCK(DRIVER_MUTEX);
        {
            if (!INITIALIZED)
                XKBLAS_FATAL("Finalize CUDA driver before initializing...");
        }
        XKBLAS_MUTEX_UNLOCK(DRIVER_MUTEX);
    }

    assert(INITIALIZED);
    INITIALIZED = 0;
}

static const char *
XKBLAS_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "HOST";
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_set_cpuset)(cpu_set_t * schedset, int device_id)
{
    return ENOTSUP;
}

static xkblas_device_t *
XKBLAS_DRIVER_ENTRYPOINT(device_create)(xkblas_driver_t * driver, int device_id)
{
    assert(INITIALIZED);
    return (xkblas_device_t *) DEVICES + device_id;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(device_init)(int device_id)
{
    assert(INITIALIZED);
    xkblas_device_host_t* device = (xkblas_device_host_t *) DEVICES + device_id;
    device->inherited.memdev.memory_allocated = 0; /* prevent usage of memdev on host device */
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_destroy)(xkblas_device_t * device)
{
    return 0;
}

static int
XKBLAS_DRIVER_ENTRYPOINT(device_attach)(int device_id)
{
    return 0;
}

/* Called on all devices of the driver after they have been initialized */
static int
XKBLAS_DRIVER_ENTRYPOINT(device_commit)(int device_id)
{
    return 0;
}

int
XKBLAS_DRIVER_ENTRYPOINT(stream_instruction_decode)(
    xkblas_stream_t * istream,
    xkblas_stream_instruction_t * instr
) {
    XKBLAS_FATAL("Tried to execute instructions on the host, not implemneted yet");
    return 0;
}

static xkblas_stream_t *
XKBLAS_DRIVER_ENTRYPOINT(stream_create)(
    xkblas_stream_type_t type,
    unsigned int capacity
) {
    xkblas_stream_t * istream = (xkblas_stream_t *) malloc(sizeof(xkblas_stream_t));
    assert(istream);

    xkblas_stream_init(istream, type, capacity, XKBLAS_DRIVER_ENTRYPOINT(stream_instruction_decode));
    return istream;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(stream_delete)(
    xkblas_stream_t * istream
) {
    free(istream);
}

void
XKBLAS_DRIVER_ENTRYPOINT(get_driver)(xkblas_driver_t * driver)
{
    # define EP(func) driver->f_##func = XKBLAS_DRIVER_ENTRYPOINT(func)

    EP(init);
    EP(finalize);
    EP(get_name);
    EP(get_ndevices_max);
    EP(device_set_cpuset);
    EP(device_create);
    EP(device_destroy);
    EP(device_init);
    EP(device_attach);
    EP(device_commit);

    EP(stream_create);
    EP(stream_delete);

    #if 0

    EP(get_flags);
    EP(get_type);
    EP(host_register);
    EP(host_register_testwait);
    EP(host_unregister);

    EP(device_info);
    EP(device_finalize);
    EP(device_detach);
    EP(get_gpublas_handle);

    #endif

    # undef EP
}
