/* ************************************************************************** */
/*                                                                            */
/*   consts.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CONSTS_H__
# define __CONSTS_H__

/* maximum number of devices in total */
# define XKRT_DEVICES_MAX (8)

typedef uint8_t xkrt_device_global_id_t;
static_assert(XKRT_DEVICES_MAX <= (1UL << (sizeof(xkrt_device_global_id_t)*8)));

typedef uint32_t xkrt_device_global_id_bitfield_t;
static_assert(XKRT_DEVICES_MAX <= sizeof(xkrt_device_global_id_bitfield_t)*8);

/* an ID representing a pure virtual host device */
# define HOST_DEVICE_GLOBAL_ID (XKRT_DEVICES_MAX)

/* an ID representing an unspecified device */
# define UNSPECIFIED_DEVICE_GLOBAL_ID (XKRT_DEVICES_MAX+1)

#endif /* __CONSTS_H__ */
