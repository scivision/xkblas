# include "memory-tree.hpp"

MemoryTree::MemoryTree() {}

MemoryTree::~MemoryTree() {}

void
MemoryTree::find(
    task_access_t * access,
    std::vector<MemoryBlock> & blocks
) {
    // TODO : implement me
    //  - fill the 'blocks' list with matching memory blocks
    //  - ensure the tree represents all memory bytes represented by 'access' -
    //    assuming it is initially valid on the host device
}

void
MemoryTree::fetch(
    xkblas_device_t * device,
    Task * task
) {
    /* bitfield for this device (all bits to 0 but the one of this device) */
    memory_block_bitfield_t devbit = (1 << device->global_id);

    # pragma message(TODO " currently, continuous blocks on the same device "   \
            "could be detected here and fetched with a single request")

    /* use task->wc to detect completion of every fetchs */
    task->wc.fetch_add(1, std::memory_order_seq_cst);

    std::vector<MemoryBlock> blocks;
    assert(task->naccesses <= TASK_MAX_ACCESSES);
    for (int i = 0 ; i < task->naccesses ; ++i)
    {
        task_access_t * access = task->accesses + i;
        this->find(access, blocks);
        for (MemoryBlock & block : blocks)
        {
            /* if reading, fetch if invalid on 'device' */
            if (access->mode & ACCESS_MODE_R)
            {
                // TODO
            }

            /* if writting, invalidate on every other devices */
            if (access->mode & ACCESS_MODE_W)
            {
                block->valid = devbit;
            }
        }
        blocks.clear();
    }

    if (task->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
    {
        // TODO : kernel is ready - queue it
    }
}
