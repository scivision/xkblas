#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/device.h"
# include "device/driver.h"
# include "device/task.hpp"
# include "memory/memory-block.hpp"
# include "logger/logger.h"
# include "logger/todo.h"
# include "sync/kinterval-btree.hpp"

/* an on-going memory block fetch */
template <int K>
class KMemoryBlockReplicateFetch {

    using Region = Intervals<K>;

    public:

        /* a view of the memory block */
        const memory_block_view_t block_view;

        /* the replicate view */
        const memory_block_replicate_view_t replicate_view;

        /* devices on which the block is valid */
        const memory_block_bitfield_t valid;

    public:

        KMemoryBlockReplicateFetch(
            const memory_block_view_t & bview,
            const memory_block_replicate_view_t & rview,
            const memory_block_bitfield_t v
        ) :
            replicate_view(rview),
            block_view(bview),
            valid(v)
        {}

        virtual ~KMemoryBlockReplicateFetch() {}

        uint64_t
        size(void) const
        {
            return this->block_view.bs_m * this->block_view.bs_n * this->block_view.sizeof_type;
        }

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
    using Access = typename KTask<K>::Access;

    public:

        /* the memory block represented by this node */
        MemoryBlock block;

    public:

        /* the region was never accessed before, create a new node */
        KMemoryTreeNode<K>(
            const Access * access,
            const Region & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            block(access->tile)
        {
            // TODO : is access->region != r ; then the access resulted in
            // several insertion nodes, which i snot s upported yet
            assert(access->region.equals(r));
        }

        /* the region was accessed before, create a new node and inherit from 'src' state */
        KMemoryTreeNode<K>(
            const Access * access,
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
        }

        void
        on_shrink(void)
        {
            // TODO : not implemented
            assert(0);
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
            // KIntervalBtree<K, DeviceInvalidRegions>::Node::dump_region_str(f);
            fprintf(f, "\\\\ host-addr=%p", (void *) this->block.view.addr);
            fprintf(f, "\\\\ block size (m, n)=(%d, %d) - LD=%d", this->block.view.bs_m, this->block.view.bs_n, this->block.view.LD);
            fprintf(f, "\\\\ tile (m, n)=(%d, %d)",  this->block.view.tm,   this->block.view.tn);

            // for (uint8_t device_global_id = 0 ; device_global_id < ctx->drivers.devices.n ; ++device_global_id)
            for (uint8_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
            {
                const int devbit = (1 << device_global_id);
                fprintf(f, "\\\\ dev %d - addr=%zu - valid=%d - fetching=%d",
                        device_global_id,
                        this->block.replicates[device_global_id].addr,
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
        fetch(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task
        ) {

            # pragma message(TODO "continuous blocks on the same device "       \
                    "could be detected here and fetched with a single request")

            /* list of invalid blocks on that device */
            DeviceInvalidRegions invalids(device->global_id);
            const int devbit = (1 << device->global_id);

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

                /* initiate fetching of each invalid block for that access */
                for (ReplicateFetch & fetch : invalids.list)
                {
                    assert(fetch.valid & devbit == 0);

                    task->fetching();

                    // TODO sequentially but asynchronously
                    //
                    //  - if needed, allocate the device replicate
                    //  - move the data to the device, and then
                    //      - update the valid and fetching bits in the memory
                    //        tree (= need to lock + search again here...)
                    //      - call task->fetched()

                    const bool require_allocation = (fetch.replicate_view.addr == 0);
                    if (require_allocation)
                    {
                        // TODO :
                        //  uint64_t size = fetch.size()
                        //  allocate(driver, device, size)
                    }

                    // TODO : fetch

                    task->fetched();
                }

                /* reset the invalid list for the next access */
                invalids.list.clear();
            }

            return task->fetched();
        }

        //////////////
        //  INSERT  //
        //////////////
        Node *
        new_node(
            DeviceInvalidRegions & dir,
            const Region & region,
            const int k,
            const Color color
        ) const {
            return new Node(dir.access, region, k, color);
        }

        Node *
        new_node(
            DeviceInvalidRegions & dir,
            const Region & region,
            const int k,
            const Color color,
            const NodeBase * nodebase
        ) const {
            return new Node(dir.access, region, k, color, reinterpret_cast<const Node *>(nodebase));
        }
};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
