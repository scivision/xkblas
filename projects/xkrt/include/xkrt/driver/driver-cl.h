/* ************************************************************************** */
/*                                                                            */
/*   driver-cl.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:48:24 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_CL_H__
# define __DRIVER_CL_H__

# include <xkrt/driver/stream.h>
# include <CL/cl.h>

typedef struct  xkrt_stream_cl_t
{
    xkrt_stream_t super;

    struct {
        cl_command_queue queue;
        cl_event * events;
        cl_context context;
    } cl;
}               xkrt_stream_cl_t;

#endif /* __DRIVER_CL_H__ */
