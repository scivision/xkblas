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

        KDependencyTreeNode<K>(const Region & r, int k, Color color) :
            Base(r, k, color),
            last_reads(),
            last_write(nullptr),
            nwrites(0)
        {}

        ////////////////////////////
        //  MANAGEMENT INTERFACES //
        ////////////////////////////
        virtual void
        on_insert(Task * & t, const access_mode_t mode)
        {
            this->register_access(t, mode);
        }

        void
        on_inherit(const Base * base)
        {
            const Node * node = reinterpret_cast<const Node *>(base);
            this->inherit_accesses(node);
        }

        //////////
        // IMPL //
        //////////
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

        inline void
        inherit_accesses(const Node * parent)
        {
            if (!parent->last_reads.empty())
            {
                this->last_reads.insert(
                    this->last_reads.end(),
                    std::make_move_iterator(parent->last_reads.begin()),
                    std::make_move_iterator(parent->last_reads.end())
                );
            }
            this->last_write = parent->last_write;
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

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline void
        intersect_from(
            Task * task,
            const Region & region,
            const access_mode_t mode,
            NodeBase * nodebase
        ) const {

            Node * node = reinterpret_cast<Node *>(nodebase);
            if (node == nullptr || !region.intersects(node->includes.region))
                return ;

            if (mode == ACCESS_MODE_R && node->nwrites == 0)
                return ;

            if (region.intersects(node->region))
            {
                if (mode & ACCESS_MODE_W && node->last_reads.size())
                    for (Task * & pred : node->last_reads)
                        pred->precedes(task, node->region.intersection(region));
                else if (node->last_write)
                {
                    Task * pred = node->last_write;
                    pred->precedes(task, node->region.intersection(region));
                }
            }

            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                this->intersect_from(task, region, mode, child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        inline void
        intersect(Task * task, const Region & region, const access_mode_t mode) const
        {
            this->intersect_from(task, region, mode, this->root);
        }

        //////////////
        //  INSERT  //
        //////////////
        virtual Node *
        new_node(
            const Region & region,
            const int k,
            const Color color
        ) const {
            return new KDependencyTreeNode<K>(region, k, color);
        }
};

using DependencyTree = KDependencyTree<2>;

#endif /* __DEPENDENCY_TREE_HPP__ */
