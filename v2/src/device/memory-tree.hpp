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
        const int ld;

    public:
        MemoryAllocation(uintptr_t addr, int ld) : addr(addr), ld(ld) {}
        virtual ~MemoryAllocation() {}

}; /* MemoryAllocation */

/* a host replicate on a device */
class MemoryReplicate
{
    public:
        /* List of views for this device replicate.
         * A device may have several views (and allocation) of the same 'host memory'
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

            /* dst allocation */
            int16_t dst_allocation_id;

            /* copy of the replicate view */
            MemoryAllocation * dst_allocation;

            /* dst memory view */
            memory_replicate_view_t dst_view;

            /* source device */
            int8_t src_device_global_id;

            /* src allocation */
            int16_t src_allocation_id;

            /* src memory view */
            memory_replicate_view_t src_view;

            /* copy of the replicate view */
            MemoryAllocation * src_allocation;

        public:

            BlockInfo(MemoryBlock * b, Region & r) :
                block(b),
                region(r),
                host_view(b->host_view),
                dst_device_global_id(-1),
                dst_allocation_id(-1),
                dst_allocation(nullptr),
                dst_view(),
                src_device_global_id(-1),
                src_allocation_id(-1),
                src_allocation(nullptr),
                src_view()
            {}

            virtual ~BlockInfo() {}

    }; /* BlockInfo */


   public:

       /* different search type */
       enum Type : uint8_t {
           INSERTING_BLOCKS     = 0,
           SEARCH_FOR_BLOCKS    = 1,
           SEARCH_OWNER         = 2,
           SET_VALID            = 3,
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

       /* list of blocks for the current access */
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

       void prepare_set_valid(void)
       {
           this->type = SET_VALID;
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

            if (mode & ACCESS_MODE_W)
                this->block.valid = (1 << search.device_global_id);
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

                case (Search::Type::SET_VALID):
                {
                    /* currently not cleaning-up the tree, so 'this->region' is
                     * always included in 'region' */
                    this->block.valid |= (1 << search.device_global_id);
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

    private:

        typedef struct  FetchInfo
        {
            KMemoryTree * memtree;
            Region region;
            int device_global_id;

            FetchInfo(KMemoryTree * t, Region & r, int devid) : memtree(t), region(r), device_global_id(devid) {}
            ~FetchInfo() {}
        }              FetchInfo;

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
            assert(XKBLAS_STREAM_CALLBACK_ARGS_MAX >= 5);

            XKBLAS_DEBUG("  Completed a transfer!");

            xkblas_driver_t * driver  = (xkblas_driver_t *) args[0];
            xkblas_device_t * device  = (xkblas_device_t *) args[1];
            Task            * task    =            (Task *) args[2];
            FetchInfo       * fetch   =       (FetchInfo *) args[3];

            /* a fetch completed */
            if (task->fetched() == TASK_STATE_DATA_FETCHED)
            {
                /* the task kernel is ready for execution */
                xkblas_device_task_access_fetched(driver, device, task);
                # pragma message(TODO "Here, we are not polling the offloader kernel streams... Do we want to ?")
            }

            /* set validity in the memory tree for that device */
            fetch->memtree->set_valid(fetch->region, fetch->device_global_id);
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

        ///////////////////////////////////////////////
        //  Set a block validity bits after fetching //
        ///////////////////////////////////////////////
        void set_valid(
            Region & region,
            int device_global_id
        ) {
            # pragma message(TODO "we also want to invalidate other allocations!! an allocation with the wrong version may be picked")

            Search search(device_global_id);
            search.prepare_set_valid();
            this->lock();
            {
                this->intersect(search, region, ACCESS_MODE_VOID);
            }
            this->unlock();
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
                info.src_allocation_id      = -1;
                info.src_allocation         = NULL;
                info.src_view.addr          = access->host_view.addr;
                info.src_view.ld            = access->host_view.ld;
            }
            else
            {
                // TODO : take the device with the smallest id, instead,
                // maybe try to balance the workload between GPU
                int src = __builtin_ffs(block->valid) - 1;
                assert(src >= 0);

                // TODO : currently always taking the first allocation
                int allocation_id = 0;
                MemoryAllocation * allocation = block->replicates.array[src].allocations[allocation_id];
                assert(allocation);

                /* set 'src' device info */
                info.src_device_global_id   = src;
                info.src_allocation_id      = allocation_id;
                info.src_allocation         = allocation;
                info.src_view.addr          = allocation->addr;
                info.src_view.ld            = allocation->ld;

                assert(info.src_allocation_id >= 0);
                assert(info.src_allocation_id < block->replicates.array[info.src_device_global_id].allocations.size());
            }
        }

        ////////////////////////
        //  FETCH ON THE HOST //
        ////////////////////////
        inline void
        fetch_on_host_access(
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

                    /* need to transfer D2H */
                    task->fetching();

                    /* copy to host */
                    info.dst_device_global_id   = HOST_DEVICE_GLOBAL_ID;
                    info.dst_view.addr          = access->host_view.addr;
                    info.dst_view.ld            = access->host_view.ld;

                    /* copy from device */
                    this->fetch_access_find_source(access, info);


                    xkblas_device_t * device = xkblas_device_get(info.src_device_global_id);
                    assert(device);

                    /* callback setup */
                    assert(XKBLAS_STREAM_CALLBACK_ARGS_MAX >= 3);
                    xkblas_stream_callback_t callback;
                    callback.func = fetch_callback;
                    callback.args[0] = driver;
                    callback.args[1] = device;
                    callback.args[2] = task;
                    callback.args[3] = new FetchInfo(this, info.region, device->global_id);

                    // the current thread is the memory async copy thread, NOT THE DEVICE THREAD !!
                    // So it creates concurrency on stream instructions allocation/submission

                    /* launch asynchronous copy */
                    # if 0
                    xkblas_stream_instruction_submit_copy(
                        driver,
                        device,
                        info.host_view,
                        info.dst_device_global_id,
                        info.dst_view,
                        info.src_device_global_id,
                        info.src_view,
                        callback
                    );
                    # else
                    task->fetched();
                    # endif
                }
            }
            this->unlock();
        }

        task_state_t
        fetch_on_host(Task * task)
        {
            // assert(ThreadWorker::get() == xkblas_context_get()->memory_coherent_worker_thread);

            XKBLAS_DEBUG("Launching async fetch of access %p", access);
            task->fetching();

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
                this->fetch_on_host_access(task, task->accesses + i);

            return task->fetched();
        }

        ////////////////////////
        //  FETCH ON A DEVICE //
        ////////////////////////

        void
        fetch_access(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access
        ) {
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
                    int         ld = access->host_view.bs_n;
                    XKBLAS_DEBUG("  allocated at %p for size %zu", (void *) addr, size);
                    assert(addr);

                    allocation = new MemoryAllocation(addr, ld);
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
                access->device_view.ld   = allocation->ld;

                /* set the copy infos if reading */
                if (access->mode & ACCESS_MODE_R)
                {
                    for (BlockInfo & info : search.blocks_info)
                    {
                        /* parameters setup */

                        /* host_view set already */

                        /* create dst view */
                        {
                            info.dst_device_global_id = device->global_id;
                         // info.dst_allocation_id = set already
                            assert(info.dst_allocation_id >= 0);
                            assert(info.dst_allocation_id < info.block->replicates.array[info.dst_device_global_id].allocations.size());

                            MemoryAllocation * dst_allocation = info.block->replicates.array[info.dst_device_global_id].allocations[info.dst_allocation_id];
                            assert(dst_allocation);
                            info.dst_view.addr = dst_allocation->addr;
                            info.dst_view.ld   = dst_allocation->ld;
                        }

                        /* create src view */
                        {
                            this->fetch_access_find_source(access, info);
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
                        info.dst_allocation_id == info.src_allocation_id
                    ) {
                        XKBLAS_DEBUG("Tried to copy the same block from/to the same device %u", info.dst_device_global_id);
                        continue ;
                    }

                    /* increment fetch counter */
                    task->fetching();

                    /* callback setup */
                    assert(XKBLAS_STREAM_CALLBACK_ARGS_MAX >= 4);
                    xkblas_stream_callback_t callback;
                    callback.func = fetch_callback;
                    callback.args[0] = driver;
                    callback.args[1] = device;
                    callback.args[2] = task;
                    callback.args[3] = new FetchInfo(this, info.region, device->global_id);

                    /* launch asynchronous copy */
                    XKBLAS_INFO(
                        "-- Copying from %d to %d --",
                        info.src_device_global_id, info.dst_device_global_id
                    );
                    // TODO : currently always forcing H2D transfers
                    // remove these 3 lines to enable D2D transfers
                    info.src_device_global_id = HOST_DEVICE_GLOBAL_ID;
                    info.src_view.addr = info.host_view.addr;
                    info.src_view.ld   = info.host_view.ld;

                    xkblas_stream_instruction_submit_copy(
                        driver,
                        device,
                        info.host_view,
                        info.dst_device_global_id,
                        info.dst_view,
                        info.src_device_global_id,
                        info.src_view,
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
