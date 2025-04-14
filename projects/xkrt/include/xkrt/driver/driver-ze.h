/* ************************************************************************** */
/*                                                                            */
/*   driver-ze.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/18 19:44:22 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_ZE_H__
# define __DRIVER_ZE_H__

# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>

# include <xkrt/xkrt-support.h>

# include <ze_api.h>
# if XKRT_SUPPORT_SYCL
#  include <sycl/sycl.hpp>
# endif /* XKRT_SUPPORT_SYCL */
# if XKRT_SUPPORT_ZES
#  include <zes_api.h>
# endif

typedef struct  xkrt_device_ze_t
{
    xkrt_device_t inherited;

    struct {

        // handles
        ze_driver_handle_t      driver;
        ze_context_handle_t     context;
        ze_device_handle_t      device;
        ze_device_properties_t  device_properties;

        // number of command queue group
        uint32_t ncommandqueuegroups;

        // per command queue group property
        ze_command_queue_group_properties_t * command_queue_group_properties;

        // per command queue number of queue used
        std::atomic<uint32_t> * command_queue_group_used;

        // memory properties
        struct {
            uint32_t pcount;
            ze_device_memory_properties_t properties[XKRT_DEVICE_MEMORIES_MAX];
        } memory;

    } ze;

    # if XKRT_SUPPORT_SYCL
    // sycl interop
    struct {
        sycl::device device;
        sycl::context context;
    } sycl;
    # endif /* XKRT_SUPPORT_SYCL */

    # if XKRT_SUPPORT_ZES
    struct {
        struct {
            zes_pwr_handle_t handle;
            zes_power_energy_counter_t e1, e2;
            uint64_t t1, t2;
        } pwr;
    } zes;
    # endif /* XKRT_SUPPORT_ZES */

}               xkrt_device_ze_t;


typedef struct  xkrt_stream_ze_t
{
    xkrt_stream_t super;

    struct {
        struct {
            ze_command_list_handle_t list;
        } command;
        struct {
            ze_event_pool_handle_t  pool;
            ze_event_handle_t     * list;
        } events;

        // bad design, but required to submit kernels with level zero
        xkrt_device_ze_t * device;

    } ze;

    # if XKRT_SUPPORT_SYCL
    struct {
        sycl::queue queue;
    } sycl;
    # endif /* XKRT_SUPPORT_SYCL */

}               xkrt_stream_ze_t;


typedef struct  xkrt_driver_ze_t
{
    xkrt_driver_t super;

}               xkrt_driver_ze_t;

#endif /* __DRIVER_ZE_H__ */
