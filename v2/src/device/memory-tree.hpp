#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "matrix-tile.h"
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

# pragma message(TODO "'fetch' implementation could be optimize by reducing critical sections")

# pragma message(TODO "merge 'Replicate' on continuous "   \
        "memory addresses - for now, just perform one data "    \
        "transfer per block")

# pragma message(TODO "Nest classes into a 'KMemory' templated class - corresponding to a global view of the memory in 'K' dimensions")

// if greater than 16, then gotta increase the bitfield type
static_assert(XKBLAS_DEVICES_MAX <= 16);
typedef uint16_t memory_replicates_bitfield_t;

/* a memory allocation */
class MemoryReplicateAllocation {

    public:

        /* address returned by the allocator */
        const uintptr_t allocation;

        /* the replicate view */
        const memory_replicate_view_t view;

    public:
        MemoryReplicateAllocation(uintptr_t allocation, uintptr_t addr, int ld) : allocation(allocation), view(addr, ld) {}
        virtual ~MemoryReplicateAllocation() {}

}; /* MemoryReplicateAllocation */

/* a host replicate on a device */
class MemoryReplicate
{
    public:

        /* List of allocations for this device replicate.
         * A device may have several allocations (and allocation) of the same 'host memory'
         * For instance, in the following case scenario where blocks are read in order
         *  ._______________________.
         *  |           |           |
         *  |    (1)    |    (2)    |
         *  |___________|___________|
         *  |           |           |
         *  |    (3)    |    (4)    |
         *  .___________|___________.
         *
         *  - (1)           - read a tile               (allocation 1)
         *  - (2)           - read a tile               (allocation 2)
         *  - (3)           - read a tile               (allocation 3)
         *  - (4)           - read a tile               (allocation 4)
         *  - (1,2,3,4)     - read all tiles at once    (no continuous allocation...)
         *
         *  As BLAS requires a single continuous allocation per matrix, we are
         *  fucked and have to reallocate on the 5-th access
         */
        std::vector<MemoryReplicateAllocation *> allocations;

    public:
        MemoryReplicate() : allocations() {}
        MemoryReplicate(const MemoryReplicate & r) : allocations(r.allocations) {}
        ~MemoryReplicate() {}

}; /* MemoryReplicate */

/* see https://stackoverflow.com/questions/68795092/how-to-copy-a-built-in-array-via-copy-constructor */
class MemoryReplicates
{
    public:
        MemoryReplicate array[XKBLAS_DEVICES_MAX+1];

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

    public:

        /* a new memory block, assume it is valid on the host */
        MemoryBlock(const memory_view_t & v) :
            host_view(v),
            replicates(),
            valid(0)
        {}

        /* a block from splitting an existing one */
        MemoryBlock(
            const MemoryBlock & block
        ) :
            host_view(block.host_view),
            replicates(block.replicates),
            valid(block.valid)
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

            /* memory block in the tree (WARNING : this is mutable outside a 'lock' section) */
            MemoryBlock * block;

            /* The region of this block (intersectoin of the access with the tree node) */
            Region region;

            /* copy of the host view */
            memory_view_t host_view;

            /* dst device */
            int8_t dst_device_global_id;

            /* copy of the replicate view */
            MemoryReplicateAllocation * dst_replicate;

            /* source device */
            int8_t src_device_global_id;

            /* copy of the replicate view */
            MemoryReplicateAllocation * src_replicate;

        public:

            BlockInfo(MemoryBlock * b, Region & r) :
                block(b),
                region(r),
                host_view(b->host_view),
                dst_device_global_id(-1),
                dst_replicate(nullptr),
                src_device_global_id(-1),
                src_replicate(nullptr)
            {}

            virtual ~BlockInfo() {}

    }; /* BlockInfo */


   public:

       /* different search type */
       enum Type : uint8_t {
           INSERTING_BLOCKS     = 0,
           SEARCH_FOR_BLOCKS    = 1,
           SEARCH_OWNER         = 2,
       };

   public:

       /////////////////////////////////
       // used in all types of search //
       /////////////////////////////////

       /* type of search performing */
       Type type;

        /* device global id, on which we are looking for invalid blocks or validating blocks */
       const uint8_t device_global_id;

       //////////////////////////////////////////////////////
       // used if type == INSERTING_BLOCKS or SEARCH_OWNER //
       //////////////////////////////////////////////////////

       /* the access being inserted / intersected */
       Access * access;

       ///////////////////////////////////////
       // used if type == SEARCH_FOR_BLOCKS //
       ///////////////////////////////////////

       /*
        * list of blocks for the current access.
        * The set { b.region / b in blocks_info } is a partition of the space represented by access->region
        */
       std::vector<BlockInfo> blocks_info;

       ///////////////////////////////////
       // used if type == SEARCH_OWNER  //
       ///////////////////////////////////

       /* the memory resident size valid per device */
        uint64_t owns[XKBLAS_DEVICES_MAX];

   public:
       KMemoryTreeNodeSearch() : KMemoryTreeNodeSearch(0) {}

       KMemoryTreeNodeSearch(
           uint8_t devid
       ) :
           type(INSERTING_BLOCKS),
           device_global_id(devid),
           access(nullptr),
           blocks_info(),
           owns()
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

       void
       prepare_who_owns(Access * a)
       {
           this->access = a;
           this->type = SEARCH_OWNER;
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
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            XKBLAS_IMPL("inserted region %dx%d", this->region[0].length(), this->region[1].length());
        }

        void
        on_shrink(const Interval & interval, int k)
        {
            XKBLAS_WARN("shrinked region (%dx%d) on dimension %d to %d",
                this->region[0].length(),
                this->region[1].length(),
                k,
                interval.length()
            );
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

        /**
         * The passed access is intersecting with 'this'
         */
        inline void
        on_intersect(
            Search & search,
            const Region & region,
            const access_mode_t mode
        ) {
            /* intersecting against 'region' that had been inserted previously,
             * so 'this' is a sub-block of 'region' */
            assert(region.includes(this->region));

            switch (search.type)
            {
                case (Search::Type::SEARCH_FOR_BLOCKS):
                {
                    # pragma message(TODO "Manage case if another fetch is already going in parallel")
                    search.blocks_info.push_back(BlockInfo(&(this->block), this->region));
                    break ;
                }

                case (Search::Type::SEARCH_OWNER):
                {
                    # pragma message(TODO "use the actual number of devices instead of 'XKBLAS_DEVICES_MAX'")
                    static_assert (XKBLAS_DEVICES_MAX <= 64);
                    for (int device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
                        if (this->block.valid & (1 << device_global_id))
                            search.owns[device_global_id] += this->region.size();
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
            fprintf(f, "\\\\ block size (m, n)=(%d, %d) - ld=%d", this->block.host_view.bs_m, this->block.host_view.bs_n, this->block.host_view.ld);
            fprintf(f, "\\\\ tile (m, n)=(%d, %d)",  this->block.host_view.tm,   this->block.host_view.tn);

         // for (uint8_t device_global_id = 0 ; device_global_id < ctx->drivers.devices.n ; ++device_global_id)
            for (uint8_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX+1 ; ++device_global_id)
            {
                const int devbit = (1 << device_global_id);
                fprintf(f, "\\\\ dev %d - valid=%d",
                    device_global_id,
                    this->block.valid    & devbit ? 1 : 0
                );
            }
        }

}; /* KMemoryTreeNode */

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

        //////////////////////////
        //  STATIC FUNCTIONS    //
        //////////////////////////
        static void
        fetch_callback(const void * args[XKBLAS_STREAM_CALLBACK_ARGS_MAX])
        {
            assert(XKBLAS_STREAM_CALLBACK_ARGS_MAX >= 4);

            XKBLAS_DEBUG("  Completed a transfer!");

            ThreadWorker    * worker  =    (ThreadWorker *) args[0];
            xkblas_driver_t * driver  = (xkblas_driver_t *) args[1];
            xkblas_device_t * device  = (xkblas_device_t *) args[2];
            Task            * task    =            (Task *) args[3];

            /* a fetch completed */
            if (task->fetched() == TASK_STATE_DATA_FETCHED)
            {
                /* the task kernel is ready for execution */
                xkblas_device_task_access_fetched(worker, driver, device, task);
                # pragma message(TODO "Here, we are not polling the offloader kernel streams... Do we want to ?")
            }
        }

        //////////////
        //  METHODS //
        //////////////
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

        //////////////////////////////////////
        //  DECIDE SRC DEVICE WHEN FETCHING //
        //////////////////////////////////////

        static inline void
        fetch_access_find_source(
            Access * access,
            BlockInfo & info
        ) {
            const MemoryBlock * block = info.block;

            /* not valid on any device, use host to copy */
            if (block->valid == 0)
            {
                XKBLAS_DEBUG("No valid block... assuming host is valid");
                info.src_device_global_id   = HOST_DEVICE_GLOBAL_ID;
                info.src_replicate    = NULL;
            }
            else
            {
                // TODO : take the device with the smallest id, instead,
                // maybe try to balance the workload between GPU
                int src = __builtin_ffs(block->valid) - 1;
                assert(src >= 0);

                // TODO : currently always taking the first allocation
                int allocation_id = 0;
                MemoryReplicateAllocation * allocation = block->replicates.array[src].allocations[allocation_id];
                assert(allocation);

                /* set 'src' device info */
                info.src_device_global_id   = src;
                info.src_replicate    = allocation;

                assert(info.src_replicate);
            }
        }


        static inline void
        fetch_access_block_info_copy(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            ThreadWorker * worker,
            BlockInfo & info
        ) {
            XKBLAS_INFO("-- Copying from %d to %d --", info.src_device_global_id, info.dst_device_global_id);

            assert(info.dst_replicate || info.src_replicate);

            /* increment fetch counter */
            task->fetching();

            /* callback setup */
            assert(XKBLAS_STREAM_CALLBACK_ARGS_MAX >= 4);
            xkblas_stream_callback_t callback;
            callback.func = fetch_callback;
            callback.args[0] = worker;
            callback.args[1] = driver;
            callback.args[2] = device;
            callback.args[3] = task;

            /* host replicate view if no allocation were found */
            memory_replicate_view_t host_replicate_view(info.host_view.begin_addr(), info.host_view.ld);

            /* launch asynchronous copy */
            xkblas_stream_instruction_submit_copy(
                driver,
                device,
                info.host_view,
                info.dst_device_global_id,
                info.dst_replicate ? info.dst_replicate->view : host_replicate_view,
                info.src_device_global_id,
                info.src_replicate ? info.src_replicate->view : host_replicate_view,
                callback
            );
        }

        ////////////////////////
        //  FETCH ON THE HOST //
        ////////////////////////
        inline void
        fetch_on_host_access(
            ThreadWorker * worker,
            Task * task,
            Access * access
        ) {
            assert(access->mode & ACCESS_MODE_R);

            # if !USE_CUDA
            XKBLAS_FATAL("Only supporting CUDA driver for D2H transfers");
            # endif

            # pragma message(TODO "Instead, get the driver or the func associated to the 'src' device")
            xkblas_driver_t * driver = xkblas_driver_get(XKBLAS_DRIVER_TYPE_CUDA);
            assert(driver);

            Search search(HOST_DEVICE_GLOBAL_ID);
            this->lock();
            {
                /* find all blocks that intersects with that access */
                search.prepare_search_blocks();
                this->intersect(search, access->region, access->mode);
                assert(search.blocks_info.size() >= 1);

                /* launch fetch on each device */
                for (BlockInfo & info : search.blocks_info)
                {
                    MemoryBlock * block = info.block;

                    /* not valid on any device, then assume valid on the host */
                    if (block->valid == 0)
                        continue ;

                    /* copy to host */
                    info.dst_device_global_id = HOST_DEVICE_GLOBAL_ID;
                    info.dst_replicate       = NULL;

                    /* copy from device */
                    this->fetch_access_find_source(access, info);
                }
            }
            this->unlock();

            /* launch fetch on each device */
            for (BlockInfo & info : search.blocks_info)
            {
                assert(info.src_device_global_id != info.dst_device_global_id);

                xkblas_device_t * device = xkblas_device_get(info.src_device_global_id);
                assert(device);

                fetch_access_block_info_copy(driver, device, task, worker, info);
            }
        }

        task_state_t
        fetch_on_host(ThreadWorker * worker, Task * task)
        {
            assert(ThreadWorker::get() == worker);

            XKBLAS_DEBUG("Launching async fetch of access %p", access);
            task->fetching();

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
                this->fetch_on_host_access(worker, task, task->accesses + i);

            return task->fetched();
        }

        ////////////////////////
        //  FETCH ON A DEVICE //
        ////////////////////////

        inline bool
        fetch_access_find_allocation(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            std::vector<BlockInfo> & blocks_info
        ) {
            int j = 0;
            int nallocations = blocks_info[0].block->replicates.array[device->global_id].allocations.size();
            int nblocks = blocks_info.size();

            /* for each allocation of the block 0 */
            while (j < nallocations)
            {
                MemoryReplicateAllocation * rj = blocks_info[0].block->replicates.array[device->global_id].allocations[j];

                /* for each other blocks */
                int i = 1;
                while (i < nblocks)
                {
                    /* for each allocation of other blocks */
                    int nallocations = blocks_info[i].block->replicates.array[device->global_id].allocations.size();
                    for (int k = 0 ; k < nallocations ; ++k)
                    {
                        /* this block has a view with the same allocation, check next block */
                        MemoryReplicateAllocation * rk = blocks_info[i].block->replicates.array[device->global_id].allocations[k];
                        if (rj->allocation == rk->allocation)
                        {
                            blocks_info[i].dst_replicate = rk;
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
                blocks_info[0].dst_replicate = rj;
                return 0;

next_view:
                ++j;
                continue ;
            }

            return 1;
        }

        /* return the left-most and upper-most block of the partition */
        inline int
        fetch_access_get_first_block(
            std::vector<BlockInfo> & blocks_info
        ) {
            const int nblocks = blocks_info.size();
            int j = 0;

            for (int i = 1 ; i < nblocks ; ++i)
            {
                BlockInfo & bi = blocks_info[i];
                BlockInfo & bj = blocks_info[j];
                for (int k = 0 ; k < K ; ++k)
                    if (bi.region[k].a < bj.region[k].a)
                        j = i;
            }

            return j;
        }

        inline void
        fetch_access_allocate(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            std::vector<BlockInfo> & blocks_info
        ) {
            // TODO : memory allocation may spinlock for a while if the
            // device memory is full... in such case, we probably want
            // to release the memory-tree lock, and restart all that
            // shit over again once memory got allocated

            /* allocate continuous memory for that access */
            uint64_t  size = access->host_view.bs_n * access->host_view.bs_m * access->host_view.sizeof_type;
            uintptr_t addr = (uintptr_t) xkblas_memory_allocate(driver, device, size);
            int         ld = access->host_view.bs_n;
            XKBLAS_DEBUG("  allocated at %p for size %zu", (void *) addr, size);
            assert(addr);

            /* retrieve upper left corner */
            const int blockID = this->fetch_access_get_first_block(blocks_info);
            const BlockInfo & corner = blocks_info[blockID];

            /* add a view to it in tree memory blocks */
            for (BlockInfo & info : blocks_info)
            {
                /* compute distance from corner */
                int d[K];
                for (int k = 0 ; k < K ; ++k)
                {
                    d[k] = info.region[k].a - corner.region[k].a;
                    assert(d[k] >= 0);
                }

                // TODO : offset the allocation address
                static_assert(K == 2);
                const uintptr_t begin_addr = addr + d[0]*ld + d[1];

                info.dst_replicate = new MemoryReplicateAllocation(addr, begin_addr, ld);
                assert(info.dst_replicate);
                info.block->replicates.array[device->global_id].allocations.push_back(info.dst_replicate);
            }
        }

        inline void
        fetch_access_set_device_view(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            std::vector<BlockInfo> & blocks_info
        ) {
            // we currently set the access view as the 'left-most' and 'upper-most' tile
            // (i.e with the smallest address - corresponding to the begining of this allocation)
            const int blockID = this->fetch_access_get_first_block(blocks_info);
            const BlockInfo & info = blocks_info[blockID];
            assert(info.dst_replicate);

            access->device_view.addr = info.dst_replicate->view.addr;
            access->device_view.ld   = info.dst_replicate->view.ld;
        }

        void
        fetch_access(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access
        ) {
            // TODO : bad design
            ThreadWorker * worker = ThreadWorker::get();

            // do not use this function to fetch onto the host
            assert(device->global_id != HOST_DEVICE_GLOBAL_ID);

            /* list of invalid blocks on that device */
            Search search(device->global_id);
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
                //      - TODO and find the 'source' device for fetching - calling the driver with the 'valid' bitmask
                //          (and the driver can return 'source' == 'device' if valid on the current device)
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                # pragma message(TODO "Step (1) and (2) could be merged to only lock/search once")
                search.prepare_search_blocks();
                this->intersect(search, access->region, access->mode);
                assert(search.blocks_info.size() >= 1);

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                // 3) check that there exists a continuous allocation for that access
                //      if no, do a new allocation and add the view to each block
                //    if write mode is set, invalidate every other copies, and release every other allocations
                //    if read mode is set, submit all memcpy
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                // find a continuous allocation
                if (this->fetch_access_find_allocation(driver, device, task, access, search.blocks_info))
                {
                    // if none, allocate
                    XKBLAS_DEBUG("No continuous allocation found for the access, reallocating and creating a new view");
                    this->fetch_access_allocate(driver, device, task, access, search.blocks_info);
                }

                // set the device view of that access (that will be used by the task kernel)
                this->fetch_access_set_device_view(driver, device, task, access, search.blocks_info);

                // if read mode is set, prepare memcpy
                if (access->mode & ACCESS_MODE_R)
                {
                    for (BlockInfo & info : search.blocks_info)
                    {
                        // create dst view
                        info.dst_device_global_id = device->global_id;
                     // info.dst_replicate is already set in the 'fetch_access_find_allocation' routine
                        assert(info.dst_replicate);

                        // create src view
                        this->fetch_access_find_source(access, info);

                        # pragma message(TODO "currently always forcing H2D transfers, remove these 3 lines to enable D2D transfers")
                        info.src_device_global_id   = HOST_DEVICE_GLOBAL_ID;
                        info.src_replicate         = NULL;
                    }
                }

                // if write mode, set the valid bits
                if (access->mode & ACCESS_MODE_W)
                {
                    for (BlockInfo & info : search.blocks_info)
                    {
                        /* set valid bits (even though the data is not copied
                         * yet, as we are writing, there is no other task
                         * accessing that block until its copy + kernel
                         * completed.... so thats ok) */
                        info.block->valid = (1 << device->global_id);

                        /* release all other allocation block that are now invalid! */
                        MemoryReplicate & replicate = info.block->replicates.array[device->global_id];
                        if (replicate.allocations.size() > 1)
                        {
                            XKBLAS_WARN("Releasing allocations from a block...");
                            # pragma message(TODO "Memory leak on the device ! need to free each allocation once supported by the allocator")
                            replicate.allocations.clear();
                            replicate.allocations.push_back(info.dst_replicate);
                        }
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
                    if (info.dst_device_global_id == info.src_device_global_id &&
                        info.dst_replicate == info.src_replicate
                    ) {
                        XKBLAS_WARN("Tried to copy the same block from/to the same device %u", info.dst_device_global_id);
                        continue ;
                    }

                    fetch_access_block_info_copy(driver, device, task, worker, info);
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
            assert(driver);
            assert(device);

            # pragma message(TODO "continuous blocks on the same device "       \
                    "could be detected here and fetched with a single request")

            /* increase task 'fetching' counter so it does not get ready early
             * (eg before we processed all accesses bellow)
             */
            task->fetching();

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
                this->fetch_access(driver, device, task, task->accesses + i);

            return task->fetched();
        }

        //////////////////////////////////////////////////////////
        //  FIND THE DEVICE THAT OWNS MOST OF THE PASSED ACCESS //
        //////////////////////////////////////////////////////////
        uint32_t
        who_owns(Access * access)
        {
            Search search;
            search.prepare_who_owns(access);

            this->lock();
            {
                this->intersect(search, access->region, access->mode);
            }
            this->unlock();

            /* retrieve the device id that owns the most memory bytes */
            # pragma message(TODO "maybe use the actual number of devices instead of 'XKBLAS_DEVICES_MAX'")
            uint32_t device_id = 0;
            for (int i = 1 ; i < XKBLAS_DEVICES_MAX ; ++i)
            {
                if (search.owns[device_id] < search.owns[i])
                    device_id = i;
            }
            XKBLAS_DEBUG("memory-tree.who_owns() returned %d", device_id);

            return device_id;
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
