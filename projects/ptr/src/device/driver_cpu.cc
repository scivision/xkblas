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

# define PTR_DRIVER_ENTRYPOINT(N) PTR_DRIVER_CPU_ ## N

# include <ptr/conf/conf.h>
# include <ptr/device/device.h>
# include <ptr/device/driver.h>
# include <ptr/logger/logger.h>
# include <ptr/runtime.h>
# include <ptr/sync/mutex.h>

# include <cassert>
# include <cstdio>
# include <cstdint>
# include <cerrno>

typedef struct  ptr_device_host_t
{
    ptr_device_t inherited;

}               ptr_device_host_t;

/* the host devices (should be '1' - using >1 for debug purposes when no cuda/gpu available :-) */
# define N_CPU_DEVICES 1
static ptr_device_host_t DEVICES[N_CPU_DEVICES];

/* initialization synchronization */
static bool INITIALIZED = false;
static ptr_mutex_t DRIVER_MUTEX = PTR_MUTEX_INITIALIZER;

static unsigned int
PTR_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return N_CPU_DEVICES;
}

static int
PTR_DRIVER_ENTRYPOINT(init)(void)
{
    if (INITIALIZED)
        return 0;

    PTR_MUTEX_LOCK(DRIVER_MUTEX);
    {
        if (INITIALIZED)
        {
            PTR_MUTEX_UNLOCK(DRIVER_MUTEX);
            return 0;
        }

        // TODO : initialize here if needed

        INITIALIZED = true;
    }
    PTR_MUTEX_UNLOCK(DRIVER_MUTEX);

    return 0;
}

static void
PTR_DRIVER_ENTRYPOINT(finalize)(void)
{
    if (!INITIALIZED)
    {
        PTR_MUTEX_LOCK(DRIVER_MUTEX);
        {
            if (!INITIALIZED)
                LOGGER_FATAL("Finalize CUDA driver before initializing...");
        }
        PTR_MUTEX_UNLOCK(DRIVER_MUTEX);
    }

    assert(INITIALIZED);
    INITIALIZED = 0;
}

static const char *
PTR_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "HOST";
}

static int
PTR_DRIVER_ENTRYPOINT(device_set_cpuset)(cpu_set_t * schedset, int device_id)
{
    return ENOTSUP;
}

static ptr_device_t *
PTR_DRIVER_ENTRYPOINT(device_create)(ptr_driver_t * driver, int device_id)
{
    assert(INITIALIZED);
    return (ptr_device_t *) DEVICES + device_id;
}

static void
PTR_DRIVER_ENTRYPOINT(device_init)(int device_id)
{
    assert(INITIALIZED);
    ptr_device_host_t* device = (ptr_device_host_t *) DEVICES + device_id;
    device->inherited.memdev.memory_allocated = 0; /* prevent usage of memdev on host device */
}

static int
PTR_DRIVER_ENTRYPOINT(device_destroy)(ptr_device_t * device)
{
    return 0;
}

static int
PTR_DRIVER_ENTRYPOINT(device_attach)(int device_id)
{
    return 0;
}

/* Called on all devices of the driver after they have been initialized */
static int
PTR_DRIVER_ENTRYPOINT(device_commit)(int device_id)
{
    return 0;
}

int
PTR_DRIVER_ENTRYPOINT(stream_instruction_launch)(
    ptr_stream_t * istream,
    ptr_stream_instruction_t * instr
) {
    LOGGER_FATAL("Tried to launch instructions on the host, not implemneted yet");
    return 0;
}

int
PTR_DRIVER_ENTRYPOINT(stream_instructions_progress)(
    ptr_stream_t * istream,
    int blocking
) {
    LOGGER_FATAL("Tried to progress instructions on the host, not implemneted yet");
    return 0;
}

static ptr_stream_t *
PTR_DRIVER_ENTRYPOINT(stream_create)(
    ptr_stream_type_t type,
    unsigned int capacity
) {
    ptr_stream_t * istream = (ptr_stream_t *) malloc(sizeof(ptr_stream_t));
    assert(istream);

    ptr_stream_init(
        istream,
        type,
        capacity,
        PTR_DRIVER_ENTRYPOINT(stream_instruction_launch),
        PTR_DRIVER_ENTRYPOINT(stream_instructions_progress)
    );
    return istream;
}

static void
PTR_DRIVER_ENTRYPOINT(stream_delete)(
    ptr_stream_t * istream
) {
    free(istream);
}

void
PTR_DRIVER_ENTRYPOINT(get_driver)(ptr_driver_t * driver)
{
    # define EP(func) driver->f_##func = PTR_DRIVER_ENTRYPOINT(func)

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
