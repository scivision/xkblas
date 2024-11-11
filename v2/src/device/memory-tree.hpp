#ifndef __MEMORY_TREE_HPP__
# define __MEMORY_TREE_HPP__

# include "matrix-tile.h"
# include "device/consts.h"
# include "device/device.h"
# include "device/device-memory.h"
# include "device/driver.h"
# include "device/stream-instruction-submit.h"
# include "device/task.hpp"
# include "logger/logger.h"
# include "logger/todo.h"
# include "sync/bits.h"
# include "sync/lockable.hpp"

// tree cutting would suppress validity/transfer information of some blocks
# undef CUBE_TREE_CUT
# undef CUBE_TREE_REBALANCE
# include "sync/cube-tree.hpp"

# include <algorithm>  // std::sort
# include <cstdint>
# include <functional>

/*
 *  Set to '1' to enable the following heuristic :
 *      If some memory is not valid on any devices, but a device A is already fetching it from the host,
 *      then if a device B wants to fetch concurrently, it does not perform
 *      another H2D transfer but instead waits for 'A' to receive that will
 *      trigger a D2D from A to B
 *
 *  Set to '0' so multiple H2D are submitted concurrently
 */
# define USE_D2D_FORWARDING 1

# pragma message(TODO "Memory allocation is currently performed within a critical section... If memory eviction must be performed, this creates double-locking issues + a lot of time spent in the critical section. Reason is : we need a partition (in the memory tree) of the access to write the allocation information on each block of the partition")

# pragma message(TODO "'fetch' implementation could be optimize by reducing critical sections")

# pragma message(TODO "merge 'Replicate' on continuous "   \
        "memory addresses - for now, just perform one data "    \
        "transfer per block")

# pragma message(TODO "Nest classes into a 'KMemory' templated class - corresponding to a global view of the memory in 'K' dimensions")

# define MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX   (4)
# define MEMORY_REPLICATE_ALLOCATION_VIEW_NONE   (MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX)

typedef uint8_t memory_allocation_view_id_t;
static_assert(MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX <= (1 << (sizeof(memory_allocation_view_id_t)*8)));

typedef uint8_t memory_allocation_view_id_bitfield_t;
static_assert(MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX <= sizeof(memory_allocation_view_id_bitfield_t) * 8);

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

}; /* MemoryReplicateAllocationView */

// if this assertion, many bitwise operation in the runtime will be wrong as
// they are implicitly done on int32 : (1 << device_global_id) will be an int -
// should update the runtime with (1UL << device_global_id) - maybe use a macro
// for 'one' depending on that size
static_assert(sizeof(memory_allocation_view_id_bitfield_t) * 8 <= 32);

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

        /* array of allocations */
        MemoryReplicateAllocationView * allocations[MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX];
        volatile memory_allocation_view_id_t nallocations;

        /* valid allocations */
        volatile memory_allocation_view_id_bitfield_t valid;

        /* fetching allocations */
        volatile memory_allocation_view_id_bitfield_t fetching;

        static_assert(sizeof(memory_allocation_view_id_bitfield_t) * 8 >= MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX);

    public:
        KMemoryReplicate() : allocations(), nallocations(0), valid(0), fetching(0) {}
        KMemoryReplicate(const KMemoryReplicate & r)
        {
            (void) r;
            XKBLAS_FATAL("Implement copy constructor");
        }
        ~KMemoryReplicate() {}

}; /* MemoryReplicate */

/* a memory block, one per tree node */
template <int K>
class KMemoryBlock {

    using Cube = KCube<K>;
    using MemoryReplicate = KMemoryReplicate<K>;
    using MemoryReplicateAllocationView = KMemoryReplicateAllocationView<K>;

    public:

        /* per device replicate info */
        MemoryReplicate replicates[XKBLAS_DEVICES_MAX];

        /* valid devices (i.e. devices with at least one valid allocation) */
        volatile xkblas_device_global_id_bitfield_t valid;

        /* fetching devices (i.e. devices with at least one fetching allocation) */
        volatile xkblas_device_global_id_bitfield_t fetching;

    public:

        /* a new memory block, assume it is valid on the host */
        KMemoryBlock() :
            replicates(),
            valid(0),
            fetching(0)
        {}

        void
        memory_block_init(
            const Cube & block_cube,
            const KMemoryBlock & inheriting_block,
            const Cube & inheriting_cube,
            const size_t sizeof_type
        ) {
            /////////////////////////////////
            //  HOST_VIEW HAS TO BE OFFSET //
            /////////////////////////////////
            INTERVAL_DIFF_TYPE_T d[K];
            Cube::distance_manhattan(inheriting_cube, block_cube, d);

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
                for (memory_allocation_view_id_t i = 0 ; i < inheriting_replicate->nallocations ; ++i)
                {
                    const MemoryReplicateAllocationView * inheriting_allocation = inheriting_replicate->allocations[i];

                    // warning: 'ld' here depends on the allocation itself
                    const INTERVAL_DIFF_TYPE_T offset   = d[ACCESS_CUBE_ROW_DIM] + d[ACCESS_CUBE_COL_DIM] * inheriting_allocation->view.ld * sizeof_type;
                    const uintptr_t begin_addr          = (uintptr_t) ((INTERVAL_DIFF_TYPE_T) inheriting_allocation->view.addr + offset);
                    assert(begin_addr >= inheriting_allocation->chunk->device_ptr);

                    # pragma message(TODO "This memory is currently leaked when 'invalidate' is called")
                    MemoryReplicateAllocationView * allocation = new MemoryReplicateAllocationView(inheriting_allocation->chunk, begin_addr, inheriting_allocation->view.ld);
                    replicate->allocations[i] = allocation;
                    // allocation->awaiting must remain empty, tasks will be notified through the shrinked block
                }

                // dupplicate fetching / valid infos
                replicate->fetching = inheriting_replicate->fetching;
                replicate->valid    = inheriting_replicate->valid;
            }

            //////////////////////////////
            //  VALID BITS ARE STILL OK //
            //////////////////////////////

            this->valid = inheriting_block.valid;
            this->fetching = inheriting_block.fetching;
        }

        /* a block from splitting an existing one */
        KMemoryBlock(
            const Cube & block_cube,
            const KMemoryBlock & inheriting_block,
            const Cube & inheriting_cube,
            const size_t sizeof_type
        ) {
            static_assert(K == 2);
            this->memory_block_init(block_cube, inheriting_block, inheriting_cube, sizeof_type);
        }
        ~KMemoryBlock() {}

}; /* KMemoryBlock */

/* storage passed when searchingi n the tree */
template <int K>
class KMemoryTreeNodeSearch {

    using Access = KMemoryAccess<K>;
    using Cube = KCube<K>;
    using MemoryBlock = KMemoryBlock<K>;
    using MemoryForward = KMemoryForward<K>;
    using Task = KTask<K>;

    public:
        class Partite {

            public:

                /* memory block in the tree (WARNING : this is mutable outside a 'lock' section) */
                MemoryBlock * block;

                /* The cube of this block (intersection of the access with the tree node) */
                const Cube cube;

                /* dst device */
                xkblas_device_global_id_t dst_device_global_id;

                /* replicate allocation to use as dst (in MemoryReplicate::allocations) */
                memory_allocation_view_id_t dst_allocation_view_id;

                /* dst view */
                memory_replicate_view_t dst_view;

                /* source device */
                memory_allocation_view_id_t src_device_global_id;

                /* replicate allocation to use as src (in MemoryReplicate::allocations) */
                xkblas_device_global_id_t src_allocation_view_id;

                /* src view */
                memory_replicate_view_t src_view;

                /* true if this block is already being fetched by a concurrent read access */
                bool must_fetch;

            public:

                Partite(MemoryBlock * b, const Cube & r) :
                    block(b),
                    cube(r),
                    dst_device_global_id(HOST_DEVICE_GLOBAL_ID),
                    dst_allocation_view_id(MEMORY_REPLICATE_ALLOCATION_VIEW_NONE),
                    dst_view(),
                    src_device_global_id(HOST_DEVICE_GLOBAL_ID),
                    src_allocation_view_id(MEMORY_REPLICATE_ALLOCATION_VIEW_NONE),
                    src_view(),
                    must_fetch(true)
                {}

                virtual ~Partite() {}

                inline memory_view_t
                create_host_view(const size_t ld, const size_t sizeof_type) const
                {
                    const INTERVAL_TYPE_T       x = this->cube[ACCESS_CUBE_ROW_DIM].a;
                    const INTERVAL_DIFF_TYPE_T dx = this->cube[ACCESS_CUBE_ROW_DIM].length();
                    const INTERVAL_TYPE_T       y = this->cube[ACCESS_CUBE_COL_DIM].a;
                    const INTERVAL_DIFF_TYPE_T dy = this->cube[ACCESS_CUBE_COL_DIM].length();
                    assert(dx > 0);
                    assert(dy > 0);

                    memory_view_t host_view;

                    host_view.order         = MATRIX_COLMAJOR;
                    host_view.addr          = x + y * ld * sizeof_type;
                    host_view.ld            = ld;
                    host_view.offset_m      = 0;
                    host_view.offset_n      = 0;
                    host_view.m             = dx / sizeof_type;
                    host_view.n             = dy;
                    host_view.sizeof_type   = sizeof_type;

                    // accesses must be aligned on sizeof(type)
                    assert(host_view.m * sizeof_type == dx);

                    return host_view;
                }

                bool
                operator<(const Partite & p) const
                {
                    const bool is_left  = this->cube[ACCESS_CUBE_COL_DIM].a < p.cube[ACCESS_CUBE_COL_DIM].a;
                    const bool is_right = this->cube[ACCESS_CUBE_COL_DIM].a > p.cube[ACCESS_CUBE_COL_DIM].a;

                    if (is_left)
                        return true;

                    if (is_right)
                        return false;

                    // vertically aligned
                    assert(this->cube[ACCESS_CUBE_COL_DIM].a == p.cube[ACCESS_CUBE_COL_DIM].a);

                    const bool is_up    = this->cube[ACCESS_CUBE_ROW_DIM].a < p.cube[ACCESS_CUBE_ROW_DIM].a;
                    const bool is_down  = this->cube[ACCESS_CUBE_ROW_DIM].a > p.cube[ACCESS_CUBE_ROW_DIM].a;

                    if (is_up)
                        return true;

                    if (is_down)
                        return false;

                    // horizontally aligned
                    assert(this == &p);
                    return false;
                }

        }; /* Partite */

        class Partition {

            public:

                /* the partite of that partition */
                std::vector<Partite> partites;

                /* all partite share the same allocation chunk */
                xkblas_alloc_chunk_t * chunk;

            public:
                Partition() : partites(), chunk(nullptr) {}
                ~Partition() {}

                /* return the left-most and upper-most block of the partition */
                inline Partite &
                get_leftmost_uppermost_block(void)
                {
                    const size_t nblocks = this->partites.size();
                    int j = 0;

                    for (int i = 1 ; i < nblocks ; ++i)
                    {
                        const Partite & bi = this->partites[i];
                        const Partite & bj = this->partites[j];
                        if (bi < bj)
                            j = i;
                    }

                    return this->partites[j];
                }

                inline memory_view_t
                create_host_view(
                    const size_t ld,
                    const size_t sizeof_type,
                    const std::vector<size_t> sorted_partition_indices
                ) const {

                    const Partite & p0 = this->partites[sorted_partition_indices[0]];
                    const Partite & pf = this->partites[sorted_partition_indices[this->partites.size() - 1]];
                    assert(p0 < pf);

                    const INTERVAL_DIFF_TYPE_T x0 = (INTERVAL_DIFF_TYPE_T) p0.cube[ACCESS_CUBE_ROW_DIM].a;
                    const INTERVAL_DIFF_TYPE_T xf = (INTERVAL_DIFF_TYPE_T) pf.cube[ACCESS_CUBE_ROW_DIM].b;
                    const INTERVAL_DIFF_TYPE_T y0 = (INTERVAL_DIFF_TYPE_T) p0.cube[ACCESS_CUBE_COL_DIM].a;
                    const INTERVAL_DIFF_TYPE_T yf = (INTERVAL_DIFF_TYPE_T) pf.cube[ACCESS_CUBE_COL_DIM].b;
                    assert(0 <= x0 && x0 <= ld * sizeof_type);
                    assert(0 <= xf && xf <= ld * sizeof_type);
                    assert(y0 < yf);

                    INTERVAL_DIFF_TYPE_T n = yf - y0;
                    INTERVAL_DIFF_TYPE_T m = xf - x0;
                    if (m < 0)
                    {
                        m += ld * sizeof_type;
                        n -= 1;
                    }
                    m = m / sizeof_type;

                    memory_view_t host_view;
                    host_view.order        = MATRIX_COLMAJOR;
                    host_view.addr         = x0 + y0 * ld * sizeof_type;
                    host_view.ld           = ld;
                    host_view.offset_m     = 0;
                    host_view.offset_n     = 0;
                    host_view.m            = (size_t) m;
                    host_view.n            = (size_t) n;
                    host_view.sizeof_type  = sizeof_type;

                    return host_view;
                }

        }; /* Partition */

   public:

       /* different search type */
       enum Type : uint8_t {
           INSERTING_BLOCKS     = 0,    // insert new blocks
           SEARCH_FOR_PARTITION = 1,    // search for a partition
           SEARCH_AWAITING      = 2,    // search tasks awaiting on blocks (to be transfered onto a gpu, typically)
           SEARCH_OWNERS        = 3,    // search how many bytes owns each device
           SEARCH_FOR_BLOCKS    = 4     // search for blocks intersecting, but not necessarily being a partition (access can be larger than the ones inserted)
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
       // used if type == SEARCH_FOR_PARTITION //
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
           partition(),
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
       prepare_search_partition(void)
       {
           assert(this->partition.partites.size() == 0);
           this->partition.partites.clear();
           this->type = SEARCH_FOR_PARTITION;
       }

       void
       prepare_search_blocks(void)
       {
           assert(this->partition.partites.size() == 0);
           this->partition.partites.clear();
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
            block()
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
            const Cube & r,
            const int k,
            const Color color,
            const Node * src,
            const size_t sizeof_type
        ) :
            Base(r, k, color),
            block(r, src->block, src->cube, sizeof_type)
        {}

    public:

        void
        dump_str(FILE * f) const
        {
            KCubeTree<K, KMemoryTreeNodeSearch<K>>::Node::dump_str(f);
        }

        void
        dump_cube_str(FILE * f) const
        {
            // KCubeTree<K, DeviceInvalidCubes>::Node::dump_cube_str(f);
            # if 0
            fprintf(f, "\\\\ host-addr=%p", (void *) this->block.host_view.addr);
            fprintf(f, "\\\\ block size (m, n)=(%d, %d) - ld=%d", this->block.host_view.m, this->block.host_view.n, this->block.host_view.ld);
            fprintf(f, "\\\\ tile (m, n)=(%d, %d)",  this->block.host_view.offset_m, this->block.host_view.offset_n);
            # endif

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
        KMemoryTree(const size_t ld, const size_t sizeof_type, const bool merge_transfers) :
            Base(), ld(ld), sizeof_type(sizeof_type), merge_transfers(merge_transfers) {}
        ~KMemoryTree() {}

        /* the ld used in that memory tree */
        const size_t ld;

        /* the size of type */
        const size_t sizeof_type;

        /* whether transfers in continuous virtual memory should be merged */
        const bool merge_transfers;

    public:

        typedef struct  fetch_t
        {
            /* logical cubes representing that fetch */
            std::vector<Cube> cubes;

            /* the host memory view */
            memory_view_t host_view;

            /* src device id */
            xkblas_device_global_id_t src_device_global_id;

            /* src view */
            memory_replicate_view_t src_view;

            /* mark 'fetched' all the tasks awaiting on that allocation */
            xkblas_alloc_chunk_t * dst_chunk;

            /* dst device id */
            xkblas_device_global_id_t dst_device_global_id;

            /* dst view */
            memory_replicate_view_t dst_view;

        }               fetch_t;

        typedef struct  fetch_list_t
        {
            /* the memory tree */
            KMemoryTree * tree;

            /* list of fetches to submit */
            fetch_t * fetches;

            /* 'fetches' capacity */
            size_t capacity;

            /* number of 'fetches' set */
            size_t n;

            /* number of pending fetches */
            volatile std::atomic<size_t> pending;

            fetch_list_t(KMemoryTree * tree, fetch_t * fetches, size_t capacity) : tree(tree), fetches(fetches), capacity(capacity), n(0), pending(0) {}
            ~fetch_list_t() {}

            fetch_t *
            prepare_next_fetch(void)
            {
                fetch_t * fetch = this->fetches + this->n;
                ++this->n;
                assert(this->n <= this->capacity);
                new (fetch) fetch_t();
                return fetch;
            }

            /* the list can be deleted if this returns '0' */
            size_t
            fetched(void)
            {
                const size_t p = this->pending.fetch_sub(1, std::memory_order_relaxed);
                assert(p >= 0);
                return p;
            }

            void
            fetching(void)
            {
                this->pending.fetch_add(1, std::memory_order_relaxed);
            }
        }               fetch_list_t;

        static inline void
        fetch_callback_task(fetch_t * fetch, Task * task)
        {
            /* a fetch completed */
            XKBLAS_DEBUG("Task `%s` fetched `%p`", task->label, fetch->dst_chunk->device_ptr);
            if (task->fetched() == TASK_STATE_DATA_FETCHED)
            {
                xkblas_device_t * device = xkblas_device_get(fetch->dst_device_global_id);
                xkblas_device_task_execute(device, task);
                # pragma message(TODO "Here, we are not polling the offloader kernel streams... Do we want to ?")
            }
        }

        static void
        fetch_callback(const void * args[XKBLAS_CALLBACK_ARGS_MAX])
        {
            assert(XKBLAS_CALLBACK_ARGS_MAX >= 1);

            fetch_list_t * list = (fetch_list_t *) args[0];
            assert(list);

            KMemoryTree * tree = list->tree;
            assert(tree);

            size_t fetch_idx = (size_t) args[1];
            assert(fetch_idx >= 0 && fetch_idx < list->n);

            fetch_t * fetch = list->fetches + fetch_idx;
            XKBLAS_DEBUG("Fetch completed for allocation `%p`", fetch->dst_chunk->device_ptr);

            /* `fetch->dst_chunk` is the memory allocated chunk on which the data had been fetched.
             * Search in the tree for awaiting tasks and forwards */
            assert(fetch->dst_chunk);

            Search search(fetch->dst_device_global_id);
            search.prepare_search_awaiting(fetch->dst_chunk);
            tree->lock();
            {
                for (Cube & cube : fetch->cubes)
                    tree->intersect(search, cube, ACCESS_MODE_VOID);
            }
            tree->unlock();

            /* callback to release awaiting tasks */
            for (Task * & task : search.awaiting.tasks)
                fetch_callback_task(fetch, task);

            /* callback to forward the data to other devices */
            size_t nforwards = search.awaiting.forwards.size();
            if (nforwards)
            {
                fetch_list_t * forward_list = (fetch_list_t *) malloc(sizeof(fetch_list_t) + nforwards * sizeof(fetch_t));
                assert(forward_list);

                new (forward_list) fetch_list_t(tree, (fetch_t *) (forward_list + 1), nforwards);
                forward_list->fetching();

                for (size_t i = 0 ; i < nforwards ; ++i)
                {
                    MemoryForward & forward = search.awaiting.forwards[i];

                    assert(forward.task);
                    assert(forward.chunk);
                    assert(0 <= forward.device_global_id && forward.device_global_id < XKBLAS_DEVICES_MAX);

                    xkblas_device_t * device = xkblas_device_get(forward.device_global_id);
                    assert(device);

                    fetch_t * forward_fetch = forward_list->prepare_next_fetch();
                    assert(fetch);
                    assert(i == forward_list->n - 1);
                    forward_list->fetching();

                    forward_fetch->cubes                = fetch->cubes;                 // the logicial view hasn't changed
                    new (&forward_fetch->host_view) memory_view_t(fetch->host_view);    // the host view hasn't changed
                    forward_fetch->dst_chunk            = forward.chunk;
                    forward_fetch->dst_device_global_id = forward.device_global_id;     // use the forwarded dst device
                    forward_fetch->dst_view             = forward.view;                 // use the forwarded dst view
                    forward_fetch->src_device_global_id = fetch->dst_device_global_id;  // the current 'dst' is the new 'src'
                    forward_fetch->src_view             = fetch->dst_view;              // the current 'dst' is the new 'src'

                    tree->fetch_list_launch_ith(device, forward_list, i);
                }

                list->fetched();
            }
        }

        //////////////////////////////////////
        //  DECIDE SRC DEVICE WHEN FETCHING //
        //////////////////////////////////////

        static inline int
        fetch_access_find_src(
            xkblas_driver_t * driver,
            xkblas_device_global_id_t dst_device_global_id,
            xkblas_device_global_id_bitfield_t valid
        ) {
            xkblas_device_global_id_bitfield_t src = driver->f_get_source(dst_device_global_id, valid);
            if (src == -1) // Driver failed to find a valid source
                src = (xkblas_device_global_id_bitfield_t) (__builtin_ffs(valid) - 1);
            assert(src >= 0);
            return src;
        }

        ////////////////////////////////////////////////////////////
        // Create a list of fetch requests for the given accesses //
        ////////////////////////////////////////////////////////////

        /* convert a partition to a minimal fetch list, merging consecutive partite to a single transfer */
        fetch_list_t *
        fetch_list_from_partition(
            Partition & partition
        ) {
            size_t capacity = partition.partites.size();
            fetch_list_t * list = (fetch_list_t *) malloc(sizeof(fetch_list_t) + capacity * sizeof(fetch_t));
            assert(list);

            new (list) fetch_list_t(this, (fetch_t *) (list + 1), capacity);

            # pragma message(TODO "The current heuristic only merges if ALL partites are consecutive on the same devices, and all must be fetched, to a single fetch")

            /* if transfers merge is enabled */
            if (this->merge_transfers)
            {
                if (capacity > 1)
                {
                    bool merge_to_a_single_fetch = true;

                    std::vector<size_t> partition_indices(capacity);
                    for (size_t i = 0 ; i < capacity ; ++i)
                        partition_indices[i] = i;

                    std::sort(
                        std::begin(partition_indices),
                        std::end(partition_indices),
                        [partition](size_t i, size_t j) {
                            assert(i != j);
                            return partition.partites[i] < partition.partites[i];
                        }
                    );
                    for (size_t i = 0 ; i < capacity - 1 ; ++i)
                    {
                        Partite & pi = partition.partites[i];

                        size_t j = i + 1;
                        Partite & pj = partition.partites[j];

                        if (    pi.must_fetch               == false                        ||
                                pi.src_device_global_id     != pj.src_device_global_id      ||
                                pi.dst_device_global_id     != pj.dst_device_global_id      ||
                                pi.src_allocation_view_id   != pj.src_allocation_view_id    ||
                                pi.dst_allocation_view_id   != pj.dst_allocation_view_id
                        ) {
                            merge_to_a_single_fetch = false;
                            break ;
                        }
                    }

                    /* can be merged to a single fetch */
                    if (merge_to_a_single_fetch)
                    {
                        Partite & p0 = partition.partites[0];

                        /* set the views */
                        const memory_view_t host_view = partition.create_host_view(this->ld, this->sizeof_type, partition_indices);
                        const memory_replicate_view_t host_replicate_view(host_view.begin_addr(), this->ld);
                        const memory_replicate_view_t dst_view = (p0.dst_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : p0.dst_view;
                        const memory_replicate_view_t src_view = (p0.src_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : p0.src_view;

                        /* allocate fetch info for the callback argument */
                        fetch_t * fetch = list->prepare_next_fetch();
                        fetch->host_view            = host_view;
                        fetch->src_device_global_id = p0.src_device_global_id;
                        fetch->src_view             = src_view;
                        fetch->dst_chunk            = partition.chunk;
                        fetch->dst_device_global_id = p0.dst_device_global_id;
                        fetch->dst_view             = dst_view;

                        # pragma message(TODO "Here, we could minimize the number of cubes in the logical view")
                        for (Partite & partite : partition.partites)
                            fetch->cubes.push_back(partite.cube);

                        list->fetching();
                        return list;
                    }
                    else
                    {
                        /* cannot be merged, fallback to default case */
                    }

                } /* only 1 partite anyway */
            } /* if enabled transfers merging */

            for (Partite & partite : partition.partites)
            {
                if (!partite.must_fetch)
                    continue ;

                /* one replicate must be non-null (a null replicate means to use the host view) */
                assert(partite.dst_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE ||
                        partite.src_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);

                /* set the views */
                const memory_view_t host_view = partite.create_host_view(this->ld, this->sizeof_type);
                const memory_replicate_view_t host_replicate_view(host_view.begin_addr(), this->ld);
                const memory_replicate_view_t dst_view = (partite.dst_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : partite.dst_view;
                const memory_replicate_view_t src_view = (partite.src_allocation_view_id == MEMORY_REPLICATE_ALLOCATION_VIEW_NONE) ? host_replicate_view : partite.src_view;

                /* allocate fetch info for the callback argument */
                fetch_t * fetch = list->prepare_next_fetch();
                fetch->cubes.push_back(partite.cube);
                fetch->host_view            = host_view;
                fetch->src_device_global_id = partite.src_device_global_id;
                fetch->src_view             = src_view;
                fetch->dst_chunk            = partition.chunk;
                fetch->dst_device_global_id = partite.dst_device_global_id;
                fetch->dst_view             = dst_view;

                list->fetching();
            }

            return list;
        }

        void
        fetch_list_to_host_setup_partition(Partition & partition)
        {
            assert(this->is_locked());

            /* launch fetch on each device */
            for (Partite & partite : partition.partites)
            {
                MemoryBlock * block = partite.block;

                /* not valid on any device, then assume valid on the host */
                if (block->valid == 0)
                    continue ;

                /////////////////////////
                // SRC - FIND BEST SRC //
                /////////////////////////

                // take first valid device, and first valid allocation for all partites,
                // so continuous partites are on the same device and allocation
                // for merging them later

                // xkblas_device_global_id_t src = __random_set_bit(partite.block->valid) - 1;
                xkblas_device_global_id_t src = (xkblas_device_global_id_t) (__builtin_ffs(partite.block->valid) - 1);
                assert(src >= 0);

                // Get the first valid allocation on that device
                MemoryReplicate & src_replicate = partite.block->replicates[src];
                assert(src_replicate.nallocations > 0);
                assert(src_replicate.valid);

                const memory_allocation_view_id_t src_allocation_view_id = (memory_allocation_view_id_t) (__builtin_ffs(src_replicate.valid) - 1);
                assert(0 <= src_allocation_view_id && src_allocation_view_id < src_replicate.nallocations);

                // retrieve and set src view infos
                MemoryReplicateAllocationView * src_allocation_view = src_replicate.allocations[src_allocation_view_id];
                assert(src_allocation_view);

                // set partite transfer infos
                partite.src_allocation_view_id  = src_allocation_view_id;
                partite.src_device_global_id    = src;
                partite.src_view                = src_allocation_view->view;

                partite.dst_allocation_view_id  = MEMORY_REPLICATE_ALLOCATION_VIEW_NONE;
                partite.dst_device_global_id    = HOST_DEVICE_GLOBAL_ID;
                // no need to set partite->dst_view - host view will be used
            }
        }

        /* append D2H fetch request to the list matching the cube */
        template <int NC>
        fetch_list_t *
        fetch_list_to_host_from_cubes(
            const Cube cubes[NC]
        ) {
            # pragma message(TODO "merge continuous partites using the same 'chunk'")

            Search search(HOST_DEVICE_GLOBAL_ID);
            this->lock();
            {
                /* find all blocks that intersects with that access */
                search.prepare_search_blocks();
                for (int i = 0 ; i < NC ; ++i)
                    this->intersect(search, cubes[i], ACCESS_MODE_R);

                /*  setup partition for D2H copies */
                this->fetch_list_to_host_setup_partition(search.partition);
            }
            this->unlock();

            /* generate the fetch list */
            return this->fetch_list_from_partition(search.partition);
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
            XKBLAS_WARN("Evicting memory...");

            // TODO : currently deallocating as much as possible, maybe stop when there is a chunk big-enough of 'size'

            size_t freed = 0;
            auto f = [device, size, &freed](NodeBase * nodebase, void * args, bool & stop) {
                (void) args;

                Node * node = reinterpret_cast<Node *>(nodebase);
                assert(node);

                MemoryBlock & block = node->block;

                const memory_allocation_view_id_bitfield_t devbit = (memory_allocation_view_id_bitfield_t) (1 << device->global_id);

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

                    block.valid &= (memory_allocation_view_id_bitfield_t) ~devbit;

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

        inline xkblas_alloc_chunk_t *
        fetch_access_allocate(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Access * access
        ) {
            //////////////////////////
            // Allocate a new chunk //
            //////////////////////////

            # pragma message(TODO "Can we manage row/col major in a better way ? hardcoded col major here for cuda")
            const size_t size = access->host_view.m * access->host_view.n * access->host_view.sizeof_type;

            xkblas_alloc_chunk_t * chunk = nullptr;
            int retry_cnt = 0;

            do {

                chunk = xkblas_memory_allocate(driver, device, size);
                if (chunk)
                    return chunk;

                // TODO : polling is risky here, because it may take a lock on the
                // memory tree, and 'xkblas_memory_allocate' is called within a
                // memory-tree lock => double-lock deadlock

                // xkblas_device_poll(device);
                fetch_access_allocate_eviction(device, size);

            } while (++retry_cnt < 32);

            XKBLAS_FATAL("!! GPU IS OUT OF MEMORY !!");

            return nullptr;
        }

        /* Create a view for each partite of the partition, for the newly allocated chunk */
        inline void
        fetch_access_create_allocation_views(
            xkblas_device_t * device,
            Access * access,
            Partition & partition,
            xkblas_alloc_chunk_t * chunk
        ) {
            assert(chunk);

            /* allocate continuous memory for that access */
            # pragma message(TODO "Can we manage row/col major in a better way ? hardcoded col major here for cuda")

            const size_t          ld = access->host_view.m;            // cuda is col major
            const size_t sizeof_type = access->host_view.sizeof_type;

            /* retrieve upper left corner */
            const Partite & corner = partition.get_leftmost_uppermost_block();

            /* add a view */
            for (Partite & partite : partition.partites)
            {
                /* compute distance from corner */
                INTERVAL_DIFF_TYPE_T d[K];
                Cube::distance_manhattan(corner.cube, partite.cube, d);
                if (d[ACCESS_CUBE_ROW_DIM] < 0)
                {
                    d[ACCESS_CUBE_ROW_DIM] += this->ld * this->sizeof_type;
                    d[ACCESS_CUBE_COL_DIM] -= 1;
                }
                assert(d[ACCESS_CUBE_ROW_DIM] >= 0);
                assert(d[ACCESS_CUBE_COL_DIM] >= 0);

                const uintptr_t offset = d[ACCESS_CUBE_ROW_DIM] + d[ACCESS_CUBE_COL_DIM]*ld*sizeof_type;
                const uintptr_t begin_addr = chunk->device_ptr + offset;

                MemoryReplicate & replicate = partite.block->replicates[device->global_id];
                const uint8_t allocation_view_id = replicate.nallocations++;
                if (allocation_view_id >= MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX)
                    XKBLAS_FATAL("Too many allocations of the same data on the same device... Increase `MEMORY_REPLICATE_ALLOCATION_VIEWS_MAX` and recompile XKBLAS");

                /* allocate the view of that block in the allocation */
                MemoryReplicateAllocationView * r = new MemoryReplicateAllocationView(chunk, begin_addr, ld);
                replicate.allocations[allocation_view_id] = r;
                partite.dst_allocation_view_id = allocation_view_id;
            }
        }

        /* look for a continuous allocation that can store 'access' for the given partition */
        inline xkblas_alloc_chunk_t *
        fetch_access_find_allocation_continuous(
            xkblas_device_t * device,
            Partition & partition
        ) {
            assert(this->is_locked());

            uint8_t j = 0;
            int nallocations = partition.partites[0].block->replicates[device->global_id].nallocations;
            size_t nblocks = partition.partites.size();

            /* for each allocation of the block 0 */
            while (j < nallocations)
            {
                MemoryReplicateAllocationView * rj = partition.partites[0].block->replicates[device->global_id].allocations[j];

                /* for each other blocks */
                size_t i = 1;
                while (i < nblocks)
                {
                    /* for each allocation of other blocks */
                    int nallocations = partition.partites[i].block->replicates[device->global_id].nallocations;
                    for (uint8_t k = 0 ; k < nallocations ; ++k)
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
                return rj->chunk;

next_view:
                ++j;
                continue ;
            }
            return nullptr;
        }

        /* set 'partition.chunk' to a memory chunk and all 'partites.dst_allocation_view_id' to a view on that chunk */
        inline void
        fetch_access_find_allocation(
            xkblas_driver_t * driver,
            xkblas_device_t * device,
            Access * access,
            Partition & partition
        ) {
            assert(this->is_locked());

            /* lookfor a continuous allocation already existing for that access block partitioning */
            xkblas_alloc_chunk_t * chunk = this->fetch_access_find_allocation_continuous(device, partition);
            if (chunk == nullptr)
            {
                /* no continuous allocation found, make a new one */
                XKBLAS_DEBUG("No continuous allocation found, reallocating and creating a new view");
                chunk = this->fetch_access_allocate(driver, device, access);
                assert(chunk);

                /* create new views */
                this->fetch_access_create_allocation_views(device, access, partition, chunk);
            }

            partition.chunk = chunk;
        }

        inline void
        fetch_access_set_device_view(
            xkblas_device_t * device,
            Access * access,
            Search & search
        ) {
            assert(this->is_locked());

            // we currently set the access view as the 'left-most' and 'upper-most' tile
            // (i.e with the smallest address - corresponding to the begining of this allocation)
            const Partite & partite = search.partition.get_leftmost_uppermost_block();
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
            Partition & partition
        ) {
            assert(this->is_locked());

            // if read mode is set
            if (access->mode & ACCESS_MODE_R)
            {
                const xkblas_device_global_id_bitfield_t dst_devbit = (1 << device->global_id);

                // for each block of that access
                for (Partite & partite : partition.partites)
                {
                    ///////////////
                    // SETUP DST //
                    ///////////////

                    /* destinary allocation view id (cannot be host in this routine) */
                    const int dst_allocation_view_id = partite.dst_allocation_view_id;
                    assert(dst_allocation_view_id != MEMORY_REPLICATE_ALLOCATION_VIEW_NONE);
                    const memory_allocation_view_id_bitfield_t dst_allocbit = (memory_allocation_view_id_bitfield_t) (1 << dst_allocation_view_id);

                    /* partite and its view are already valid on that device */
                    MemoryReplicate & dst_replicate = partite.block->replicates[device->global_id];
                    if (dst_replicate.valid & dst_allocbit)
                    {
                        partite.must_fetch = false;
                        continue ;
                    }

                    /* retrieve the invalid view */
                    MemoryReplicateAllocationView * dst_allocation_view = dst_replicate.allocations[dst_allocation_view_id];

                    /* increment task fetch counter */
                    task->fetching();
                    XKBLAS_DEBUG("Task `%s` fetching one by `%s` on `%p`", task->label, (dst_replicate.fetching & dst_allocbit) ? "awaiting" : "launching", dst_allocation_view->view.addr);

                    /* register a task awaiting on the fetch completion */
                    dst_allocation_view->awaiting.tasks.push_back(task);

                    /* partite is already being fetched on that device */
                    if (dst_replicate.fetching & dst_allocbit)
                    {
                        partite.must_fetch = false;
                        XKBLAS_DEBUG("Skipping fetch of a block already being fetched (concurrent read)");
                        continue ;
                    }

                    ///////////////
                    // SETUP SRC //
                    ///////////////

                    // find source:
                    //  - if its already valid on a device, use it as a source
                    //  - else, its already transfering to any device, wait for it and forward using D2D
                    //  - else, transfer H2D

                    if (partite.block->valid)
                    {
                        partite.must_fetch = true;

                        /* create dst view */
                        partite.dst_device_global_id = device->global_id;
                        partite.dst_view = dst_allocation_view->view;

                        /* get a valid source */
                        xkblas_device_global_id_t src = driver->f_get_source(device->global_id, partite.block->valid);
                        assert(partite.block->valid & (1 << src));

                        /* Get the first valid allocation on that device */
                        MemoryReplicate & src_replicate = partite.block->replicates[src];
                        assert(src_replicate.nallocations > 0);
                        assert(src_replicate.valid);

                        memory_allocation_view_id_t src_allocation_view_id = (memory_allocation_view_id_t) (__builtin_ffs(src_replicate.valid) - 1);
                        assert(src_replicate.valid & (1 << src_allocation_view_id));
                        assert(0 <= src_allocation_view_id && src_allocation_view_id < src_replicate.nallocations);

                        /* Retrieve and set src view infos */
                        MemoryReplicateAllocationView * src_allocation_view = src_replicate.allocations[src_allocation_view_id];
                        partite.src_device_global_id   = src;
                        partite.src_allocation_view_id = src_allocation_view_id;
                        partite.src_view               = src_allocation_view->view;
                    }
                    # if USE_D2D_FORWARDING
                    else if (partite.block->fetching)
                    {
                        /* the fetch will be initiated by the other device that
                         * is already fetching that data for a D2D transfer.
                         * No need to create partite.src and partite.dst as we
                         * are not fetching now */
                        partite.must_fetch = false;

                        /* one device is already fetching, add a D2D forward callback */
                        xkblas_device_global_id_t fetching = driver->f_get_source(device->global_id, partite.block->fetching);
                        assert(0 <= fetching && fetching < XKBLAS_DEVICES_MAX);
                        assert(partite.block->fetching & (1 << fetching));

                        MemoryReplicate & fetching_replicate = partite.block->replicates[fetching];
                        assert(fetching_replicate.fetching);

                        // TODO : maybe there is several fetching alloc ?
                        // in such case, which one to pick ? currently select the first one

                        memory_allocation_view_id_t fetching_allocation_view_id = (memory_allocation_view_id_t) (__builtin_ffs(fetching_replicate.fetching) - 1);
                        assert(0 <= fetching_allocation_view_id && fetching_allocation_view_id < fetching_replicate.nallocations);

                        MemoryReplicateAllocationView * fetching_allocation_view = fetching_replicate.allocations[fetching_allocation_view_id];
                        assert(fetching_allocation_view);

                        XKBLAS_DEBUG("registered a forward for task `%s`", task->label);

                        const MemoryForward forward(task, partition.chunk, device->global_id, dst_allocation_view->view);
                        fetching_allocation_view->awaiting.forwards.push_back(forward);
                    }
                    # endif /* USE_D2D_FORWARDING */
                    else
                    {
                        assert(!partite.block->valid);
                        # if USE_D2D_FORWARDING
                        assert(!partite.block->fetching);
                        # endif /* !USE_D2D_FORWARDING */

                        partite.must_fetch = true;

                        /* create dst view */
                        partite.dst_device_global_id = device->global_id;
                        partite.dst_view = dst_allocation_view->view;

                        /* using host as src, which is assumed valid */
                        partite.src_device_global_id   = HOST_DEVICE_GLOBAL_ID;
                        partite.src_allocation_view_id = MEMORY_REPLICATE_ALLOCATION_VIEW_NONE;
                    }

                    /* update bitfields so no other concurrent fetch occurs */
                    dst_replicate.fetching  |= dst_allocbit;
                    partite.block->fetching |= dst_devbit;

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
            xkblas_device_t * device,
            Access * access,
            Partition & partition
        ) {
            assert(this->is_locked());

            /* if access has a write mode, invalidate all copies */
            if (access->mode & ACCESS_MODE_W)
            {
                const memory_allocation_view_id_bitfield_t devbit = (memory_allocation_view_id_bitfield_t) (1 << device->global_id);
                for (Partite & partite : partition.partites)
                {
                    const memory_allocation_view_id_bitfield_t allocbit = (memory_allocation_view_id_bitfield_t) (1 << partite.dst_allocation_view_id);

                    /* invalidate all replicates */
                    # pragma message(TODO "Can validity be managed in a more lazy way ?")
                    for (xkblas_device_global_id_t device_global_id = 0 ;
                            device_global_id < XKBLAS_DEVICES_MAX ;
                            ++device_global_id)
                    {
                        MemoryReplicate & replicate = partite.block->replicates[device_global_id];
                        replicate.valid = 0;
                    }
                    partite.block->valid = 0;

                    /* There is no concurrent access anyway, so make data valid now
                     * (even though the kernel has not executed, and the data is not rigourously valid yet) */
                    MemoryReplicate & replicate = partite.block->replicates[device->global_id];
                    replicate.valid = allocbit;
                    partite.block->valid = devbit;
                }
            }
        }

        /* launch a single fetch */
        inline void
        fetch_list_launch_ith(
            xkblas_device_t * device,
            fetch_list_t * list,
            size_t i
        ) {
            assert(i >= 0 && i < list->n);

            fetch_t * fetch = list->fetches + i;

            /* callback setup */
            assert(XKBLAS_CALLBACK_ARGS_MAX >= 2);
            xkblas_callback_t callback;
            callback.func = fetch_callback;
            callback.args[0] = list;
            callback.args[1] = (void *) i;

            /* launch asynchronous copy */
            xkblas_stream_instruction_submit_copy(
                device,
                fetch->host_view,
                fetch->dst_device_global_id,
                fetch->dst_view,
                fetch->src_device_global_id,
                fetch->src_view,
                callback
            );
        }

        inline void
        fetch_list_launch(
            xkblas_device_t * device,
            fetch_list_t * list
        ) {
            for (size_t i = 0 ; i < list->n ; ++i)
            {
                fetch_t * fetch = list->fetches + i;

                /* callback setup */
                assert(XKBLAS_CALLBACK_ARGS_MAX >= 2);
                xkblas_callback_t callback;
                callback.func = fetch_callback;
                callback.args[0] = list;
                callback.args[1] = (void *) i;

                /* launch asynchronous copy */
                xkblas_stream_instruction_submit_copy(
                    device,
                    fetch->host_view,
                    fetch->dst_device_global_id,
                    fetch->dst_view,
                    fetch->src_device_global_id,
                    fetch->src_view,
                    callback
                );
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

                # if 1
                XKBLAS_DEBUG(
                    "KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) %p, %lu, %lu, %lu, %lu, %lu, %lu, %s),",
                    access->host_view.addr,
                    access->host_view.ld,
                    access->host_view.offset_m,
                    access->host_view.offset_n,
                    access->host_view.m,
                    access->host_view.n,
                    access->host_view.sizeof_type,
                    access_mode_to_str(access->mode)
                );
                # endif

                /* step (1) ensure the access is represented in the tree as blocks */
                search.prepare_insert(access);
                this->insert(search, access->cubes[0], access->mode);
                this->insert(search, access->cubes[1], access->mode);

                /* step (2) find all blocks representing the access */
                search.prepare_search_partition();
                this->intersect(search, access->cubes[0], access->mode);
                this->intersect(search, access->cubes[1], access->mode);
                assert(search.partition.partites.size() >= 1);

                /* step (3) find or allocate continuous memory for that access on that device */
                this->fetch_access_find_allocation(driver, device, access, search.partition);

                /* step (4) set the access view on the device (that will be used by the kernel) */
                this->fetch_access_set_device_view(device, access, search);

                /* step (5) if read access, find src/dst, and setup views to transfer on step (7) */
                this->fetch_access_setup_copies(driver, device, task, access, search.partition);

                /* step (6) if write access, invalidate other copies */
                this->fetch_access_set_valid(device, access, search.partition);

            } /* this->lock(); */
            this->unlock();

            if (access->mode & ACCESS_MODE_R)
            {
                /* step (7) - convert a partition to the minimum number of fetches to run */
                fetch_list_t * list = this->fetch_list_from_partition(search.partition);

                /* step (8) - launch transfers */
                this->fetch_list_launch(device, list);
            }

            /* step (7) - launch copies for each partite */
            // this->fetch_access_launch_copies(driver, device, task, access, search.partition);
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

        memory_allocation_view_id_bitfield_t
        who_owns(
            const Access * access
        ) {
            // find how much bytes are owned per device
            Search search;
            search.prepare_search_owners();
            this->lock();
            {
                this->intersect(search, access->cubes[0], access->mode);
                this->intersect(search, access->cubes[1], access->mode);
            }
            this->unlock();

            // find devices which owns the most bytes
            memory_allocation_view_id_bitfield_t owners = 0;
            size_t bytes_owned_max = 0;
            for (xkblas_device_global_id_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
            {
                const size_t bytes_owned = search.bytes_owned[device_global_id];
                if (bytes_owned_max < bytes_owned)
                {
                    bytes_owned_max = bytes_owned;
                    owners = (memory_allocation_view_id_bitfield_t) (1 << device_global_id);
                }
                else if (bytes_owned_max && bytes_owned_max == bytes_owned)
                    owners |= (memory_allocation_view_id_bitfield_t) (1 << device_global_id);
            }

            return owners;
        }

        //////////////
        //  INSERT  //
        //////////////

        void
        on_insert(
            NodeBase * nodebase,
            Search & search,
            const access_mode_t mode
        ) {
            (void) nodebase;
            (void) search;
            (void) mode;
            assert(search.type == Search::Type::INSERTING_BLOCKS);
        }

        /* shrinking on dimension 'k' from 'this->cube[k]' to 'interval' */
        void
        on_shrink(
            NodeBase * nodebase,
            const Interval & interval,
            int k
        ) {
            static_assert(K == 2);
            Node * node = reinterpret_cast<Node *>(nodebase);

            assert(k < K);
            assert(node->cube[k].includes(interval));

            ///////////////////////
            //  SHRINK HOST VIEW //
            ///////////////////////

            assert(node->cube[k].a <= interval.a);
            const INTERVAL_DIFF_TYPE_T da = interval.a - node->cube[k].a;

            assert(node->cube[k].b >= interval.b);

            // must be aligned on sizeof(type)
            if (k == ACCESS_CUBE_ROW_DIM)
            {
                const INTERVAL_DIFF_TYPE_T db = node->cube[k].b - interval.b;
                (void) db;
                assert(da % this->sizeof_type == 0);
                assert(db % this->sizeof_type == 0);
            }

            // shrinked-left, gotta offset the views
            if (da)
            {
                // REPLICATES VIEW
                for (MemoryReplicate & replicate : node->block.replicates)
                {
                    for (memory_allocation_view_id_t i = 0 ; i < replicate.nallocations ; ++i)
                    {
                        MemoryReplicateAllocationView * allocation_view = replicate.allocations[i];
                        const INTERVAL_DIFF_TYPE_T offset = (k == ACCESS_CUBE_ROW_DIM) ? da : (da * allocation_view->view.ld * this->sizeof_type);
                        allocation_view->view.addr += offset;
                        assert(allocation_view->view.addr >= allocation_view->chunk->device_ptr);
                    }
                }
            }
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_stop_test(
            NodeBase * nodebase,
            Search & search,
            const Cube & cube,
            const access_mode_t mode
        ) const {

            (void) nodebase;
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
            NodeBase * nodebase,
            Search & search,
            const Cube & cube,
            const access_mode_t mode
        ) const {
            (void) mode;

            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);
            assert(cube.intersects(node->cube));

            switch (search.type)
            {
                case (Search::Type::SEARCH_FOR_PARTITION):
                {
                    /* intersecting against 'cube' that had been inserted
                     * previously, so 'node' must be a sub-block of 'cube'
                     */
                    assert(cube.includes(node->cube));
                    search.partition.partites.push_back(Partite(&(node->block), node->cube));
                    break ;
                }

                case (Search::Type::SEARCH_FOR_BLOCKS):
                {
                    search.partition.partites.push_back(Partite(&(node->block), node->cube));
                    break ;
                }

                /* search for tasks awaiting on that cube for a given allocation */
                case (Search::Type::SEARCH_AWAITING):
                {
                    const xkblas_device_global_id_bitfield_t devbit = (xkblas_device_global_id_bitfield_t) (1 << search.device_global_id);
                    MemoryReplicate & replicate = node->block.replicates[search.device_global_id];

                    /* for each allocation of that block */
                    for (memory_allocation_view_id_t allocation_view_id = 0 ; allocation_view_id < replicate.nallocations ; ++allocation_view_id)
                    {
                        const memory_allocation_view_id_bitfield_t allocbit = (memory_allocation_view_id_bitfield_t) (1 << allocation_view_id);
                        MemoryReplicateAllocationView * allocation_view = replicate.allocations[allocation_view_id];

                        /* if it matches the allocation being searched */
                        if (allocation_view->chunk == search.chunk)
                        {
                            /* move the awaiting tasks */
                            search.awaiting.tasks.insert(search.awaiting.tasks.end(), allocation_view->awaiting.tasks.begin(), allocation_view->awaiting.tasks.end());
                            allocation_view->awaiting.tasks.clear();

                            /* move awaiting forwards */
                            search.awaiting.forwards.insert(search.awaiting.forwards.end(), allocation_view->awaiting.forwards.begin(), allocation_view->awaiting.forwards.end());
                            allocation_view->awaiting.forwards.clear();

                            /* this replicate just got fetched and is now valid */

                            // this assertion is not always true, if coming from
                            // an ACCESS_MODE_W, the data was already set valid
                            // assert((replicate.valid & allocbit) == 0);
                            replicate.valid |= (memory_allocation_view_id_bitfield_t) allocbit;

                            assert(replicate.fetching & allocbit);
                            replicate.fetching &= (memory_allocation_view_id_bitfield_t) ~allocbit;

                            break ;
                        }
                    }

                    /* set device bits */
                    assert(replicate.valid);
                    node->block.valid |= devbit;

                    if (replicate.fetching == 0)
                        node->block.fetching &= ~devbit;

                    break ;
                }

                /* search for owners of the access */
                case (Search::Type::SEARCH_OWNERS):
                {
                    const size_t bytes = cube.size();
                    for (xkblas_device_global_id_t device_global_id = 0 ; device_global_id < XKBLAS_DEVICES_MAX ; ++device_global_id)
                        if (node->block.valid & (1 << device_global_id))
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
            (void) search;
            assert(search.type == Search::Type::INSERTING_BLOCKS);
            assert(!cube.intersects(inherit->cube));
            return new Node(cube, k, color, reinterpret_cast<const Node *>(inherit), this->sizeof_type);
        }
};

using MemoryTree = KMemoryTree<2>;

#endif /* __MEMORY_TREE_HPP__ */
