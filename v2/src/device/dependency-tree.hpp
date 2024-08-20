#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# include "device/task.hpp"
# include "sync/kinterval-btree.hpp"

template <int K>
class KDependencyTreeNode : public KIntervalBtree<K>::Node {

    using Region = Intervals<K>;
    using Node = KDependencyTreeNode<K>;
    using NodeBase = typename KIntervalBtree<K>::Node;

    public:

        /* last tasks that performed a read access */
        std::vector<Task *> last_reads;

        /* last task that performed a write access */
        Task * last_write;

        /* number of writes in all subtrees */
        int nwrites;

    public:

        KDependencyTreeNode<K>(const Region & r, int k, Color color) :
            NodeBase(r, k, color),
            last_reads(),
            last_write(nullptr),
            nwrites(0)
        {}

        inline Node *
        get_child(int k, Direction dir)
        {
            return reinterpret_cast<Node *>(this->st[k].children[dir]);
        }

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
            KIntervalBtree<K>::Node::update_includes();
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
        inherit_accesses(KDependencyTreeNode * parent)
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
            KIntervalBtree<K>::Node::dump_str(f);
            fprintf(f, "\\nreads=%zu\\nwrites=%d", this->last_reads.size(), this->last_write ? 1 : 0);
        }

        virtual void
        dump_region_str(FILE * f) const
        {
            KIntervalBtree<K>::Node::dump_region_str(f);
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
class KDependencyTree : public KIntervalBtree<K> {

    using Base = KIntervalBtree<K>;
    using Node = KDependencyTreeNode<K>;
    using NodeBase = typename KIntervalBtree<K>::Node;
    using Region = Intervals<K>;

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

# ifdef CUT
        inline void
        insert_from_cut(
            Task * task,
            Region & region,
            const access_mode_t mode,
            Node * parent
        ) {
            tassert(mode & ACCESS_MODE_W);

            FOREACH_CHILD_BEGIN(parent, child, k, dir)
            {
                this->cut(parent, k, dir);
            }
            FOREACH_CHILD_END(parent, child, k, dir);

            parent->region.copy(region);
            parent->register_access(task, mode);

            this->outdate(parent);
        }
# endif /* CUT */

        inline void
        insert_from(
            Task * task,
            Region & region,
            const access_mode_t mode,
            NodeBase * parentbase,
            int k,
            Node * node
        ) {
            Node * parent = reinterpret_cast<Node *>(parentbase);

            while (k < K)
            {
# ifdef CUT
#  pragma message("Tree cut enable")
                // quick-way out, if the region includes all subregion with an
                // 'out' access, we can discard all children
                if (mode & ACCESS_MODE_W)
                {
                    // the includes test is accelerated as we know we are
                    // already matching dimensions <k
                    if (region.includes(parent->includes.region, k))
                    {
                        // TODO : what if 'node' is not null ?  probably want to
                        // return something to callee for the case (3)
                        tassert(node == nullptr);
                        this->insert_from_cut(task, region, mode, parent);
                        break ;
                    }
                }
# else
#  pragma message("Tree cut disabled. Enable it using '-DCUT'")
# endif /* CUT */

                // case (1)    J << I
                if (region[k].b <= parent->region[k].a)
                {
                    if (parent->st[k].left == nullptr)
                    {
                        if (node == nullptr)
                            node = new Node(region, k, RED);
                        else
                        {
                            node->k = k;
                            node->colors[k] = RED;
                        }
                        node->register_access(task, mode);
                        this->insert_fixup(parent, k, LEFT, node);
                        break ;
                    }
                    else
                    {
                        parent = parent->get_child(k, LEFT);
                    }
                }
                // case (2)     J >> I
                else if (region[k].a >= parent->region[k].b)
                {
                    if (parent->st[k].right == nullptr)
                    {
                        if (node == nullptr)
                            node = new Node(region, k, RED);
                        else
                        {
                            node->k = k;
                            node->colors[k] = RED;
                        }
                        node->register_access(task, mode);
                        this->insert_fixup(parent, k, RIGHT, node);
                        break ;
                    }
                    else
                    {
                        parent = parent->get_child(k, RIGHT);
                    }
                }
                // case (3)     J c I   (or I == J)
                else if (parent->region[k].a <= region[k].a && region[k].b <= parent->region[k].b)
                {
                    // I == J
                    if (region[k].a == parent->region[k].a && region[k].b == parent->region[k].b)
                    {
insert_from_case_3_equals:
                        if (++k == K)
                        {
                            tassert(node == nullptr);
                            parent->register_access(task, mode);
                            this->outdate(parent);
                            break ;
                        }
                    }
                    // J c I
                    else
                    {
                        // prerequisities here:
                        //  - all dimensions k' < k is equal some parent node
                        //  - dimension k is included in the parent node
                        //  - we don't know about dimensions k" > k
                        // how we insert
                        //  - shrink dimension 'k' of the current node and all its (k+1) subtree
                        //  - dupplicate shrinked nodes and reinsert them

                        // TODO : we can probably do better than above performance-wide, by
                        //  - dupplicating the subtree
                        //  - rewritting its included information
                        //  - connecting it to 'parent'

                        // TODO only tested for K \in {1, 2} - it should work
                        // for higher dimensions as well, but needs to be verified
                        assert(K == 1 || K == 2);

                        class ReinsertRegion {
                            public:
                                Node * sibling;
                                Region region;
                            public:
                                ReinsertRegion(Node * s, Region & r) : sibling(s), region(r) {}
                                virtual ~ReinsertRegion() {}
                        };

                        int x[4] = { parent->region[k].a, region[k].a, region[k].b, parent->region[k].b };

                        std::vector<ReinsertRegion> to_reinsert;
                        std::function<void(Node *)> f = [&x, &to_reinsert, &region, &k](Node * node)
                        {
                            // generate side nodes region
                            for (int i = 0 ; i < 2 ; ++i)
                            {
                                if (x[2*i+0] == x[2*i+1])
                                    continue ;
                                ReinsertRegion rr(node, node->region);
                                rr.region[k].a = x[2*i+0];
                                rr.region[k].b = x[2*i+1];
                                to_reinsert.push_back(rr);
                            }

                            // shrink node 
                            node->region[k] = region[k];
                        };

                        f(parent);

                        if (k < K - 1)
                            this->template foreach_k_child<Node>(parent, k+1, f);

                        // insert all side nodes
                        for (ReinsertRegion & rr : to_reinsert)
                        {
                            Node * node = new Node(rr.region, k, RED);
                            this->insert_from(task, node->region, ACCESS_MODE_VOID, parent, k, node);
                            node->inherit_accesses(rr.sibling);
                        }

                        // continue inserting, the node 

                        assert(region[k].a == parent->region[k].a && region[k].b == parent->region[k].b);
                        goto insert_from_case_3_equals;
                        break ;

                    } /* I == J ||  J c I */
                }
                // case (4)     I c J
                else if (region[k].a <= parent->region[k].a && parent->region[k].b <= region[k].b)
                {
                    int xs[4] = { region[k].a, parent->region[k].a, parent->region[k].b, region[k].b };
                    for (int i = 0 ; i < 3 ; ++i)
                    {
                        if (xs[i+0] == xs[i+1])
                            continue ;
                        Region r(region);
                        r[k].a = xs[i+0];
                        r[k].b = xs[i+1];
                        // this->insert_from(task, r, mode, parent, k, node);
                        this->insert_from(task, r, mode, this->root, 0, node);
                    }
                    this->outdate(parent);
                    break ;
                }
                // case (5)     J < I    (and I n J != o)   is (1) + (3)
                else if (parent->region[k].a <= region[k].b && region[k].b <= parent->region[k].b)
                {
                    const int a = region[k].a;
                    const int b = region[k].b;

                    // region[k].a = region[k].a;
                    region[k].b = parent->region[k].a;
                    this->insert_from(task, region, mode, parent, k, node);         // (1)
                    // this->insert_from(this->root, mode, region, obj, 0, node);   // (1)

                    region[k].a = parent->region[k].a;
                    region[k].b = b;
                    this->insert_from(task, region, mode, parent, k, node);         // (3)
                    // this->insert_from(this->root, mode, region, obj, 0, node);   // (3)

                    region[k].a = a;
                    region[k].b = b;

                    ++k;
                }
                // case (6)     J > I    (and I n J != o)   is (2) + (3)
                else if (parent->region[k].a <= region[k].a && region[k].a <= parent->region[k].b)
                {
                    int a = region[k].a;
                    int b = region[k].b;

                    // region[k].a = region[k].a;
                    region[k].b = parent->region[k].b;
                    this->insert_from(task, region, mode, parent, k, node);  // (3)
                    // this->insert_from(this->root, mode, region, obj, 0, node);  // (3)

                    region[k].a = parent->region[k].b;
                    region[k].b = b;
                    this->insert_from(task, region, mode, parent, k, node);  // (2)
                    // this->insert_from(this->root, mode, region, obj, k, node);  // (2)

                    region[k].a = a;
                    region[k].b = b;

                    ++k;
                }
                else
                {
                    tassert(0 && "Impossible occured");
                }
            }
        }

        inline void
        insert(
            Task * task,
            Region & region,
            const access_mode_t mode
        ) {
            tassert(!region.is_empty());

            if (this->root == nullptr)
            {
                Node * node = new Node(region, 0, BLACK);
                node->register_access(task, mode);
                this->root = node;
                this->outdate(this->root);
            }
            else
            {
                this->insert_from(task, region, mode, this->root, 0, nullptr);
            }

            Base::post_insert();
        }
};

using DependencyTree = KDependencyTree<2>;

#endif /* __DEPENDENCY_TREE_HPP__ */
