# include "device/driver.h"
# include "logger/logger.h"

typedef struct  xkblas_device_host
{
    xkblas_device_t inherited;
}               xkblas_device_host_t;

static int
XKBLAS_DRIVER_ENTRYPOINT(init)(void)
{
    return 0;
}

static void
XKBLAS_DRIVER_ENTRYPOINT(finalize)(void)
{
}

static const char *
XKBLAS_DRIVER_ENTRYPOINT(get_name)(void)
{
    return "host";
}

static unsigned int
XKBLAS_DRIVER_ENTRYPOINT(get_ndevices_max)(void)
{
    return 1;
}

void
XKBLAS_DRIVER_ENTRYPOINT(get_host_driver)(xkblas_driver_t * driver)
{
    # define EP(func) driver->f_##func = XKBLAS_DRIVER_ENTRYPOINT(func)

    EP(init);
    EP(finalize);
    EP(get_name);
    EP(get_ndevices_max);

    #if 0

    EP(get_flags);
    EP(get_type);
    EP(get_number);
    EP(host_register);
    EP(host_register_testwait);
    EP(host_unregister);

    EP(device_set_cpuset);
    EP(device_create);
    EP(device_destroy);
    EP(device_info);
    EP(device_init);
    EP(device_commit);
    EP(device_finalize);
    EP(device_attach);
    EP(device_detach);
    EP(get_gpublas_handle);

    #endif

    # undef EP
}
