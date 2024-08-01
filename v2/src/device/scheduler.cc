# include "logger/logger.h"
# include "device/consts.h"
# include "device/scheduler.hpp"

// Warning: this is called by a ThreadProducer - to enqueue a task in a ThreadWorker
void
xkblas_scheduler_enqueue(xkblas_scheduler_t * scheduler, Task * task)
{
    assert(task->state == TASK_STATE_READY);

    // Find the worker to offloadUINT8_MAX the task
    uint8_t device_id = UINT8_MAX;
    assert(XKBLAS_DEVICES_MAX < UINT8_MAX);

    // if an ocr parameter is set, match
    if (task->ocr_access_index != UINT8_MAX)
    {
        assert(task->ocr_access_index >= 0);
        // TODO
    }

    if (task->targetted_device_id != UINT8_MAX)
    {
        assert(task->targetted_device_id >= 0);
        assert(task->targetted_device_id < XKBLAS_DEVICES_MAX);
        device_id = task->targetted_device_id;
    }

    if (device_id == UINT8_MAX)
    {
        do {
            device_id = scheduler->round_robin_device_id.fetch_add(1, std::memory_order_relaxed);
            device_id = device_id % XKBLAS_DEVICES_MAX;
        } while (((volatile ThreadWorker *)scheduler->workers.list[device_id]) == nullptr);
    }

    ThreadWorker * worker = scheduler->workers.list[device_id];
    if (worker == NULL)
    {
        XKBLAS_ERROR("Trying to enqueue a task to an uninitialized worker %d", device_id);
        return ;
    }

    // XKBLAS_DEBUG("Enqueuing task %p to device %d", task, device_id);
    worker->push(task);
}

void
xkblas_scheduler_register(
    xkblas_scheduler_t * scheduler,
    ThreadWorker * worker,
    int device_id
) {
    assert(device_id >= 0);
    assert(device_id < XKBLAS_DEVICES_MAX);
    assert(scheduler->workers.list[device_id] == nullptr);
    scheduler->workers.list[device_id] = worker;
    scheduler->workers.n = MAX(scheduler->workers.n, device_id);
}
