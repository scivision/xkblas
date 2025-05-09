/* ************************************************************************** */
/*                                                                            */
/*   driver-sycl.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/09 04:25:13 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_SYCL_H__
# define __DRIVER_SYCL_H__

# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>

# include <xkrt/support.h>

# include <sycl/sycl.hpp>

typedef struct  xkrt_device_sycl_t
{
    xkrt_device_t inherited;

    struct {
        sycl::platform  platform;
        sycl::device    device;
        sycl::queue     alloc_queue;
    } sycl;

}               xkrt_device_sycl_t;


typedef struct  xkrt_stream_sycl_t
{
    xkrt_stream_t super;

    struct {
        sycl::queue queue;
        struct {
            sycl::event * buffer;
            size_t capacity;
        } events ;
    } sycl;
}               xkrt_stream_sycl_t;


typedef struct  xkrt_driver_sycl_t
{
    xkrt_driver_t super;

}               xkrt_driver_sycl_t;

#endif /* __DRIVER_SYCL_H__ */
