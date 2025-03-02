/* ************************************************************************** */
/*                                                                            */
/*   runtime-task-submit.cc                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 02:03:11 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/sync/bits.h>

void
xkrt_runtime_submit_task_host(xkrt_runtime_t * runtime, task_t * task)
{
    if (task->fmtid == runtime->formats.coherent_async)
        runtime->memory_coherent_worker_thread->push(task);
    else
        LOGGER_FATAL("Task execution onto the host is not supported");
}

void
xkrt_runtime_submit_task_device(xkrt_runtime_t * runtime, task_t * task)
{
    // task must be a device task
    assert(task->flags & TASK_FLAG_DEVICE);
    task_dev_info_t * dev = TASK_DEV_INFO(task);

    // user should whether specify a target device id or an ocr, not both !
    assert((dev->ocr_access_index != UNSPECIFIED_TASK_ACCESS) + (dev->targeted_device_id != UNSPECIFIED_DEVICE_GLOBAL_ID) <= 1);

    // Find the worker to offload the task
    Thread * worker = nullptr;
    xkrt_device_global_id_t device_id = UNSPECIFIED_DEVICE_GLOBAL_ID;

    // if an ocr parameter is set, retrieve the device accordingly
    if (dev->ocr_access_index != UNSPECIFIED_TASK_ACCESS)
    {
        // if an ocr is set, task must be a dependent task (i.e. with some accesses)
        assert(task->flags & TASK_FLAG_DEPENDENT);

        // retrieve the access
        task_dep_info_t * dep = TASK_DEP_INFO(task);
        assert(dev->ocr_access_index >= 0 && dev->ocr_access_index < dep->ac);
        access_t * access = TASK_ACCESSES(task) + dev->ocr_access_index;

        // looking for the device that owns the data
        MemoryCoherencyController * memcontroller = runtime->get_or_insert_memory_controller(access->host_view.ld, access->host_view.sizeof_type);
        assert(memcontroller);
        const xkrt_device_global_id_bitfield_t owners = memcontroller->who_owns(access);
        if (owners)
            device_id = (xkrt_device_global_id_t) (__random_set_bit(owners) - 1);
    }

    // if a target device is set
    else if (dev->targeted_device_id != UNSPECIFIED_DEVICE_GLOBAL_ID)
    {
        assert(dev->ocr_access_index == UNSPECIFIED_TASK_ACCESS);
        device_id = dev->targeted_device_id;
    }

    // fallback to round robin if no devices found
    if (device_id == UNSPECIFIED_DEVICE_GLOBAL_ID)
    {
        if (runtime->drivers.devices.n)
        {
            while (1)
            {
                device_id = runtime->drivers.devices.round_robin_device_global_id.fetch_add(1, std::memory_order_relaxed);
                device_id = device_id % runtime->drivers.devices.n;
                if (runtime->drivers.devices.list[device_id])
                    break ;
            }
        }
        else
            return xkrt_runtime_submit_task_host(runtime, task);
    }

    // only coherent async are supported onto the host device yet
    if (device_id == HOST_DEVICE_GLOBAL_ID)
        return xkrt_runtime_submit_task_host(runtime, task);

    if (worker == nullptr)
    {
        assert((device_id >= 0 && device_id < runtime->drivers.devices.n));
        worker = runtime->drivers.devices.list[device_id]->thread;
    }

    LOGGER_DEBUG("Enqueuing task `%s` to device %d", task->label, device_id);

    assert(worker);
    worker->push(task);
}

void
xkrt_runtime_submit_task(xkrt_runtime_t * runtime, task_t * task)
{
    assert(task->state.value == TASK_STATE_READY);
    if (task->flags & TASK_FLAG_DEVICE)
        xkrt_runtime_submit_task_device(runtime, task);
    else
        xkrt_runtime_submit_task_host(runtime, task);
}
