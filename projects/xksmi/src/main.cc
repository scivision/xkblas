# include <xkrt/xkrt.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;

int
main(void)
{
    // init runtime
    xkrt_init(&runtime);

    // for each driver
    for (int driver_type = 0 ; driver_type != XKRT_DRIVER_TYPE_MAX ; ++driver_type)
    {
        xkrt_driver_t * driver = runtime.driver_get((xkrt_driver_type_t)driver_type);
        if (driver == NULL)
            continue ;

        LOGGER_INFO("####################################");
        // print driver name
        if (driver->f_get_name)
            LOGGER_INFO("Driver `%s`", driver->f_get_name());
        else
            LOGGER_INFO("Driver `%d`", driver_type);

        // print all devices
        for (int i = 0 ; i < driver->ndevices_commited ; ++i)
        {
            xkrt_device_t * device = driver->devices[i];
            if (device == NULL)
                continue ;

            LOGGER_INFO("------------------------------------");
            LOGGER_INFO("Device %u of global id %u", device->driver_id, device->global_id);
            if (driver->f_memory_device_info)
            {
                xkrt_device_memory_info_t meminfo[XKRT_DEVICE_MEMORIES_MAX];
                int nmemories;
                driver->f_memory_device_info(device->driver_id, meminfo, &nmemories);

                for (int j = 0 ; j < nmemories ; ++j)
                {
                    xkrt_device_memory_info_t * mem = meminfo + j;

                    char memory_used_str[64];
                    xkrt_metric_byte(memory_used_str, sizeof(memory_used_str), mem->used);

                    char memory_total_str[64];
                    xkrt_metric_byte(memory_total_str, sizeof(memory_used_str), mem->capacity);

                    LOGGER_INFO(
                        "Memory %s %d - usage %s/%s",
                        mem->name, j,
                        memory_used_str, memory_total_str
                    );
                }
            }
        }
    }

    // deinit runtime
    xkrt_deinit(&runtime);
    return 0;
}
