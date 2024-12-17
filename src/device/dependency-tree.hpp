#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# pragma message(TODO "Dependency tree cut is always disabled currently, because the memory tree is included before the deptree... Fix me by removing 'mode' from the 'CubeTree::insert' - and overriding the 'insert_from' to cut in the dep tree")

# include "sync/cube-tree.hpp"
# include "device/task.hpp"

# include <array>
# include <unordered_map>

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
        std::unordered_map<Task *, std::array<bool, TASK_MAX_ACCESSES>> * conflicts;
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
            std::unordered_map<Task *, std::array<bool, TASK_MAX_ACCESSES>> * conflicts,
            const Access * access
        ) {
            this->type = SEARCH_TYPE_CONFLICTING;
            this->conflicts = conflicts;
            this->access = access;
        }

} /* class KDependencyTreeSearch */;

# if PRINT_IDS
static int PRINT_IDS_NEXT_VALUE = 0;
# endif

# define CUT false

template <int K>
class KDependencyTreeNode : public KCubeTree<K, KDependencyTreeSearch<K>, CUT>::Node {

    using Access        = KMemoryAccess<K>;
    using Base          = typename KCubeTree<K, KDependencyTreeSearch<K>, CUT>::Node;
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

        # if PRINT_IDS
        int id;
        # endif /* PRINT_IDS */

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
        {
            # if PRINT_IDS
            this->id = ++PRINT_IDS_NEXT_VALUE;
            # endif /* PRINT_IDS */
        }

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
            # if PRINT_IDS
            this->id = ++PRINT_IDS_NEXT_VALUE;
            # endif /* PRINT_IDS */
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
            KCubeTree<K, KDependencyTreeSearch<K>, CUT>::Node::update_includes();
            this->update_includes_nwrites();
        }

        void
        dump_str(FILE * f) const
        {
            # if PRINT_IDS
            fprintf(f, "%d", this->id);
            # else
            KCubeTree<K, KDependencyTreeSearch<K>, CUT>::Node::dump_str(f);
            fprintf(f, "\\nreads=%zu\\nwrites=%d", this->last_reads.size(), this->last_write.task ? 1 : 0);
            # endif
        }

        void
        dump_cube_str(FILE * f) const
        {
            # if PRINT_IDS
            fprintf(f, "%d", this->id);
            # else
            KCubeTree<K, KDependencyTreeSearch<K>, CUT>::Node::dump_cube_str(f);

            fprintf(f, "\\\\ reads=%zu \\\\ writes=%d", this->last_reads.size(), this->last_write.task ? 1 : 0);
            fprintf(f, "\\\\ nwrites = %d ", this->nwrites);
            fprintf(f, "\\\\ reads = [ ");
            for (const TaskAccess & task_access : this->last_reads)
                fprintf(f, "%p ", task_access.task);
            fprintf(f, "]");
            # endif
        }
};

template<int K>
class KDependencyTree : public KCubeTree<K, KDependencyTreeSearch<K>, CUT> {

    using Access        = KMemoryAccess<K>;
    using Base          = KCubeTree<K, KDependencyTreeSearch<K>, CUT>;
    using Node          = KDependencyTreeNode<K>;
    using NodeBase      = typename KCubeTree<K, KDependencyTreeSearch<K>, CUT>::Node;
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

        # pragma message(TODO "Any better option than this unordered map ?")
        inline void
        conflicting(
            std::unordered_map<Task *, std::array<bool, TASK_MAX_ACCESSES>> * conflicts,
            const Access * access
        ) {
            Search search;
            search.prepare_conflicting(conflicts, access);
            this->intersect(search, access->cubes[0]);
            this->intersect(search, access->cubes[1]);
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

            const Access * access = search.task_access.task->accesses + search.task_access.access_id;
            assert(access);

            if (access->mode & ACCESS_MODE_W)
            {
                node->last_reads.clear();
                node->last_write = search.task_access;
            }
            else if (access->mode == ACCESS_MODE_R)
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

        inline bool
        should_cut(
            Search & search,
            Cube & cube,
            NodeBase * parent,
            int k
        ) const {
            (void) cube;
            (void) parent;
            (void) k;
            return search.access->mode & ACCESS_MODE_W;
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_stop_test(
            NodeBase * nodebase,
            Search & search,
            const Cube & cube
        ) const {
            (void) cube;

            Node * node = reinterpret_cast<Node *>(nodebase);
            assert(node);

            const Access * access = (search.type == Search::SEARCH_TYPE_RESOLVE) ? search.task_access.task->accesses + search.task_access.access_id : search.access;
            assert(access);

            return (access->mode == ACCESS_MODE_R) && (node->nwrites == 0);
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
            const Cube & cube
        ) const {
            (void) cube;

            assert(nodebase);
            Node * node = reinterpret_cast<Node *>(nodebase);

            switch (search.type)
            {
                case (Search::Type::SEARCH_TYPE_RESOLVE):
                {
                    const Access * access = search.task_access.task->accesses + search.task_access.access_id;
                    if (access->mode & ACCESS_MODE_W && node->last_reads.size())
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

                        const TaskAccess & task_access = node->last_write;
                        if (search.conflicts->count(task_access.task) == 0)
                        {
                            std::array<bool, TASK_MAX_ACCESSES> accesses;
                            for (int i = 0 ; i < TASK_MAX_ACCESSES ; ++i)
                                accesses[i] = false;
                            accesses[task_access.access_id] = true;
                            (*search.conflicts)[task_access.task] = accesses;
                        }
                        else
                        {
                            std::array<bool, TASK_MAX_ACCESSES> & accesses = (*search.conflicts)[task_access.task];
                            accesses[task_access.access_id] = true;
                        }
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
