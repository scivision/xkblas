/* ************************************************************************** */
/*                                                                            */
/*   driver_cpu.cc                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:57:14 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# define KAAPI_DRIVER_ENTRYPOINT(N) KAAPI_DRIVER_CPU_ ## N

# include <kaapi/conf/conf.h>
# include <kaapi/device/device.h>
# include <kaapi/device/driver.h>
# include <kaapi/logger/logger.h>
# include <kaapi/runtime.h>
# include <kaapi/sync/mutex.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>

typedef struct  kaapi_device_host_t
{
    kaapi_device_t inherited;

}               kaapi_device_host_t;

/* the host devices (should be '1' - using >1 for debug purposes when no cuda/gpu available :-) */
# define N_CPU_DEVICES 1
static kaapi_device_host_t DEVICES[N_CPU_DEVICES];

/* initialization synchronization */
static bool INITIALIZED = false;
static kaapi_mutex_t DRIVER_MUTEX = KAAPI_MUTEX_INITIALIZER;

static unsigned int
KAAPI_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return N_CPU_DEVICES;
}

static int
KAAPI_DRIVER_ENTRYPOINT(init)(void)
{
    if (INITIALIZED)
        return 0;

    KAAPI_MUTEX_LOCK(DRIVER_MUTEX);
    {
        if (INITIALIZED)
        {
            KAAPI_MUTEX_UNLOCK(DRIVER_MUTEX);
            return 0;
        }

        // TODO : initialize here if needed

        INITIALIZED = true;
    }
    KAAPI_MUTEX_UNLOCK(DRIVER_MUTEX);

    return 0;
}

static void
KAAPI_DRIVER_ENTRYPOINT(finalize)(void)
{
    if (!INITIALIZED)
    {
        KAAPI_MUTEX_LOCK(DRIVER_MUTEX);
        {
            if (!INITIALIZED)
                LOGGER_FATAL("Finalize CUDA driver before initializing...");
        }
        KAAPI_MUTEX_UNLOCK(DRIVER_MUTEX);
    }

    assert(INITIALIZED);
    INITIALIZED = 0;
}

static const char *
KAAPI_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "HOST";
}

static int
KAAPI_DRIVER_ENTRYPOINT(device_set_cpuset)(cpu_set_t * schedset, int device_id)
{
    return ENOTSUP;
}

static kaapi_device_t *
KAAPI_DRIVER_ENTRYPOINT(device_create)(kaapi_driver_t * driver, int device_id)
{
    assert(INITIALIZED);
    return (kaapi_device_t *) DEVICES + device_id;
}

static void
KAAPI_DRIVER_ENTRYPOINT(device_init)(int device_id)
{
    assert(INITIALIZED);
    kaapi_device_host_t* device = (kaapi_device_host_t *) DEVICES + device_id;
    device->inherited.memdev.memory_allocated = 0; /* prevent usage of memdev on host device */
}

static int
KAAPI_DRIVER_ENTRYPOINT(device_destroy)(kaapi_device_t * device)
{
    return 0;
}

static int
KAAPI_DRIVER_ENTRYPOINT(device_attach)(int device_id)
{
    return 0;
}

/* Called on all devices of the driver after they have been initialized */
static int
KAAPI_DRIVER_ENTRYPOINT(device_commit)(int device_id)
{
    return 0;
}

int
KAAPI_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    kaapi_stream_t * istream,
    kaapi_stream_instruction_t * instr
) {
    LOGGER_FATAL("Tried to launch instructions on the host, not implemneted yet");
    return 0;
}

int
KAAPI_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    kaapi_stream_t * istream,
    int blocking
) {
    LOGGER_FATAL("Tried to progress instructions on the host, not implemneted yet");
    return 0;
}

static kaapi_stream_t *
KAAPI_DRIVER_ENTRYPOINT(stream_create)(
    kaapi_stream_type_t type,
    unsigned int capacity
) {
    kaapi_stream_t * istream = (kaapi_stream_t *) malloc(sizeof(kaapi_stream_t));
    assert(istream);

    kaapi_stream_init(
        istream,
        type,
        capacity,
        KAAPI_DRIVER_ENTRYPOINT(stream_instruction_launch),
        KAAPI_DRIVER_ENTRYPOINT(stream_instructions_progress)
    );
    return istream;
}

static void
KAAPI_DRIVER_ENTRYPOINT(stream_delete)(
    kaapi_stream_t * istream
) {
    free(istream);
}

void
KAAPI_DRIVER_ENTRYPOINT(get_driver)(kaapi_driver_t * driver)
{
    # define EP(func) driver->f_##func = KAAPI_DRIVER_ENTRYPOINT(func)

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
