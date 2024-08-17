#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/device.h"
# include "device/memory-block.hpp"
# include "device/task.hpp"
# include "logger/todo.h"
# include "sync/kinterval-btree.hpp"

template <int K>
class KMemoryTreeNode : public KIntervalBtree<K>::Node {

    using Base = typename KIntervalBtree<K>::Node;
    using Region = Intervals<K>;
    using Node = KMemoryTreeNode<K>;

    public:

        /* the memory block represented by this node */
        MemoryBlock block;

    public:

        KMemoryTreeNode(const Region & r, int k, Color color) :
            KIntervalBtree<K>::Node(r, k, color),
            block()
        {}

}; /* KMemoryTreeNode */

template <int K>
class KMemoryTree : public KIntervalBtree<K> {

    using Base = KIntervalBtree<K>;
    using Node = KMemoryTreeNode<K>;
    using BaseNode = typename KIntervalBtree<K>::Node;
    using Region = Intervals<K>;
    using Task = KTask<K>;
    using Access = typename KTask<K>::Access;

    public:

        /** initiate memory transfer to ensure coherency */
        void
        fetch(xkblas_device_t * device, Task * task)
        {
            /* bitfield for this device (all bits to 0 but the one of this device) */
            memory_block_bitfield_t devbit = (1 << device->global_id);

            # pragma message(TODO " currently, continuous blocks on the same device "   \
                    "could be detected here and fetched with a single request")

            # pragma message(TODO " need synchronizations on memory tree use, " \
                    "as blocks may be split / merged")

            /* use task->wc to detect completion of every fetchs */
            task->wc.fetch_add(1, std::memory_order_seq_cst);

            /* for each access */
            std::vector<MemoryBlock> blocks;
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
            {
                /* find memory blocks intersecting with the access */
                Access * access = task->accesses + i;
                this->find(access, blocks);
                for (MemoryBlock & block : blocks)
                {
                    /* if reading, fetch if invalid on 'device' */
                    if (access->mode & ACCESS_MODE_R)
                    {
                        if (block.valid & devbit)
                        {
                            /* data is valid on that device, no need to fetch */
                        }
                        else
                        {
                            /* data must be fetched */
                            task->wc.fetch_add(1, std::memory_order_seq_cst);
                            // TODO : fetch data and decr wc on completion
                        }
                    }

                    /* if writting, invalidate on every other devices */
                    if (access->mode & ACCESS_MODE_W)
                    {
                        block.valid = devbit;
                    }
                }
                blocks.clear();
            }

            if (task->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
            {
                // TODO : kernel is ready - queue it
            }
        }

        void
        find_from(
            const Access * access,
            std::vector<MemoryBlock> & blocks,
            Node * parent,
            int k
        ) {
            // TODO : ensure that loop is unrolled - else maybe generate code
            while (k < K)
            {
                // TODO
            }
        }

        /** find memory blocks required for the given task - if not found, new
         * blocks are inserted assumed valid on the host device only */
        void
        find(
            const Access * access,
            std::vector<MemoryBlock> & blocks
        ) {
            tassert(!access->region.is_empty());

            if (this->root == nullptr)
            {
                this->root = new Node(access->region, 0, BLACK);
                this->root->update_includes();
            }
            else
            {
                this->find_from(access, blocks, reinterpret_cast<Node *>(this->root), 0);
                this->update();
            }

            Base::post_insert();
        }

};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
