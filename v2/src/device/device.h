#ifndef __DEVICE_H__
# define __DEVICE_H__

# include "device/driver.h"

void xkblas_device_init(xkblas_driver_t * driver, xkblas_device_t * device, int driver_device_id);

#endif /* __DEVICE_H__ */
