#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# include "device/task.hpp"
# include "sync/kinterval-btree.hpp"

template <int K>
class KDependencyTreeNode : public KIntervalBtree<K, KTask<K> *>::Node {

    using Region = Intervals<K>;
    using Task = KTask<K>;
    using Node = KDependencyTreeNode<K>;
    using Base = typename KIntervalBtree<K, KTask<K> *>::Node;

    public:

        /* last tasks that performed a read access */
        std::vector<Task *> last_reads;

        /* last task that performed a write access */
        Task * last_write;

        /* number of writes in all subtrees */
        int nwrites;

    public:

        KDependencyTreeNode<K>(
            const Region & r,
            const int k,
            const Color color
        ) :
            Base(r, k, color),
            last_reads(),
            last_write(nullptr),
            nwrites(0)
        {}

        /* a new node from a split, inherit 'src' accesses */
        KDependencyTreeNode<K>(
            const Region & r,
            const int k,
            const Color color,
            const Node * src
        ) :
            Base(r, k, color),
            last_reads(),
            last_write(src->last_write),
            nwrites(0)
        {
            if (!src->last_reads.empty())
            {
                this->last_reads.insert(
                    this->last_reads.end(),
                    std::make_move_iterator(src->last_reads.begin()),
                    std::make_move_iterator(src->last_reads.end())
                );
            }
        }

        ////////////////////////////////
        // When an access is inserted //
        ////////////////////////////////
        inline void
        register_access(
            Task * task,
            const access_mode_t mode
        ) {
            if (mode & ACCESS_MODE_W)
            {
                this->last_reads.clear();
                this->last_write = task;
            }
            else if (mode == ACCESS_MODE_R)
            {
                this->last_reads.push_back(task);
            }
        }

        virtual void
        on_insert(Task * & t, const access_mode_t mode)
        {
            this->register_access(t, mode);
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline bool
        intersect_test(
            Task * & task,
            const Region & region,
            const access_mode_t mode
        ) const {
            (void) task;
            (void) region;
            return (mode == ACCESS_MODE_R && this->nwrites == 0);
        }

        inline void
        on_intersect(
            Task * & task,
            const Region & region,
            const access_mode_t mode
        ) const {
            Region intersection = this->region.intersection(region);
            if (mode & ACCESS_MODE_W && this->last_reads.size())
                for (Task * const & pred : this->last_reads)
                    pred->precedes(task, intersection);
            else if (this->last_write)
                this->last_write->precedes(task, intersection);
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
            KIntervalBtree<K, KTask<K> *>::Node::update_includes();
            this->update_includes_nwrites();
        }

        virtual void
        dump_str(FILE * f) const
        {
            KIntervalBtree<K, KTask<K> *>::Node::dump_str(f);
            fprintf(f, "\\nreads=%zu\\nwrites=%d", this->last_reads.size(), this->last_write ? 1 : 0);
        }

        virtual void
        dump_region_str(FILE * f) const
        {
            KIntervalBtree<K, KTask<K> *>::Node::dump_region_str(f);
            fprintf(f, "\\\\ reads=%zu \\\\ writes=%d", this->last_reads.size(), this->last_write ? 1 : 0);
            if (this->last_reads.size())
            {
                fprintf(f, "\\\\ reads = [ ");
                for (const KTask<K> * task : this->last_reads)
                    fprintf(f, "%zu ", ((uintptr_t)task) % 131072);
                fprintf(f, "]");
            }
        }
};

template<int K>
class KDependencyTree : public KIntervalBtree<K, KTask<K> *> {

    using Region = Intervals<K>;
    using Task = KTask<K>;
    using Node = KDependencyTreeNode<K>;
    using Base = KIntervalBtree<K, KTask<K> *>;
    using NodeBase = typename KIntervalBtree<K, KTask<K> *>::Node;

    public:

        //////////////
        //  INSERT  //
        //////////////
        Node *
        new_node(
            const Region & region,
            const int k,
            const Color color
        ) const {
            return new Node(region, k, color);
        }

        Node *
        new_node(
            const Region & region,
            const int k,
            const Color color,
            const NodeBase * nodebase
        ) const {
            return new Node(region, k, color, reinterpret_cast<const Node *>(nodebase));
        }
};

using DependencyTree = KDependencyTree<2>;

#endif /* __DEPENDENCY_TREE_HPP__ */
