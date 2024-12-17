/* ************************************************************************** */
/*                                                                            */
/*   stream-instruction-submit.cc                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "device/device.h"
# include "device/driver.h"
# include "device/stream-instruction.h"
# include "device/task.hpp"
# include "logger/logger.h"

# if USE_STATS
#  include "stats/stats.h"
# endif /* USE_STATS */

/**
 *  Several threads may concurrently submit instructions to a device, in the case of "D2D" forwarding.
 *  The source device "src" will queue forwarding cudaMemcpy to its stream, and on completion, submit the kernel to the "dst" device
 *  In parallel, the "dst" device thread may queue other instructions
 */

/* commit a stream instruction and wakeup thread */
static inline void
xkblas_device_submit(
    xkblas_device_t * device,
    xkblas_stream_t * stream,
    xkblas_stream_instruction_t * instr
) {
    // assert(device->thread == ThreadWorker::self());

    /* commit instruction to the stream */
    stream->commit(instr);

    /* wakeup device worker thread */
    device->thread->wakeup();

    /* unlock the stream */
    stream->unlock();
}

/* submit a kernel execution instruction on that device */
void
xkblas_stream_instruction_submit_kernel(
    xkblas_device_t * device,
    Task * task,
    const xkblas_callback_t & callback
) {
    # ifndef NDEBUG
    XKBLAS_INFO("Task `%s` is ready for kernel execution", task->label);
    # endif /* NDEBUG */

    // assert(ThreadWorker::self() == device->thread);

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
    assert(stream->is_locked());

    /* create a new kernel instruction */
    instr->kern.task = task;

    /* submit instruction to the stream */
    xkblas_device_submit(device, stream, instr);
}

# pragma message(TODO "using a full 'host_view' here is overkill, only needing (sizeof_type, n, m) i believe")
void
xkblas_stream_instruction_submit_copy(
    xkblas_device_t                 * device,
    const memory_view_t             & host_view,
    const xkblas_device_global_id_t   dst_device_global_id,
    const memory_replicate_view_t   & dst_device_view,
    const xkblas_device_global_id_t   src_device_global_id,
    const memory_replicate_view_t   & src_device_view,
    const xkblas_callback_t         & callback
) {
    // assert(ThreadWorker::self() == device->thread);
    assert(device->global_id == dst_device_global_id || device->global_id == src_device_global_id);

    assert(dst_device_view.addr);
    assert(dst_device_view.ld);

    assert(src_device_view.addr);
    assert(src_device_view.ld);

    /* find the type of copy instruction */
    xkblas_stream_instruction_type_t itype;
    if (src_device_global_id == HOST_DEVICE_GLOBAL_ID)
    {
        if (dst_device_global_id == HOST_DEVICE_GLOBAL_ID)
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_H2H;
        else
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_H2D;
    }
    else
    {
        if (dst_device_global_id == HOST_DEVICE_GLOBAL_ID)
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_D2H;
        else
            itype = XKBLAS_STREAM_INSTR_TYPE_COPY_D2D;
    }

    /* find the type of stream to use */
    xkblas_stream_type_t stype;
    switch(itype)
    {
        # pragma message(TODO "No H2H streams, do we want one ? Currently using H2D stream for H2H copies, mimicing original xkblas")
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
        {
            assert(device->global_id == dst_device_global_id);
            stype = XKBLAS_STREAM_TYPE_H2D;
            break ;
        }

        case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
        {
            assert(device->global_id == src_device_global_id);
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

    # if USE_STATS
    xkblas_stats_t * stats = xkblas_stats_get();
    stats->streams[stype].transfered += host_view.size();
    # endif /* USE_STATS */
}
