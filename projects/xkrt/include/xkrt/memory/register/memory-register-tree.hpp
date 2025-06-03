/* ************************************************************************** */
/*                                                                            */
/*   memory-register-tree.hpp                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/06/03 02:55:31 by Romain PEREIRA            \_)     (_/    */
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

# define K                 1
# define REBALANCE         0
# define CUT_ON_INSERT     0
# define MAINTAIN_SIZE     0
# define MAINTAIN_HEIGHT   0

class MemoryRegisterBlock;

/* storage passed when searching in the tree */
class MemoryRegisterTreeNodeSearch {

    public:
        typedef enum    Type
        {
            INSERTING,
            TOUCHING,
            TOUCHED,
            PINNING,
            PINNED,
            UNPINNING,
            UNPINNED,
            TRANSFERING,
            TRANSFERED
        }               Type;

    public:
       MemoryRegisterTreeNodeSearch(
           const Type & t,
           std::vector<MemoryRegisterBlock> * b,
           int pid
       ) : type(t), blocks(b), pinning_id(pid) {}

       virtual ~MemoryRegisterTreeNodeSearch() {}

   public:
       Type type;
       std::vector<MemoryRegisterBlock> * blocks;
        int pinning_id;

}; /* MemoryRegisterTreeNodeSearch */



class MemoryRegisterTreeNode : public KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>::Node
{
    public:
        using Node = MemoryRegisterTreeNode;
        using Search = MemoryRegisterTreeNodeSearch;
        using BaseTree = KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>;
        using Base = typename BaseTree::Node;
        using Hyperrect = KHyperrect<K>;

    public:

        typedef struct  state_t
        {
            /* the block is pinned */
            std::atomic<bool> pinned;

            /* the block is being pinned */
            std::atomic<bool> pinning;

            /* the block is being pinned */
            std::atomic<bool> unpinning;

            /* the block had been touched */
            std::atomic<bool> touched;

            /* the block is being touched */
            std::atomic<bool> touching;

            /* the block is being transfered */
            std::atomic<bool> transfering;

        }               state_t;

        /* the block state */
        state_t state;

        /* node pinning id */
        int pinning_id;

    public:

        /* the rect was never accessed before, create a new node */
        MemoryRegisterTreeNode(
            const Hyperrect & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            state{}
        {}

        /**
         * A new node is being created from a split, make it inherit its original node 'src'
         *  - r - the shrinked rect that this is inheriting from
         *  - k - the dimension that got splitted
         *  - color - the node color
         *  - src - the node that got split
         *
         * We have:
         *  U (src->hyperrect, r) == the node rect before being shrinked
         *  n (src->hyperrect, r) = {} - empty intersection
         */
        MemoryRegisterTreeNode(
            const Hyperrect & r,
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
        dump_hyperrect_str(FILE * f) const
        {
            Base::dump_hyperrect_str(f);
        }

}; /* MemoryRegisterTreeNode */

class MemoryRegisterBlock
{
    public:
        MemoryRegisterBlock() : interval(), state{} {}

        MemoryRegisterBlock(
            const Interval & i,
            MemoryRegisterTreeNode::state_t & s,
            int pid
        ) :
            interval(i),
            state{
                .pinned      = s.pinned,
                .pinning     = s.pinning,
                .unpinning   = s.unpinning,
                .touched     = s.touched,
                .touching    = s.touching,
                .transfering = s.transfering,
                .padding     = 0
            },
            pinning_id(pid)
        {}

        ~MemoryRegisterBlock() {}

    public:

        /* the memory region represented */
        Interval interval;

        /* block state */
        union {
            struct {

                /* the block is pinned */
                bool pinned      : 1;

                /* the block is being pinned */
                bool pinning     : 1;

                /* the block is being unpinned */
                bool unpinning   : 1;

                /* the block had been touched */
                bool touched     : 1;

                /* the block is being touched */
                bool touching    : 1;

                /* the block is being transfered */
                bool transfering : 1;

                /* padding for char */
                bool padding     : 2;

            } state;

            unsigned char state_char;
        };

        /* block pinning id, for merging unpinning requests */
        int pinning_id;

    public:

        bool
        operator<(const MemoryRegisterBlock & other) const
        {
            return this->interval < other.interval;
        }
};

class MemoryRegisterTree : public KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>
{
    public:
        using Base = KHPTree<K, MemoryRegisterTreeNodeSearch, REBALANCE, CUT_ON_INSERT, MAINTAIN_SIZE, MAINTAIN_HEIGHT>;
        using Hyperrect = KHyperrect<K>;
        using Node = MemoryRegisterTreeNode;
        using NodeBase = typename KHPTree<K, MemoryRegisterTreeNodeSearch>::Node;
        using Search = MemoryRegisterTreeNodeSearch;

    public:
        typedef Search::Type Op;

    public:

        MemoryRegisterTree(void) : Base(), pinning_ids()
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

        /**
         *  an identifier for pinning requests
         */
        std::atomic<int> pinning_ids;

        ////////////////////////////////////////////////////////
        // PUBLIC INTERFACES TO INTERACT WITH THE MEMORY TREE //
        ////////////////////////////////////////////////////////

        // Ensure the interval exists in the tree
        void
        ensure(const Interval & interval)
        {
            const Interval intervals[1] = { interval };
            Hyperrect h(intervals);
            Search search(Op::INSERTING, NULL, 0);

            pthread_rwlock_wrlock(&this->rwlock);
            {
                this->insert(search, h);
            }
            pthread_rwlock_unlock(&this->rwlock);
        }

        // Retrieve a list of intervals to prepare the operation 'op' that is:
        // TODO: what if only a small portion is ready for 'op' ?
        int
        run(
            const Interval & interval,
            std::vector<MemoryRegisterBlock> * blocks,
            const Op & op
        ) {
            const Interval intervals[1] = { interval };
            const Hyperrect h(intervals);

            // search
            int pinning_id = op == Op::PINNED ? 1 + this->pinning_ids.fetch_add(1, std::memory_order_relaxed) : 0;
            Search search(op, blocks, pinning_id);

            // run the operation to swap the bits
            pthread_rwlock_rdlock(&this->rwlock);
            {
                this->intersect(search, h);
            }
            pthread_rwlock_unlock(&this->rwlock);

            if (blocks)
            {
                // intersects run in-order, so the blocks list is sorted by now
                // reduce the list with continuous intervals only
                assert(std::is_sorted(blocks->begin(), blocks->end()));

                // Index to write the next merged interval
                size_t write = 1;

                for (size_t i = 1; i < blocks->size(); ++i)
                {
                    MemoryRegisterBlock &    last = (*blocks)[write - 1];
                    MemoryRegisterBlock & current = (*blocks)[i];

                    // merge if in same state, with same pinning id, and continuous
                    if (last.state_char == current.state_char && last.pinning_id == current.pinning_id && last.interval.b == current.interval.a)
                    {
                        // Merge into last interval
                        last.interval.b = current.interval.b;
                    }
                    else
                    {
                        // Move current to the next write position
                        (*blocks)[write++] = current;
                    }
                }

                // Remove the unused tail
                blocks->resize(write);
            }

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

        /* shrinking on dimension 'k' from 'this->hyperrect[k]' to 'interval' */
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
            const Hyperrect & h
        ) const {

            (void) nodebase;
            (void) search;
            (void) h;
            // nothing to do
            return false;
        }

        /**
         * The passed rect is intersecting with 'this'
         */
        inline void
        on_intersect(
            NodeBase * nodebase,
            Search & search,
            const Hyperrect & h
        ) const {

            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);
            assert(h.intersects(node->hyperrect));

            switch (search.type)
            {
                case (Op::TOUCHING):
                {
                    // try to take the lock on the interval by setting the touching bit
                          bool expected = false;
                    const bool newvalue = true;
                    if (node->state.touching.compare_exchange_strong(expected, newvalue))
                    {
                        // if the segment was already touched, release the touching bit lock
                        if (node->state.touched.load())
                        {
                            assert(node->state.touching.load() == true);
                            node->state.touching.store(false);
                        }
                        // else, it must be touched
                        else
                        {
                            search.blocks->push_back(
                                MemoryRegisterBlock(node->hyperrect[0], node->state, 0)
                            );
                        }
                    }
                    break ;
                }

                case (Search::Type::TOUCHED):
                {
                    assert( node->state.touching.load());
                    assert(!node->state.touched.load());
                    node->state.touching.store(false);
                    node->state.touched.store(true);
                    break ;
                }

                case (Op::PINNING):
                {
                    assert(node->state.pinned.load() == false);

                          bool expected = false;
                    const bool newvalue = true;
                    if (node->state.pinning.compare_exchange_strong(expected, newvalue))
                    {
                        node->state.touching.store(true);
                        search.blocks->push_back(
                            MemoryRegisterBlock(node->hyperrect[0], node->state, 0)
                        );
                    }
                    else
                    {
                        LOGGER_FATAL("Tried to pin the same memory twice");
                    }
                    break ;
                }

                case (Search::Type::PINNED):
                {
                    assert( node->state.pinning.load());
                    assert(!node->state.pinned.load());
                    node->pinning_id = search.pinning_id;
                    node->state.pinning.store(false);
                    node->state.touched.store(true);
                    node->state.pinned.store(true);
                    break ;
                }

                case (Op::UNPINNING):
                {
                    assert(node->state.pinned.load());
                    assert(node->pinning_id > 0);

                          bool expected = false;
                    const bool newvalue = true;
                    if (node->state.unpinning.compare_exchange_strong(expected, newvalue))
                    {
                        node->state.unpinning.store(true);
                        search.blocks->push_back(
                            MemoryRegisterBlock(node->hyperrect[0], node->state, node->pinning_id)
                        );
                    }
                    else
                    {
                        LOGGER_FATAL("Tried to unpin the same memory segment twice");
                    }
                    break ;
                }

                case (Search::Type::UNPINNED):
                {
                    assert(!node->state.pinning.load());
                    assert( node->state.pinned.load());
                    assert( node->state.unpinning.load());
                    assert( node->pinning_id > 0);
                    node->pinning_id = 0;
                    node->state.pinned.store(false);
                    node->state.unpinning.store(false);
                    break ;
                }

                case (Search::Type::TRANSFERING):
                {
                    // TODO : append blocks to list if not being pinned/touched/unpinned
                    // otherwise, add a completion callback that will launch the transfer
                    LOGGER_FATAL("TODO");
                    break ;
                }

                case (Search::Type::TRANSFERED):
                {
                    assert(!node->state.pinning.load());
                    assert(!node->state.unpinning.load());
                    assert(!node->state.touching.load());
                    assert( node->state.transfering.load());
                    node->state.touched.store(true);
                    node->state.transfering.store(false);
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
            const Hyperrect & h,
            const int k,
            const Color color
        ) const {
            assert(search.type == Op::INSERTING);
            return new Node(h, k, color);
        }

        Node *
        new_node(
            Search & search,
            const Hyperrect & h,
            const int k,
            const Color color,
            const NodeBase * inherit
        ) const {
            (void) search;
            assert(search.type == Op::INSERTING);
            assert(!h.intersects(inherit->hyperrect));
            return new Node(h, k, color, reinterpret_cast<const Node *>(inherit));
        }
};

# undef K
# undef REBALANCE
# undef CUT_ON_INSERT
# undef MAINTAIN_SIZE
# undef MAINTAIN_HEIGHT

#endif /* __MEMORY_REGISTER_TREE_HPP__ */
