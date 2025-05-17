/* ************************************************************************** */
/*                                                                            */
/*   dependency-tree.hpp                                                      */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/11 22:17:10 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# define KHP_TREE_REBALANCE         0
# define KHP_TREE_CUT_ON_INSERT     0
# define KHP_TREE_MAINTAIN_SIZE     0
# define KHP_TREE_MAINTAIN_HEIGHT   0
# include <xkrt/memory/access/common/khp-tree.hpp>
# include <xkrt/memory/access/dependency-domain.hpp>
# include <xkrt/task/task.hpp>

# include <vector>
# include <unordered_map>

template<int K>
class KDependencyTreeSearch
{
    public:
        enum Type
        {
            SEARCH_TYPE_RESOLVE,
            SEARCH_TYPE_CONFLICTING
        };

    public:
        Type type;

        // USED IF TYPE == SEARCH_TYPE_RESOLVE or type == SEARCH_TYPE_CONFLICTING
        access_t * access;

        // USED IF TYPE == SEARCH_TYPE_CONFLICTING
        std::vector<void *> * conflicts;

    public:
        KDependencyTreeSearch() {}
        ~KDependencyTreeSearch() {}

    public:
        void
        prepare_resolve(access_t * access)
        {
            this->type = SEARCH_TYPE_RESOLVE;
            this->access = access;
        }

        void
        prepare_conflicting(
            std::vector<void *> * conflicts,
            access_t * access
        ) {
            this->type = SEARCH_TYPE_CONFLICTING;
            this->conflicts = conflicts;
            this->access = access;
        }

} /* class KDependencyTreeSearch */;

template <int K>
class KDependencyTreeNode : public KHPTree<K, KDependencyTreeSearch<K>>::Node {

    using Base      = typename KHPTree<K, KDependencyTreeSearch<K>>::Node;
    using Node      = KDependencyTreeNode<K>;
    using Hypercube = KHypercube<K>;
    using Search    = KDependencyTreeSearch<K>;

    public:

        /* last tasks that performed a read access */
        std::vector<access_t *> last_reads;

        /* last task that performed a write access */
        access_t * last_write;

        /* number of writes in all subtrees */
        int nwrites;

    public:

        KDependencyTreeNode<K>(
            const Hypercube & h,
            const int k,
            const Color color
        ) :
            Base(h, k, color),
            last_reads(),
            last_write(),
            nwrites(0)
        {
        }

        /* a new node from a split, inherit 'src' accesses */
        KDependencyTreeNode<K>(
            const Hypercube & h,
            const int k,
            const Color color,
            const Node * inherit
        ) :
            Base(h, k, color),
            last_reads(),
            last_write(),
            nwrites(0)
        {
            this->last_write = inherit->last_write;
            this->last_reads.insert(
                this->last_reads.end(),
                inherit->last_reads.begin(),
                inherit->last_reads.end()
            );
        }

        ////////////
        // UPDATE //
        ////////////
        inline void
        update_includes_nwrites(void)
        {
            this->nwrites = this->last_write ? 1 : 0;
            FOREACH_CHILD_BEGIN(this, child, k, dir)
            {
                this->nwrites += child->nwrites;
            }
            FOREACH_CHILD_END(this, child, k, dir);
        }

        inline void
        update_includes(void)
        {
            KHPTree<K, KDependencyTreeSearch<K>>::Node::update_includes();
            this->update_includes_nwrites();
        }

        void
        dump_str(FILE * f) const
        {
            KHPTree<K, KDependencyTreeSearch<K>>::Node::dump_str(f);
            fprintf(f, "\\nreads=%zu\\nwrites=%d", this->last_reads.size(), this->last_write->task ? 1 : 0);
        }

        void
        dump_hypercube_str(FILE * f) const
        {
            KHPTree<K, KDependencyTreeSearch<K>>::Node::dump_hypercube_str(f);

            fprintf(f, "\\\\ reads=%zu \\\\ writes=%d", this->last_reads.size(), this->last_write->task ? 1 : 0);
            fprintf(f, "\\\\ nwrites = %d ", this->nwrites);
            fprintf(f, "\\\\ reads = [ ");
            for (const access_t * access : this->last_reads)
                fprintf(f, "%p ", access->task);
            fprintf(f, "]");
        }
};

template<int K>
class KDependencyTree : public KHPTree<K, KDependencyTreeSearch<K>>, public DependencyDomain
{
    using Base      = KHPTree<K, KDependencyTreeSearch<K>>;
    using Hypercube = KHypercube<K>;

    public:
        using Node      = KDependencyTreeNode<K>;
        using NodeBase  = Base::Node;
        using Search    = KDependencyTreeSearch<K>;

        /* alignment is ld.sizeof_type */
        KDependencyTree(const size_t ld, const size_t sizeof_type) :
            Base(), ld(ld), sizeof_type(sizeof_type) {}
        ~KDependencyTree() {}

        /* alignement for this dep tree */
        const size_t ld;
        const size_t sizeof_type;

    public:

        inline void
        conflicting(
            std::vector<void *> * conflicts,
            access_t * access
        ) {
            // impl assumes this
            assert((access->mode & ACCESS_MODE_R) && !(access->mode & ACCESS_MODE_W));

            Search search;
            search.prepare_conflicting(conflicts, access);
            Base::intersect(search, access->hypercubes[0]);
            Base::intersect(search, access->hypercubes[1]);
        }

        //////////////
        //  INSERT  //
        //////////////

        inline void
        on_insert(
            NodeBase * nodebase,
            Search & search
        ) {
            assert(nodebase);
            assert(search.type == Search::Type::SEARCH_TYPE_RESOLVE);

            Node * node = reinterpret_cast<Node *>(nodebase);
            if (search.access->mode & ACCESS_MODE_W)
            {
                node->last_reads.clear();
                node->last_write = search.access;
            }
            else if (search.access->mode == ACCESS_MODE_R)
                node->last_reads.push_back(search.access);
        }

        inline void
        on_shrink(
            NodeBase * nodebase,
            const Interval & interval,
            int k
        ) {
            (void) nodebase;
            (void) interval;
            (void) k;
        }

        Node *
        new_node(
            Search & search,
            const Hypercube & h,
            const int k,
            const Color color
        ) const {
            (void) search;
            return new Node(h, k, color);
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
            return new Node(h, k, color, reinterpret_cast<const Node *>(inherit));
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
            (void) h;

            Node * node = reinterpret_cast<Node *>(nodebase);
            assert(node);

            assert(search.access);
            return (search.access->mode == ACCESS_MODE_R) && (node->nwrites == 0);
        }

        static inline void
        __access_precedes(access_t * pred, access_t * succ)
        {
            pred->successors.push_back(succ);
        }

        static inline void
        precedence(access_t * pred, access_t * succ)
        {
            // succ must be a dependent task
            assert(succ->task->flags & TASK_FLAG_DEPENDENT);

            // succ must have a wc>0 at this point: we are still processing dependencies, it cannot be scheduled yet
            assert(TASK_DEP_INFO(succ->task)->wc > 0);

            // succ has reached the maximum number of dependencies
            assert(TASK_DEP_INFO(succ->task)->wc < (1 << (8 * sizeof(task_wait_counter_type_t)) - 1));

            // avoid redundant edges
            if (pred->successors.size() && pred->successors.back()->task == succ->task)
                return ;

            // set edge
            __task_precedes(pred->task, succ->task, __access_precedes, pred, succ);
        }

        inline void
        on_intersect(
            NodeBase * nodebase,
            Search & search,
            const Hypercube & h
        ) const {

            (void) h;

            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);

            switch (search.type)
            {
                case (Search::Type::SEARCH_TYPE_RESOLVE):
                {
                    if ((search.access->mode & ACCESS_MODE_W) && node->last_reads.size())
                        for (access_t * pred : node->last_reads)
                            precedence(pred, search.access);
                    else if (node->last_write)
                        precedence(node->last_write, search.access);

                    break ;
                }

                case (Search::Type::SEARCH_TYPE_CONFLICTING):
                {
                    if (node->last_write)
                    {
                        assert(search.conflicts);
                        search.conflicts->push_back(node);
                    }

                    break ;
                }

                default:
                {
                    assert(0);
                    break ;
                }
            }
        }

        template<int AC>
        inline void
        intersect(access_t * accesses)
        {
            Search search;
            for (int i = 0 ; i < AC ; ++i)
            {
                access_t * access = accesses + i;
                assert(this->can_resolve(access));

                search.prepare_resolve(access);
                Base::intersect(search, access->hypercubes[0]);
                Base::intersect(search, access->hypercubes[1]);
            }
        }

        template<int AC>
        inline void
        insert(access_t * accesses)
        {
            Search search;
            for (int i = 0 ; i < AC ; ++i)
            {
                access_t * access = accesses + i;

                search.prepare_resolve(access);
                Base::insert(search, access->hypercubes[0]);
                Base::insert(search, access->hypercubes[1]);
            }
        }

        template<int AC>
        inline void
        resolve(access_t * accesses)
        {
            # pragma message(TODO "If we semantically force a accesses region to be disjointed, then these 2 loops can be merged with no risks of dependency cycle")
            this->intersect<AC>(accesses);
            this->insert<AC>(accesses);
        }

        void
        resolve(access_t * access, int naccesses)
        {
            (void) access;
            (void) naccesses;
            LOGGER_FATAL("not implemented");
        }

        bool
        can_resolve(const access_t * access) const
        {
            assert(access);
            return (this->ld == access->host_view.ld) && (this->sizeof_type == access->host_view.sizeof_type);
        }
};

using DependencyTree = KDependencyTree<2>;

#endif /* __DEPENDENCY_TREE_HPP__ */
