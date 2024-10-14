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

# define CUBE_TREE_CUT
# define CUBE_TREE_REBALANCE
# include "sync/cube-tree.hpp"
# undef CUBE_TREE_CUT
# undef CUBE_TREE_REBALANCE

# include "device/device-memory.h"
# include "sync/bits.h"
# include "sync/lockable.hpp"

# include <cstdint>
# include <functional>
# include <map>

# define USE_D2D_FORWARDING 1

# pragma message(TODO "Memory allocation is currently performed within a critical section... If memory eviction must be performed, this creates double-locking issues + a lot of time spent in the critical section. Reason is : we need a partition (in the memory tree) of the access to write the allocation information on each block of the partition")

# pragma message(TODO "'fetch' implementation could be optimize by reducing critical sections")

# pragma message(TODO "merge 'Replicate' on continuous "   \
        "memory addresses - for now, just perform one data "    \
        "transfer per block")

# pragma message(TODO "Nest classes into a 'KMemory' templated class - corresponding to a global view of the memory in 'K' dimensions")

# define MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX   (1)
# define MEMORY_REPLICATE_ALLOCATION_VIEW_NONE   (MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX)

typedef uint8_t memory_allocation_view_id_t;
static_assert(MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX <= (1 << (sizeof(memory_allocation_view_id_t)*8)));

typedef uint8_t memory_allocation_view_id_bitfield_t;
static_assert(MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX <= sizeof(memory_allocation_view_id_bitfield_t) * 8);

static std::map<uintptr_t, bool> LAUNCHED;

/* a forward request */
template <int K>
class KMemoryForward {

    using Task = KTask<K>;

    public:

        /* the task that requested the forward */
        Task * task;

        /* dst chunk */
        xkblas_alloc_chunk_t * chunk;

        /* the dst device */
        xkblas_device_global_id_t device_global_id;

        /* the dst alloc view id to use */
        memory_replicate_view_t view;

    public:

        KMemoryForward(
            Task * task,
            xkblas_alloc_chunk_t * chunk,
            xkblas_device_global_id_t device_global_id,
            memory_replicate_view_t & view
        ) :
            task(task),
            chunk(chunk),
            device_global_id(device_global_id),
            view(view)
        {}

        ~KMemoryForward() {}

}; /* KMemoryForward */

/* a view of memory allocation */
template <int K>
class KMemoryReplicateAllocationView {

    using MemoryForward = KMemoryForward<K>;
    using Task = KTask<K>;

    public:

        /* the device memory chunk */
        xkblas_alloc_chunk_t * chunk;

        /* the address of that view in [allocation, allocation + allocation->size[ */
        memory_replicate_view_t view;

        /* awaiting operations */
        struct {
            /* tasks awaiting on that view to be transfered */
            std::vector<Task *> tasks;

            /* must forward this view to other views using D2D transfer */
            std::vector<MemoryForward> forwards;

        } awaiting;

    public:

        KMemoryReplicateAllocationView(
            xkblas_alloc_chunk_t * chunk,
            const uintptr_t addr,
            const size_t ld
        ) :
            chunk(chunk),
            view(addr, ld),
            awaiting()
        {
            ++(chunk->use_counter);
        }

        virtual ~KMemoryReplicateAllocationView() {}

}; /* KMemoryReplicateAllocationView */

/* a host replicate on a device */
template <int K>
class KMemoryReplicate
{
    using MemoryReplicateAllocationView = KMemoryReplicateAllocationView<K>;

    public:

        /* List of allocations for this device replicate.
         * A device may have several allocations for the same 'host memory'
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

        /* if this fails, replace 'uint8_t' indexing views by a larger type */
        /* array of allocations */
        MemoryReplicateAllocationView * allocations[MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX];
        volatile memory_allocation_view_id_t nallocations;

        /* valid allocations */
        volatile memory_allocation_view_id_bitfield_t valid;

        /* fetching allocations */
        volatile memory_allocation_view_id_bitfield_t fetching;

    public:
        KMemoryReplicate() : allocations(), nallocations(0), valid(0), fetching(0) {}
        KMemoryReplicate(const KMemoryReplicate & r)
        {
            XKBLAS_FATAL("Implement copy constructor");
        }
        ~KMemoryReplicate() {}

}; /* KMemoryReplicate */

// if this assertion, many bitwise operation in the runtime will be wrong as
// they are implicitly done on int32 : (1 << device_global_id) will be an int -
// should update the runtime with (1UL << device_global_id) - maybe use a macro
// for 'one' depending on that size
static_assert(sizeof(xkblas_device_global_id_bitfield_t) * 8 <= 32);

/* a memory block, one per tree node */
template <int K>
class KMemoryBlock {

    using Cube = KCube<K>;
    using MemoryReplicate = KMemoryReplicate<K>;
    using MemoryReplicateAllocationView = KMemoryReplicateAllocationView<K>;

    public:

        /* host memory view of that block */
        memory_view_t host_view;

        /* per device replicate info */
        MemoryReplicate replicates[XKBLAS_DEVICES_MAX];

        /* valid devices (i.e. devices with at least one valid allocation) */
        volatile xkblas_device_global_id_bitfield_t valid;

        /* fetching devices (i.e. devices with at least one fetching allocation) */
        volatile xkblas_device_global_id_bitfield_t fetching;

    public:

        /* a new memory block, assume it is valid on the host */
        KMemoryBlock(const memory_view_t & v) :
            host_view(v),
            replicates(),
            valid(0),
            fetching(0)
        {}

        void
        memory_block_init(
            const Cube & block_cube,
            const KMemoryBlock & inheriting_block,
            const Cube & inheriting_cube,
            const int k
        ) {
            /////////////////////////////////
            //  HOST_VIEW HAS TO BE OFFSET //
            /////////////////////////////////
            INTERVAL_DIFF_TYPE_T d[K];
            Cube::distance_manhattan(inheriting_cube, block_cube, d);

            assert(inheriting_block.host_view.order == MATRIX_COLMAJOR);
            const size_t sizeof_type = inheriting_block.host_view.sizeof_type;

            this->host_view = inheriting_block.host_view;
            this->host_view.offset_m += d[1] / sizeof_type;
            this->host_view.m         = block_cube[1].length() / sizeof_type;
            this->host_view.offset_n += d[0];
            this->host_view.n         = block_cube[0].length();

            assert(this->host_view.offset_m >= 0);
            assert(this->host_view.offset_n >= 0);
            assert(this->host_view.m         > 0);
            assert(this->host_view.n         > 0);

            //////////////////////////////////
            //  DUPPLICATE REPLICATE INFOS  //
            //////////////////////////////////

            for (xkblas_device_global_id_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
            {
                // retrieve this device replicate
                      MemoryReplicate *            replicate =            this->replicates + device_global_id;
                const MemoryReplicate * inheriting_replicate = inheriting_block.replicates + device_global_id;

                // dupplicate allocations
                replicate->nallocations = inheriting_replicate->nallocations;
                for (int i = 0 ; i < inheriting_replicate->nallocations ; ++i)
                {
                    const MemoryReplicateAllocationView * inheriting_allocation = inheriting_replicate->allocations[i];

                    // warning: 'ld' here depends on the allocation itself
                    const uintptr_t offset      = d[1] + d[0] * inheriting_allocation->view.ld * sizeof_type;
                    const uintptr_t begin_addr  = inheriting_allocation->view.addr + offset;

                    # pragma message(TODO "This memory is currently leaked when 'invalidate' is called")
                    MemoryReplicateAllocationView * allocation = new MemoryReplicateAllocationView(inheriting_allocation->chunk, begin_addr, inheriting_allocation->view.ld);
                    replicate->allocations[i] = allocation;
                    // allocation->awaiting must remain empty, tasks will be notified through the shrinked block
                }

                // dupplicate fetching / valid infos
                replicate->valid    = inheriting_replicate->valid;
                replicate->fetching = inheriting_replicate->fetching;
            }

            //////////////////////////////
            //  VALID BITS ARE STILL OK //
            //////////////////////////////
            this->valid     = inheriting_block.valid;       // copy validity
            this->fetching  = inheriting_block.fetching;    // copy fetching
        }

        /* a block from splitting an existing one */
        KMemoryBlock(
            const Cube & block_cube,
            const KMemoryBlock & inheriting_block,
            const Cube & inheriting_cube,
            const int k
        ) {
            static_assert(K == 2);
            this->memory_block_init(block_cube, inheriting_block, inheriting_cube, k);
        }
        ~KMemoryBlock() {}

}; /* KMemoryBlock */

/* storage passed when searching in the tree */
template <int K>
class KMemoryTreeNodeSearch {

    using Access = KMemoryAccess<K>;
    using Cube = KCube<K>;
    using MemoryBlock = KMemoryBlock<K>;
    using MemoryForward = KMemoryForward<K>;

    public:

        class Partite {

            public:

                /* memory block in the tree (WARNING : this is mutable outside a 'lock' section) */
                MemoryBlock * block;

                /* copy of the host view */
                memory_view_t host_view;

                /* The cube of this block (intersection of the access with the tree node) */
                const Cube cube;

                /* dst device */
                xkblas_device_global_id_t dst_device_global_id;

                /* replicate allocation to use as dst (in MemoryReplicate::allocations) */
                memory_allocation_view_id_t dst_allocation_view_id;

                /* dst view */
                memory_replicate_view_t dst_view;

                /* source device */
                xkblas_device_global_id_t src_device_global_id;

                /* replicate allocation to use as src (in MemoryReplicate::allocations) */
                memory_allocation_view_id_t src_allocation_view_id;

                /* src view */
                memory_replicate_view_t src_view;

                /* true if this block is already being fetched by a concurrent read access */
                bool must_fetch;

            public:

                Partite(MemoryBlock * b, const Cube & r) :
                    block(b),
                    cube(r),
                    host_view(b->host_view),
                    dst_device_global_id(HOST_DEVICE_GLOBAL_ID),
                    dst_allocation_view_id(MEMORY_REPLICATE_ALLOCATION_VIEW_NONE),
                    dst_view(),
                    src_device_global_id(HOST_DEVICE_GLOBAL_ID),
                    src_allocation_view_id(MEMORY_REPLICATE_ALLOCATION_VIEW_NONE),
                    src_view(),
                    must_fetch(true)
                {}

                virtual ~Partite() {}

        }; /* Partite */

        class Partition {

            public:

                /* the partite of that partition */
                std::vector<Partite> partites;

                /* all partite share the same allocation chunk */
                xkblas_alloc_chunk_t * chunk;

            public:
                Partition(xkblas_alloc_chunk_t * chunk) : chunk(chunk) {}
                ~Partition() {}

                /* return the left-most and upper-most block of the partition */
                inline int
                get_uppermost_leftmost_block(void) const
                {
                    const size_t nblocks = this->partites.size();
                    int j = 0;

                    for (int i = 1 ; i < nblocks ; ++i)
                    {
                        const Partite & bi = this->partites[i];
                        const Partite & bj = this->partites[j];
                        for (int k = 0 ; k < K ; ++k)
                            if (bi.cube[k].a < bj.cube[k].a)
                                j = i;
                    }

                    return j;
                }

                Partite &
                get_corner(void)
                {
                    const int i = this->get_uppermost_leftmost_block();
                    return this->partites[i];
                }

        }; /* Partition */

   public:

       /* different search type */
       enum Type : uint8_t {
           INSERTING_BLOCKS     = 0,
           SEARCH_FOR_BLOCKS    = 1,
           SEARCH_AWAITING      = 2,
           SEARCH_OWNERS        = 3
       };

   public:

       /////////////////////////////////
       // used in all types of search //
       /////////////////////////////////

       /* type of search performing */
       Type type;

        /* device global id, on which we are looking for invalid blocks or validating blocks */
       const xkblas_device_global_id_t device_global_id;

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
        * The set { b.cube / b in partition } is a partition of the space represented by access->cube
        */
       Partition partition;

        ///////////////////////////////////////////
        // used if type == SEARCH_AWAITING //
        ///////////////////////////////////////////
        xkblas_alloc_chunk_t * chunk;
        struct {
            std::vector<Task *> tasks;
            std::vector<MemoryForward> forwards;
        } awaiting;

        ///////////////////////////////////
        // used if type == SEARCH_OWNERS //
        ///////////////////////////////////
        size_t bytes_owned[XKBLAS_DEVICES_MAX];

   public:
       KMemoryTreeNodeSearch() : KMemoryTreeNodeSearch(0) {}

       KMemoryTreeNodeSearch(
           xkblas_device_global_id_t devid
       ) :
           type(INSERTING_BLOCKS),
           device_global_id(devid),
           access(nullptr),
           partition(nullptr),
           chunk(nullptr),
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
           assert(this->partition.partites.size() == 0);
           this->partition.partites.clear();
           this->partition.chunk = nullptr;
           this->type = SEARCH_FOR_BLOCKS;
       }

       void
       prepare_search_awaiting(xkblas_alloc_chunk_t * chunk)
       {
           this->chunk = chunk;
           this->type = SEARCH_AWAITING;
       }

       void
       prepare_search_owners(void)
       {
           memset(this->bytes_owned, 0, sizeof(this->bytes_owned));
           this->type = SEARCH_OWNERS;
       }

}; /* KMemoryTreeNodeSearch */

template <int K>
class KMemoryTreeNode : public KCubeTree<K, KMemoryTreeNodeSearch<K>>::Node {

    using Access = KMemoryAccess<K>;
    using Base = typename KCubeTree<K, KMemoryTreeNodeSearch<K>>::Node;
    using Cube = KCube<K>;
    using MemoryBlock = KMemoryBlock<K>;
    using MemoryForward = KMemoryForward<K>;
    using MemoryReplicate = KMemoryReplicate<K>;
    using MemoryReplicateAllocationView = KMemoryReplicateAllocationView<K>;
    using Node = KMemoryTreeNode<K>;
    using Partite = typename KMemoryTreeNodeSearch<K>::Partite;
    using Search = KMemoryTreeNodeSearch<K>;

    public:

        /* the memory block represented by this node */
        MemoryBlock block;

    public:

        /* the cube was never accessed before, create a new node */
        KMemoryTreeNode<K>(
            const Access * access,
            const Cube & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            block(access->host_view)
        {}

        /**
         * A new node is being created from a split, make it inherit its original node 'src'
         *  - access - the access
         *  - r - the shrinked cube that this is inheriting from
         *  - k - the dimension that got splitted
         *  - color - the node color
         *  - src - the node that got split
         *
         * We have:
         *  U (src->cube, r) == the node cube before being shrinked
         *  n (src->cube, r) = {} - empty intersection
         */
        KMemoryTreeNode<K>(
            const Access * access,
            const Cube & r,
            const int k,
            const Color color,
            const Node * src
        ) :
            Base(r, k, color),
            block(r, src->block, src->cube, k)
        {}

    public:

        void
        on_insert(
            Search & search,
            const access_mode_t mode
        ) {
            assert(search.type == Search::Type::INSERTING_BLOCKS);
        }

        /* shrinking on dimension 'k' from 'this->cube[k]' to 'interval' */
        void
        on_shrink(
            const Interval & interval,
            int k
        ) {
            static_assert(K == 2);

            assert(k < K);
            assert(this->cube[k].includes(interval));

            ///////////////////////
            //  SHRINK HOST VIEW //
            ///////////////////////

            const size_t sizeof_type = this->block.host_view.sizeof_type;

            assert(this->cube[k].a <= interval.a);
            const INTERVAL_DIFF_TYPE_T da = interval.a - this->cube[k].a;

            assert(this->cube[k].b >= interval.b);
            const INTERVAL_DIFF_TYPE_T db = this->cube[k].b - interval.b;

            if (k == 1)
            {
                assert(da % sizeof_type == 0);
                assert(db % sizeof_type == 0);
            }

            // shrinked-left, gotta offset the views
            if (da)
            {
                // HOST VIEW
                if (k == 1)
                    this->block.host_view.offset_m += (da / sizeof_type);
                else
                    this->block.host_view.offset_n += da;

                // REPLICATES VIEW
                for (MemoryReplicate & replicate : this->block.replicates)
                {
                    for (int i = 0 ; i < replicate.nallocations ; ++i)
                    {
                        MemoryReplicateAllocationView * alloc_view = replicate.allocations[i];
                        const INTERVAL_DIFF_TYPE_T offset = (k == 1) ? da : (da * alloc_view->view.ld * sizeof_type);
                        alloc_view->view.addr += offset;
                        assert(alloc_view->view.addr >= alloc_view->chunk->device_ptr);
                    }
                }
            }

            // resize the views
            if (k == 1)
                this->block.host_view.m = interval.length() / sizeof_type;
            else
                this->block.host_view.n = interval.length();

            assert(this->block.host_view.offset_m >= 0);
            assert(this->block.host_view.offset_n >= 0);
            assert(this->block.host_view.m > 0);
            assert(this->block.host_view.n > 0);
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_stop_test(
            Search & search,
            const Cube & cube,
            const access_mode_t mode
        ) const {
            (void) search;
            (void) cube;
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
            const Cube & cube,
            const access_mode_t mode
        ) {
            assert(cube.intersects(this->cube));

            switch (search.type)
            {
                case (Search::Type::SEARCH_FOR_BLOCKS):
                {
                    /* intersecting against 'cube' that had been inserted previously,
                     * so 'this' must be a sub-block of 'cube' */
                    assert(cube.includes(this->cube));

                    const Partite partite(&(this->block), this->cube);
                    search.partition.partites.push_back(partite);

                    break ;
                }

                /* search for awaiting operation on that cube */
                case (Search::Type::SEARCH_AWAITING):
                {
                    MemoryReplicate & replicate = this->block.replicates[search.device_global_id];
                    const xkblas_device_global_id_bitfield_t devbit = (1 << search.device_global_id);

                    /* find the allocation that got fetched */
                    bool found = false;
                    for (memory_allocation_view_id_t alloc_view_id = 0 ; alloc_view_id < replicate.nallocations ; ++alloc_view_id)
                    {
                        const xkblas_device_global_id_bitfield_t allocbit = (1 << alloc_view_id);
                        MemoryReplicateAllocationView * alloc_view = replicate.allocations[alloc_view_id];

                        /* found the allocation that got fetched */
                        if (alloc_view->chunk == search.chunk)
                        {
                            /* this replicate just got fetched and is now valid */
                            replicate.valid    |= (memory_allocation_view_id_bitfield_t)  allocbit;
                            replicate.fetching &= (memory_allocation_view_id_bitfield_t) ~allocbit;

                            /* move the awaiting tasks */
                            search.awaiting.tasks.insert(search.awaiting.tasks.end(), alloc_view->awaiting.tasks.begin(), alloc_view->awaiting.tasks.end());
                            alloc_view->awaiting.tasks.clear();

                            /* move the awaiting tasks */
                            search.awaiting.forwards.insert(search.awaiting.forwards.end(), alloc_view->awaiting.forwards.begin(), alloc_view->awaiting.forwards.end());
                            alloc_view->awaiting.forwards.clear();

                            found = true;

                            break ;
                        }
                    }
                    assert(found);

                    /* update status bit masks */
                    this->block.valid |= devbit;
                    if (replicate.fetching == 0)
                    {
                        assert(this->block.fetching & devbit);
                        this->block.fetching &= ~devbit;
                    }

                    break ;
                }

                /* search for owners of the access */
                case (Search::Type::SEARCH_OWNERS):
                {
                    const size_t bytes = cube.size();
                    for (xkblas_device_global_id_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
                        if (this->block.valid & (1 << device_global_id))
                            search.bytes_owned[device_global_id] += bytes;
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
            KCubeTree<K, KMemoryTreeNodeSearch<K>>::Node::dump_str(f);
        }

        void
        dump_cube_str(FILE * f) const
        {
            // KCubeTree<K, DeviceInvalidCubes>::Node::dump_cube_str(f);
            fprintf(f, "\\\\ host-addr=%p", (void *) this->block.host_view.addr);
            fprintf(f, "\\\\ block size (m, n)=(%d, %d) - ld=%d", this->block.host_view.m, this->block.host_view.n, this->block.host_view.ld);
            fprintf(f, "\\\\ tile (m, n)=(%d, %d)",  this->block.host_view.offset_m, this->block.host_view.offset_n);

         // for (uint8_t device_global_id = 0 ; device_global_id < ctx->drivers.devices.n ; ++device_global_id)
            for (xkblas_device_global_id_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX+1 ; ++device_global_id)
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
class KMemoryTree : public KCubeTree<K, KMemoryTreeNodeSearch<K>>, Lockable {

    using Access = KMemoryAccess<K>;
    using Base = KCubeTree<K, KMemoryTreeNodeSearch<K>>;
    using Cube = KCube<K>;
    using MemoryBlock = KMemoryBlock<K>;
    using MemoryForward = KMemoryForward<K>;
    using MemoryReplicate = KMemoryReplicate<K>;
    using MemoryReplicateAllocationView = KMemoryReplicateAllocationView<K>;
    using Node = KMemoryTreeNode<K>;
    using NodeBase = typename KCubeTree<K, KMemoryTreeNodeSearch<K>>::Node;
    using Partite = typename KMemoryTreeNodeSearch<K>::Partite;
    using Partition = typename KMemoryTreeNodeSearch<K>::Partition;
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

            /* mark 'fetched' this task */
            Task * task;

            /* logical cube */
            const Cube cube;

            /* host view of this fetch */
            memory_view_t host_view;

            /* dst chunk */
            xkblas_alloc_chunk_t * dst_chunk;

            /* dst device id */
            xkblas_device_global_id_t dst_device_global_id;

            /* dst view */
            memory_replicate_view_t dst_view;

        }               internal_fetch_t;

        static inline void
        fetch_callback_task(internal_fetch_t * fetch, Task * task)
        {
            /* a fetch completed */
            XKBLAS_DEBUG("Task `%s` fetched on device ptr `%p`", task->label, fetch->dst_chunk->device_ptr);
            if (task->fetched() == TASK_STATE_DATA_FETCHED)
            {
                /* the task kernel is ready for execution */
                xkblas_device_task_execute(fetch->driver, fetch->device, task);
                # pragma message(TODO "Here, we are not polling the offloader kernel streams... Do we want to ?")
            }
        }

        /** callback when a fetch completed */
        static void
        fetch_callback(const void * args[XKBLAS_CALLBACK_ARGS_MAX])
        {
            // parse args
            assert(XKBLAS_CALLBACK_ARGS_MAX >= 1);
            internal_fetch_t * fetch = (internal_fetch_t *) args[0];
            assert(fetch);

            XKBLAS_DEBUG("Fetch completed for allocation `%p`", fetch->dst_chunk->device_ptr);

            // the current thread must be the device one
            assert(ThreadWorker::self() == fetch->device->thread);

            //  `fetch->task` is the task that initiated the fetch: notify that the data has arrived
            assert(fetch->task);
            fetch_callback_task(fetch, fetch->task);

            //  `fetch->dst_chunk` is the memory allocated chunk on which the data had been fetched.
            //  Search in the tree for awaiting tasks and forwards
            assert(fetch->dst_chunk);

            // search for blocks and its allocation, that matches the region fetched onto 'chunk'
            Search search(fetch->dst_device_global_id);
            search.prepare_search_awaiting(fetch->dst_chunk);
            fetch->tree->lock();
            {
                fetch->tree->intersect(search, fetch->cube, ACCESS_MODE_VOID);
            }
            fetch->tree->unlock();

            // notify awaiting tasks that the data had been fetched
            for (Task * & task : search.awaiting.tasks)
                fetch_callback_task(fetch, task);

            // forward the data to other devices
            for (MemoryForward & forward : search.awaiting.forwards)
            {
                XKBLAS_ERROR(
                    "Forwarding from %d to %d using alloc %p",
                    fetch->device->global_id, forward.device_global_id, forward.chunk
                );

                assert(forward.task);
                assert(forward.chunk);
                assert(0 <= forward.device_global_id && forward.device_global_id < XKBLAS_DEVICES_MAX);

                fetch->tree->fetch_access_launch_copy(
                    fetch->driver,                  // use the same driver
                    fetch->device,                  // use the same (old dst, that is now src) device
                    forward.task,                   // use the forwarded task
                    forward.chunk,                  // the chunk allocated when requesting the forward
                    fetch->cube,                    // the logicial view hasn't changed
                    fetch->host_view,               // the host view hasn't changed
                    forward.device_global_id,       // use the forwarded dst device
                    forward.view,                   // use the forwarded dst view
                    fetch->dst_device_global_id,    // the current 'dst' is the new 'src'
                    fetch->dst_view                 // the current 'dst' is the new 'src'
                );
            }

            delete fetch;
        }

        //////////////////////////////////////
        //  DECIDE SRC DEVICE WHEN FETCHING //
        //////////////////////////////////////

        inline void
        fetch_access_launch_fetch(
            const internal_fetch_t          * fetch,
            const xkblas_device_global_id_t   src_device_global_id,
            const memory_replicate_view_t   & src_view
        ) {
            /* callback setup */
            assert(XKBLAS_CALLBACK_ARGS_MAX >= 1);
            xkblas_callback_t callback;
            callback.func = fetch_callback;
            callback.args[0] = fetch;

            /* launch asynchronous copy */
            xkblas_stream_instruction_submit_copy(
                fetch->driver,
                fetch->device,
                fetch->host_view,
                fetch->dst_device_global_id,
                fetch->dst_view,
                src_device_global_id,
                src_view,
                callback
            );
        }

        inline void
        fetch_access_launch_copy(
                  xkblas_driver_t           * driver,
                  xkblas_device_t           * device,
                  Task                      * task,
                  xkblas_alloc_chunk_t      * dst_chunk,
            const Cube                      & cube,
            const memory_view_t             & host_view,
            const xkblas_device_global_id_t   dst_device_global_id,
            const memory_replicate_view_t   & dst_view,
            const xkblas_device_global_id_t   src_device_global_id,
            const memory_replicate_view_t   & src_view
        ) {
            /* allocate fetch info for the callback argument */
            const internal_fetch_t * fetch = new internal_fetch_t{
                .tree       = this,
                .driver     = driver,
                .device     = device,
                .task       = task,
                .cube{cube},
                .host_view{host_view},
                .dst_chunk  = dst_chunk,
                .dst_device_global_id = dst_device_global_id,
                .dst_view{dst_view}
            };

            this->fetch_access_launch_fetch(
                fetch,
                src_device_global_id,
                src_view
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

        void
        create_fetch_list_for_host(
            ThreadWorker * thread,
            const Cube & cube,
            fetch_list_t * list
        ) {
            assert(thread);
            assert(!cube.is_empty());
            assert(list);

            list->tree = this;
            list->fetches = NULL;

            Search search(HOST_DEVICE_GLOBAL_ID);
            search.prepare_search_blocks();
            this->lock();
            {
                /* find all blocks that intersects with that access */
                this->intersect(search, cube, ACCESS_MODE_R);

                /* launch fetch on each device */
                for (Partite & partite : search.partition.partites)
                {
                    MemoryBlock * block = partite.block;

                    /* not valid on any device, then assume valid on the host */
                    if (block->valid == 0)
                        continue ;

                    /////////////////////////
                    // SRC - FIND BEST SRC //
                    /////////////////////////

                    // xkblas_device_global_id_t src = (xkblas_device_global_id_t) (__builtin_ffs(partite.block->valid) - 1);
                    xkblas_device_global_id_t src = __random_set_bit(partite.block->valid) - 1;
                    assert(src >= 0);

                    // Get the first valid allocation on that device
                    MemoryReplicate & replicate = partite.block->replicates[src];
                    assert(replicate.nallocations > 0);
                    assert(replicate.valid != 0);

                    const memory_allocation_view_id_t allocation_view_id = (memory_allocation_view_id_t) (__builtin_ffs(replicate.valid) - 1);

                    // retrieve and set src view infos
                    MemoryReplicateAllocationView * alloc_view = replicate.allocations[allocation_view_id];

                    // allocate fetch info for the callback argument
                    // fetch_t * fetch = (fetch_t *) thread->allocate(sizeof(fetch_t));
                    fetch_t * fetch = (fetch_t *) malloc(sizeof(fetch_t));
                    fetch->host_view            = partite.host_view;
                    fetch->src_device_global_id = src;
                    fetch->src_view             = alloc_view->view;
                    fetch->next                 = list->fetches;

                    list->fetches = fetch;
                }
            }
            this->unlock();
        }

        ////////////////////////
        //  FETCH ON A DEVICE //
        ////////////////////////

        inline void
        fetch_access_allocate_eviction(
            xkblas_device_t * device,
            size_t size
        ) {

            /* adapted from 'kaapi_memory_cache_evict_fromlist' */

            // TODO : currently deallocating as much as possible, maybe stop when there is a chunk big-enough of 'size'

            size_t freed = 0;
            auto f = [device, size, &freed](NodeBase * nodebase, void * args, bool & stop) {
                (void) args;

                Node * node = reinterpret_cast<Node *>(nodebase);
                assert(node);

                MemoryBlock & block = node->block;

                const xkblas_device_global_id_bitfield_t devbit = 1 << device->global_id;

                const bool valid_on_any_device        = block.valid != 0;
                const bool valid_on_device            = block.valid &  devbit;
                const bool valid_on_any_other_devices = block.valid & ~devbit;

                MemoryReplicate & replicate = block.replicates[device->global_id];
                if (replicate.fetching)
                    return ;

                if (!valid_on_any_device || valid_on_any_other_devices)
                {
                    /* evict all allocations */
                    for (int i = 0 ; i < replicate.nallocations ; ++i)
                    {
                        const xkblas_device_global_id_bitfield_t allocbit = 1 << i;

                        MemoryReplicateAllocationView * allocation = replicate.allocations[i];
                        assert(allocation);

                        /* if only this block uses the allocation */
                        if (allocation->chunk->use_counter == 1)
                        {
                            XKBLAS_WARN("Evicted a block of size %u MB", allocation->chunk->size/1024/1024);
                            xkblas_memory_deallocate(device, allocation->chunk);
                            freed += allocation->chunk->size;
                        }
                        /* else: what to do ? */
                        else
                        {
                            XKBLAS_FATAL("Couldn't evict a chunk that is used in several allocation view - valid_on_any_device=%d, valid_on_any_other_devices=%d", valid_on_any_device, valid_on_any_other_devices);
                        }

                        // delete allocation;
                    }
                    replicate.nallocations  = 0;
                    replicate.valid         = 0;
                    assert(replicate.fetching == 0);

                    block.valid &= (xkblas_device_global_id_bitfield_t) ~devbit;

                    // stop = freed >= 2*size;
                    stop = false;
                }
                else if (valid_on_device && replicate.nallocations > 1)
                {
                    // TODO : only keep 1 valid allocation
                    XKBLAS_FATAL("valid_on_device=%d, nallocations=%d",
                            valid_on_device, replicate.nallocations);
                }
            };

            this->foreach_node_until(f, NULL);
        }

        /* retrieve or allocate memory on 'device' to hold 'access' - and store the allocated chunk in `partition.chunk` */
        inline void
        fetch_access_allocate(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Partition & partition
        ) {
            // TODO : memory allocation may spinlock for a while if the
            // device memory is full... in such case, we probably want
            // to release the memory-tree lock, and restart all that
            // shit over again once memory got allocated

            # pragma message(TODO "Can we manage row/col major in a better way ? hardcoded col major here for cuda")

            /* allocate continuous memory for that access */
            const size_t          ld = access->host_view.m;            // cuda is col major
            const size_t sizeof_type = access->host_view.sizeof_type;
            const size_t size        = access->host_view.m * access->host_view.n * access->host_view.sizeof_type;

            //////////////////////////
            // Allocate a new chunk //
            //////////////////////////

            int retry_cnt = 0;

            do {

                partition.chunk = xkblas_memory_allocate(driver, device, size);
                if (partition.chunk)
                    break ;

                // TODO : polling is risky here, because it may take a lock on the
                // memory tree, and 'xkblas_memory_allocate' is called within a
                // memory-tree lock => double-lock deadlock

                // xkblas_device_poll(device);
                fetch_access_allocate_eviction(device, size);

            } while (++retry_cnt < 32);

            if (partition.chunk == NULL)
                XKBLAS_FATAL("!! GPU IS OUT OF MEMORY !!");

            XKBLAS_DEBUG("Allocated a new chunk `%p` on device %d", partition.chunk, device->global_id);

            ////////////////////////////////////////////
            // Create a view from the allocated chunk //
            ////////////////////////////////////////////
            assert(partition.chunk);

            /* retrieve upper left corner */
            const Partite & corner = partition.get_corner();

            /* add a view */
            for (Partite & partite : partition.partites)
            {
                /* compute distance from corner */
                INTERVAL_DIFF_TYPE_T d[K];
                Cube::distance_manhattan(corner.cube, partite.cube, d);

                // TODO : the allocation is assumed col major, cuda
                static_assert(K == 2);
                const uintptr_t begin_addr = partition.chunk->device_ptr + d[1] + d[0]*ld*access->host_view.sizeof_type;

                MemoryReplicate & replicate = partite.block->replicates[device->global_id];
                const memory_allocation_view_id_t allocation_view_id = replicate.nallocations++;
                if (allocation_view_id >= MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX)
                    XKBLAS_FATAL("Too many allocations of the same data on the same device... Increase `MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX` and recompile XKBLAS");

                /* allocate the view of that block in the allocation */
                MemoryReplicateAllocationView * r = new MemoryReplicateAllocationView(partition.chunk, begin_addr, ld);
                replicate.allocations[allocation_view_id] = r;
                partite.dst_allocation_view_id = allocation_view_id;
            }
        }

        inline void
        fetch_access_find_allocation_continuous(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Partition & partition
        ) {
            assert(partition.chunk == nullptr);
            assert(this->is_locked());

            const memory_allocation_view_id_t nallocations = partition.partites[0].block->replicates[device->global_id].nallocations;
            const size_t nblocks = partition.partites.size();
            memory_allocation_view_id_bitfield_t j = 0;

            /* for each allocation of the block 0 */
            while (j < nallocations)
            {
                MemoryReplicateAllocationView * rj = partition.partites[0].block->replicates[device->global_id].allocations[j];

                /* for each other blocks */
                int i = 1;
                while (i < nblocks)
                {
                    /* for each allocation of other blocks */
                    memory_allocation_view_id_t nallocations = partition.partites[i].block->replicates[device->global_id].nallocations;
                    for (memory_allocation_view_id_t k = 0 ; k < nallocations ; ++k)
                    {
                        /* this block has a view with the same allocation, check next block */
                        MemoryReplicateAllocationView * rk = partition.partites[i].block->replicates[device->global_id].allocations[k];
                        if (rj->chunk == rk->chunk)
                        {
                            partition.partites[i].dst_allocation_view_id = k;
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
                partition.partites[0].dst_allocation_view_id = j;
                partition.chunk = rj->chunk;
                return ;

next_view:
                ++j;
                continue ;
            }

            /* no continuous allocation found */
            partition.chunk = nullptr;
        }

        inline void
        fetch_access_find_allocation(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Partition & partition
        ) {
            assert(this->is_locked());

            /* lookfor a continuous allocation already existing for that access block partitioning */
            this->fetch_access_find_allocation_continuous(driver, device, task, access, partition);
            if (partition.chunk)
                return ;

            /* no continuous allocation found, make a new one */
            XKBLAS_DEBUG("No continuous allocation found, reallocating and creating a new view");
            this->fetch_access_allocate(driver, device, task, access, partition);
        }

        inline void
        fetch_access_set_device_view(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Partition & partition
        ) {
            assert(this->is_locked());

            // we currently set the access view as the 'left-most' and 'upper-most' tile
            // (i.e with the smallest address - corresponding to the begining of this allocation)
            const Partite & partite = partition.get_corner();
            assert(partite.dst_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);

            const MemoryReplicateAllocationView * alloc_view = partite.block->replicates[device->global_id].allocations[partite.dst_allocation_view_id];
            assert(alloc_view);

            access->device_view = alloc_view->view;
        }

        inline void
        fetch_access_setup_copies(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Task * task,
            Access * access,
            Partition & partition
        ) {
            assert(this->is_locked());

            // if read mode is set
            if (access->mode & ACCESS_MODE_R)
            {
                xkblas_device_global_id_bitfield_t devbit = (1 << device->global_id);

                // for each block of that access
                for (Partite & partite : partition.partites)
                {
                    ///////////////
                    // SETUP DST //
                    ///////////////

                    // destinary allocation view id
                    const memory_allocation_view_id_t allocation_view_id = partite.dst_allocation_view_id;
                    assert(allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);

                    const memory_allocation_view_id_bitfield_t allocbit = (1 << allocation_view_id);

                    // partite is already valid on that device
                    MemoryReplicate & dst_replicate = partite.block->replicates[device->global_id];
                    if (dst_replicate.valid & allocbit)
                    {
                        partite.must_fetch = false;
                        continue ;
                    }
                    assert((partite.block->valid & devbit) == 0);

                    MemoryReplicateAllocationView * dst_alloc_view = dst_replicate.allocations[allocation_view_id];

                    /* increment task fetch counter */
                    task->fetching();
                    XKBLAS_DEBUG("Task `%s` fetching one by `%s` on `%p`",
                            task->label,
                            (dst_replicate.fetching & allocbit) ? "awaiting" : "launching",
                            dst_alloc_view->view.addr
                    );

                    // partite is already being fetched on that device, on the same allocation
                    if (dst_replicate.fetching & allocbit)
                    {
                        partite.must_fetch = false;
                        XKBLAS_DEBUG("Skipping fetch of a block already being fetched (concurrent read)");

                        // add the task to the awaiting list of that block
                        dst_alloc_view->awaiting.tasks.push_back(task);

                        continue ;
                    }

                    ///////////////
                    // SETUP SRC //
                    ///////////////

                    // find source:
                    //  - if its already valid on a device, use it as a source
                    //  - else, its already transfering to any device, wait for it and forward using D2D
                    //  - else, transfer H2D

                    // valid on some devices
                    if (partite.block->valid)
                    {
                        // must perform a D2D fetch
                        partite.must_fetch = true;

                        // create dst view
                        partite.dst_device_global_id = device->global_id;
                        partite.dst_view = dst_alloc_view->view;

                        // get a valid source
                        xkblas_device_global_id_t src = driver->f_get_source(device->global_id, partite.block->valid);
                        assert(partite.block->valid & (1 << src));

                        // Get device replicate
                        MemoryReplicate & src_replicate = partite.block->replicates[src];

                        // Get the first valid allocation on that device
                        memory_allocation_view_id_t src_alloc_view_id = (memory_allocation_view_id_t) (__builtin_ffs(src_replicate.valid) - 1);
                        assert(src_replicate.valid & (1 << src_alloc_view_id));
                        assert(0 <= src_alloc_view_id && src_alloc_view_id < src_replicate.nallocations);

                        // retrieve and set src view infos
                        MemoryReplicateAllocationView * r = src_replicate.allocations[allocation_view_id];
                        partite.src_device_global_id   = src;
                        partite.src_allocation_view_id = src_alloc_view_id;
                        partite.src_view               = dst_alloc_view->view;
                    }
# if USE_D2D_FORWARDING
                    // already fetching on some devices
                    else if (partite.block->fetching)
                    {
                        // the D2D fetch will be performed on other H2D completion
                        partite.must_fetch = false;

                        // no need to create partite.src and partite.dst as we are not fetching now

                        // one device is already fetching, add a D2D forward callback
                        xkblas_device_global_id_t src = driver->f_get_source(device->global_id, partite.block->fetching);
                        assert(0 <= src && src < XKBLAS_DEVICES_MAX);

                        MemoryReplicate & src_replicate = partite.block->replicates[src];
                        assert(src_replicate.fetching);

                        // TODO : maybe there is several fetching alloc ? in such case, which one to pick ? currently select the first one
                        memory_allocation_view_id_t src_alloc_view_id = (memory_allocation_view_id_t) (__builtin_ffs(src_replicate.fetching) - 1);
                        assert(0 <= src_alloc_view_id && src_alloc_view_id < src_replicate.nallocations);

                        MemoryReplicateAllocationView * src_alloc_view = src_replicate.allocations[src_alloc_view_id];
                        assert(src_alloc_view);

                        const MemoryForward forward(task, partition.chunk, device->global_id, dst_alloc_view->view);
                        src_alloc_view->awaiting.forwards.push_back(forward);
                    }
# endif /* USE_D2D_FORWARDING */
                    // not valid on any devices, assume valid on host
                    else
                    {
                        // must perform a H2D fetch
                        partite.must_fetch = true;

                        // create dst view
                        partite.dst_device_global_id = device->global_id;
                        partite.dst_view = dst_alloc_view->view;

                        // no device are fetching, assume valid on host and launch H2D
                        XKBLAS_DEBUG("No valid block... assuming host is valid");
                        partite.src_device_global_id = HOST_DEVICE_GLOBAL_ID;
                        assert(partite.src_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);
                    }

                    // this task must perform the fetch
                    dst_replicate.fetching  |= allocbit;
                    partite.block->fetching |= devbit;

                    //////////////////////////////
                    // ASSERTION ON SRC AND DST //
                    //////////////////////////////
                    assert(partite.dst_device_global_id   != partite.src_device_global_id ||
                           partite.dst_allocation_view_id != partite.src_allocation_view_id);

                } /* foreach partite */
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
                const xkblas_device_global_id_bitfield_t devbit = (1 << device->global_id);

                for (Partite & partite : search.partition.partites)
                {
                    # pragma message(TODO "Can validity be managed in a more lazy way ?")
                    for (int i = 0 ; i < XKBLAS_DEVICES_MAX ; ++i)
                    {
                        MemoryReplicate & replicate = partite.block->replicates[i];
                        replicate.valid = 0;
                    }

                    MemoryReplicate & replicate = partite.block->replicates[device->global_id];

                    # pragma message(TODO "Free each allocation")
                    # if 0
                    // release allocation that are no longer required, as we are rewritting that block.
                    XKBLAS_IMPL("Releasing allocations from a block...");
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
                    const memory_allocation_view_id_bitfield_t allocbit = (1 << partite.dst_allocation_view_id);
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
            Partition & partition
        ) {
            if (access->mode & ACCESS_MODE_R)
            {
                /* this code currently assumed 'dst' is a non-null device - so
                 * there must be a valid allocation */
                assert(partition.chunk);

                for (Partite & partite : partition.partites)
                {
                    if (!partite.must_fetch)
                        continue ;

                    /* this code is currently only executed when 'dst' is a device */
                    assert(0 <= partite.dst_device_global_id && partite.dst_device_global_id < XKBLAS_DEVICES_MAX);
                    assert(partite.dst_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);

                    // TODO: use this to debug single GPU correctnesss issue
                    # if 0
                    // TODO : remove me (begin)
                    uintptr_t addr = host_view.begin_addr();
                    assert(LAUNCHED.count(addr) == 0);
                    LAUNCHED[addr] = true;
                    # endif

                    // TODO : remove me (end)

                    /* host replicate view if no allocation were found */
                    const memory_replicate_view_t host_replicate_view(partite.host_view.begin_addr(), partite.host_view.ld);

                    this->fetch_access_launch_copy(
                        driver,
                        device,
                        task,
                        partition.chunk,
                        partite.cube,
                        partite.host_view,
                        partite.dst_device_global_id,
                       (partite.dst_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : partite.dst_view,
                        partite.src_device_global_id,
                       (partite.src_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : partite.src_view
                    );
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
            Search search(device->global_id);

            this->lock();
            {
                # pragma message(TODO "Step (1) and (2) could be merged to only search once")

                # if 0
                XKBLAS_DEBUG(
                    "Interval(%16d, %16d), Interval(%16d, %16d),",
                    access->cube[0].a, access->cube[0].b,
                    access->cube[1].a, access->cube[1].b
                );
                # endif

               /* step (1) ensure the access is represented in the tree as blocks */
                search.prepare_insert(access);
                this->insert(search, access->cube, access->mode);

                /* step (2) find all blocks representing the access */
                search.prepare_search_blocks();
                this->intersect(search, access->cube, access->mode);
                assert(search.partition.partites.size() >= 1);

                /* step (3) find or allocate continuous memory for that access on that device */
                this->fetch_access_find_allocation(driver, device, task, access, search.partition);

                /* step (4) set the access view on the device (that will be used by the kernel) */
                this->fetch_access_set_device_view(driver, device, task, access, search.partition);

                /* step (5) if read access, find src/dst, and setup views to transfer on step (7) */
                this->fetch_access_setup_copies(driver, device, task, access, search.partition);

                /* step (6) if write access, invalidate other copies */
                this->fetch_access_set_valid(driver, device, task, access, search);

            } /* this->lock(); */
            this->unlock();

            /* step (7) - launch transfers with info set on step (5) */
            this->fetch_access_launch_copies(driver, device, task, access, search.partition);
        }

        # pragma message(TODO "Driver and device shouldn't be passed as a parameter here... use device_global_id instead - the memory tree should abstract all that shit")
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

        //////////////////
        //  INVALIDATE  //
        //////////////////
        void
        invalidate(void)
        {
            // empty the tree
            this->clear();

            // release all memory allocated on every devices
            xkblas_memory_deallocate_all();
        }

        //////////////
        //  OCR     //
        //////////////

        xkblas_device_global_id_bitfield_t
        who_owns(
            Access * access
        ) {
            // find how much bytes are owned per device
            Search search;
            search.prepare_search_owners();
            this->lock();
            {
                this->intersect(search, access->cube, access->mode);
            }
            this->unlock();

            // find devices which owns the most bytes
            xkblas_device_global_id_bitfield_t owners = 0;
            size_t bytes_owned_max = 0;
            for (xkblas_device_global_id_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
            {
                const size_t bytes_owned = search.bytes_owned[device_global_id];
                if (bytes_owned_max < bytes_owned)
                {
                    bytes_owned_max = bytes_owned;
                    owners = (xkblas_device_global_id_bitfield_t) (1 << device_global_id);
                }
                else if (bytes_owned_max && bytes_owned_max == bytes_owned)
                    owners |= (xkblas_device_global_id_bitfield_t) (1 << device_global_id);
            }

            return owners;
        }

        //////////////
        //  INSERT  //
        //////////////

        Node *
        new_node(
            Search & search,
            const Cube & cube,
            const int k,
            const Color color
        ) const {
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            return new Node(search.access, cube, k, color);
        }

        Node *
        new_node(
            Search & search,
            const Cube & cube,
            const int k,
            const Color color,
            const NodeBase * inherit
        ) const {
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            assert(!cube.intersects(inherit->cube));
            return new Node(search.access, cube, k, color, reinterpret_cast<const Node *>(inherit));
        }
};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
