/* ************************************************************************** */
/*                                                                            */
/*   driver-sycl.h                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/17 14:41:47 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:00:10 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
