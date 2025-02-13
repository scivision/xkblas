/* ************************************************************************** */
/*                                                                            */
/*   driver_ze.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:48:24 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_ZE_H__
# define __DRIVER_ZE_H__

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

#endif /* __DRIVER_ZE_H__ */
