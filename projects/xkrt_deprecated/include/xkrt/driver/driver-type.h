/* ************************************************************************** */
/*                                                                            */
/*   driver-type.h                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 19:31:10 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_TYPE_H__
# define __DRIVER_TYPE_H__

# include <stdint.h>

typedef enum    xkrt_driver_type_t : uint8_t
{
    XKRT_DRIVER_TYPE_HOST   = 0,  // cpu driver
    XKRT_DRIVER_TYPE_CUDA   = 1,  // cuda devices driver
    XKRT_DRIVER_TYPE_ZE     = 2,  // level zero devices driver
    XKRT_DRIVER_TYPE_CL     = 3,  // opencl driver
    XKRT_DRIVER_TYPE_HIP    = 4,  // hip driver
    XKRT_DRIVER_TYPE_SYCL   = 5,  // sycl driver
    XKRT_DRIVER_TYPE_MAX    = 6
}               xkrt_driver_type_t;

typedef uint8_t xkrt_driver_type_bitfield_t;
static_assert(XKRT_DRIVER_TYPE_MAX <= sizeof(xkrt_driver_type_bitfield_t)*8);

extern "C"
const char * xkrt_driver_name(xkrt_driver_type_t driver_type);

extern "C"
xkrt_driver_type_t xkrt_driver_type_from_name(const char * name);

extern "C"
int xkrt_support_driver(xkrt_driver_type_t driver_type);

#endif /* __DRIVER_TYPE_H__ */
