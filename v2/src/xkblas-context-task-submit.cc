# include "xkblas-context.h"
# include "sync/bits.h"

// Warning: this is called by a ThreadProducer - to enqueue a task in a ThreadWorker
void
xkblas_context_submit_task(xkblas_context_t * context, Task * task)
{
    assert(task->state.value == TASK_STATE_READY);

    // whether ocr, whether target device id, not both !
    assert((task->ocr_access_index != UNSPECIFIED_TASK_ACCESS) +
            (task->targeted_device_id != UNSPECIFIED_DEVICE_GLOBAL_ID) <= 1);

    // Find the worker to offload the task
    ThreadWorker * worker = nullptr;
    xkblas_device_global_id_t device_id = UNSPECIFIED_DEVICE_GLOBAL_ID;

    // if an ocr parameter is set, retrieve the device accordingly
    if (task->ocr_access_index != UNSPECIFIED_TASK_ACCESS)
    {
        assert(task->ocr_access_index >= 0 && task->ocr_access_index < task->naccesses);
        memory_replicates_bitfield_t owners = context->memtree.who_owns(task->accesses + task->ocr_access_index);
        if (owners)
            device_id = (xkblas_device_global_id_t) __random_set_bit(owners) - 1;
    }

    // if a target device is set
    else if (task->targeted_device_id != UNSPECIFIED_DEVICE_GLOBAL_ID)
    {
        assert(task->ocr_access_index == UNSPECIFIED_TASK_ACCESS);
        device_id = task->targeted_device_id;
    }

    // fallback to round robin if no devices found
    if (device_id == UNSPECIFIED_DEVICE_GLOBAL_ID)
    {
        while (1)
        {
            device_id = context->drivers.devices.round_robin_device_global_id.fetch_add(1, std::memory_order_relaxed);
            device_id = device_id % context->drivers.devices.n;
            if (context->drivers.devices.list[device_id])
                break ;
        }
    }

    if (worker == nullptr)
    {
        assert((device_id >= 0 && device_id < context->drivers.devices.n) || device_id == HOST_DEVICE_GLOBAL_ID);
        if (device_id == HOST_DEVICE_GLOBAL_ID)
            worker = context->memory_coherent_worker_thread;
        else
            worker = context->drivers.devices.list[device_id]->thread;
    }

    assert(worker);

    # ifndef NDEBUG
    XKBLAS_DEBUG("Enqueuing task `%s` to device %d", task->label, device_id);
    # else
    XKBLAS_INFO("Enqueuing a task to device %d", device_id);
    # endif /* NDEBUG */

    worker->push(task);
}
