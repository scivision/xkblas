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
    using Task = KTask<K>;
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
class KDependencyTreeNode : public KCubeTree<K, KDependencyTreeSearch<K>>::Node {

    using Access        = KMemoryAccess<K>;
    using Base          = typename KCubeTree<K, KDependencyTreeSearch<K>>::Node;
    using Node          = KDependencyTreeNode<K>;
    using Cube          = KCube<K>;
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
            const Cube & r,
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
            const Cube & r,
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
            KCubeTree<K, KDependencyTreeSearch<K>>::Node::update_includes();
            this->update_includes_nwrites();
        }

        void
        dump_str(FILE * f) const
        {
            KCubeTree<K, KDependencyTreeSearch<K>>::Node::dump_str(f);
            fprintf(f, "\\nreads=%zu\\nwrites=%d", this->last_reads.size(), this->last_write.task ? 1 : 0);
        }

        void
        dump_cube_str(FILE * f) const
        {
            KCubeTree<K, KDependencyTreeSearch<K>>::Node::dump_cube_str(f);

            fprintf(f, "\\\\ reads=%zu \\\\ writes=%d", this->last_reads.size(), this->last_write.task ? 1 : 0);
            fprintf(f, "\\\\ nwrites = %d ", this->nwrites);
            fprintf(f, "\\\\ reads = [ ");
            for (const TaskAccess & task_access : this->last_reads)
                fprintf(f, "%p ", task_access.task);
            fprintf(f, "]");
        }
};

template<int K>
class KDependencyTree : public KCubeTree<K, KDependencyTreeSearch<K>> {

    using Base          = KCubeTree<K, KDependencyTreeSearch<K>>;
    using Node          = KDependencyTreeNode<K>;
    using NodeBase      = typename KCubeTree<K, KDependencyTreeSearch<K>>::Node;
    using Cube          = KCube<K>;
    using Task          = KTask<K>;
    using TaskAccess    = KTaskAccess<K>;

    public:

        using Search = KDependencyTreeSearch<K>;

        KDependencyTree(const size_t ld) : Base(), ld(ld) {}
        ~KDependencyTree() {}

        /* ld for this dep tree */
        const size_t ld;

    public:

        inline void
        conflicting(
            std::vector<TaskAccess> * conflicts,
            const Access * access
        ) {
            Search search;
            search.prepare_conflicting(conflicts, access);
            this->intersect(search, access->cubes[0], access->mode);
            this->intersect(search, access->cubes[1], access->mode);
        }

        //////////////
        //  INSERT  //
        //////////////

        inline void
        on_insert(
            NodeBase * nodebase,
            Search & search,
            const access_mode_t mode
        ) {
            assert(nodebase);
            assert(search.type == Search::Type::SEARCH_TYPE_RESOLVE);

            Node * node = reinterpret_cast<Node *>(nodebase);

            const Access * access = search.task_access.task->accesses + search.task_access.access_id;
            assert(access);

            if (access->mode & ACCESS_MODE_W)
            {
                node->last_reads.clear();
                node->last_write = search.task_access;
            }
            else if (mode == ACCESS_MODE_R)
                node->last_reads.push_back(search.task_access);
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
            const Cube & cube,
            const int k,
            const Color color
        ) const {
            (void) search;
            return new Node(cube, k, color);
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
            return new Node(cube, k, color, reinterpret_cast<const Node *>(inherit));
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
            (void) search;
            (void) cube;
            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);
            return (mode == ACCESS_MODE_R && node->nwrites == 0);
        }

        inline void
        precedence(Task * pred, Task * succ) const
        {
            /* avoid redundant edges when 2 tasks are conflicting on several
             * accesses, this optimization is possible as we are in a
             * sequential task flow paradigm : if pred -> succ, then
             * 'succ' must be the last successor inserted */
            if (pred->edges.size() == 0 || pred->edges.back().successor != succ)
                pred->precedes(succ);
        }

        inline void
        on_intersect(
            NodeBase * nodebase,
            Search & search,
            const Cube & cube,
            const access_mode_t mode
        ) const {
            (void) cube;

            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);

            switch (search.type)
            {
                case (Search::Type::SEARCH_TYPE_RESOLVE):
                {
                    if (mode & ACCESS_MODE_W && node->last_reads.size())
                        for (TaskAccess & pred : node->last_reads)
                            this->precedence(pred.task, search.task_access.task);
                    else if (node->last_write.task)
                        this->precedence(node->last_write.task, search.task_access.task);

                    break ;
                }

                case (Search::Type::SEARCH_TYPE_CONFLICTING):
                {
                    if (node->last_write.task)
                    {
                        assert(search.conflicts);
                        search.conflicts->push_back(node->last_write);
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


};

using TaskAccess = KTaskAccess<2>;
using DependencyTree = KDependencyTree<2>;

#endif /* __DEPENDENCY_TREE_HPP__ */
