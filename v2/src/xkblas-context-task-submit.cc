# include "xkblas-context.h"

// Warning: this is called by a ThreadProducer - to enqueue a task in a ThreadWorker
void
xkblas_context_submit_task(xkblas_context_t * context, Task * task)
{
    assert(task->state.value == TASK_STATE_READY);

    // Find the worker to offload the task
    ThreadWorker * worker = nullptr;
    uint8_t device_id = task->targeted_device_id;

    // if an ocr parameter is set, retrieve the device accordingly
    if (task->ocr_access_index < TASK_MAX_ACCESSES)
    {
        assert(task->ocr_access_index >= 0 && task->ocr_access_index < task->naccesses);
        // TODO
        //  - find in the memtree where the 'task->ocr_access_index' access is valid
        //  - get a random valid device
        uint8_t global_device_id = context->memtree.who_owns(task->accesses + task->ocr_access_index);
        XKBLAS_FATAL("in `xkblas_drivers_enqueue` - OCR feature is not fully implemented yet");
    }

    // targeted device and OCR failed, fallback to round robin
    if (device_id >= context->drivers.devices.n)
    {
        while (1)
        {
            device_id = context->drivers.devices.round_robin_device_id.fetch_add(1, std::memory_order_relaxed);
            device_id = device_id % context->drivers.devices.n;
            if (device_id == 0) /* never enqueue onto the host */
                continue ;
            if (context->drivers.devices.list[device_id])
                break ;
        }
    }

    XKBLAS_DEBUG("Enqueuing task %p to device %d", task, device_id);

    // we found the thread
    assert(device_id >= 0 && device_id < context->drivers.devices.n);
    worker = context->drivers.devices.list[device_id]->thread;

    if (worker == NULL)
        XKBLAS_FATAL("Trying to enqueue a task to an uninitialized worker %d", device_id);

    worker->queue.push(task);
    worker->wakeup();
}
