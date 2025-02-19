/* ************************************************************************** */
/*                                                                            */
/*   consts.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/19 18:50:23 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CONSTS_H__
# define __CONSTS_H__

#  include <stdint.h>

/* maximum number of devices in total */
# define XKRT_DEVICES_MAX (16)

/* maximum number of performance ranks between devices. */
# define XKRT_DEVICES_PERF_RANK_MAX (4)

typedef uint8_t xkrt_device_global_id_t;
static_assert(XKRT_DEVICES_MAX <= (1UL << (sizeof(xkrt_device_global_id_t)*8)));

typedef uint32_t xkrt_device_global_id_bitfield_t;
static_assert(XKRT_DEVICES_MAX <= sizeof(xkrt_device_global_id_bitfield_t)*8);

/* an ID representing a pure virtual host device */
# define HOST_DEVICE_GLOBAL_ID (XKRT_DEVICES_MAX)

/* an ID representing an unspecified device */
# define UNSPECIFIED_DEVICE_GLOBAL_ID (XKRT_DEVICES_MAX+1)

/* a bitmask that represents all devices */
# define XKRT_DEVICES_MASK_ALL (~((xkrt_device_global_id_bitfield_t)0))

#endif /* __CONSTS_H__ */
