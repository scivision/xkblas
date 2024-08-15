#ifndef __DEPENDENCY_TREE_HPP__
# define __DEPENDENCY_TREE_HPP__

# include "device/task.hpp"
# include "sync/kinterval-btree.hpp"

template <int K>
class DependencyTreeNode : public KIntervalBtreeNode<K> {

    using Base = KIntervalBtreeNode<K>;
    using Region = Intervals<K>;

    public:

        /* last tasks that performed a read access */
        std::vector<Task *> last_reads;

        /* last task that performed a write access */
        Task * last_write;

        /* number of writes in all subtrees */
        int nwrites;

    public:

        DependencyTreeNode(Region & r, int k, Color color) :
            last_reads(),
            last_write(nullptr),
            nwrites(0),
            Base(r, k, color)
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
            Base::update_includes();
            this->update_includes_nwrites();
        }

        inline void
        register_access(Task * task, const task_access_t<K> * access)
        {
            if (access->mode & ACCESS_MODE_W)
            {
                this->last_reads.clear();
                this->last_write = task;
            }
            else if (access->mode == ACCESS_MODE_R)
            {
                this->last_reads.push_back(task);
            }
        }

        inline void
        inherit_accesses(DependencyTreeNode * parent)
        {
            this->last_reads.insert(
                this->last_reads.end(),
                std::make_move_iterator(parent->last_reads.begin()),
                std::make_move_iterator(parent->last_reads.end())
            );
            this->last_write = parent->last_write;
        }
};

template<int K>
class DependencyTree : public KIntervalBtree<K, DependencyTreeNode<K>> {

    using Node = DependencyTreeNode<K>;
    using Base = KIntervalBtree<K, Node>;
    using Region = Intervals<K>;

    public:

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline void
        intersect_from(
            Task * task,
            const task_access_t<K> * access,
            Node * node
        ) const {

            if (node == nullptr || !access->region.intersects(node->includes.region))
                return ;

            if (access->mode == ACCESS_MODE_R && node->includes.nwrites == 0)
                return ;

            if (access->region.intersects(node->region))
            {
                if (access->mode & ACCESS_MODE_W && node->last_reads.size())
                    for (Task * & pred : node->last_reads)
                        pred->precedes(task, node->region.intersection(access->region));
                else if (node->last_write)
                {
                    Task * pred = node->last_write;
                    pred->precedes(task, node->region.intersection(access->region));
                }
            }

            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                this->intersect_from(task, access, child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        inline void
        intersect(Task * task, const task_access_t<K> * access) const
        {
            this->intersect_from(task, access, this->root);
        }

        //////////////
        //  INSERT  //
        //////////////

# ifdef CUT
        inline void
        insert_from_cut(
            Task * task,
            const task_access_t<K> * access,
            N * parent
        ) {
            tassert(access->mode & ACCESS_MODE_W);

            FOREACH_CHILD_BEGIN(parent, child, k, dir)
            {
                this->limbs.push_back(child);
                parent->st[k].children[dir] = nullptr;
            }
            FOREACH_CHILD_END(parent, child, k, dir);

            parent->region.copy(access->region);
            parent->register_access(task, access);

            this->outdate(parent);
        }
# endif /* CUT */

        inline void
        insert_from(
            Task * task,
            const task_access_t<K> * access,
            Node * parent,
            int k,
            Node * node
        ) {
            const Region & region = access->region;

            // TODO : ensure that loop is unrolled
            while (k < K)
            {
# ifdef CUT
#  pragma message("Tree cut enable")
                // quick-way out, if the region includes all subregion with an
                // 'out' access, we can discard all children
                if (access->mode & ACCESS_MODE_W)
                {
                    // TODO : the includes test could be accelerated simply
                    // checking >=k dimensions, as we know we are already matching
                    // <k dimensions

                    if (access->region.includes(parent->includes.region))
                    {
                        // TODO : what if 'node' is not null ?  probably want to
                        // return something to callee for the case (3)
                        tassert(node == nullptr);
                        this->insert_from_cut(task, access, parent);
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
                        node->register_access(task, access);
                        this->insert_fixup(parent, k, LEFT, node);
                        break ;
                    }
                    else
                    {
                        parent = parent->st[k].left;
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
                        node->register_access(task, access);
                        this->insert_fixup(parent, k, RIGHT, node);
                        break ;
                    }
                    else
                    {
                        parent = parent->st[k].right;
                    }
                }
                // case (3)     J c I   (or I == J)
                else if (parent->region[k].a <= region[k].a && region[k].b <= parent->region[k].b)
                {
                    if (region[k].a == parent->region[k].a && region[k].b == parent->region[k].b)   /* I == J */
                    {
                        if (++k == K)
                        {
                            tassert(node == nullptr);
                            parent->register_access(task, access);
                            this->outdate(parent);
                            break ;
                        }
                    }
                    else /* J c I */
                    {
                        // TODO only work for K \in {1, 2} right now - FIX me for K>=3
                        assert(K == 1 || K == 2);

                        // shrink parent
                        int x[4] = { parent->region[k].a, region[k].a, region[k].b, parent->region[k].b };
                        parent->shrink(region, k);

                        // reinsert 2 nodes for parent's shrinked borders
                        for (int i = 0 ; i < 2 ; ++i)
                        {
                            if (x[2*i+0] == x[2*i+1])
                                continue ;
                            Region r(parent->region);
                            r[k].a = x[2*i+0];
                            r[k].b = x[2*i+1];
                            Node * node = new Node(r, k, RED);
                            // this->insert_from(parent, mode, r, nullptr, k, node);
                            this->insert_from(this->root, ACCESS_MODE_VOID, r, obj, 0, node);
                            node->inherit_accesses(parent);
                        }

                        // shrink children for dimensions k' > k
                        if (k < K-1)
                        {
                            std::function<void(Node *)> f = [this, &k, &region, &x, &obj](Node * node) {
                                node->shrink(region, k);
                                for (int i = 0 ; i < 2 ; ++i)
                                {
                                    if (x[2*i+0] == x[2*i+1])
                                        continue ;
                                    Region r(node->region);
                                    r[k].a = x[2*i+0];
                                    r[k].b = x[2*i+1];
                                    Node * split = new Node(r, k, RED);
                                    // TODO : probably no need to restart from root and dim 0
                                    this->insert_from(this->root, ACCESS_MODE_VOID, r, obj, 0, split);
                                    split->inherit_accesses(node);
                                }
                            };
                            foreach_k_child(parent, k+1, f);
                        }

                        // TODO : unnecessary if we outdated another child
                        this->outdate(parent);
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
                        // this->insert_from(parent, mode, r, nullptr, k, node);
                        this->insert_from(this->root, mode, r, obj, 0, node);
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
                    this->insert_from(parent, mode, region, obj, k, node);  // (1)
                    // this->insert_from(this->root, mode, region, obj, 0, node);  // (1)

                    region[k].a = parent->region[k].a;
                    region[k].b = b;
                    this->insert_from(parent, mode, region, obj, k, node);  // (3)
                    // this->insert_from(this->root, mode, region, obj, 0, node);  // (3)

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
                    this->insert_from(parent, mode, region, obj, k, node);  // (3)
                    // this->insert_from(this->root, mode, region, obj, 0, node);  // (3)

                    region[k].a = parent->region[k].b;
                    region[k].b = b;
                    this->insert_from(parent, mode, region, obj, k, node);  // (2)
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
        insert(Task * task, const task_access_t<K> * access)
        {
            tassert(!access->region.is_empty());

            if (this->root == nullptr)
            {
                this->root = new Node(access->region, 0, BLACK);
                this->root->register_access(task, access);
                this->root->update_includes();
            }
            else
            {
                this->insert_from(task, access, this->root, 0, nullptr);
                this->update();
            }

            Base::post_insert();
        }
};

#endif /* __DEPENDENCY_TREE_HPP__ */
