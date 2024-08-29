#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/consts.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/stream-instruction-submit.h"
# include "device/task.hpp"
# include "logger/logger.h"
# include "logger/todo.h"
# include "sync/kinterval-btree.hpp"

# include <cstdint>
# include <functional>
# include <map>

# pragma message(TODO "'fetch' implementation could be optimize")

# pragma message(TODO "merge 'Replicate' on continuous "   \
        "memory addresses - for now, just perform one data "    \
        "transfer per block")

# pragma message(TODO "Nest classes into a 'KMemory' templated class - corresponding to a global view of the memory in 'K' dimensions")

// if greater than 16, then gotta increase the bitfield type
static_assert(XKBLAS_DEVICES_MAX <= 16);
typedef uint16_t memory_replicates_bitfield_t;

/* a memory allocation */
class MemoryAllocation {

    public:

        /* address returned by the allocator */
        const uintptr_t addr;

        /* leading dimension */
        const int LD;

    public:
        MemoryAllocation(uintptr_t addr, int LD) : addr(addr), LD(LD) {}
        virtual ~MemoryAllocation() {}

}; /* MemoryAllocation */

/* a host replicate on a device */
class MemoryReplicate
{
    public:
        /* List of views for this device replicate.  A device may have several
         * views (and allocation) of the same 'host memory' - as it may
         * asynchronously be read by different concurrent kernel requiring
         * different memory alignment for BLAS operations)
         */
        std::vector<MemoryAllocation *> allocations;

    public:
        MemoryReplicate() : allocations() {}
        MemoryReplicate(const MemoryReplicate & r) : allocations(r.allocations) {}
        ~MemoryReplicate() {}

}; /* MemoryReplicate */

/* see https://stackoverflow.com/questions/68795092/how-to-copy-a-built-in-array-via-copy-constructor */
class MemoryReplicates
{
    public:
        MemoryReplicate array[XKBLAS_DEVICES_MAX];

        MemoryReplicates() {}
        MemoryReplicates(MemoryReplicates const &) = default;
};

/* a memory block, one per tree node */
class MemoryBlock {

    public:

        /* host memory view of that block */
        memory_view_t host_view;

        /* per device replicate info */
        MemoryReplicates replicates;

        /* if i-th bit is set, the i-th device has a view with a valid copy */
        volatile memory_replicates_bitfield_t valid;

        /* if i-th bit is set, the i-th device has a view that is fetching */
        volatile memory_replicates_bitfield_t fetching;

    public:

        /* a new memory block, assume it is valid on the host (device id = 0) */
        MemoryBlock(const memory_view_t & v) :
            host_view(v),
            replicates(),
            valid(0),
            fetching(0)
        {
            # pragma message(TODO "Reimplementfor each view of the replicate")
            # if 0
            const int host_devid = 0;
            this->valid = (1 << host_devid);
            this->replicates.array[host_devid].addr = host_view.begin_addr();
            this->replicates.array[host_devid].LD = host_view.LD;
            # endif
        }

        /* a block from splitting an existing one */
        MemoryBlock(
            const MemoryBlock & block
        ) :
            host_view(block.host_view),
            replicates(block.replicates),
            valid(block.valid),
            fetching(block.fetching)
        {}

        virtual ~MemoryBlock() {}

}; /* MemoryBlock */

/* storage passed when searchingi n the tree */
template <int K>
class KMemoryTreeNodeSearch {

    using Access = KMemoryAccess<K>;
    using Region = Intervals<K>;

    public:
    class BlockInfo {

        public:

            /* memory block in the tree */
            MemoryBlock * block;

            /* copy of the host view */
            memory_view_t host_view;

            /* dst device */
            int8_t dst_device_global_id;

            /* dst allocation */
            int16_t dst_allocation_id;

            /* copy of the replicate view */
            MemoryAllocation * dst_allocation;

            /* source device */
            int8_t src_device_global_id;

            /* src allocation */
            int16_t src_allocation_id;

            /* copy of the replicate view */
            MemoryAllocation * src_allocation;

        public:

            BlockInfo(MemoryBlock * b) :
                block(b),
                host_view(b->host_view),
                dst_device_global_id(-1),
                dst_allocation_id(-1),
                dst_allocation(nullptr),
                src_device_global_id(-1),
                src_allocation_id(-1),
                src_allocation(nullptr)
            {}

            virtual ~BlockInfo() {}

    }; /* BlockInfo */


   public:

       /* different search type */
       enum Type : uint8_t {
           INSERTING_BLOCKS    = 0,
           SEARCH_FOR_BLOCKS   = 1,
       };

   public:

       /////////////////////////////////
       // used in all types of search //
       /////////////////////////////////

       /* type of search performing */
       Type type;

        /* device global id, on which we are looking for invalid blocks or validating blocks */
       const uint8_t device_global_id;

       ////////////////////////////////////////////////////////////////////
       // used if type == INSERTING_BLOCKS  //
       ////////////////////////////////////////////////////////////////////

       /* the access being inserted / intersected */
       Access * access;

       ///////////////////////////////////////////////
       // used if type == SEARCH_FOR_BLOCKS //
       ///////////////////////////////////////////////

       /* list of blocks for the current access */
       std::vector<BlockInfo> blocks_info;

   public:

       KMemoryTreeNodeSearch(
           uint8_t devid
       ) :
           type(INSERTING_BLOCKS),
           device_global_id(devid),
           access(nullptr),
           blocks_info()
       {}

       virtual ~KMemoryTreeNodeSearch() {}

       void
       prepare_insert(Access * a)
       {
           this->access = a;
           this->type = INSERTING_BLOCKS;
       }

       void
       prepare_search_blocks(void)
       {
           assert(this->blocks_info.size() == 0);
           this->blocks_info.clear();
           this->type = SEARCH_FOR_BLOCKS;
       }

}; /* KMemoryTreeNodeSearch */


template <int K>
class KMemoryTreeNode : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node {

    using Access = KMemoryAccess<K>;
    using Base = typename KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node;
    using BlockInfo = typename KMemoryTreeNodeSearch<K>::BlockInfo;
    using Node = KMemoryTreeNode<K>;
    using Region = Intervals<K>;
    using Search = KMemoryTreeNodeSearch<K>;

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
            block(access->host_view)
        {
            // TODO : is access->region != r ; then the access resulted in
            // several insertion nodes, which i snot s upported yet
            assert(access->region.equals(r));

            /* initialize with host allocation */
            const int host_device_id = 0;
            const memory_replicates_bitfield_t host_device_bitmask = (1 << host_device_id);

            MemoryAllocation * allocation = new MemoryAllocation(access->host_view.addr, access->host_view.LD);
            assert(this->block.replicates.array[host_device_id].allocations.size() == 0);
            this->block.replicates.array[host_device_id].allocations.push_back(allocation);

            assert(this->block.valid == 0);
            this->block.valid |= host_device_bitmask;
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
            Search & search,
            const access_mode_t mode
        ) {
            (void) search;
            (void) mode;
            assert(search.type == Search::Type::INSERTING_BLOCKS);
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
            Search & search,
            const Region & region,
            const access_mode_t mode
        ) const {
            (void) search;
            (void) region;
            (void) mode;

            // TODO : can we fasten intersection by keeping track of an included 'valid' bitmask ?

            return false;
        }

        inline void
        on_intersect(
            Search & search,
            const Region & region,
            const access_mode_t mode
        ) {
            assert(region.includes(this->region));
            const int devbit = (1 << search.device_global_id);

            /* two intersection search possible : whether looking for invalid
             * blocks to initiate fetches, whether setting them valid (after
             * fetching) */
            switch (search.type)
            {
                case (Search::Type::SEARCH_FOR_BLOCKS):
                {
                    # pragma message(TODO "Manage case if another fetch is already going in parallel")
                    search.blocks_info.push_back(BlockInfo(&(this->block)));
                    break ;
                }

                default:
                {
                    XKBLAS_FATAL("Invalid search type in memory tree");
                    assert(0);
                }
            }
        }

        void
        dump_str(FILE * f) const
        {
            KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node::dump_str(f);
        }

        void
        dump_region_str(FILE * f) const
        {
            // KIntervalBtree<K, DeviceInvalidRegions>::Node::dump_region_str(f);
            fprintf(f, "\\\\ host-addr=%p", (void *) this->block.host_view.addr);
            fprintf(f, "\\\\ block size (m, n)=(%d, %d) - LD=%d", this->block.host_view.bs_m, this->block.host_view.bs_n, this->block.host_view.LD);
            fprintf(f, "\\\\ tile (m, n)=(%d, %d)",  this->block.host_view.tm,   this->block.host_view.tn);

            // for (uint8_t device_global_id = 0 ; device_global_id < ctx->drivers.devices.n ; ++device_global_id)
            for (uint8_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
            {
                const int devbit = (1 << device_global_id);
                fprintf(f, "\\\\ dev %d - valid=%d - fetching=%d",
                    device_global_id,
                    this->block.valid    & devbit ? 1 : 0,
                    this->block.fetching & devbit ? 1 : 0
                );
            }
        }

}; /* KMemoryTreeNode */

static inline void
fetch_callback(const void * args[XKBLAS_STREAM_CALLBACK_ARGS_MAX])
{
    assert(XKBLAS_STREAM_CALLBACK_ARGS_MAX >= 3);

    XKBLAS_DEBUG("  Completed a transfer!");

    xkblas_driver_t * driver = (xkblas_driver_t *) args[0];
    xkblas_device_t * device = (xkblas_device_t *) args[1];
    Task            * task   =            (Task *) args[2];

    /* a fetch completed */
    if (task->fetched() == TASK_STATE_DATA_FETCHED)
    {
        /* the task kernel is ready for execution */
        xkblas_device_task_access_fetched(driver, device, task);
    }
}

template <int K>
class KMemoryTree : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>> {

    using Access = KMemoryAccess<K>;
    using Base = KIntervalBtree<K, KMemoryTreeNodeSearch<K>>;
    using BlockInfo = typename KMemoryTreeNodeSearch<K>::BlockInfo;
    using Node = KMemoryTreeNode<K>;
    using NodeBase = typename KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node;
    using Region = Intervals<K>;
    using Search = KMemoryTreeNodeSearch<K>;
    using Task = KTask<K>;

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

        void
        fetch_access(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access
        ) {
            /* list of invalid blocks on that device */
            Search search(device->global_id);
            const int devbit = (1 << device->global_id);

            this->lock();
            {

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                // 1) Ensure the access is represented in the memory tree
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                # pragma message(TODO "Step (1) and (2) could be merged to only lock/search once")
                search.prepare_insert(access);
                this->insert(search, access->region, access->mode);

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                // 2) intersect - to find all nodes that form the access->region
                //      - mark fetching bit
                //      - and find the 'source' device for fetching - calling the driver with the 'valid' bitmask
                //          (and the driver can return 'source' == 'device' if valid on the current device)
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                # pragma message(TODO "Step (1) and (2) could be merged to only lock/search once")
                /* find invalid memory blocks for that access on that device */
                search.prepare_search_blocks();
                this->intersect(search, access->region, access->mode);
                assert(search.blocks_info.size() >= 1);

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                // 3) check that there exists a continuous allocation for that access
                //      if no, do a new allocation and add the view to each block
                //    then create a list of memcpy request to perform if read mode is set
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                // just to be warn whenver this occurs, as it is fairly unlikely
                if (search.blocks_info.size() > 1)
                    XKBLAS_WARN("Several memory blocks found for an access");

                MemoryAllocation * allocation = nullptr;
                int j = 0;
                int nallocations = search.blocks_info[0].block->replicates.array[device->global_id].allocations.size();
                int nblocks = search.blocks_info.size();

                /* for each view of the block 0 */
                while (j < nallocations)
                {
                    /* get the view allocation */
                    allocation = search.blocks_info[0].block->replicates.array[device->global_id].allocations[j];

                    /* for each other blocks */
                    int i = 1;
                    while (i < nblocks)
                    {
                        /* for each view of other blocks */
                        int nallocations = search.blocks_info[i].block->replicates.array[device->global_id].allocations.size();
                        for (int k = 0 ; k < nallocations ; ++k)
                        {
                            /* this block has a view with the same allocation, check next block */
                            if (allocation == search.blocks_info[i].block->replicates.array[device->global_id].allocations[k])
                            {
                                search.blocks_info[i].dst_allocation_id = k;
                                goto next_block;
                            }
                        }

                        /* this block has no view view the same allocation, restart from the next view of block 0 */
                        goto next_view;

next_block:
                        ++i;
                        continue ;
                    }

                    /* every blocks have a view with the allocation 'allocation' */
                    search.blocks_info[0].dst_allocation_id = j;
                    break ;

next_view:
                    ++j;
                    continue ;
                }

                /* no allocation found */
                if (allocation == nullptr)
                {
                    XKBLAS_DEBUG("No continuous allocation found for the access, reallocating and creating a new view");

                    // TODO : memory allocation may spinlock for a while if the
                    // device memory is full... in such case, we probably want
                    // to release the memory-tree lock, and restart all that
                    // shit over again once memory got allocated

                    /* allocate continuous memory for that access */
                    uint64_t  size = access->host_view.bs_n * access->host_view.bs_m * access->host_view.sizeof_type;
                    uintptr_t addr = (uintptr_t) xkblas_memory_allocate(driver, device, size);
                    int         LD = access->host_view.bs_n;
                    XKBLAS_DEBUG("  allocated at %p for size %zu", (void *) addr, size);
                    assert(addr);

                    allocation = new MemoryAllocation(addr, LD);
                    assert(allocation);

                    /* add a view to it in tree memory blocks */
                    for (BlockInfo & info : search.blocks_info)
                    {
                        std::vector<MemoryAllocation *> & allocations = info.block->replicates.array[device->global_id].allocations;
                        info.dst_allocation_id = allocations.size();
                        allocations.push_back(allocation);
                    }
                }

                access->device_view.addr = allocation->addr;
                access->device_view.LD   = allocation->LD;

                /* set the copy infos if reading */
                if (access->mode & ACCESS_MODE_R)
                {
                    for (BlockInfo & info : search.blocks_info)
                    {
                        /* parameters setup */

                     // info.host_view = set already

                        info.dst_device_global_id = device->global_id;
                     // info.dst_allocation_id = set already
                        info.dst_allocation = info.block->replicates.array[info.dst_device_global_id].allocations[info.dst_allocation_id];

                        info.src_device_global_id = 0; // TODO find best device - only using host for now
                        info.src_allocation_id = 0; // TODO currently always use the first allocation on that source device
                        info.src_allocation = info.block->replicates.array[info.src_device_global_id].allocations[info.src_allocation_id];

                        /* assertion tests on parameters */
                        assert(info.dst_allocation_id >= 0);
                        assert(info.dst_allocation_id < info.block->replicates.array[info.dst_device_global_id].allocations.size());

                        assert(info.src_allocation_id >= 0);
                        assert(info.src_allocation_id < info.block->replicates.array[info.src_device_global_id].allocations.size());
                    }
                }

            } /* this->lock(); */
            this->unlock();

            /////////////////////////////////////////////////////////////////////////
            // 4) TODO : do fetches
            //     TODO : once fetch completed - lock + intersect + unlock
            /////////////////////////////////////////////////////////////////////////

            if (access->mode & ACCESS_MODE_R)
            {
                for (BlockInfo & info : search.blocks_info)
                {
                    /* callback setup */
                    assert(XKBLAS_STREAM_CALLBACK_ARGS_MAX >= 3);
                    xkblas_stream_callback_t callback;
                    callback.func = fetch_callback;
                    callback.args[0] = driver;
                    callback.args[1] = device;
                    callback.args[2] = task;

                    /* set parameters */
                    memory_replicate_view_t dst_view(info.dst_allocation->addr, info.dst_allocation->LD);
                    memory_replicate_view_t src_view(info.src_allocation->addr, info.src_allocation->LD);

                    /* launch asynchronous copy */
                    xkblas_stream_instruction_submit_copy(
                        driver,
                        device,
                        info.host_view,
                        info.dst_device_global_id,
                        dst_view,
                        info.src_device_global_id,
                        src_view,
                        callback
                    );
                }
            }
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

            /* increase task 'fetching' counter so it does not get ready early
             * (eg before we processed all accesses bellow)
             */
            task->fetching();

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
            {
                Access * access = task->accesses + i;
                task->fetching();
                this->fetch_access(driver, device, task, access);
            }

            return task->fetched();
        }

        //////////////////
        //  INVALIDATE  //
        //////////////////
        void
        invalidate_caches(void)
        {
            # pragma message(TODO "Empty the memory tree")
        }

        //////////////
        //  INSERT  //
        //////////////
        Node *
        new_node(
            Search & search,
            const Region & region,
            const int k,
            const Color color
        ) const {
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            return new Node(search.access, region, k, color);
        }

        Node *
        new_node(
            Search & search,
            const Region & region,
            const int k,
            const Color color,
            const NodeBase * nodebase
        ) const {
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            return new Node(search.access, region, k, color, reinterpret_cast<const Node *>(nodebase));
        }
};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
