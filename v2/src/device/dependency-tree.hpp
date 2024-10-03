#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# include "device/task.hpp"

# define CUBE_TREE_CUT
# define CUBE_TREE_REBALANCE
# include "sync/cube-tree.hpp"
# undef CUBE_TREE_CUT
# undef CUBE_TREE_REBALANCE

template <int K>
class KTaskAccess
{
    using Task = KTask<K>;

    public:
        Task * task;
        int access_id;

    public:
        KTaskAccess() : task(nullptr), access_id(0) {}
        KTaskAccess(const KTaskAccess & other) : task(other.task), access_id(other.access_id) {}
        KTaskAccess(const Task * task, const int access_id) : task(task), access_id(access_id) {}
        ~KTaskAccess() {}

        KTaskAccess &
        operator=(const KTaskAccess & other)
        {
            this->task      = other.task;
            this->access_id = other.access_id;
            return *this;
        }

}; /* class KTaskAccess */

template<int K>
class KDependencyTreeSearch
{
    using TaskAccess = KTaskAccess<K>;

    public:
        enum Type
        {
            SEARCH_TYPE_RESOLVE,
            SEARCH_TYPE_CONFLICTING
        };

    public:
        Type type;

        // USED IF TYPE == SEARCH_TYPE_RESOLVE
        TaskAccess task_access;

        // USED IF TYPE == SEARCH_TYPE_CONFLICTING
        std::vector<TaskAccess> * conflicts;
        const Access * access;

    public:
        KDependencyTreeSearch() {}
        ~KDependencyTreeSearch() {}

    public:
        void
        prepare_resolve(Task * task, const int access_id)
        {
            this->type = SEARCH_TYPE_RESOLVE;
            this->task_access.task      = task;
            this->task_access.access_id = access_id;
        }

        void
        prepare_conflicting(
            std::vector<TaskAccess> * conflicts,
            const Access * access
        ) {
            this->type = SEARCH_TYPE_CONFLICTING;
            this->conflicts = conflicts;
            this->access = access;
        }

} /* class KDependencyTreeSearch */;

template <int K>
class KDependencyTreeNode : public KIntervalBtree<K, KDependencyTreeSearch<K>>::Node {

    using Base          = typename KIntervalBtree<K, KDependencyTreeSearch<K>>::Node;
    using Node          = KDependencyTreeNode<K>;
    using Hypercube        = KHypercube<K>;
    using Search        = KDependencyTreeSearch<K>;
    using TaskAccess    = KTaskAccess<K>;

    public:

        /* last tasks that performed a read access */
        std::vector<TaskAccess> last_reads;

        /* last task that performed a write access */
        TaskAccess last_write;

        /* number of writes in all subtrees */
        int nwrites;

    public:

        KDependencyTreeNode<K>(
            const Hypercube & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            last_reads(),
            last_write(),
            nwrites(0)
        {}

        /* a new node from a split, inherit 'src' accesses */
        KDependencyTreeNode<K>(
            const Hypercube & r,
            const int k,
            const Color color,
            const Node * inherit
        ) :
            Base(r, k, color),
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

        ////////////////////////////////
        // When an access is inserted //
        ////////////////////////////////
        inline void
        on_insert(
            Search & search,
            const access_mode_t mode
        ) {
            assert(search.type == Search::Type::SEARCH_TYPE_RESOLVE);

            const Access * access = search.task_access.task->accesses + search.task_access.access_id;
            assert(access);
            assert(access->mode == mode);

            if (access->mode & ACCESS_MODE_W)
            {
                this->last_reads.clear();
                this->last_write = search.task_access;
            }
            else if (mode == ACCESS_MODE_R)
                this->last_reads.push_back(search.task_access);
        }

        inline void
        on_shrink(const Interval & interval, int k)
        {
            (void) interval;
            (void) k;
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_stop_test(
            Search & search,
            const Hypercube & cube,
            const access_mode_t mode
        ) const {
            (void) search;
            (void) cube;
            return (mode == ACCESS_MODE_R && this->nwrites == 0);
        }

        inline void
        on_intersect(
            Search & search,
            const Hypercube & cube,
            const access_mode_t mode
        ) {
            switch (search.type)
            {
                case (Search::Type::SEARCH_TYPE_RESOLVE):
                {
                    if (mode & ACCESS_MODE_W && this->last_reads.size())
                        for (TaskAccess & pred : this->last_reads)
                            pred.task->precedes(search.task_access.task);
                    else if (this->last_write.task)
                        this->last_write.task->precedes(search.task_access.task);

                    break ;
                }

                case (Search::Type::SEARCH_TYPE_CONFLICTING):
                {
                    if (this->last_write.task)
                    {
                        assert(search.conflicts);
                        search.conflicts->push_back(this->last_write);
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

        ////////////
        // UPDATE //
        ////////////
        inline void
        update_includes_nwrites(void)
        {
            this->nwrites = this->last_write.task ? 1 : 0;
            FOREACH_CHILD_BEGIN(this, child, k, dir)
            {
                this->nwrites += child->nwrites;
            }
            FOREACH_CHILD_END(this, child, k, dir);
        }

        inline void
        update_includes(void)
        {
            KIntervalBtree<K, KDependencyTreeSearch<K>>::Node::update_includes();
            this->update_includes_nwrites();
        }

        void
        dump_str(FILE * f) const
        {
            KIntervalBtree<K, KDependencyTreeSearch<K>>::Node::dump_str(f);
            fprintf(f, "\\nreads=%zu\\nwrites=%d", this->last_reads.size(), this->last_write.task ? 1 : 0);
        }

        void
        dump_cube_str(FILE * f) const
        {
            KIntervalBtree<K, KDependencyTreeSearch<K>>::Node::dump_cube_str(f);

            fprintf(f, "\\\\ reads=%zu \\\\ writes=%d", this->last_reads.size(), this->last_write.task ? 1 : 0);
            fprintf(f, "\\\\ nwrites = %d ", this->nwrites);
            fprintf(f, "\\\\ reads = [ ");
            for (const TaskAccess & task_access : this->last_reads)
                fprintf(f, "%p ", task_access.task);
            fprintf(f, "]");
        }
};

template<int K>
class KDependencyTree : public KIntervalBtree<K, KDependencyTreeSearch<K>> {

    using Base          = KIntervalBtree<K, KDependencyTreeSearch<K>>;
    using Node          = KDependencyTreeNode<K>;
    using NodeBase      = typename KIntervalBtree<K, KDependencyTreeSearch<K>>::Node;
    using Hypercube        = KHypercube<K>;
    using Search        = KDependencyTreeSearch<K>;
    using Task          = KTask<K>;
    using TaskAccess    = KTaskAccess<K>;

    public:

        template <int N>
        inline void
        resolve(Task * task)
        {
            assert(task->naccesses == N);

            # pragma message(TODO "If we semantically force a task accesses to be disjointed, then these 2 loops can be merged with no risks of dependency cycle")

            if (N)
            {
                Search search;

                // intersect with past tasks
                for (int access_id = 0 ; access_id < N ; ++access_id)
                {
                    search.prepare_resolve(task, access_id);

                    const Access * access = task->accesses + access_id;
                    assert(access);

                    this->intersect(search, access->cube, access->mode);
                }

                // insert for future tasks
                for (int access_id = 0 ; access_id < N ; ++access_id)
                {
                    search.prepare_resolve(task, access_id);

                    const Access * access = task->accesses + access_id;
                    assert(access);

                    this->insert(search, access->cube, access->mode);
                }
            }
        }

        inline void
        conflicting(
            std::vector<TaskAccess> * conflicts,
            const Access * access
        ) {
            Search search;
            search.prepare_conflicting(conflicts, access);
            this->intersect(search, access->cube, access->mode);
        }

        //////////////
        //  INSERT  //
        //////////////

        Node *
        new_node(
            Search & search,
            const Hypercube & cube,
            const int k,
            const Color color
        ) const {
            (void) search;
            return new Node(cube, k, color);
        }

        Node *
        new_node(
            Search & search,
            const Hypercube & cube,
            const int k,
            const Color color,
            const NodeBase * inherit
        ) const {
            (void) search;
            return new Node(cube, k, color, reinterpret_cast<const Node *>(inherit));
        }
};

using TaskAccess = KTaskAccess<2>;
using DependencyTree = KDependencyTree<2>;

#endif /* __DEPENDENCY_TREE_HPP__ */
