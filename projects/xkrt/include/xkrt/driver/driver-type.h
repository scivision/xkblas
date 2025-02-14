/* ************************************************************************** */
/*                                                                            */
/*   driver-type.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:48:24 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_TYPE_H__
# define __DRIVER_TYPE_H__

typedef enum    xkrt_driver_type_t : uint8_t
{
    XKRT_DRIVER_TYPE_HOST   = 0,  // cpu driver
    XKRT_DRIVER_TYPE_CUDA   = 1,  // cuda devices driver
    XKRT_DRIVER_TYPE_ZE     = 2,  // level zero devices driver
    XKRT_DRIVER_TYPE_CL     = 3,  // opencl driver
    XKRT_DRIVER_TYPE_MAX    = 4
}               xkrt_driver_type_t;

#endif /* __DRIVER_TYPE_H__ */
