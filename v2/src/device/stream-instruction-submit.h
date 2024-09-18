#ifndef __STREAM_INSTRUCTION_SUBMIT_H__
# define __STREAM_INSTRUCTION_SUBMIT_H__

# include "device/device.h"
# include "device/driver.h"
# include "device/task.hpp"
# include "device/memory-view.hpp"
# include "xkblas-callback.h"
# include "device/stream-instruction.h"

/* submit a kernel execution instruction on that device */
void xkblas_stream_instruction_submit_kernel(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    Task * task,
    const xkblas_callback_t & callback
);

/* submit a memory copy */
void xkblas_stream_instruction_submit_copy(
    const xkblas_driver_t          * driver,
    xkblas_device_t                * device,
    const memory_view_t            & host_view,
    const uint8_t                   dst_device_global_id,
    const memory_replicate_view_t  & dst_device_view,
    const uint8_t                   src_device_global_id,
    const memory_replicate_view_t  & src_device_view,
    const xkblas_callback_t & callback
);

#endif /* __STREAM_INSTRUCTION_SUBMIT_H__ */
