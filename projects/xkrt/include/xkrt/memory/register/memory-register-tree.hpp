/* ************************************************************************** */
/*                                                                            */
/*   memory-register-tree.hpp                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/28 06:36:09 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

/**
 *  Keep track of pinned, touched and memory being transfered
 *  This had been designed for K dimension in case nvidia gives us 2D memory
 *  pinning in the future
 */

#ifndef __MEMORY_REGISTER_TREE_HPP__
# define __MEMORY_REGISTER_TREE_HPP__

# include <xkrt/memory/access/common/khp-tree.hpp>

//  TODO : the design of this is terrible with a cyclic ownership with 'xkrt_runtime_t'
//  Redesign me !! This should be fully independent with callbacks that can be parametrized, raised with global device ids

# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/bits.h>
# include <xkrt/sync/lockable.hpp>

# include <xkrt/task/task.hpp>          // this should gtfo

# include <algorithm>  // std::sort
# include <cstdint>
# include <functional>
# include <numeric> // std::iota

/* storage passed when searching in the tree */
class MemoryRegisterTreeNodeSearch {

    public:
        typedef enum    Type
        {
            INSERTING,
            TOUCHING,
            TOUCHED,
            TRANSFERING,
            TRANSFERED,
            PINNING,
            PINNED

        }               Type;

    public:
       MemoryRegisterTreeNodeSearch(const Type & t) : type(t) {}
       virtual ~MemoryRegisterTreeNodeSearch() {}

   public:
       Type type;

}; /* MemoryRegisterTreeNodeSearch */

class MemoryRegisterBlock
{
    public:
        MemoryRegisterBlock() : interval(), state{} {}
        MemoryRegisterBlock(const Interval & i) : interval(i), state{} {}
        ~MemoryRegisterBlock() {}

    public:

        /* the memory region represented */
        Interval interval;

        /* block state */
        struct {

            /* the block is pinned */
            bool pinned     : 1;

            /* the block is being pinned */
            bool pinning    : 1;

            /* the block had been touched */
            bool touched    : 1;

            /* the block is being touched */
            bool touching   : 1;

        } state;

    public:

        bool
        operator<(const MemoryRegisterBlock & other) const
        {
            return this->interval < other.interval;
        }
};

# define K                 1
# define REBALANCE         0
# define CUT_ON_INSERT     0
# define MAINTAIN_SIZE     0
# define MAINTAIN_HEIGHT   0

class MemoryRegisterTreeNode : public KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>::Node{

    public:
        using Node = MemoryRegisterTreeNode;
        using Search = MemoryRegisterTreeNodeSearch;
        using BaseTree = KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>;
        using Base = typename BaseTree::Node;
        using Hypercube = KHypercube<K>;

    public:

        typedef struct  state_t
        {
            /* the block is pinned */
            std::atomic<bool> pinned;

            /* the block is being pinned */
            std::atomic<bool> pinning;

            /* the block had been touched */
            std::atomic<bool> touched;

            /* the block is being touched */
            std::atomic<bool> touching;

        }               state_t;

        /* the block state */
        state_t state;

    public:

        /* the cube was never accessed before, create a new node */
        MemoryRegisterTreeNode(
            const Hypercube & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            state{}
        {}

        /**
         * A new node is being created from a split, make it inherit its original node 'src'
         *  - r - the shrinked cube that this is inheriting from
         *  - k - the dimension that got splitted
         *  - color - the node color
         *  - src - the node that got split
         *
         * We have:
         *  U (src->hypercube, r) == the node cube before being shrinked
         *  n (src->hypercube, r) = {} - empty intersection
         */
        MemoryRegisterTreeNode(
            const Hypercube & r,
            const int k,
            const Color color,
            const Node * src
        ) :
            Base(r, k, color),
            state{}
        {
            (void) src;
            LOGGER_FATAL("TODO");
        }

    public:

        void
        dump_str(FILE * f) const
        {
            Base::dump_str(f);
        }

        void
        dump_hypercube_str(FILE * f) const
        {
            Base::dump_hypercube_str(f);
        }

}; /* MemoryRegisterTreeNode */

class MemoryRegisterTree : public KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>
{
    public:
        using Base = KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>;
        using Hypercube = KHypercube<K>;
        using Node = MemoryRegisterTreeNode;
        using NodeBase = typename KHPTree<K, MemoryRegisterTreeNodeSearch>::Node;
        using Search = MemoryRegisterTreeNodeSearch;

    public:
        typedef Search::Type Op;

    public:

        MemoryRegisterTree(void) : Base()
        {
            pthread_rwlock_init(&rwlock, NULL);
        }

        ~MemoryRegisterTree()
        {
            pthread_rwlock_destroy(&rwlock);
        }

    public:

        /**
         * read/write lock:
         *  - write = if inserting new nodes
         *  - read  = if updating existing nodes
         */
        pthread_rwlock_t rwlock;

        ////////////////////////////////////////////////////////
        // PUBLIC INTERFACES TO INTERACT WITH THE MEMORY TREE //
        ////////////////////////////////////////////////////////

        // Ensure the interval exists in the tree
        void
        ensure(Hypercube & h)
        {
            pthread_rwlock_wrlock(&this->rwlock);
            {
                Search search(Op::INSERTING);
                this->insert(search, h);
            }
            pthread_rwlock_unlock(&this->rwlock);
        }

        // Retrieve a list of intervals to prepare the operation 'op' that is:
        // TODO: what if only a small portion is ready for 'op' ?
        int
        run(const Hypercube & h, std::vector<MemoryRegisterBlock> & blocks, const Op & op)
        {
            // run the operation to swap the bits
            pthread_rwlock_rdlock(&this->rwlock);
            {
                Search search(op);
                this->intersect(search, h);
            }
            pthread_rwlock_rdlock(&this->rwlock);

            // intersects run in-order, so the blocks list is sorted by now
            // reduce the list with continuous intervals only
            assert(std::is_sorted(blocks.begin(), blocks.end()));

            // Index to write the next merged interval
            size_t write = 1;

            for (size_t i = 1; i < blocks.size(); ++i)
            {
                MemoryRegisterBlock &    last = blocks[write - 1];
                MemoryRegisterBlock & current = blocks[i];

                if (last.interval.b == current.interval.a)
                {
                    // Merge into last interval
                    last.interval.b = current.interval.b;
                }
                else
                {
                    // Move current to the next write position
                    blocks[write++] = current;
                }
            }

            // Remove the unused tail
            blocks.resize(write);

            return 0;
        }

        //////////////
        //  INSERT  //
        //////////////


        void
        on_insert(
            NodeBase * nodebase,
            Search & search
        ) {
            (void) nodebase;
            (void) search;
             // nothing to do
            assert(search.type == Search::Type::INSERTING);
        }

        /* shrinking on dimension 'k' from 'this->hypercube[k]' to 'interval' */
        void
        on_shrink(
            NodeBase * nodebase,
            const Interval & interval,
            int k
        ) {
            (void) nodebase;
            (void) interval;
            (void) k;
             // nothing to do
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_stop_test(
            NodeBase * nodebase,
            Search & search,
            const Hypercube & h
        ) const {

            (void) nodebase;
            (void) search;
            (void) h;
            // nothing to do
            return false;
        }

        /**
         * The passed cube is intersecting with 'this'
         */
        inline void
        on_intersect(
            NodeBase * nodebase,
            Search & search,
            const Hypercube & h
        ) const {

            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);
            assert(h.intersects(node->hypercube));

            switch (search.type)
            {
                case (Op::TOUCHING):
                {
                    if (!node->state.touched.load())
                        if (node->state.touching.compare_exchange_strong(false, true))
                            search.partites.push_back(MemoryRegisterBlock(node->hypercube, node->state));
                    break ;
                }

                case (Search::Type::TOUCHED):
                {
                    assert(!node->state.touched.load());
                    node->state.touched.store(true);
                    break ;
                }

                default:
                {
                    LOGGER_FATAL("Invalid search type in memory tree");
                    assert(0);
                }
            }
        }

        Node *
        new_node(
            Search & search,
            const Hypercube & h,
            const int k,
            const Color color
        ) const {
            assert(search.type == Op::INSERTING);
            return new Node(search.access, h, k, color);
        }

        Node *
        new_node(
            Search & search,
            const Hypercube & h,
            const int k,
            const Color color,
            const NodeBase * inherit
        ) const {
            (void) search;
            assert(search.type == Op::INSERTING);
            assert(!h.intersects(inherit->hypercube));
            return new Node(h, k, color, reinterpret_cast<const Node *>(inherit), this->sizeof_type);
        }
};

# undef K
# undef REBALANCE
# undef CUT_ON_INSERT
# undef MAINTAIN_SIZE
# undef MAINTAIN_HEIGHT

#endif /* __MEMORY_REGISTER_TREE_HPP__ */
