# include "device/driver/driver.h"

void
xkblas_devices_init(void)
{
    /* global vars. init */
//    memset(kaapi_drivers_bytype, 0, sizeof(kaapi_drivers_bytype));

    /* Load device plugins and functions.
     * The driver starts a thread to initialize/commit the device. */
//    kaapi_offload_find_plugins();
    xkblas_drivers_init();
}
