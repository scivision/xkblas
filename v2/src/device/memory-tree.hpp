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

# pragma message(TODO "'fetch' implementation could be optimize")

# pragma message(TODO "merge 'Replicate' on continuous "   \
        "memory addresses - for now, just perform one data "    \
        "transfer per block")

# pragma message(TODO "Nest classes into a 'KMemory' templated class - corresponding to a global view of the memory in 'K' dimensions")

// if greater than 16, then gotta increase the bitfield type
static_assert(XKBLAS_DEVICES_MAX <= 16);
typedef uint16_t memory_replicates_bitfield_t;

class MemoryBlock {

    public:

        /* host memory view of that block */
        memory_view_t view;

        /* per device replicate info */
        memory_replicate_t replicates[XKBLAS_DEVICES_MAX];

        /* if i-th bit is set, the i-th device has a view with a valid copy */
        volatile memory_replicates_bitfield_t valid;

        /* if i-th bit is set, the i-th device has a view that is fetching */
        volatile memory_replicates_bitfield_t fetching;

    public:

        /* a new memory block, assume it is valid on the host (device id = 0) */
        MemoryBlock(const memory_view_t & v) :
            view(v),
            replicates(),
            valid(0),
            fetching(0)
        {
            # pragma message(TODO "Reimplementfor each view of the replicate")
            # if 0
            const int host_devid = 0;
            this->valid = (1 << host_devid);
            this->replicates[host_devid].addr = view.begin_addr();
            this->replicates[host_devid].LD = view.LD;
            # endif
        }

        /* a block from splitting an existing one */
        MemoryBlock(
            const MemoryBlock & block
        ) :
            view(block.view),
            replicates(block.replicates),
            valid(block.valid),
            fetching(block.fetching)
        {}

        virtual ~MemoryBlock() {}

}; /* MemoryBlock */


/* an on-going memory block fetch */
template <int K>
class KMemoryBlockReplicateFetch {

    using Region = Intervals<K>;

    public:

        /* the memory-tree region view */
        const Region region;

        /* host view of the memory block */
        const memory_view_t host_view;

        /* destination device */
        uint8_t dst_device_global_id;

        /* the replicate */
        const memory_replicate_t dst_replicate;

        /* the replicate view to use for fetching (in [0 .. replicate.view.size()[) */
        int dst_replicate_view_id;

        /* source device */
        uint8_t src_device_global_id;

        /* source device replicate view */
        memory_replicate_view_t src_device_view;

    public:

        KMemoryBlockReplicateFetch(
            const uint8_t device_global_id,
            const Region & r,
            const MemoryBlock & block
        ) :
            region(r),
            host_view(block.view),
            dst_device_global_id(device_global_id),
            dst_replicate(block.replicates[device_global_id]),
            dst_replicate_view_id(-1),
            src_device_global_id(-1),
            src_device_view()
        {}

        virtual ~KMemoryBlockReplicateFetch() {}

        uint64_t
        size(void) const
        {
            return this->host_view.bs_m * this->host_view.bs_n * this->host_view.sizeof_type;
        }

}; /* KMemoryBlockReplicateFetch */


# pragma message(TODO "Currently memory tree is cut on 'write' accesses, but we "   \
        "therefore loose allocation information on devices... Two ideas, "          \
        "whether: (1) do not cut, keeping all blocks and invalidating them or "     \
        "(2) free the device blocks in the destructor of a KMemoryTreeNode")


/* storage passed when searching in the tree */
template <int K>
class KMemoryTreeNodeSearch {

    using Access = KMemoryAccess<K>;
    using Region = Intervals<K>;
    using BlockReplicateFetch = KMemoryBlockReplicateFetch<K>;

    public:

        /* different search type */
        enum Type : uint8_t {
            INSERTING_BLOCKS    = 0,
            SEARCH_FOR_BLOCKS   = 1,
            INSERT_ALLOCATION   = 2,
            VALIDATE_BLOCKS     = 3
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
        // used if type == INSERTING_BLOCKS  or type == INSERT_ALLOCATION //
        ////////////////////////////////////////////////////////////////////

        /* the access being inserted / intersected */
        Access * access;

        ///////////////////////////////////////////////
        // used if type == SEARCH_FOR_BLOCKS //
        ///////////////////////////////////////////////

        /* list of invalid replicate */
        std::vector<BlockReplicateFetch> blocks;

        ///////////////////////////////////////////////
        // used if type == INSERT_ALLOCATION //
        ///////////////////////////////////////////////
        uintptr_t allocation;

        /////////////////////////////////////
        // used if type == VALIDATE_BLOCKS //
        /////////////////////////////////////
        Region region;

    public:

        KMemoryTreeNodeSearch(uint8_t devid) : device_global_id(devid), access(nullptr), blocks(), region() {}
        virtual ~KMemoryTreeNodeSearch() {}

        void
        prepare_insert(Access * a)
        {
            this->type = INSERTING_BLOCKS;
            this->access = a;
        }

        void
        prepare_search_blocks(void)
        {
            this->type = SEARCH_FOR_BLOCKS;
            this->blocks.clear();
        }

        void
        prepare_insert_allocation(Access * access, uintptr_t ptr)
        {
            assert(this->access == access);
            this->type = INSERT_ALLOCATION;
            this->allocation = ptr;
        }

        void
        prepare_validate(const Region & r)
        {
            this->type = VALIDATE_BLOCKS;
            this->region.copy(r);
        }

}; /* KMemoryTreeNodeSearch */


template <int K>
class KMemoryTreeNode : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node {

    using BlockReplicateFetch = KMemoryBlockReplicateFetch<K>;
    using Region = Intervals<K>;
    using Base = typename KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node;
    using Node = KMemoryTreeNode<K>;
    using Access = KMemoryAccess<K>;
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
                    // this         == a node in the tree
                    // this->block  == the 'MemoryBlock' represented by the node with
                    //      - the host view
                    //      - the list of view on each devices replicate
                    // this->region == a 2D host view of 'this->block'
                    //       region == a 2D host view of the access we are intersecting against

                    # pragma message(TODO "Manage case if another fetch is already going in parallel")
                    assert(!(this->block.fetching & devbit));
                    this->block.fetching |= devbit;

                    search.blocks.push_back(
                        BlockReplicateFetch(
                            search.device_global_id,
                            this->region,
                            this->block
                        )
                    );

                    break ;
                }

                case (Search::Type::INSERT_ALLOCATION):
                {
                    # pragma message(TODO "search.allocation is the allocation start, need to be somehow offset here as well")
                    this->block.replicates[search.device_global_id].views.push_back(
                        memory_replicate_view_t(search.allocation, search.access->host_view.bs_n)
                    );
                    break ;
                }

                case (Search::Type::VALIDATE_BLOCKS):
                {
                    assert(this->block.fetching & devbit);
                    assert(!(this->block.valid & devbit));
                    this->block.fetching &= ~(devbit);
                    this->block.valid |= devbit;
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
            fprintf(f, "\\\\ host-addr=%p", (void *) this->block.view.addr);
            fprintf(f, "\\\\ block size (m, n)=(%d, %d) - LD=%d", this->block.view.bs_m, this->block.view.bs_n, this->block.view.LD);
            fprintf(f, "\\\\ tile (m, n)=(%d, %d)",  this->block.view.tm,   this->block.view.tn);

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

template <int K>
class KMemoryTree : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>> {

    using BlockReplicateFetch = KMemoryBlockReplicateFetch<K>;
    using Base = KIntervalBtree<K, KMemoryTreeNodeSearch<K>>;
    using Node = KMemoryTreeNode<K>;
    using NodeBase = typename KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node;
    using Region = Intervals<K>;
    using Task = KTask<K>;
    using Access = KMemoryAccess<K>;
    using Search = KMemoryTreeNodeSearch<K>;

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
            Search search(device->global_id);
            const int devbit = (1 << device->global_id);

            /* increase task 'fetching' counter so it does not get ready early
             * (eg before we processed all accesses bellow)
             */
            task->fetching();

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
            {
                Access * access = task->accesses + i;

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                // 1) Ensure the access is represented in the memory tree
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                # pragma message(TODO "Step (1) and (2) could be merged to only lock/search once")
                search.prepare_insert(access);
                this->lock();
                {
                    this->insert(search, access->region, access->mode);
                }
                this->unlock();

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                // 2) lock + intersect + unlock - to find all blocks that form the access->region
                //      - mark fetching bit
                //      - and find the 'source' device for fetching - calling the driver with the 'valid' bitmask
                //          (and the driver can return 'source' == 'device' if valid on the current device)
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                # pragma message(TODO "Step (1) and (2) could be merged to only lock/search once")

                /* if the kernel reads that memory */
                if (access->mode & ACCESS_MODE_R)
                {
                    /* find invalid memory blocks on that device */
                    search.prepare_search_blocks();
                    this->lock();
                    {
                        this->intersect(search, access->region, access->mode);
                    }
                    this->unlock();
                }
                assert(search.blocks.size() >= 1);

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                // 3) TODO : check that there exists a continuous allocation for that access
                //      TODO : if no, do a new allocation - and lock + intersect + unlock, to add a new view to each block
                //////////////////////////////////////////////////////////////////////////////////////////////////////

                // just to be warn whenver this occurs, as it is fairly unlikely
                if (search.blocks.size() > 1)
                    XKBLAS_WARN("Several memory blocks are invalid for the same access");

                uintptr_t allocation = 0;
                int j = 0;
                int nviews = search.blocks[0].dst_replicate.views.size();
                int nblocks = search.blocks.size();

                /* for each view of the block 0 */
                while (j < nviews)
                {
                    /* get the view allocation */
                    allocation = search.blocks[0].dst_replicate.views[j].addr;

                    /* for each other blocks */
                    int i = 1;
                    while (i < nblocks)
                    {
                        /* for each view of other blocks */
                        int nviews = search.blocks[i].dst_replicate.views.size();
                        for (int k = 0 ; k < nviews ; ++k)
                        {
                            /* this block has a view with the same allocation, check next block */
                            if (allocation == search.blocks[i].dst_replicate.views[k].addr)
                            {
                                search.blocks[i].dst_replicate_view_id = k;
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
                    break ;

                next_view:
                    ++j;
                    continue ;
                }

                # pragma message(TODO "Allocation is always the start of a tile... need to offset it!!")

                if (allocation == 0)
                {
                    XKBLAS_DEBUG("No continuous allocation found for the access, reallocating and creating a new view");
                    allocation = (uintptr_t) xkblas_memory_allocate(driver, device, access->size());
                    assert(allocation);

                    XKBLAS_DEBUG("  allocated at %p", allocation);

                    search.prepare_insert_allocation(access, allocation);
                    this->lock();
                    {
                        this->intersect(search, access->region, access->mode);
                    }
                    this->unlock();
                }

                access->device_view.addr = allocation;
                access->device_view.LD = access->host_view.bs_n;

                /////////////////////////////////////////////////////////////////////////
                // 4) TODO : do fetches
                //     TODO : once fetch completed - lock + intersect + unlock
                /////////////////////////////////////////////////////////////////////////

                # pragma message(TODO "Maybe block is already valid, so no need to fetch it")
                for (BlockReplicateFetch & fetch : search.blocks)
                {
                    std::function<void()> callback = []() {
                        XKBLAS_DEBUG("  Completed a transfer!");
                    };

                    xkblas_stream_instruction_submit_copy(
                        driver,
                        device,
                        fetch.host_view,
                        fetch.dst_device_global_id,
                        fetch.dst_replicate.views[fetch.dst_replicate_view_id],
                        fetch.src_device_global_id,
                        fetch.src_device_view,
                        callback
                    );
                }

# if 0
                /* initiate fetching of each invalid block for that access */
                for (Replicate & fetch : search.blocks)
                {
                    assert(!(fetch.valid & devbit));

                    /* increase task 'fetching' counter : the kernel execution
                     * must wait for one fetch to complete */
                    task->fetching();

                    /* allocate the replicate on the device if needed */
                    const bool require_allocation = (fetch.replicate_view.addr == 0);
                    if (require_allocation)
                    {
                        uint64_t size = fetch.size();
                        void* addr = NULL;
                        xkblas_memory_allocate(driver, device, &addr, size);
                    }

                    // TODO : launch asynchronous fetch here, currently assume
                    // data is always valid and run activation callback
                    // straight-ahead

                    // TODO : call this code (aka activation) on fetch completion
                    {
                        /* update the valid and fetching bits in the memory tree */
                        search.prepare_validate(fetch.region);
                        this->lock();
                        {
                            this->intersect(search, fetch.region, ACCESS_MODE_VOID);
                        }
                        this->unlock();

                        /* a fetch completed */
                        if (task->fetched() == TASK_STATE_DATA_FETCHED)
                        {
                            /* all data has been fetched */

                            // TODO : ensure each 'Replicate' are continuous in memory, else free/reallocate/memmove accordingly
                            // TODO : set 'access->device_view.addr' and 'access->device_view.LD'

                            /* the task kernel is ready for execution */
                            xkblas_device_task_fetched(driver, device, task);
                        }
                    }
                }
                # endif






            }

            return task->fetched();
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
