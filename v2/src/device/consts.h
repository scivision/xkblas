#ifndef __CONSTS_H__
# define __CONSTS_H__

/* maximum number of devices in total */
# define XKBLAS_DEVICES_MAX (4)

typedef uint8_t xkblas_device_global_id_t;
static_assert(XKBLAS_DEVICES_MAX <= 8*sizeof(xkblas_device_global_id_t));

/* an ID representing a pure virtual host device */
# define HOST_DEVICE_GLOBAL_ID (XKBLAS_DEVICES_MAX)

/* an ID representing an unspecified device */
# define UNSPECIFIED_DEVICE_GLOBAL_ID (XKBLAS_DEVICES_MAX+1)

#endif /* __CONSTS_H__ */
