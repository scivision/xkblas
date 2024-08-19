#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/device.h"
# include "device/memory-block.hpp"
# include "device/task.hpp"
# include "logger/logger.h"
# include "logger/todo.h"
# include "sync/kinterval-btree.hpp"

template <int K>
class KMemoryTreeNode : public KIntervalBtree<K>::Node {

    using Base = typename KIntervalBtree<K>::Node;
    using Region = Intervals<K>;
    using Node = KMemoryTreeNode<K>;

    public:

        /* an on-going memory block fetch */
        class ReplicateFetch {

            public:

                /* region */
                Region region;

                /* the replicate view */
                memory_block_replicate_view_t replicate_view;

                /* a view of the memory block */
                memory_block_view_t block_view;

                /* devices on which the block is valid */
                memory_block_bitfield_t valid;

            public:

                ReplicateFetch(
                    Node * node,
                    uint8_t device_global_id
                ) :
                    region(node->region),
                    replicate_view(node->block.replicates[device_global_id].view),
                    block_view(node->block.view),
                    valid(node->block.valid)
                {}

                virtual ~ReplicateFetch() {}

        }; /* ReplicateFetch */

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
    using NodeBase = typename KIntervalBtree<K>::Node;
    using ReplicateFetch = typename KMemoryTreeNode<K>::ReplicateFetch;
    using Region = Intervals<K>;
    using Task = KTask<K>;
    using Access = typename KTask<K>::Access;

    public:

        /* lock for accessing the tree structure */
        spinlock_t spinlock;

    public:

        void
        lock(void)
        {
            SPINLOCK_LOCK(this->spinlock);
        }

        void
        unlock(void)
        {
            SPINLOCK_UNLOCK(this->spinlock);
        }

        /** initiate memory transfer to ensure coherency */
        task_state_t
        fetch(xkblas_device_t * device, Task * task)
        {
            task->fetch();

            # pragma message(TODO " currently, continuous blocks on the same device "   \
                    "could be detected here and fetched with a single request")

            # pragma message(TODO " need synchronizations on memory tree use, " \
                    "as blocks may be split / merged")

            /* use task->wc to detect completion of every fetches */
            task->wc.fetch_add(1, std::memory_order_seq_cst);

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
            {
                Access * access = task->accesses + i;
                if ((access->mode & ACCESS_MODE_R) != ACCESS_MODE_R)
                    continue ;

                /* find fetches to perform */
                std::vector<ReplicateFetch> fetches;
                this->lock();
                this->find_fetches(access, fetches, device->global_id);
                this->unlock();

                /* fetch each block */
                for (ReplicateFetch & fetch : fetches)
                {
                    task->wc.fetch_add(1, std::memory_order_seq_cst);
                    // TODO : fetch data and on completion :
                    //  - decr wc
                    //  - update the memory tree's block valid bits
                    //  - update the memory tree's block state (from 'fetching' to 'fecthed')
                    task->wc.fetch_sub(1, std::memory_order_seq_cst); // this on completion
                }
            }

            if (task->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
            {
                // TODO : kernel is ready - queue it
                XKBLAS_DEBUG("Task `%s` is ready for kernel execution", task->label);
                task->fetched();
            }

            return task->state.value;
        }

        /** the given block must be fetched */
        void
        find_fetches_push(
            const Access * access,
            std::vector<ReplicateFetch> & fetches,
            uint8_t device_global_id,
            NodeBase * nodebase
        ) {
            Node * node = reinterpret_cast<Node *>(nodebase);

            const int devbit = (1 << device_global_id);
            assert(!(node->block.valid & devbit));

            /* check if the block is not already being fetched by another access */
            if (node->block.fetching & devbit)
            {
                // TODO : this block is being fetched by another access
                // need to notify the task performing 'access' when its done
                return ;
            }
            node->block.fetching |= devbit;
            fetches.push_back(ReplicateFetch(node, device_global_id));
        }

        /* same as 'find_fetches' but starting search from the passed node */
        void
        find_fetches_from(
            const Access * access,
            std::vector<ReplicateFetch> & fetches,
            NodeBase * parentbase,
            int k
        ) {
            Node * parent = reinterpret_cast<Node *>(parentbase);

            // TODO : ensure that loop is unrolled - else maybe generate code
            while (k < K)
            {
                // TODO
                ++k;
            }
        }

        /** find the minimal list of invalid blocks for the given access */
        void
        find_fetches(
            const Access * access,
            std::vector<ReplicateFetch> & fetches,
            uint8_t device_global_id
        ) {
            tassert(access->mode & ACCESS_MODE_R);
            tassert(!access->region.is_empty());

            if (this->root == nullptr)
            {
                this->root = new Node(access->region, 0, BLACK);
                this->outdate(this->root);
                find_fetches_push(access, fetches, device_global_id, this->root);
            }
            else
            {
                this->find_fetches_from(access, fetches, this->root, 0);
            }

            Base::post_insert();
        }

};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
