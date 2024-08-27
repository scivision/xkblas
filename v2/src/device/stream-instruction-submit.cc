# include "device/device.h"
# include "device/driver.h"
# include "device/stream-instruction.h"
# include "device/task.hpp"
# include "logger/logger.h"

# include <functional>

/* commit a stream instruction and wakeup thread */
static inline void
xkblas_device_submit(
    xkblas_device_t * device,
    xkblas_stream_t * stream,
    xkblas_stream_instruction_t * instr
) {
    /* commit instruction to the stream */
    stream->commit(instr);

    /* wakeup device worker thread */
    device->thread->wakeup();
}

/* submit a kernel execution instruction on that device */
void
xkblas_stream_instruction_submit_kernel(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    Task * task
) {
    XKBLAS_INFO("Task `%s` is ready for kernel execution", task->label);

    /* create a new instruction and retrieve its offload stream */
    xkblas_stream_t * stream;
    xkblas_stream_instruction_t * instr;
    device->offloader.instruction_new(
        XKBLAS_STREAM_TYPE_KERN,    /* IN */
        &stream,                    /* OUT */
        XKBLAS_STREAM_INSTR_KERN,   /* IN */
        &instr                      /* OUT */
    );
    assert(stream);
    assert(instr);

    /* create a new kernel instruction */
    instr->kern.task = task;

    /* submit instruction to the stream */
    xkblas_device_submit(device, stream, instr);
}

void
xkblas_stream_instruction_submit_copy(
    const xkblas_driver_t         * driver,
    const memory_view_t           & host_view,
    const uint8_t                   dst_device_global_id,
    const memory_replicate_view_t & dst_device_view,
    const uint8_t                   src_device_global_id,
    const memory_replicate_view_t & src_device_view,
    const std::function<void()> callback
) {
    XKBLAS_DEBUG("  Copy from %u to %u", src_device_global_id, dst_device_global_id);
}
