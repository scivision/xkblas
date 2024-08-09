# include "memory-tree.hpp"

MemoryTree::MemoryTree() {}


void
MemoryTree::fetch(
    xkblas_device_t * device,
    Task * task
) {

    /* bitfield for this device (all bits to 0 but the one of this device) */
    memory_block_bitfield_t devbit = (1 << device->global_id);

    /* use task->wc as counter for asynchronous callback to detect
       completion of them
       - each callback decr counter
       - each access without need for callback decr it
       - after all parameters have been visited, then decr the counter.
       The last decr that set counter to 0 push the task on the kernel stream
       */
    for (int i = 0 ; i < TASK_MAX_ACCESSES ; ++i)
    {
        task_access_t * access = task->accesses + i;
        if (access->mode == ACCESS_MODE_VOID)
            break ;

        # pragma message(TODO " currently, continuous blocks on the same device "   \
                "may be  fetched with multiple memcpy, while they could be "        \
                "joined in a single memcpy")

    }
}
