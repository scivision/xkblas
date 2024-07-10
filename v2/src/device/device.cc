# include "device/driver.h"

# include <cassert>

void
xkblas_device_init(xkblas_device_t * device, int device_id)
{
    device->device_id = device_id;
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
}
