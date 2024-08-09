#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# include "device/task.hpp"
# include "sync/access-btree.hpp"

class DependencyNode : public Node<2> {

    public:

        /* last tasks that performed a read access */
        std::vector<Task *> last_reads;

        /* last task that performed a write access */
        Task * last_write;

        /* number of writes in all subtrees */
        int nwrites;

    public:

        DependencyNode() : DependencyNode(Intervals<K>()) {}

        DependencyNode(Intervals<K> & r) : {}

        DependencyNode(Intervals<K> & r, int k, Color color) :
            Node(r, k, color),
            last_reads(),
            last_write(nullptr),
            nwrites(0)
        {}

        inline void
        update_includes_nwrites(void)
        {
            this->includes.nwrites = this->last_write ? 1 : 0;
            FOREACH_CHILD_BEGIN(this, child, k, dir)
            {
                this->includes.nwrites += child->includes.nwrites;
            }
            FOREACH_CHILD_END(this, child, k, dir);
        }

        inline void
        update_includes(void)
        {
            Node::update_includes();
            this->update_includes_nwrites();
        }

        inline void
        register_access(access_mode_t mode, const Task * obj)
        {
            if (mode & ACCESS_MODE_W)
            {
                this->last_reads.clear();
                this->last_write = obj;
                this->has_write = 1;
            }
            else if (mode == ACCESS_MODE_R)
            {
                this->last_reads.push_back(obj);
            }
        }

        inline void
        inherit_accesses(Node * parent)
        {
            this->last_reads.insert(
                this->last_reads.end(),
                std::make_move_iterator(parent->last_reads.begin()),
                std::make_move_iterator(parent->last_reads.end())
            );
            this->last_write = parent->last_write;
            this->has_write = parent->has_write;
        }

        /* return true to stop intersection search */
        inline bool
        intersect_test(Intervals<K> & region)
        {
            if (mode == ACCESS_MODE_R && node->includes.nwrites == 0)
                return true;
            return false;
        }

        /* we are intersecting 'region' and it intersected with 'this' node */
        inline void
        on_intersect(Intervals<K> & region)
        {
            if (mode & ACCESS_MODE_W && this->last_reads.size())
                for (Task * & pred : node->last_reads)
                    this->on_hazard(node->region, pred, region, obj);
            else if (node->has_write)
                this->on_hazard(node->region, node->last_write, region, obj);
        }
};

class DependencyTree : public AccessBtree<2> {

   /**
    *  Callback when a dependence is detected
    */
    void
    on_hazard(
        const Region & rx,
        Task * const & x,
        const Region & ry,
        Task * const & y
    ) const {
        x->precedes(y, rx.intersection(ry));
    }
};

#endif /* __DEPENDENCY_TREE_HPP__ */
