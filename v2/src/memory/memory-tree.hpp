#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "device/consts.h"
# include "device/device.h"
# include "device/driver.h"
# include "device/task.hpp"
# include "logger/logger.h"
# include "logger/todo.h"
# include "sync/kinterval-btree.hpp"
# include <cstdint>


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

    # if 0
    using Region = Intervals<K>;

    public:

        /* the memory-tree region view */
        const Region region;

        /* a view of the memory block */
        const memory_view_t block_view;

        /* the replicate view */
        const memory_replicate_view_t replicate_view;

        /* devices on which the block is valid */
        const memory_replicates_bitfield_t valid;

    public:

        KMemoryBlockReplicateFetch(
            const Region & r,
            const memory_view_t & bview,
            const memory_replicate_view_t & rview,
            const memory_replicates_bitfield_t v
        ) :
            region(r),
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

        # endif
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

    public:

        /* different search type */
        enum Type : uint8_t {
            INSERTING_BLOCKS    = 0,
            SEARCH_FOR_BLOCKS   = 1,
            VALIDATE_BLOCKS     = 2
        };

    public:

        /////////////////////////////////
        // used in all types of search //
        /////////////////////////////////

        /* type of search performing */
        Type type;

        /* device global id, on which we are looking for invalid blocks or validating blocks */
        const uint8_t device_global_id;

        //////////////////////////////////////
        // used if type == INSERTING_BLOCKS //
        //////////////////////////////////////

        /* the access being inserted / intersected */
        Access * access;

        ///////////////////////////////////////////////
        // used if type == SEARCH_FOR_BLOCKS //
        ///////////////////////////////////////////////

        /* list of invalid replicate */
        std::vector<KMemoryBlockReplicateFetch<K>> blocks;

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
        prepare_validate(const Region & r)
        {
            this->type = VALIDATE_BLOCKS;
            this->region.copy(r);
        }

}; /* KMemoryTreeNodeSearch */


template <int K>
class KMemoryTreeNode : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node {

    using ReplicateFetch = KMemoryBlockReplicateFetch<K>;
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
                    // this         == un noeud de l'arbre
                    // this->block  == 'MemoryBlock' - representé par le noeud, avec
                    //      - vue host
                    //      - list des replicats
                    // this->region == vue host 2D de 'this->block'
                    //       region == vue host 2D de l'accès qu'on est en train de fetch


                    # if 0
                    if (this->block.fetching & devbit)
                    {
                        // TODO : this block is being fetched by another access
                        // need to notify the task performing 'access' when its done
                        return ;
                    }
                    this->block.fetching |= devbit;

                    /* add this block to the fetching list */
                    search.blocks.push_back(
                        ReplicateFetch(
                            this->region.intersection(region),
                            this->block.view,
                            this->block.replicates[search.device_global_id],
                            this->block.valid
                        )
                    );
                    # endif
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

            # pragma message(TODO "Reimplement with multiple view per replicate")
            # if 0
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
            # endif
        }

}; /* KMemoryTreeNode */

template <int K>
class KMemoryTree : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>> {

    using ReplicateFetch = KMemoryBlockReplicateFetch<K>;
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

                /* ensure it is represented in the memory tree */
                search.prepare_insert(access);
                this->lock();
                {
                    this->insert(search, access->region, access->mode);
                }
                this->unlock();

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

                # pragma message(TODO "merge 'ReplicateFetch' on continuous "   \
                        "memory addresses - for now, just perform one data "    \
                        "transfer per block")

                // TODO : also get the list of 'valid blocks' and always
                //  - reallocate the access
                //  - memcpy (local) valid blocks
                //  - memcpy (remote) invalid blocks
                //
                //  But it raises an issue : if the block is valid, how not to
                //  synchronize another concurrent consumer on the same device ?

                if (search.blocks.size() > 1)
                    XKBLAS_IMPL("Several memory blocks are invalid for the same access");

                // TODO : lock + intersect + unlock - to find all blocks that form the access->region
                //      - mark fetching bit
                //      - and find the 'source' device for fetching - calling the driver with the 'valid' bitmask
                //          (and the driver can return 'source' == 'device' if valid on the current device)

                // TODO : check that there exists a continuous allocation for that access
                    // TODO : if no, do a new allocation - and lock + intersect + unlock again the tree, to add a new view to each block

                // TODO : do fetches
                    // TODO : once fetch completed - lock + intersect + unlock






# if 0
                /* initiate fetching of each invalid block for that access */
                for (ReplicateFetch & fetch : search.blocks)
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

                            // TODO : ensure each 'ReplicateFetch' are continuous in memory, else free/reallocate/memmove accordingly
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
