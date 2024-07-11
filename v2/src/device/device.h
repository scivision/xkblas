#ifndef __DEVICE_H__
# define __DEVICE_H__

# include "device/driver.h"

typedef struct  xkblas_driver_thread_arg_t
{
    xkblas_driver_t * driver;
    int driver_device_id;
}               xkblas_driver_thread_arg_t;

void * xkblas_device_thread_main(void * a);

#endif /* __DEVICE_H__ */
