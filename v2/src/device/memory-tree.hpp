#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/device.h"
# include "device/memory-block.hpp"
# include "device/task.hpp"
# include "logger/logger.h"
# include "logger/todo.h"
# include "sync/kinterval-btree.hpp"

/* an on-going memory block fetch */
template <int K>
class KMemoryBlockReplicateFetch {

    using Region = Intervals<K>;

    public:

        /* region */
        const Region region;

        /* a view of the memory block */
        const memory_block_view_t block_view;

        /* the replicate view */
        const memory_block_replicate_view_t replicate_view;

        /* devices on which the block is valid */
        const memory_block_bitfield_t valid;

    public:

        KMemoryBlockReplicateFetch(
            const Region & r,
            const memory_block_view_t & bview,
            const memory_block_replicate_view_t & rview,
            const memory_block_bitfield_t v
        ) :
            region(r),
            replicate_view(rview),
            block_view(bview),
            valid(v)
        {}

        virtual ~KMemoryBlockReplicateFetch() {}

}; /* KMemoryBlockReplicateFetch */

template <int K>
class DeviceInvalidKRegions {

    using Access = typename KTask<K>::Access;
    using ReplicateFetch = KMemoryBlockReplicateFetch<K>;

    public:

        /* device global id which has to perform the fetches */
        const uint8_t device_global_id;

        /* list of invalid replicate */
        std::vector<KMemoryBlockReplicateFetch<K>> list;

        /* the access being inserted / intersected */
        Access * access;

    public:
        DeviceInvalidKRegions(uint8_t id) : device_global_id(id), list(), access(nullptr) {}
        virtual ~DeviceInvalidKRegions() {}

}; /* DeviceInvalidKRegions */

template <int K>
class KMemoryTreeNode : public KIntervalBtree<K, DeviceInvalidKRegions<K>>::Node {

    using ReplicateFetch = KMemoryBlockReplicateFetch<K>;
    using DeviceInvalidRegions = DeviceInvalidKRegions<K>;
    using Region = Intervals<K>;
    using Base = typename KIntervalBtree<K, DeviceInvalidKRegions<K>>::Node;
    using Node = KMemoryTreeNode<K>;

    public:

        /* the memory block represented by this node */
        MemoryBlock block;

    public:

        KMemoryTreeNode<K>(
            const Region & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            block()
        {}

        KMemoryTreeNode<K>(
            const Region & r,
            const int k,
            const Color color,
            const Node * src
        ) :
            Base(src->region, k, color),
            block(src->block)
        {}

    public:

        void
        on_insert(
            DeviceInvalidRegions & dir,
            const access_mode_t mode
        ) {
            (void) dir;
            (void) mode;
            XKBLAS_DEBUG("Inserted a new memory region");
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_stop_test(
            DeviceInvalidRegions & dir,
            const Region & region,
            const access_mode_t mode
        ) const {
            (void) dir;
            (void) region;
            (void) mode;

            // TODO : can we fasten intersection by keeping track of an included 'valid' bitmask ?

            return false;
        }

        inline void
        on_intersect(
            DeviceInvalidRegions & dir,
            const Region & region,
            const access_mode_t mode
        ) {

            assert(dir.access->mode == mode);
            assert(dir.access->region.equals(region));
            assert(this->region.intersects(region));

            /* the block is valid on that device, nothing to do */
            const int devbit = (1 << dir.device_global_id);
            if (this->block.valid & devbit)
                return ;
            assert(!(this->block.valid & devbit));

            /* check if the block is not already being fetched by another access */
            if (this->block.fetching & devbit)
            {
                // TODO : this block is being fetched by another access
                // need to notify the task performing 'access' when its done
                return ;
            }

            /* add this block to the fetching list */
            this->block.fetching |= devbit;
            dir.list.push_back(
                ReplicateFetch(
                    this->region,
                    this->block.view,
                    this->block.replicates[dir.device_global_id],
                    this->block.valid
                )
            );
        }


        void
        dump_str(FILE * f) const
        {
            KIntervalBtree<K, DeviceInvalidRegions>::Node::dump_str(f);
        }

        void
        dump_region_str(FILE * f) const
        {
            KIntervalBtree<K, DeviceInvalidRegions>::Node::dump_region_str(f);
            fprintf(f, "\\\\ LD=%d, m=%d, n=%d", this->block.view.LD, this->block.view.m, this->block.view.n);

            // for (uint8_t device_global_id = 0 ; device_global_id < ctx->drivers.devices.n ; ++device_global_id)
            for (uint8_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
            {
                const int devbit = (1 << device_global_id);
                fprintf(f, "\\\\ dev %d - addr=%p - valid=%d - fetching=%d",
                        device_global_id,
                        (void *) this->block.replicates[device_global_id].addr,
                        this->block.valid    & devbit ? 1 : 0,
                        this->block.fetching & devbit ? 1 : 0

                );
            }
        }

}; /* KMemoryTreeNode */

template <int K>
class KMemoryTree : public KIntervalBtree<K, DeviceInvalidKRegions<K>> {

    using ReplicateFetch = KMemoryBlockReplicateFetch<K>;
    using DeviceInvalidRegions = DeviceInvalidKRegions<K>;
    using Base = KIntervalBtree<K, DeviceInvalidKRegions<K>>;
    using Node = KMemoryTreeNode<K>;
    using NodeBase = typename KIntervalBtree<K, DeviceInvalidKRegions<K>>::Node;
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
            # pragma message(TODO "continuous blocks on the same device "       \
                    "could be detected here and fetched with a single request")

            /* list of invalid blocks on that device */
            DeviceInvalidRegions invalids(device->global_id);

            /* use task->wc to detect completion of every fetches */
            task->fetching();

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
            {
                invalids.access = task->accesses + i;

                /* ensure it is represented in the memory tree */
                this->lock();
                {
                    this->insert(invalids, invalids.access->region, invalids.access->mode);
                }
                this->unlock();

                /* if the kernel reads that memory */
                if (invalids.access->mode & ACCESS_MODE_R)
                {
                    /* find invalid memory blocks on that device */
                    this->lock();
                    {
                        this->intersect(invalids, invalids.access->region, invalids.access->mode);
                    }
                    this->unlock();
                }
            }

            /* initiate fetching of each invalid block */
            for (ReplicateFetch & fetch : invalids.list)
            {
                task->fetching();

                // TODO : fetch data and on completion :
                //  - call task->fetched()
                //  - update the memory tree's block valid bits
                //  - update the memory tree's block state (from 'fetching' to 'fetched')

                task->fetched();
            }

            return task->fetched();
        }

        //////////////
        //  INSERT  //
        //////////////
        Node *
        new_node(
            const Region & region,
            const int k,
            const Color color
        ) const {
            return new Node(region, k, color);
        }

        Node *
        new_node(
            const Region & region,
            const int k,
            const Color color,
            const NodeBase * nodebase
        ) const {
            return new Node(region, k, color, reinterpret_cast<const Node *>(nodebase));
        }

# if 0
        /** the given block must be fetched */
        void
        find_fetches_push(
            const Access * access,
            std::vector<ReplicateFetch> & fetches,
            uint8_t device_global_id,
            NodeBase * nodebase
        ) {
            (void) access;
        }

# endif

};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
