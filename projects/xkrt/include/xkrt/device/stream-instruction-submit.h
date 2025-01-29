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

# include <xkrt/callback.h>
# include <xkrt/device/device.h>
# include <xkrt/device/driver.h>
# include <xkrt/device/memory-view.hpp>
# include <xkrt/device/stream-instruction.h>
# include <xkrt/device/task.hpp>

/* submit a kernel execution instruction on that device */
void xkrt_stream_instruction_submit_kernel(
    xkrt_device_t * device,
    Task * task,
    const xkrt_callback_t & callback
);

/* submit a memory copy */
void xkrt_stream_instruction_submit_copy(
    xkrt_device_t                * device,
    const memory_view_t            & host_view,
    const uint8_t                   dst_device_global_id,
    const memory_replicate_view_t  & dst_device_view,
    const uint8_t                   src_device_global_id,
    const memory_replicate_view_t  & src_device_view,
    const xkrt_callback_t & callback
);

#endif /* __STREAM_INSTRUCTION_SUBMIT_H__ */
