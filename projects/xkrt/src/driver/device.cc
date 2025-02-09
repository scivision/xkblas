/* ************************************************************************** */
/*                                                                            */
/*   device.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:56:36 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/driver/device.h>

int
xkrt_device_poll(xkrt_device_t * device)
{
    int err = 0;
    assert(ThreadWorker::self() == device->thread);

    err = device->offloader.launch_ready_instructions(XKRT_STREAM_TYPE_ALL);
    assert( (err == 0) || (err == EINPROGRESS));

    err = device->offloader.progress_pending_instructions(XKRT_STREAM_TYPE_ALL, false);
    assert( (err == 0) || (err == EINPROGRESS));

    return err;
}

////////////////////////////
// INSTRUCTION SUBMISSION //
////////////////////////////
/* commit a stream instruction and wakeup thread */
static inline void
xkrt_device_stream_instruction_submit(
    xkrt_device_t * device,
    xkrt_stream_t * stream,
    xkrt_stream_instruction_t * instr
) {

    /* commit instruction to the stream */
    stream->commit(instr);

    /* wakeup device worker thread */
    device->thread->wakeup();

    /* unlock the stream */
    stream->unlock();
}

/* submit a kernel execution instruction on that device */
void
xkrt_device_stream_instruction_submit_kernel(
    xkrt_device_t * device,
    void (*launch)(void * handle, void * vargs),
    void * vargs,
    const xkrt_callback_t & callback
) {
    /* create a new instruction and retrieve its offload stream */
    xkrt_stream_t * stream;
    xkrt_stream_instruction_t * instr;
    device->offloader.instruction_new(
        XKRT_STREAM_TYPE_KERN,          /* IN */
        &stream,                        /* OUT */
        XKRT_STREAM_INSTR_TYPE_KERN,    /* IN */
        &instr,                         /* OUT */
        callback
    );
    assert(stream);
    assert(instr);
    assert(stream->is_locked());

    /* create a new kernel instruction */
    instr->kern.launch = launch;
    instr->kern.vargs = vargs;

    xkrt_device_stream_instruction_submit(device, stream, instr);
}

# pragma message(TODO "using a full 'host_view' here is overkill, only needing (sizeof_type, n, m) i believe")
void
xkrt_device_stream_instruction_submit_copy(
    xkrt_device_t                 * device,
    const memory_view_t           & host_view,
    const xkrt_device_global_id_t   dst_device_global_id,
    const memory_replicate_view_t & dst_device_view,
    const xkrt_device_global_id_t   src_device_global_id,
    const memory_replicate_view_t & src_device_view,
    const xkrt_callback_t         & callback
) {
    // assert(ThreadWorker::self() == device->thread);
    assert(device->global_id == dst_device_global_id || device->global_id == src_device_global_id);

    assert(dst_device_view.addr);
    assert(dst_device_view.ld);

    assert(src_device_view.addr);
    assert(src_device_view.ld);

    /* find the type of copy instruction */
    xkrt_stream_instruction_type_t itype;
    if (src_device_global_id == HOST_DEVICE_GLOBAL_ID)
    {
        if (dst_device_global_id == HOST_DEVICE_GLOBAL_ID)
            itype = XKRT_STREAM_INSTR_TYPE_COPY_H2H;
        else
            itype = XKRT_STREAM_INSTR_TYPE_COPY_H2D;
    }
    else
    {
        if (dst_device_global_id == HOST_DEVICE_GLOBAL_ID)
            itype = XKRT_STREAM_INSTR_TYPE_COPY_D2H;
        else
            itype = XKRT_STREAM_INSTR_TYPE_COPY_D2D;
    }

    /* find the type of stream to use */
    xkrt_stream_type_t stype;
    switch(itype)
    {
        # pragma message(TODO "No H2H streams, do we want one ? Currently using H2D stream for H2H copies, mimicing original ptr")
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
        {
            assert(device->global_id == dst_device_global_id);
            stype = XKRT_STREAM_TYPE_H2D;
            break ;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H):
        {
            assert(device->global_id == src_device_global_id);
            stype = XKRT_STREAM_TYPE_D2H;
            break ;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D):
        {
            stype = XKRT_STREAM_TYPE_D2D;
            break ;
        }

        default:
        {
            LOGGER_FATAL("Impossible occured");
            break ;
        }
    }

    /* create a new instruction and retrieve its offload stream */
    xkrt_stream_t * stream;
    xkrt_stream_instruction_t * instr;
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

    xkrt_device_stream_instruction_submit(device, stream, instr);
}
