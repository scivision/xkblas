# include "conf/conf.h"
# include "device/driver.h"
# include "device/stream.hpp"
# include "logger/logger.h"
# include "logger/todo.h"

# include <cassert>

# pragma message(TODO "Move these initializer into class member functions")

void
xkblas_device_stream_init(xkblas_device_t * device, Stream * stream, unsigned int capacity)
{
    # pragma message(TODO "wtf is this madness ? Lacking knowledge on the rest of the code, leaving stream initialization for now")

    # if 0
    unsigned int cnt = 0;
    unsigned int prefix[XKBLAS_IO_STREAM_ALL+1];

    prefix[XKBLAS_IO_STREAM_H2D] = 0;
    cnt += (stream->count[XKBLAS_IO_STREAM_H2D]  = XKBLAS_CONF.cuda_conc_h2d);
    prefix[XKBLAS_IO_STREAM_D2H] = cnt;
    cnt += (stream->count[XKBLAS_IO_STREAM_D2H]  = XKBLAS_CONF.cuda_conc_d2h);
    prefix[XKBLAS_IO_STREAM_D2D] = cnt;
    cnt += (stream->count[XKBLAS_IO_STREAM_D2D]  = XKBLAS_CONF.cuda_conc_d2d);
    prefix[XKBLAS_IO_STREAM_KERN] = cnt;
    cnt += (stream->count[XKBLAS_IO_STREAM_KERN] = XKBLAS_CONF.cuda_conc_stream_kernel);
    prefix[XKBLAS_IO_STREAM_KERN+1] = cnt;

    stream->next[XKBLAS_IO_STREAM_D2H]  = 0;
    stream->next[XKBLAS_IO_STREAM_H2D]  = 0;
    stream->next[XKBLAS_IO_STREAM_D2D]  = 0;
    stream->next[XKBLAS_IO_STREAM_KERN] = 0;

    xkblas_io_stream_t** ios;
    stream->ios = ios = (xkblas_io_stream_t **) malloc(sizeof(xkblas_io_stream_t*) * cnt );
    assert( stream->ios[0]!= 0 );
    stream->ios[XKBLAS_IO_STREAM_H2D]  = stream->ios[0]+prefix[XKBLAS_IO_STREAM_H2D];
    stream->ios[XKBLAS_IO_STREAM_D2H]  = stream->ios[0]+prefix[XKBLAS_IO_STREAM_D2H];
    stream->ios[XKBLAS_IO_STREAM_D2D]  = stream->ios[0]+prefix[XKBLAS_IO_STREAM_D2D];
    stream->ios[XKBLAS_IO_STREAM_KERN] = stream->ios[0]+prefix[XKBLAS_IO_STREAM_KERN];

    for (unsigned int i = 0; i < cnt; ++i)
    {
        xkblas_io_stream_type_t type =
            i < prefix[XKBLAS_IO_STREAM_D2H] ? XKBLAS_IO_STREAM_H2D :
            i < prefix[XKBLAS_IO_STREAM_D2D] ? XKBLAS_IO_STREAM_D2H :
            i < prefix[XKBLAS_IO_STREAM_KERN] ? XKBLAS_IO_STREAM_D2D : XKBLAS_IO_STREAM_KERN
            ;
        ios[i]  = stream->f_stream_alloc( device, type, capacity );
        ios[i]->sid = i;
        assert( ios[i] != 0 );
        ios[i]->stream = s;
        //printf("%i:: init stream %i type: %s\n", device->ld->ldid, i, 
        //    type == XKBLAS_IO_STREAM_H2D ? "H2D" : type == XKBLAS_IO_STREAM_KERN ? "kern": type == XKBLAS_IO_STREAM_D2H ? "D2H" : type == XKBLAS_IO_STREAM_D2D ? "D2D" : "<NOTYPE>" );
        assert( 0 == _xkblas_offload_iostream_init( stream->ios[i], type, capacity )); 
    }
    # endif
}

void
xkblas_device_init(xkblas_driver_t * driver, xkblas_device_t * device, int driver_device_id)
{
    device->driver_device_id = driver_device_id;
    device->state  = XKBLAS_DEVICE_STATE_CREATE;
    device->tid = 0;
    device->spawn_count = 0;
    device->exec_count = 0;
    device->finalize = false;
    assert( 0 == pthread_mutex_init(&device->lock, 0));
    assert( 0 == pthread_cond_init(&device->cond, 0));
    assert( 0 == pthread_cond_init(&device->cond_sleep, 0));
    device->issleeping = 0;
    device->request.op = XKBLAS_DEVICEOP_NOP;
    device->request.arg = 0;
    device->request.counter = 0;

    device->cnt_push = 0;

    driver->f_device_init(driver_device_id);

    assert(0== pthread_mutex_init(&device->pipe_lock, 0));

    device->p_write   = 0; /* next position where to write new task */
    device->p_ready   = 0; /* position of the next task to insert into the kernel submission stream */
    device->p_finish  = 0; /* position of the next task to erase from the pipeline */
    device->pipe_size = XKBLAS_CONF.cuda_conc_kernel;
    device->pipeline  = (Task**)malloc(sizeof(Task*)*device->pipe_size);
    for (int i=0; i<device->pipe_size; ++i)
        device->pipeline[i] = 0;

    device->time_tasks = 0.0;
    device->exectasks  = 0;
    device->flops_exectasks = 0.0;
    device->data_exectasks = 0.0;
    device->submittasks = 0;
    device->flops_submittasks= 0.0;
    device->data_submittasks = 0.0;

    device->cnt_pending = 0;
    device->cnt_ready = 0;
    device->cnt_exec = 0;

    xkblas_device_stream_init(device, &device->stream, XKBLAS_STREAM_CAPACITY);

    assert(0 == pthread_mutex_lock(&device->lock));
    if (device->state == XKBLAS_DEVICE_STATE_CREATE)
        device->state = XKBLAS_DEVICE_STATE_INIT;
    assert(0 == pthread_cond_signal(&device->cond_sleep));
    assert(0 == pthread_mutex_unlock(&device->lock));
}
