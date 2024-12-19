/* ************************************************************************** */
/*                                                                            */
/*   stream-instruction-submit.h                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:54:32 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STREAM_INSTRUCTION_SUBMIT_H__
# define __STREAM_INSTRUCTION_SUBMIT_H__

# include <kaapi/callback.h>
# include <kaapi/device/device.h>
# include <kaapi/device/driver.h>
# include <kaapi/device/memory-view.hpp>
# include <kaapi/device/stream-instruction.h>
# include <kaapi/device/task.hpp>

/* submit a kernel execution instruction on that device */
void kaapi_stream_instruction_submit_kernel(
    kaapi_device_t * device,
    Task * task,
    const kaapi_callback_t & callback
);

/* submit a memory copy */
void kaapi_stream_instruction_submit_copy(
    kaapi_device_t                * device,
    const memory_view_t            & host_view,
    const uint8_t                   dst_device_global_id,
    const memory_replicate_view_t  & dst_device_view,
    const uint8_t                   src_device_global_id,
    const memory_replicate_view_t  & src_device_view,
    const kaapi_callback_t & callback
);

#endif /* __STREAM_INSTRUCTION_SUBMIT_H__ */
