/* ************************************************************************** */
/*                                                                            */
/*   consts.h                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/01 12:09:49 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 17:59:19 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CONSTS_H__
# define __CONSTS_H__

#  include <stdint.h>

/* maximum number of devices in total */
# define XKRT_DEVICES_MAX (16)

/* maximum number of memory per device */
# define XKRT_DEVICE_MEMORIES_MAX (1)

/* maximum number of performance ranks between devices. */
# define XKRT_DEVICES_PERF_RANK_MAX (4)

typedef uint8_t xkrt_device_global_id_t;
static_assert(XKRT_DEVICES_MAX <= (1UL << (sizeof(xkrt_device_global_id_t)*8)));

typedef uint16_t xkrt_device_global_id_bitfield_t;
static_assert(XKRT_DEVICES_MAX <= sizeof(xkrt_device_global_id_bitfield_t)*8);

/* an ID representing the host device */
# define HOST_DEVICE_GLOBAL_ID (0)

/* an ID representing an unspecified device */
# define UNSPECIFIED_DEVICE_GLOBAL_ID (XKRT_DEVICES_MAX)

/* a bitmask that represents all devices */
# define XKRT_DEVICES_MASK_ALL (~((xkrt_device_global_id_bitfield_t)0))

/* maximum number of threads per device */
# define XKRT_MAX_THREADS_PER_DEVICE (4)

/* maximum number of memory per thread */
# define THREAD_MAX_MEMORY ((size_t)2*1024*1024*1024)

#endif /* __CONSTS_H__ */
