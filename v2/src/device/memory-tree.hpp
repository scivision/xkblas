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

    using Region = Intervals<K>;
    using Base = typename KIntervalBtree<K>::Node;
    using Node = KMemoryTreeNode<K>;

    public:

        /* the memory block represented by this node */
        MemoryBlock block;

    public:

        KMemoryTreeNode(const Region & r, int k, Color color) :
            KIntervalBtree<K>::Node(r, k, color),
            block()
        {}

        virtual void
        on_insert(
            void * & t,
            const access_mode_t mode
        ) {
            (void) t;
            (void) mode;
            assert(0); // TODO
        }

        virtual void
        on_inherit(const Base * base)
        {
            const Node * node = reinterpret_cast<const Node *>(base);
            assert(0); // TODO
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_test(
            void * & t,
            const Region & region,
            const access_mode_t mode
        ) const {
            (void) t;
            (void) region;
            (void) mode;

            // TODO
            assert(0);
            return false;
        }

        inline void
        on_intersect(
            void * & t,
            const Region & region,
            const access_mode_t mode
        ) const {
            (void) t;
            (void) region;
            (void) mode;
            assert(0); // TODO
        }


        virtual void
        dump_str(FILE * f) const
        {
            KIntervalBtree<K>::Node::dump_str(f);
        }

        virtual void
        dump_region_str(FILE * f) const
        {
            KIntervalBtree<K>::Node::dump_region_str(f);
        }

}; /* KMemoryTreeNode */

/* an on-going memory block fetch */
template <int K>
class KMemoryBlockReplicateFetch {

    using Region = Intervals<K>;
    using Node = KMemoryTreeNode<K>;

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

        KMemoryBlockReplicateFetch(
            Node * node,
            uint8_t device_global_id
        ) :
            region(node->region),
            replicate_view(node->block.replicates[device_global_id].view),
            block_view(node->block.view),
            valid(node->block.valid)
        {}

        virtual ~KMemoryBlockReplicateFetch() {}

}; /* KMemoryBlockReplicateFetch */

template <int K>
class KMemoryTree : public KIntervalBtree<K> {

    using Base = KIntervalBtree<K>;
    using Node = KMemoryTreeNode<K>;
    using NodeBase = typename KIntervalBtree<K>::Node;
    using ReplicateFetch = KMemoryBlockReplicateFetch<K>;
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
            # pragma message(TODO " currently, continuous blocks on the same device "   \
                    "could be detected here and fetched with a single request")

            # pragma message(TODO " need synchronizations on memory tree use, " \
                    "as blocks may be split / merged")


            /* use task->wc to detect completion of every fetches */
            task->fetching();

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
            {
                Access * access = task->accesses + i;

                /* ensure it is represented in the memory tree */
                this->lock();
                {
                    this->insert(access, device->global_id);
                }
                this->unlock();

                /* if the kernel reads that memory */
                if (access->mode & ACCESS_MODE_R)
                {
                    /* find invalid memory blocks on that device */
                    std::vector<ReplicateFetch> invalids;
                    this->lock();
                    {
                        this->intersect(access, device->global_id, invalids);
                    }
                    this->unlock();

                    /* initiate fetching of each block */
                    for (ReplicateFetch & fetch : invalids)
                    {
                        task->fetching();

                        // TODO : fetch data and on completion :
                        //  - call task->fetched()
                        //  - update the memory tree's block valid bits
                        //  - update the memory tree's block state (from 'fetching' to 'fecthed')

                        task->fetched();
                    }
                }
            }

            return task->fetched();
        }

        //////////////
        //  INSERT  //
        //////////////

        virtual Node *
        new_node(
            const Region & region,
            const int k,
            const Color color
        ) const {
            return new Node(region, k, color);
        }




# if 0
        ////////////////
        //  INTERSECT //
        ////////////////

        /* find memory block that are invalid on the given device */
        inline void
        intersect(
            const Access * access,
            uint8_t device_global_id,
            std::vector<ReplicateFetch> & invalids
        ) {
        }

        /* ensure the given access is represented in the memory tree */
        inline void
        insert(
            const Access * access,
            uint8_t device_global_id
        ) {
        }

        /** the given block must be fetched */
        void
        find_fetches_push(
            const Access * access,
            std::vector<ReplicateFetch> & fetches,
            uint8_t device_global_id,
            NodeBase * nodebase
        ) {
            (void) access;

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
            (void) access;
            (void) fetches;

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
                this->root->update_includes();
                find_fetches_push(access, fetches, device_global_id, this->root);
            }
            else
            {
                this->find_fetches_from(access, fetches, this->root, 0);
            }

            Base::post_insert();
        }
# endif

};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
