#ifndef __CONSTS_H__
# define __CONSTS_H__

/* maximum number of devices in total */
# define XKBLAS_DEVICES_MAX (16)

/* an ID representing a pure virtual host device */
# define HOST_DEVICE_GLOBAL_ID (XKBLAS_DEVICES_MAX)

/* an ID representing an unspecified device */
# define UNSPECIFIED_GLOBAL_DEVICE_ID (XKBLAS_DEVICES_MAX+1)

#endif /* __CONSTS_H__ */
