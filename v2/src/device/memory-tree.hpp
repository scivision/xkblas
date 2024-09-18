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
# include "sync/lockable.hpp"

# include <cstdint>
# include <functional>
# include <map>

# pragma message(TODO "'fetch' implementation could be optimize by reducing critical sections")

# pragma message(TODO "merge 'Replicate' on continuous "   \
        "memory addresses - for now, just perform one data "    \
        "transfer per block")

# pragma message(TODO "Nest classes into a 'KMemory' templated class - corresponding to a global view of the memory in 'K' dimensions")

typedef uint16_t memory_replicates_bitfield_t;
static_assert(sizeof(memory_replicates_bitfield_t) * 8 >= XKBLAS_DEVICES_MAX);

/* a memory allocation */
class MemoryReplicateAllocationView {

    public:

        /* address returned by the allocator */
        uintptr_t allocation;

        /* the address of that view in [allocation, allocation + allocation->size[ */
        memory_replicate_view_t view;

        /* list of tasks awaiting on that view to be transfered */
        std::vector<Task *> awaiting;

    public:

        MemoryReplicateAllocationView(
            const uintptr_t allocation,
            const uintptr_t addr,
            const int ld
        ) :
            allocation(allocation),
            view(addr, ld),
            awaiting()
        {}

        virtual ~MemoryReplicateAllocationView() {}

}; /* MemoryReplicateAllocationView */

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
         *
         *  The 'MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX' controls how many
         *  allocations of the same data may exists at most
         */
        # define MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX   (1)
        # define MEMORY_REPLICATE_ALLOCATION_VIEW_NONE   (MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX)
        MemoryReplicateAllocationView * allocations[MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX];
        volatile uint8_t nallocations;

        /* valid allocations */
        volatile memory_replicates_bitfield_t valid;

        /* fetching allocations */
        volatile memory_replicates_bitfield_t fetching;

        static_assert(sizeof(memory_replicates_bitfield_t) * 8 >= MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX);

    public:
        MemoryReplicate() : allocations(), nallocations(0), valid(0), fetching(0) {}
        MemoryReplicate(const MemoryReplicate & r)
        {
            XKBLAS_FATAL("Implement copy constructor");
        }
        ~MemoryReplicate() {}

}; /* MemoryReplicate */

/* a memory block, one per tree node */
class MemoryBlock {

    public:

        /* host memory view of that block */
        memory_view_t host_view;

        /* per device replicate info */
        MemoryReplicate replicates[XKBLAS_DEVICES_MAX];

        /* valid devices (i.e. devices with at least one valid allocation) */
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
        ) {

            // replicates(block.replicates),
            XKBLAS_FATAL("Not implemented");
        }

        virtual ~MemoryBlock() {}

}; /* MemoryBlock */

/* storage passed when searchingi n the tree */
template <int K>
class KMemoryTreeNodeSearch {

    using Access = KMemoryAccess<K>;
    using Region = Intervals<K>;

    public:
    class Partite {

        public:

            /* memory block in the tree (WARNING : this is mutable outside a 'lock' section) */
            MemoryBlock * block;

            /* The region of this block (intersectoin of the access with the tree node) */
            Region region;

            /* copy of the host view */
            memory_view_t host_view;

            /* dst device */
            int8_t dst_device_global_id;

            /* replicate allocation to use as dst (in MemoryReplicate::allocations) */
            uint8_t dst_allocation_view_id;

            /* the allocation address in which belongs the 'dst' view */
            uintptr_t dst_allocation;

            /* dst view */
            memory_replicate_view_t dst_view;

            /* source device */
            int8_t src_device_global_id;

            /* replicate allocation to use as src (in MemoryReplicate::allocations) */
            uint8_t src_allocation_view_id;

            /* src view */
            memory_replicate_view_t src_view;

            /* true if this block is already being fetched by a concurrent read access */
            bool must_fetch;

        public:

            Partite(MemoryBlock * b, Region & r) :
                block(b),
                region(r),
                host_view(b->host_view),
                dst_device_global_id(HOST_DEVICE_GLOBAL_ID),
                dst_allocation_view_id(MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX),
                dst_allocation(0),
                dst_view(),
                src_device_global_id(HOST_DEVICE_GLOBAL_ID),
                src_allocation_view_id(MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX),
                src_view(),
                must_fetch(true)
            {}

            virtual ~Partite() {}

    }; /* Partite */


   public:

       /* different search type */
       enum Type : uint8_t {
           INSERTING_BLOCKS     = 0,
           SEARCH_FOR_BLOCKS    = 1,
           SEARCH_AWAITING      = 2
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
       // used if type == INSERTING_BLOCKS //
       //////////////////////////////////////////////////////

       /* the access being inserted / intersected */
       Access * access;

       ///////////////////////////////////////
       // used if type == SEARCH_FOR_BLOCKS //
       ///////////////////////////////////////

       /*
        * list of blocks for the current access.
        * The set { b.region / b in partition } is a partition of the space represented by access->region
        */
       std::vector<Partite> partition;

        ///////////////////////////////////////////
        // used if type == SEARCH_AWAITING //
        ///////////////////////////////////////////

        uintptr_t dst_allocation;
        std::vector<Task *> awaiting;

   public:
       KMemoryTreeNodeSearch() : KMemoryTreeNodeSearch(0) {}

       KMemoryTreeNodeSearch(
           uint8_t devid
       ) :
           type(INSERTING_BLOCKS),
           device_global_id(devid),
           access(nullptr),
           partition(),
           dst_allocation(0),
           awaiting()
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
           assert(this->partition.size() == 0);
           this->partition.clear();
           this->type = SEARCH_FOR_BLOCKS;
       }

       void
       prepare_search_awaiting(const uintptr_t alloc_addr)
       {
           this->dst_allocation = alloc_addr;
           this->type = SEARCH_AWAITING;
       }

}; /* KMemoryTreeNodeSearch */


template <int K>
class KMemoryTreeNode : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node {

    using Access = KMemoryAccess<K>;
    using Base = typename KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node;
    using Partite = typename KMemoryTreeNodeSearch<K>::Partite;
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
            XKBLAS_FATAL("Shrink not supported yet");
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
                    search.partition.push_back(Partite(&(this->block), this->region));
                    break ;
                }

                /* search for tasks awaiting on that region for a given allocation */
                case (Search::Type::SEARCH_AWAITING):
                {
                    MemoryReplicate & replicate = this->block.replicates[search.device_global_id];
                    const memory_replicates_bitfield_t devbit = (1 << search.device_global_id);
                    this->block.valid |= devbit;

                    /* for each allocation of that block */
                    for (int i = 0 ; i < replicate.nallocations ; ++i)
                    {
                        const memory_replicates_bitfield_t allocbit = (1 << i);
                        MemoryReplicateAllocationView * view = replicate.allocations[i];

                        /* if it matches the allocation being searched */
                        if (view->allocation == search.dst_allocation)
                        {
                            /* move the awaiting tasks */
                            search.awaiting.insert(search.awaiting.end(), view->awaiting.begin(), view->awaiting.end());
                            view->awaiting.clear();

                            /* this replicate just got fetched and is now valid */
                            replicate.valid    |=  allocbit;
                            replicate.fetching &= ~allocbit;

                            break ;
                        }
                    }

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
            fprintf(f, "\\\\ block size (m, n)=(%d, %d) - ld=%d", this->block.host_view.m, this->block.host_view.n, this->block.host_view.ld);
            fprintf(f, "\\\\ tile (m, n)=(%d, %d)",  this->block.host_view.offset_m, this->block.host_view.offset_n);

         // for (uint8_t device_global_id = 0 ; device_global_id < ctx->drivers.devices.n ; ++device_global_id)
            for (uint8_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX+1 ; ++device_global_id)
            {
                const int devbit = (1 << device_global_id);
                fprintf(f, "\\\\ dev %d - valid=%d",
                    device_global_id,
                    this->block.valid & devbit ? 1 : 0
                );
            }
        }

}; /* KMemoryTreeNode */

template <int K>
class KMemoryTree : public KIntervalBtree<K, KMemoryTreeNodeSearch<K>>, Lockable {

    using Access = KMemoryAccess<K>;
    using Base = KIntervalBtree<K, KMemoryTreeNodeSearch<K>>;
    using Partite = typename KMemoryTreeNodeSearch<K>::Partite;
    using Node = KMemoryTreeNode<K>;
    using NodeBase = typename KIntervalBtree<K, KMemoryTreeNodeSearch<K>>::Node;
    using Region = Intervals<K>;
    using Search = KMemoryTreeNodeSearch<K>;
    using Task = KTask<K>;

    public:

        typedef struct  internal_fetch_t
        {
            /* the memory tree */
            KMemoryTree * tree;

            /* driver of the device */
            xkblas_driver_t * driver;

            /* device fetching */
            xkblas_device_t * device;

            /* logical region */
            Region region;

            /* mark 'fetched' all the tasks awaiting on that allocation */
            uintptr_t allocation;

            /* mark 'fetched' this task */
            Task * task;

        }               internal_fetch_t;

        static inline void
        fetch_callback_task(internal_fetch_t * f, Task * task)
        {
            /* a fetch completed */
            XKBLAS_DEBUG("Task `%s` fetched `%p`", task->label, f->allocation);
            if (task->fetched() == TASK_STATE_DATA_FETCHED)
            {
                /* the task kernel is ready for execution */
                xkblas_device_task_execute(f->driver, f->device, task);
                # pragma message(TODO "Here, we are not polling the offloader kernel streams... Do we want to ?")
            }
        }

        static void
        fetch_callback(const void * args[XKBLAS_CALLBACK_ARGS_MAX])
        {
            assert(XKBLAS_CALLBACK_ARGS_MAX >= 1);

            internal_fetch_t * f = (internal_fetch_t *) args[0];
            assert(f);

            XKBLAS_DEBUG("Fetch completed for allocation `%p`", f->allocation);

            if (f->task)
                fetch_callback_task(f, f->task);

            if (f->allocation)
            {
                Search search(f->device->global_id);
                search.prepare_search_awaiting(f->allocation);
                f->tree->lock();
                {
                    f->tree->intersect(search, f->region, ACCESS_MODE_VOID);
                }
                f->tree->unlock();

                for (Task * & task : search.awaiting)
                    fetch_callback_task(f, task);
            }

            delete f;
        }

        //////////////////////////////////////
        //  DECIDE SRC DEVICE WHEN FETCHING //
        //////////////////////////////////////

        static inline void
        fetch_access_find_src(
            xkblas_driver_t * driver,
            Access * access,
            Partite & partite
        ) {
            assert(partite.block->valid);

            int dst = partite.dst_device_global_id;
            int valid = partite.block->valid;
            int src = driver->f_get_source(dst, valid);
            if( src == -1 ) // Driver failed to found a valid source
                src = __builtin_ffs(partite.block->valid) - 1;
            assert(src >= 0);

            // Get the first valid allocation on that device
            MemoryReplicate & replicate = partite.block->replicates[src];
            assert(replicate.nallocations > 0);
            assert(replicate.valid != 0);
            int allocation_view_id = __builtin_ffs(replicate.valid) - 1;

            // retrieve and set src view infos
            MemoryReplicateAllocationView * r = replicate.allocations[allocation_view_id];
            partite.src_device_global_id   = src;
            partite.src_allocation_view_id = allocation_view_id;
            partite.src_view               = r->view;
        }

        inline void
        fetch_access_copy_partite(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Partite & partite,
            uintptr_t allocation
        ) {
            XKBLAS_INFO("-- Copying from %d to %d --", partite.src_device_global_id, partite.dst_device_global_id);

            /* one replicate must be non-null (a null replicate means to use the host view) */
            assert(partite.dst_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE ||
                   partite.src_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);

            /* host replicate view if no allocation were found */
            memory_replicate_view_t host_replicate_view(partite.host_view.begin_addr(), partite.host_view.ld);

            /* allocate fetch info for the callback argument */
            internal_fetch_t * f = new internal_fetch_t{
                .tree       = this,
                .driver     = driver,
                .device     = device,
                .region{partite.region},
                .allocation = allocation,
                .task       = task
            };

            /* callback setup */
            assert(XKBLAS_CALLBACK_ARGS_MAX >= 1);
            xkblas_callback_t callback;
            callback.func = fetch_callback;
            callback.args[0] = f;

            /* launch asynchronous copy */
            xkblas_stream_instruction_submit_copy(
                driver,
                device,
                partite.host_view,
                partite.dst_device_global_id,
               (partite.dst_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : partite.dst_view,
                partite.src_device_global_id,
               (partite.src_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : partite.src_view,
                callback
            );
        }

        ////////////////////////////////////////////////////////////
        // Create a list of fetch requests for the given accesses //
        ////////////////////////////////////////////////////////////

        typedef struct  fetch_t
        {
            /* dst = host view */
            memory_view_t host_view;

            /* src view */
            memory_replicate_view_t src_view;

            /* src device id */
            xkblas_device_global_id_t src_device_global_id;

            /* the next fetch in the list */
            fetch_t * next;

        }               fetch_t;

        typedef struct  fetch_list_t
        {
            /* the memory tree */
            KMemoryTree * tree;

            /* list of fetches to submit */
            fetch_t * fetches;

            /* number of pending fetches */
            volatile std::atomic<int32_t> pending;

            /* the list can be deleted if this returns '0' */
            int32_t
            fetched(void)
            {
                const int32_t p = pending.fetch_sub(1, std::memory_order_relaxed);
                assert(p >= 0);
                return p;
            }

        }               fetch_list_t;

        inline void
        create_fetch_list_for_host_access(
            ThreadWorker * thread,
            Access * access,
            fetch_list_t * list
        ) {
            assert(access);
            assert(access->mode & ACCESS_MODE_R);

            Search search(HOST_DEVICE_GLOBAL_ID);
            search.prepare_search_blocks();
            this->lock();
            {
                /* find all blocks that intersects with that access */
                this->intersect(search, access->region, access->mode);

                /* launch fetch on each device */
                for (Partite & partite : search.partition)
                {
                    MemoryBlock * block = partite.block;

                    /* not valid on any device, then assume valid on the host */
                    if (block->valid == 0)
                        continue ;

                    /////////////////////////
                    // SRC - FIND BEST SRC //
                    /////////////////////////

                    # pragma message(TODO "Find best device and balance workload for D2H transfers")
                    // TODO : currently taking source as the device with the smallest id,
                    // instead, maybe try to balance the workload between GPU and use
                    // devices with the best bandwidth
                    int src = __builtin_ffs(partite.block->valid) - 1;
                    assert(src >= 0);

                    // Get the first valid allocation on that device
                    MemoryReplicate & replicate = partite.block->replicates[src];
                    assert(replicate.nallocations > 0);
                    assert(replicate.valid != 0);
                    int allocation_view_id = __builtin_ffs(replicate.valid) - 1;

                    // retrieve and set src view infos
                    MemoryReplicateAllocationView * r = replicate.allocations[allocation_view_id];

                    // allocate fetch info for the callback argument
                    // fetch_t * fetch = (fetch_t *) thread->allocate(sizeof(fetch_t));
                    fetch_t * fetch = (fetch_t *) malloc(sizeof(fetch_t));
                    fetch->host_view            = partite.host_view;
                    fetch->src_device_global_id = src;
                    fetch->src_view             = r->view;
                    fetch->next                 = list->fetches;

                    list->fetches = fetch;
                }
            }
            this->unlock();
        }

        void
        create_fetch_list_for_host(
            ThreadWorker * thread,
            Access * accesses,
            int naccesses,
            fetch_list_t * list
        ) {
            assert(naccesses > 0);
            assert(accesses);

            list->tree = this;
            list->fetches = NULL;
            for (int i = 0 ; i < naccesses ; ++i)
                create_fetch_list_for_host_access(thread, accesses + i, list);
        }

        ////////////////////////
        //  FETCH ON A DEVICE //
        ////////////////////////

        /* return the left-most and upper-most block of the partition */
        inline int
        fetch_access_get_first_block(
            std::vector<Partite> & partition
        ) {
            const int nblocks = partition.size();
            int j = 0;

            for (int i = 1 ; i < nblocks ; ++i)
            {
                Partite & bi = partition[i];
                Partite & bj = partition[j];
                for (int k = 0 ; k < K ; ++k)
                    if (bi.region[k].a < bj.region[k].a)
                        j = i;
            }

            return j;
        }

        inline uintptr_t
        fetch_access_allocate(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            std::vector<Partite> & partition
        ) {
            // TODO : memory allocation may spinlock for a while if the
            // device memory is full... in such case, we probably want
            // to release the memory-tree lock, and restart all that
            // shit over again once memory got allocated

            /* allocate continuous memory for that access */
            uint64_t  size = access->host_view.n * access->host_view.m * access->host_view.sizeof_type;
            uintptr_t addr = (uintptr_t) xkblas_memory_allocate(driver, device, size);
            int         ld = access->host_view.n;
            XKBLAS_DEBUG("  allocated at %p for size %zu", (void *) addr, size);
            assert(addr);

            /* retrieve upper left corner */
            const int blockID = this->fetch_access_get_first_block(partition);
            const Partite & corner = partition[blockID];

            /* add a view */
            for (Partite & partite : partition)
            {
                /* compute distance from corner */
                int d[K];
                for (int k = 0 ; k < K ; ++k)
                {
                    d[k] = partite.region[k].a - corner.region[k].a;
                    assert(d[k] >= 0);
                }

                // TODO : offset the allocation address
                static_assert(K == 2);
                const uintptr_t begin_addr = addr + d[0]*ld + d[1];

                MemoryReplicate & replicate = partite.block->replicates[device->global_id];
                const int allocation_view_id = replicate.nallocations++;
                if (allocation_view_id >= MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX)
                    XKBLAS_FATAL("Too many allocations of the same data on the same device... Increase `MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX` and recompile XKBLAS");

                /* allocate the view of that block in the allocation */
                MemoryReplicateAllocationView * r = new MemoryReplicateAllocationView(addr, begin_addr, ld);
                replicate.allocations[allocation_view_id] = r;
                partite.dst_allocation_view_id = allocation_view_id;
            }

            return addr;
        }

        inline uintptr_t
        fetch_access_find_allocation_continuous(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            std::vector<Partite> & partition
        ) {
            assert(this->is_locked());

            int j = 0;
            int nallocations = partition[0].block->replicates[device->global_id].nallocations;
            int nblocks = partition.size();

            /* for each allocation of the block 0 */
            while (j < nallocations)
            {
                MemoryReplicateAllocationView * rj = partition[0].block->replicates[device->global_id].allocations[j];

                /* for each other blocks */
                int i = 1;
                while (i < nblocks)
                {
                    /* for each allocation of other blocks */
                    int nallocations = partition[i].block->replicates[device->global_id].nallocations;
                    for (int k = 0 ; k < nallocations ; ++k)
                    {
                        /* this block has a view with the same allocation, check next block */
                        MemoryReplicateAllocationView * rk = partition[i].block->replicates[device->global_id].allocations[k];
                        if (rj->allocation == rk->allocation)
                        {
                            partition[i].dst_allocation_view_id = k;
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
                partition[0].dst_allocation_view_id = j;
                return rj->allocation;

next_view:
                ++j;
                continue ;
            }
            return 0;
        }

        inline uintptr_t
        fetch_access_find_allocation(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            std::vector<Partite> & partition
        ) {
            assert(this->is_locked());

            /* lookfor a continuous allocation already existing for that access block partitioning */
            const uintptr_t allocation = this->fetch_access_find_allocation_continuous(driver, device, task, access, partition);
            if (allocation)
                return allocation;

            /* no continuous allocation found, make a new one */
            XKBLAS_DEBUG("No continuous allocation found, reallocating and creating a new view");
            return this->fetch_access_allocate(driver, device, task, access, partition);
        }

        inline void
        fetch_access_set_device_view(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Search & search
        ) {
            assert(this->is_locked());

            // we currently set the access view as the 'left-most' and 'upper-most' tile
            // (i.e with the smallest address - corresponding to the begining of this allocation)
            const int blockID = this->fetch_access_get_first_block(search.partition);
            const Partite & partite = search.partition[blockID];
            assert(partite.dst_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);

            const MemoryReplicateAllocationView * r = partite.block->replicates[device->global_id].allocations[partite.dst_allocation_view_id];
            assert(r);

            access->device_view = r->view;
        }

        inline void
        fetch_access_setup_copies(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Search & search
        ) {
            assert(this->is_locked());

            // if read mode is set
            if (access->mode & ACCESS_MODE_R)
            {
                // for each block of that access
                for (Partite & partite : search.partition)
                {
                    ///////////////
                    // SETUP DST //
                    ///////////////

                    // destinary allocation view id
                    const int allocation_view_id = partite.dst_allocation_view_id;
                    assert(allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);
                    const memory_replicates_bitfield_t allocbit = (1 << allocation_view_id);

                    // partite is already valid on that device
                    MemoryReplicate & replicate = partite.block->replicates[device->global_id];
                    if (replicate.valid & allocbit)
                    {
                        partite.must_fetch = false;
                        continue ;
                    }

                    MemoryReplicateAllocationView * r = replicate.allocations[allocation_view_id];

                    /* increment task fetch counter */
                    task->fetching();
                    XKBLAS_DEBUG("Task `%s` fetching one by `%s` on `%p`", task->label, (replicate.fetching & allocbit) ? "awaiting" : "launching", r->view.addr);

                    // partite is already being fetched on that device
                    if (replicate.fetching & allocbit)
                    {
                        partite.must_fetch = false;

                        // add the task to the awaiting list of that block
                        r->awaiting.push_back(task);

                        continue ;
                    }

                    // this task must perform the fetch
                    partite.must_fetch = true;
                    replicate.fetching |= allocbit;

                    // create dst view
                    partite.dst_device_global_id = device->global_id;
                    partite.dst_view = r->view;

                    ///////////////
                    // SETUP SRC //
                    ///////////////

                    // not valid on any devices, using the host as the source
                    if (partite.block->valid == 0)
                    {
                        XKBLAS_DEBUG("No valid block... assuming host is valid");
                        partite.src_device_global_id   = HOST_DEVICE_GLOBAL_ID;
                        partite.src_allocation_view_id = MEMORY_REPLICATE_ALLOCATION_VIEW_NONE;
                    }
                    // valid on some devices
                    else
                    {
                        this->fetch_access_find_src(driver, access, partite);
                    }

                    //////////////////////////////
                    // ASSERTION ON SRC AND DST //
                    //////////////////////////////

                    assert(partite.dst_device_global_id   != partite.src_device_global_id ||
                           partite.dst_allocation_view_id != partite.src_allocation_view_id);
                }
            }
        }

        inline void
        fetch_access_set_valid(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Search & search
        ) {
            assert(this->is_locked());

            // if write mode, set the valid bits
            if (access->mode & ACCESS_MODE_W)
            {
                const memory_replicates_bitfield_t devbit = (1 << device->global_id);

                for (Partite & partite : search.partition)
                {
                    # pragma message(TODO "Manage validity in a more lazy way")
                    for (int i = 0 ; i < XKBLAS_DEVICES_MAX ; ++i)
                    {
                        MemoryReplicate & replicate = partite.block->replicates[i];
                        replicate.valid = 0;
                        replicate.fetching = 0;
                    }

                    MemoryReplicate & replicate = partite.block->replicates[device->global_id];

                    # if 0
                    // release allocation that are no longer required, as we are rewritting that block.
                    XKBLAS_IMPL("Releasing allocations from a block...");
                    # pragma message(TODO "Free each allocation")
                    for (int i = 0 ; i < replicate.nallocations ; ++i)
                    {
                        if (i != partite.dst_allocation_view_id)
                        {
                            // TODO : xkblas_memory_device_free(...)
                            delete replicate.allocations[i];
                        }
                    }

                    // update the allocations id
                    const int allocation_view_id = 0;
                    replicate.nallocations = 1;
                    replicate.allocations[0] = replicate.allocations[partite.dst_allocation_view_id];
                    partite.dst_allocation_view_id = allocation_view_id;
                    # endif

                    // set valid bits
                    // Even though the data is not written, as we are writing,
                    // there are no other tasks accessing concurrently
                    const memory_replicates_bitfield_t allocbit = (1 << partite.dst_allocation_view_id);
                    partite.block->valid = devbit;
                    replicate.valid      = allocbit;
                    replicate.fetching   = allocbit;
                }
            }
        }

        /* launch asynchronous copies for the given partition to the given device, using the given allocation as dst */
        inline void
        fetch_access_launch_copies(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            std::vector<Partite> & partition,
            const uintptr_t allocation
        ) {
            if (access->mode & ACCESS_MODE_R)
            {
                for (Partite & partite : partition)
                {
                    if (!partite.must_fetch)
                    {
                        XKBLAS_DEBUG("Skipping fetch of a block already being fetched (concurrent read ?)");
                        continue ;
                    }

                    this->fetch_access_copy_partite(driver, device, task, partite, allocation);
                }
            }
        }

        void
        fetch_access(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access
        ) {

            // Debugging (WIP) - steps
            //  - (1) - OK
            //  - (2) - OK
            //  - (3) - OK
            //  - (4) - OK
            //  - (5) - ?
            //  - (6) - ?
            //  - (7) - OK

            Search search(device->global_id);
            uintptr_t allocation = 0;
            this->lock();
            {
                # pragma message(TODO "Step (1) and (2) could be merged to only search once")

                XKBLAS_DEBUG("Inserting (%d,%d) of size (%d,%d)", access->host_view.offset_m, access->host_view.offset_n, access->host_view.m, access->host_view.m);

                /* step (1) ensure the access is represented in the tree as blocks */
                search.prepare_insert(access);
                this->insert(search, access->region, access->mode);

                /* step (2) find all blocks representing the access */
                search.prepare_search_blocks();
                this->intersect(search, access->region, access->mode);
                assert(search.partition.size() >= 1);

                /* step (3) find or allocate continuous memory for that access on that device */
                allocation = this->fetch_access_find_allocation(driver, device, task, access, search.partition);

                /* step (4) set the access view on the device (that will be used by the kernel) */
                this->fetch_access_set_device_view(driver, device, task, access, search);

                /* step (5) if read access, find src/dst, and setup views to transfer on step (7) */
                this->fetch_access_setup_copies(driver, device, task, access, search);

                /* step (6) if write access, invalidate other copies */
                this->fetch_access_set_valid(driver, device, task, access, search);

            } /* this->lock(); */
            this->unlock();

            /* step (7) - launch transfers with info set on step (5) */
            this->fetch_access_launch_copies(driver, device, task, access, search.partition, allocation);
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
            XKBLAS_DEBUG("Task `%s` fetching one (avoid early launch)", task->label);

            /* for each access */
            assert(task->naccesses <= TASK_MAX_ACCESSES);
            for (int i = 0 ; i < task->naccesses ; ++i)
                this->fetch_access(driver, device, task, task->accesses + i);

            XKBLAS_DEBUG("Task `%s` fetched one (avoid early launch)", task->label);
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
