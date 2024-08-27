# include "device/device.h"
# include "device/driver.h"
# include "device/stream-instruction.h"
# include "device/task.hpp"
# include "logger/logger.h"

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
    Task * task,
    const xkblas_stream_callback_t & callback
) {
    XKBLAS_INFO("Task `%s` is ready for kernel execution", task->label);

    /* create a new instruction and retrieve its offload stream */
    xkblas_stream_t * stream;
    xkblas_stream_instruction_t * instr;
    device->offloader.instruction_new(
        XKBLAS_STREAM_TYPE_KERN,        /* IN */
        &stream,                        /* OUT */
        XKBLAS_STREAM_INSTR_TYPE_KERN,  /* IN */
        &instr,                         /* OUT */
        callback
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
    const xkblas_driver_t          * driver,
    xkblas_device_t                * device,
    const memory_view_t            & host_view,
    const uint8_t                    dst_device_global_id,
    const memory_replicate_view_t  & dst_device_view,
    const uint8_t                    src_device_global_id,
    const memory_replicate_view_t  & src_device_view,
    const xkblas_stream_callback_t & callback
) {
    assert(device->global_id == dst_device_global_id);

    XKBLAS_DEBUG("  Copy from src=%u to dst=%u", src_device_global_id, dst_device_global_id);

    /* find the type of copy instruction */
    xkblas_stream_instruction_type_t itype;
    if (src_device_global_id == 0)
    {
        if (dst_device_global_id == 0)
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_H2H;
        else
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_H2D;
    }
    else
    {
        if (dst_device_global_id == 0)
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_D2H;
        else
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_D2D;
    }

    /* find the type of stream to use */
    xkblas_stream_type_t stype;
    switch(itype)
    {
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
        {
            stype = XKBLAS_STREAM_TYPE_H2D;
            break ;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
        {
            stype = XKBLAS_STREAM_TYPE_D2H;
            break ;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
        {
            stype = XKBLAS_STREAM_TYPE_D2D;
            break ;
        }

        default:
        {
            XKBLAS_FATAL("Impossible occured");
            break ;
        }
    }

    /* create a new instruction and retrieve its offload stream */
    xkblas_stream_t * stream;
    xkblas_stream_instruction_t * instr;
    device->offloader.instruction_new(
        stype,      /* IN */
        &stream,    /* OUT */
        itype,      /* IN */
        &instr,     /* OUT */
        callback
    );
    assert(stream);
    assert(instr);

    /* create a new copy instruction */
    instr->copy.host_view       = host_view;
    instr->copy.dst_device_view = dst_device_view;
    instr->copy.src_device_view = src_device_view;

    /* submit instruction to the stream */
    xkblas_device_submit(device, stream, instr);
}
