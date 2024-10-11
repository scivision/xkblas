#ifndef __CONSTS_H__
# define __CONSTS_H__

/* maximum number of devices in total */
# define XKBLAS_DEVICES_MAX (8)

typedef uint8_t xkblas_device_global_id_t;
static_assert(XKBLAS_DEVICES_MAX <= (1UL << (sizeof(xkblas_device_global_id_t)*8)));

typedef uint32_t xkblas_device_global_id_bitfield_t;
static_assert(XKBLAS_DEVICES_MAX <= sizeof(xkblas_device_global_id_t)*8);

/* an ID representing a pure virtual host device */
# define HOST_DEVICE_GLOBAL_ID (XKBLAS_DEVICES_MAX)

/* an ID representing an unspecified device */
# define UNSPECIFIED_DEVICE_GLOBAL_ID (XKBLAS_DEVICES_MAX+1)

#endif /* __CONSTS_H__ */
