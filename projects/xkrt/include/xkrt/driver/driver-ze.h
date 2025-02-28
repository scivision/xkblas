/* ************************************************************************** */
/*                                                                            */
/*   driver-ze.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/26 04:52:04 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_ZE_H__
# define __DRIVER_ZE_H__

# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/driver/stream.h>
# include <ze_api.h>

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
    } ze;

}               xkrt_stream_ze_t;

typedef struct  xkrt_device_ze_t
{
    xkrt_device_t inherited;

    ze_driver_handle_t      ze_driver;
    ze_context_handle_t     ze_context;
    ze_device_handle_t      ze_device;
    ze_device_properties_t  ze_device_properties;

    // memory properties
    struct {
        uint32_t pcount;
        ze_device_memory_properties_t ze_properties[XKRT_DEVICE_MEMORIES_MAX];
    } memory;

    // number of command queue group
    uint32_t ncommandqueuegroups;

    // per command queue group property
    ze_command_queue_group_properties_t * ze_command_queue_group_properties;

    // per command queue number of queue used
    std::atomic<uint32_t> * ze_command_queue_group_used;

}               xkrt_device_ze_t;

typedef struct  xkrt_driver_ze_t
{
    xkrt_driver_t super;

}               xkrt_driver_ze_t;

#endif /* __DRIVER_ZE_H__ */
