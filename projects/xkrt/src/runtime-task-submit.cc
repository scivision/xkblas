/* ************************************************************************** */
/*                                                                            */
/*   runtime-task-submit.cc                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/16 21:29:59 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/sync/bits.h>

static inline void
xkrt_runtime_submit_task_host(xkrt_runtime_t * runtime, task_t * task)
{
    xkrt_driver_t * driver = runtime->drivers.list[XKRT_DRIVER_TYPE_HOST];
    assert(driver->ndevices_commited == 1);

    xkrt_device_t * device = driver->devices[0];
    assert(device);

    device->push(task);
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
    xkrt_device_t * device = NULL;
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
        // must have at least one non-host device
        if (runtime->drivers.devices.n > 1)
        {
            while (1)
            {
                device_id = runtime->drivers.devices.round_robin_device_global_id.fetch_add(1, std::memory_order_relaxed);
                device_id = (xkrt_device_global_id_t) (1 + (device_id % (runtime->drivers.devices.n - 1)));

                xkrt_device_t * device = runtime->drivers.devices.list[device_id];
                if (device)
                    break ;
            }
        }
        else
            LOGGER_FATAL("No device available to execute the task");
    }

    // only coherent async are supported onto the host device yet
    if (device_id == HOST_DEVICE_GLOBAL_ID)
        return xkrt_runtime_submit_task_host(runtime, task);

    assert((device_id >= 0 && device_id < runtime->drivers.devices.n));
    device = runtime->drivers.devices.list[device_id];
    assert(device);

    LOGGER_DEBUG("Enqueuing task `%s` to device %d", task->label, device_id);
    device->push(task);
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
