#ifndef __ACCESS_INTERVAL_MULTI_TREE_H__
# define __ACCESS_INTERVAL_MULTI_TREE_H__

// tree assert, must be called within a member function
# ifdef NDEBUG
#  define tassert(ignore) ((void)0)
# else /* NDEBUG */
#  pragma message("NDEBUG unset, use -DNDEBUG for max performance")
#  define tassert(expr)                                                         \
    do {                                                                        \
        if (!(expr))                                                            \
        {                                                                       \
            this->export_pdf();                                                 \
            fprintf(stdout, "%s:%d: assertion `" #expr "` failed in `%s`\n",    \
                   __FILE__,__LINE__,__func__);                                 \
            abort();                                                            \
        }                                                                       \
    } while (0)
# endif /* NDEBUG */

# include <cassert>
# include <cstdio>
# include <cstring>
# include <cstdlib>
# include <vector>
# include <climits>

# include <functional>
using namespace std::placeholders;

# include <ostream>
# include <iostream>

# include "access-mode.h"
# include "history.hpp"
# include "region.hpp"
# include "utils.hpp"

///////////////
// Constants //
///////////////

typedef enum    Color
{
    BLACK   = 0,
    RED     = 1,
}               Color;

static const char * COLORS[] = {
    "#000000",
    "#FF0000",
    "#00FF00",
    "#0000FF",
    "#FFFF00",
    "#FF00FF",
    "#00FFFF",
};

typedef enum    Direction
{
    LEFT            = 0,
    RIGHT           = 1,
    DIRECTION_MAX   = 2,
}               Direction;

/* K is the number of dimensions */
template<int K, typename T>
class AccessIntervalMultiTree : public History<K, T> {

    protected:
        using Region = Intervals<K>;

        class Node {

            # define FOREACH_CHILD_BEGIN(N, C, I, D)                \
            do {                                                    \
                for (int I = 0 ; I < K ; ++I)                       \
                {                                                   \
                    for (int D = LEFT ; D < DIRECTION_MAX ; ++D)    \
                    {                                               \
                        Node * C = N->st[I].children[D];            \
                        if (C)                                      \
                        {
                            # define FOREACH_CHILD_END(N, C, I, D)  \
                        }                                           \
                    }                                               \
                }                                                   \
            } while (0)                                             \

            public:
                // structs used as class members
                typedef union
                {
                    Node * children[2];
                    struct {
                        Node * left;
                        Node * right;
                    };
                } subtree_t;

                // members
                Node * parent;
                int k;
                subtree_t st[K];
                Region region;
                Color colors[K];
                std::vector<T *> last_reads;
                T * last_write;
                bool has_write;
                struct {
                    Region region; // subtree englobing region
                    int nwrites;            // subtree number of 'writes' elements
                    int nelements[K];       // subtree number of elements
                    int height[K];          // subtree height
                    int outdated;           // whether 'includes' struct must be recomputed
                } includes;

                #ifndef NDEBUG
                struct {
                    int id;
                } checks;
                #endif /* NDEBUG */

                // constructor
                Node() {}

                Node(Region & r) : Node(r, 0, BLACK) {}

                Node(Region & r, int k, Color color) :
                    parent(nullptr),
                    k(k),
                    region(r),
                    colors{BLACK},
                    last_reads(),
                    last_write(),
                    has_write(0)
                {
                    memset(this->st, 0, sizeof(this->st));

                    this->includes.region.copy(r);
                    this->includes.nwrites = 0;
                    this->includes.outdated = 0;

                    memset(this->includes.nelements, 0, sizeof(this->includes.nelements));
                    this->includes.nelements[k] = 1;

                    memset(this->includes.height, 0, sizeof(this->includes.height));
                    for (int i = k ; i < K ; ++i)
                        this->includes.height[i] = 1;

                    this->colors[k] = color;
                }

                virtual ~Node() {}

                inline int
                size(void) const
                {
                    int nelements = 0;
                    for (int k = 0 ; k < K ; ++k)
                        nelements += this->includes.nelements[k];
                    return nelements;
                }

                inline int
                height(void) const
                {
                    int height = 0;
                    for (int k = 0 ; k < K ; ++k)
                        height = MAX(height, this->includes.height[k]);
                    return height;
                }

                inline void
                register_access(access_mode_t mode, T * obj)
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

                void
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

                void
                dump(FILE * f) const
                {

                    {
                        const char * color = COLORS[this->colors[this->k] == BLACK ? 0 : this->k+1];
                        char writes[32];
                        snprintf(writes, sizeof(writes), "%d", this->has_write);

                        char reads[32];
                        snprintf(reads, sizeof(reads), "%ld", this->last_reads.size());

                        char region[1024];
                        this->region.tostring(region, sizeof(region));

                        char include_region[1024];
                        this->includes.region.tostring(include_region, sizeof(include_region));

                        fprintf(f, "    N%p[fontcolor=\"#ffffff\", label=\"--- node ---\\nk=%d\\n%s\\nreads=%s\\nwrites=%s\\n\\n--- includes ---\\n%s\\nnwrites=%d\\nsize=%d\\nnelements={%d, %d}\\nheight=%d\", style=filled, fillcolor=\"%s\"] ;\n", this, this->k, region, reads, writes, include_region, this->includes.nwrites, this->size(), this->includes.nelements[0], this->includes.nelements[1], this->height(), color);
                    }

                    FOREACH_CHILD_BEGIN(this, child, k, dir)
                    {
                        child->dump(f);
                        fprintf(f, "    N%p->N%p ; \n", this, child);
                    }
                    FOREACH_CHILD_END(this, child, k, dir);
                }

                void
                dump_region(FILE * f) const
                {
                    assert(K == 1 || K == 2);
                    if (K == 1)
                    {
                        fprintf(f, "    \\draw (%d,-%d) rectangle (%d,-%d) node[midway] {[%d..%d[ \\\\ reads: %ld \\\\ write:     $%d$};\n",
                                this->region[0].a, 0,
                                this->region[0].b, 2,
                                this->region[0].a, this->region[0].b,
                                this->last_reads.size(), this->has_write
                               );
                    }
                    else if (K == 2)
                    {
                        fprintf(f, "    \\draw (%d,-%d) rectangle (%d,-%d) node[midway] {[%d..%d[ x [%d..%d[ \\\\ reads: %ld \\\\ write: %d};\n",
                                this->region[0].a, this->region[1].a,
                                this->region[0].b, this->region[1].b,
                                this->region[0].a, this->region[0].b,
                                this->region[1].a, this->region[1].b,
                                this->last_reads.size(), this->has_write
                               );
                    }

                    FOREACH_CHILD_BEGIN(this, child, k, dir)
                    {
                        child->dump_region(f);
                    }
                    FOREACH_CHILD_END(this, child, k, dir);
                }
        }; /* class Node */

    public:
        AccessIntervalMultiTree() : root(nullptr), limbs(), outdated() {}

        static void
        subtree_delete(Node * node)
        {
            if (node == nullptr)
                return ;

            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                subtree_delete(child);
            }
            FOREACH_CHILD_END(node, child, k, dir);

            delete node;
        }

        void
        garbage_collector_run(void)
        {
            for (Node * & node : this->limbs)
                subtree_delete(node);
            this->limbs.clear();
        }

        virtual ~AccessIntervalMultiTree()
        {
            subtree_delete(this->root);
            this->garbage_collector_run();
        }

        ///////////
        // UTILS //
        ///////////
        void
        foreach_k_child(Node * root, int k, std::function<void(Node *)> f)
        {
            for (int i = 0 ; i < 2 ; ++i)
            {
                Node * child = root->st[k].children[i];
                if (child)
                {
                    f(child);
                    foreach_k_child(child, k, f);
                }
            }
        }

        void
        foreach_node(Node * root, std::function<void(Node *, void *)> f, void * args) const
        {
            f(root, args);
            FOREACH_CHILD_BEGIN(root, child, k, dir)
            {
                foreach_node(child, f, args);
            }
            FOREACH_CHILD_END(root, child, k, dir);
        }

        int
        height(void) const
        {
            return (this->root ? this->root->height() : 0);
        }

        int
        size(void) const
        {
            return this->root ? this->root->size() : 0;
        }

        void
        export_pdf(void) const
        {
            FILE * file = fopen("tree.dot", "w");
            this->dump(file);
            fclose(file);
            int r = system("dot -Tpdf tree.dot > tree.pdf"); (void) r;
            if (r)
                fprintf(stderr, "dot failed\n");

            file = fopen("region.tex", "w");
            this->dump_region(file);
            fclose(file);
            r = system("pdflatex -interaction=nonstopmode region.tex > /dev/null 2>&1");
            if (r)
                fprintf(stderr, "pdflatex failed\n");
        }

        static inline void
        update_includes_height(Node * node)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                int hleft  = node->st[k].left  ? node->st[k].left->includes.height[k]   : 0;
                int hright = node->st[k].right ? node->st[k].right->includes.height[k]  : 0;
                node->includes.height[k] = 1 + MAX(hleft, hright);
            }
        }

        // TODO : maintaining the size (n-k) per k-tree is a bothersome O(K²)
        // It is currently used to detecting imbalance on a k-subtree
        //
        // Another way would to be maintain the size (n) for the entire b-tree,
        // and if load imbalance is detected - h >=2*K*log(n) - then compute
        // the n-k's and rebalance where it needs
        static inline void
        update_includes_nelements(Node * node)
        {
            for (int k = 0 ; k < K ; ++k)
                node->includes.nelements[k] = 0;
            node->includes.nelements[node->k] = 1;

            for (int k = node->k ; k < K ; ++k)
            {
                for (int kk = 0 ; kk < K ; ++kk)
                {
                    int nl  = node->st[kk].left  ? node->st[kk].left->includes.nelements[k]  : 0;
                    int nr = node->st[kk].right ? node->st[kk].right->includes.nelements[k] : 0;
                    node->includes.nelements[k] += nr + nl;
                }
            }
        }

        static inline void
        update_includes_nwrites(Node * node)
        {
            node->includes.nwrites = node->has_write ? 1 : 0;
            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                node->includes.nwrites += child->includes.nwrites;
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        static inline void
        update_includes_interval(Node * node)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                node->includes.region[k].a = node->region[k].a;
                node->includes.region[k].b = node->region[k].b;

                FOREACH_CHILD_BEGIN(node, child, kk, dir)
                {
                    node->includes.region[k].a = MIN(
                             node->includes.region[k].a,
                            child->includes.region[k].a
                    );

                    node->includes.region[k].b = MAX(
                             node->includes.region[k].b,
                            child->includes.region[k].b
                    );
                }
                FOREACH_CHILD_END(node, child, kk, dir);
            }
        }

        static inline void
        update_includes(Node * node)
        {
            update_includes_nwrites(node);
            update_includes_interval(node);
            update_includes_nelements(node);
            update_includes_height(node);
        }

        // TODO : proecssing 'outdated' list by depth-order would remove
        // redundant updates
        void
        update(void)
        {
            for (Node * & node : this->outdated)
            {
                while (1)
                {
                    if (node->includes.outdated)
                    {
                        update_includes(node);
                        node->includes.outdated = false;
                    }
                    else
                        break ;

                    if (node->parent)
                    {
                        node->parent->includes.outdated = true;
                        node = node->parent;
                    }
                    else
                    {
                        break ;
                    }
                }
            }
            this->outdated.clear();
        }

        void
        outdate(Node * node)
        {
            node->includes.outdated = true;
            this->outdated.push_back(node);
        }

        /**
         *      C              A
         *     / \            / \
         *    A   E    <-    B   C
         *   / \                / \
         *  B   D              D   E
         */
        void
        rotate_left(Node * A, int k)
        {
            tassert(A->st[k].right);

         // Node * B = A->st[k].left;
            Node * C = A->st[k].right;
            Node * D = C->st[k].left;
         // Node * E = C->st[k].right;

//            printf("rl(%d, %d, %d, %d, %d)\n", A->parent ? A->parent->k : -1, A->k, C->k, D ? D->k : -1, k);

            C->st[k].left  = A;
         // C->right = E;
         // A->left  = B;
            A->st[k].right = D;

            C->parent = A->parent;
            if (A->parent == nullptr)
                this->root = C;
            else if (A->parent->st[k].left == A)
                A->parent->st[k].left = C;
            else
                A->parent->st[k].right = C;

         // B->parent = A;
            A->parent = C;
            if (D)
                D->parent = A;
         // E->parent = C;

         // update_includes(B);
         // update_includes(D);
            update_includes(A);
         // update_includes(E);
            update_includes(C);
        }


        /**
         *      A              B
         *     / \            / \
         *    B   C    ->    D   A
         *   / \                / \
         *  D   E              E   C
         */
        void
        rotate_right(Node * A, int k)
        {
            Node * B = A->st[k].left;
         // Node * C = A->st[k].right;
         // Node * D = B->st[k].left;
            Node * E = B->st[k].right;

//            printf("rr(%d, %d, %d, %d, %d)\n", A->parent ? A->parent->k : -1, A->k, B->k, E ? E->k : -1, k);

            // UPDATE LINKS

         // B->st[k].left  = D;
            B->st[k].right = A;
            A->st[k].left  = E;
         // A->st[k].right = C;

            B->parent = A->parent;
            if (A->parent == nullptr)
                this->root = B;
            else if (A->parent->st[k].left == A)
                A->parent->st[k].left = B;
            else
                A->parent->st[k].right = B;

            if (E)
                E->parent = A;
         // C->parent = A;
            A->parent = B;
         // D->parent = B;

            // UPDATE ACCESS_MODE_RCLUDES
         // update_includes(E);
         // update_includes(C);
            update_includes(A);
         // update_includes(D);
            update_includes(B);
        }

        void
        balance_fixup(Node * z, int k)
        {
            tassert(z->colors[k] == RED);

            // Traditional red-black tree balancing...
            while (z->parent && z->parent->colors[k] == RED)
            {
                // .. but stopping on the k-root
                if (z->parent->parent && z->parent->parent->k < k)
                {
                    tassert(z->colors[k]            == RED);
                    tassert(z->parent->colors[k]    == RED);
                    z->parent->colors[k] = BLACK;
                    break ;
                }

                if (z->parent == z->parent->parent->st[k].left)
                {
                    Node * y = z->parent->parent->st[k].right;
                    if (y && y->colors[k] == RED)
                    {
                        z->parent->colors[k] = BLACK;
                        y->colors[k] = BLACK;
                        z->parent->parent->colors[k] = RED;
                        z = z->parent->parent;
                    }
                    else
                    {
                        if (z == z->parent->st[k].right)
                        {
                            z = z->parent;
                            this->rotate_left(z, k);
                        }
                        z->parent->colors[k] = BLACK;
                        z->parent->parent->colors[k] = RED;
                        this->rotate_right(z->parent->parent, k);
                    }
                }
                else
                {
                    Node * y = z->parent->parent->st[k].left;

                    if (y && y->colors[k] == RED)
                    {
                        z->parent->colors[k] = BLACK;
                        y->colors[k] = BLACK;
                        z->parent->parent->colors[k] = RED;
                        z = z->parent->parent;
                    }
                    else
                    {
                        if (z == z->parent->st[k].left)
                        {
                            z = z->parent;
                            this->rotate_right(z, k);
                        }
                        z->parent->colors[k] = BLACK;
                        z->parent->parent->colors[k] = RED;
                        this->rotate_left(z->parent->parent, k);
                    }
                }
            }
            this->root->colors[k] = BLACK;
        }

        void
        insert_fixup(
            Node * parent,
            int k,
            Direction dir,
            Node * node,
            access_mode_t mode,
            T * obj
        ) {
            tassert(node);
            parent->st[k].children[dir] = node;
            node->parent = parent;
            node->register_access(mode, obj);

            this->outdate(node);

            // inserting a new k-subtree, this k-root is black
            if (parent->k < k)
                node->colors[k] = BLACK;
            // rebalance the k-subtree
            else
                this->balance_fixup(node, k);
        }

# ifdef REBALANCE

        static inline void
        compress(Node * root, int k, int m)
        {
            Node * tmp = root->st[k].right;

            for (int i = 0; i < m ; ++i)
            {
                Node * oldtmp = tmp;
                tmp = tmp->st[k].right;
                root->st[k].right = tmp;
                oldtmp->st[k].right = tmp->st[k].left;
                tmp->st[k].left = oldtmp;
                root = tmp;
                tmp = tmp->st[k].right;
            }
        }

        static inline int
        vine_to_rbtree(Node * root, int k, int n)
        {
            int h = log2(n + 1);
            int m = twopow(h) - 1;

            compress(root, k, n - m);

            for (m = m / 2; m > 0; m /= 2)
                compress(root, k, m);

            return h;
        }

        static void
        rbtree_to_vine(Node * root, int k)
        {
            Node * tmp = root->st[k].right;

            while (tmp)
            {
                if (tmp->st[k].left)
                {
                    Node * oldtmp = tmp;
                    tmp = tmp->st[k].left;
                    oldtmp->st[k].left = tmp->st[k].right;
                    tmp->st[k].right = oldtmp;
                    root->st[k].right = tmp;
                }
                else
                {
                    root = tmp;
                    tmp = tmp->st[k].right;
                }
            }
        }

        // fixup the tree that just got rebalanced
        static inline void
        rebalance_fixup(Node * parent, Node * node, int k, int depth, int height)
        {
            if (node == nullptr)
                return ;

            rebalance_fixup(node, node->st[k].left,  k, depth + 1, height);
            rebalance_fixup(node, node->st[k].right, k, depth + 1, height);

            node->parent = parent;
            node->colors[k] = (height == depth) ? RED : BLACK;
            update_includes(node);
        }

        // rebalance the k-subtree using a Day-Stout-Warren algorithm
        inline void
        rebalance(Node * root, int k)
        {
            printf("Rebalancing for k=%d\n", k);
            tassert(k == 0 && K == 0 && "Not implemented when K>1");

            Node pseudo_root;
            pseudo_root.st[k].right = root;

            rbtree_to_vine(&pseudo_root, k);
            int height = vine_to_rbtree(&pseudo_root, k, root->includes.nelements[k]);

            // fixup the tree
            Node * new_root = pseudo_root.st[k].right;
            if (root->parent == nullptr)
                this->root = new_root;
            else
            {
                // k > 0 here
                tassert(0 && "Not implemented when K>1");
            }

            rebalance_fixup(nullptr, new_root, k, 0, height);

# ifndef NDEBUG
            this->coherency();
# endif /* NDEBUG */
        }

        inline void
        rebalance(Node * root)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->requires_rebalance(root, k))
                    rebalance(root, k);
            }
        }

        inline void
        rebalance(void)
        {
            this->garbage_collector_run();
            this->rebalance(this->root);
        }

        // heuristic to determine whether the tree needs rebalancing
        template<int KDIM>
        inline int
        requires_rebalance(const int nelements, const int height) const
        {
            int ideal_height = log2(nelements + 1);
            // return (height >= 8 && height > 2 * KDIM * ideal_height);
            return (height > 2 * KDIM * ideal_height);
        }

        // if the k-subtree starting at 'root' requires rebalance
        inline int
        requires_rebalance(Node * root, int k)
        {
            const int nelements = root->includes.nelements[k];
            const int height = root->includes.height[k];
            return this->requires_rebalance<1>(nelements, height);
        }

        // if the multi-tree at 'root' requires rebalance
        inline int
        requires_rebalance(Node * root) const
        {
            const int nelements = root->size();
            const int height = root->height();
            return this->requires_rebalance<K>(nelements, height);
        }

        // if 'this' multi-tree requires rebalance
        inline bool
        requires_rebalance(void) const
        {
            return this->root && requires_rebalance(this->root);
        }
# endif /* REBALANCE */

# ifdef CUT
        void
        insert_from_cut(
            Node * parent,
            access_mode_t mode,
            Region & region,
            T * obj
        ) {
            tassert(mode & ACCESS_MODE_W);

            FOREACH_CHILD_BEGIN(parent, child, k, dir)
            {
                this->limbs.push_back(child);
                parent->st[k].children[dir] = nullptr;
            }
            FOREACH_CHILD_END(parent, child, k, dir);

            parent->region.copy(region);
            parent->register_access(mode, obj);

            parent->includes.region.copy(region);
            parent->includes.nwrites = 1;

            for (int k = 0 ; k < K ; ++k)
            {
                parent->includes.nelements[k]   = 1;
                parent->includes.height[k]      = 1;
            }

            this->outdate(parent);
        }
# endif /* CUT */

        void
        insert_from(
            Node * parent,
            access_mode_t mode,
            Region & region,
            T * obj,
            int k,
            Node * node
        ) {
            DEBUG("---  (start) subinsert of dimension %d and type %s for obj %p", K, access_mode_to_str(mode), obj);
            for (int i = 0 ; i < K ; ++i)
                DEBUG("---   [%d, %d[", region[i].a, region[i].b);

            // TODO : optimisation
            //  Unroll this loop using a recursive template
            while (k < K)
            {
                // quick-way out, if the region includes all subregion with an
                // 'out' access, we can discard all children
# ifdef CUT
#  pragma message("Tree cut enable")
                // TODO : the includes test could be accelerated simply
                // checking >=k dimensions, as we know we are already matching
                // <k dimensions
                if (mode & ACCESS_MODE_W)
                {
                   if (region.includes(parent->includes.region))
                   {
                       // TODO : what if 'node' is not null ?  probably want to
                       // return something to callee for the case (3)
                       tassert(node == nullptr);
                       this->insert_from_cut(parent, mode, region, obj);
                       break ;
                   }
                }
# else
#  pragma message("Tree cut disabled. Enable it using '-DCUT'")
# endif /* CUT */

                // Process 6 cases


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
                        this->insert_fixup(parent, k, LEFT, node, mode, obj);
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
                        this->insert_fixup(parent, k, RIGHT, node, mode, obj);
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
                            parent->register_access(mode, obj);
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
                        parent->region[k] = region[k];

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
                            this->insert_from(this->root, mode, r, nullptr, 0, node);
                            node->inherit_accesses(parent);
                        }

                        // shrink children for dimensions k' > k
                        if (k < K-1)
                        {
                            std::function<void(Node *)> f = [this, &k, &region, &x](Node * node) {
                                node->region[k] = region[k];
                                for (int i = 0 ; i < 2 ; ++i)
                                {
                                    if (x[2*i+0] == x[2*i+1])
                                        continue ;
                                    Region r(node->region);
                                    r[k].a = x[2*i+0];
                                    r[k].b = x[2*i+1];
                                    Node * split = new Node(r, k, RED);
                                    // TODO : probably no need to restart from root and dim 0
                                    this->insert_from(this->root, ACCESS_MODE_VOID, r, nullptr, 0, split);
                                    split->inherit_accesses(node);
                                }
                            };
                            foreach_k_child(parent, k+1, f);
                        }

                        // TODO : optimisation, unnecessary if we outdated another child
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
            DEBUG("---  (done) subinsert of dimension %d and type %s for obj %p", K, access_mode_to_str(mode), obj);
        }

        // Insert the new access in the tree
        void
        insert(access_mode_t mode, Region & region, T * obj)
        {
            DEBUG("##############################");
            DEBUG("--- New insert of dimension %d and type %s for obj %p", K, access_mode_to_str(mode), obj);
            for (int i = 0 ; i < K ; ++i)
                DEBUG("---  [%d, %d[", region[i].a, region[i].b);
            DEBUG("##############################");

            tassert(!region.is_empty());

            if (this->root == nullptr)
            {
                this->root = new Node(region);
                this->root->register_access(mode, obj);
                update_includes(this->root);
            }
            else
            {
                this->insert_from(this->root, mode, region, obj, 0, nullptr);
                this->update();
            }

# ifdef REBALANCE
#  pragma message("Automatic rebalance enabled.")
            if (this->requires_rebalance())
                this->rebalance();
# else /* REBALANCE */
#  pragma message("Automatic rebalance disabled. Enable it with '-DREBALANCE'")
# endif /* REBALANCE */

# ifndef NDEBUG
            this->coherency();
# endif /* NDEBUG */
        }

        void
        intersect_from(Node * node, access_mode_t mode, Region & region, T * obj) const
        {
            if (node == nullptr)
                return ;

            if (mode == ACCESS_MODE_R && node->includes.nwrites == 0)
                return ;

            if (!region.intersects(node->includes.region))
                return ;

            if (region.intersects(node->region))
            {
                if (mode & ACCESS_MODE_W && node->last_reads.size())
                    for (T * & pred : node->last_reads)
                        this->on_hazard(node->region, pred, region, obj);
                else if (node->has_write)
                    this->on_hazard(node->region, node->last_write, region, obj);
            }

            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                this->intersect_from(child, mode, region, obj);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        // Retrieve objects previously inserted that intersect with the interval
        void
        intersect(access_mode_t mode, Region & region, T * obj) const
        {
            this->intersect_from(this->root, mode, region, obj);
        }

        // Dump the tree to the given file
        void
        dump(FILE * f) const
        {
            fprintf(f, "digraph g {\n");
            if (this->root)
                this->root->dump(f);
            fprintf(f, "}\n");
        }

        // Dump represented region to the given file
        void
        dump_region(FILE * f) const
        {
            fprintf(f, "\\documentclass[crop,tikz]{standalone}\n");
            fprintf(f, "\\usetikzlibrary{shapes.multipart}\n");
            fprintf(f, "\\begin{document}\n");
            fprintf(f, "\\begin{tikzpicture}[every text node part/.style={align=center}]\n"  );

            if constexpr (K == 1 || K == 2)
            {
                if (this->root)
                    this->root->dump_region(f);
            }
            else
            {
                fprintf(f, " Output for K=%d is not supported", K);
            }

            fprintf(f, "  \\end{tikzpicture}\n");
            fprintf(f, "\\end{document}\n");
        }

    private:
        // Root node
        Node * root;

        // List of cut-out branches whose subtree requires deletion
        std::vector<Node *> limbs;

        // Buffer of nodes inserted by an insert() call
        std::vector<Node *> outdated;

#ifndef NDEBUG

    public:
        //////////////////////
        // Coherency checks //
        //////////////////////
        void
        coherency_k(Node * node) const
        {
            // if k < k', then k'-nodes must be children of k-nodes
            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                tassert(node->k <= child->k);
                tassert(child->k == k);
                coherency_k(child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        void
        coherency_single_path_reset(Node * node) const
        {
            node->checks.id = 0;
            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                coherency_single_path_reset(child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        void
        coherency_single_path_set(Node * node) const
        {
            tassert(node->checks.id == 0);
            node->checks.id = 1;
            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                coherency_single_path_set(child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        void
        coherency_single_path(Node * node) const
        {
            // graph must be a tree
            coherency_single_path_reset(node);
            coherency_single_path_set(node);
        }

        void
        coherency_region_includes_check(Node * ref, void * args) const
        {
            Node * root = (Node *) args;
            tassert(root->includes.region.includes(ref->region));
        }

        void
        coherency_region_includes_foreach(Node * node, void * args) const
        {
            (void) args;
            auto f = std::bind(&AccessIntervalMultiTree<K, T>::coherency_region_includes_check, this, _1, _2);
            foreach_node(node, f, node);
        }

        void
        coherency_region_includes(Node * root) const
        {
            auto f = std::bind(&AccessIntervalMultiTree<K, T>::coherency_region_includes_foreach, this, _1, _2);
            foreach_node(root, f, root);
        }

        void
        coherency_region_disjoint_compare(Node * ref, void * args) const
        {
            Node * node = (Node *) args;
            tassert(node == ref || !node->region.intersects(ref->region));
        }

        void
        coherency_region_disjoint_for(Node * node, void * args) const
        {
            Node * root = (Node *) args;
            auto f = std::bind(&AccessIntervalMultiTree<K, T>::coherency_region_disjoint_compare, this, _1, _2);
            foreach_node(root, f, node);
        }

        void
        coherency_region_disjoint(Node * root) const
        {
            auto f = std::bind(&AccessIntervalMultiTree<K, T>::coherency_region_disjoint_for, this, _1, _2);
            foreach_node(root, f, root);
        }

        void
        coherency_color(Node * node) const
        {
            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                // a node is black or red
                tassert(node->colors[k] == BLACK || node->colors[k] == RED);

                // children of a red nodes are black
                if (node->colors[k] == RED)
                {
                    tassert(child->colors[k] == BLACK);
                }

                coherency_color(child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        int
        coherency_black_height_k(Node * node, int k) const
        {
#ifdef CUT
            // when cut is enabled, black_height is not guaranteed
            return 1;
#endif
            if (node == nullptr)
                return 1;

            int color = (node->colors[k] == BLACK) ? 1 : 0;
            int left_child_height  = color + coherency_black_height_k(node->st[k].left,  k);
            int right_child_height = color + coherency_black_height_k(node->st[k].right, k);
            tassert(left_child_height == right_child_height);

            if (k+1 < K)
            {
                coherency_black_height_k(node->st[k+1].left,  k+1);
                coherency_black_height_k(node->st[k+1].right, k+1);
            }

            return left_child_height;
        }

        void
        coherency_black_height(Node * node) const
        {
            coherency_black_height_k(node, 0);
        }

        void
        coherency_balance(void) const
        {
            int height    = this->height();
            int nelements = this->size();
            int ideal_height = log2(nelements + 1);
            // tassert(height < 8 || height < 2 * K * ideal_height); // TODO : cut condition
            tassert(height <= 2 * K * ideal_height);
        }

        void
        coherency_nelements(Node * node) const
        {
            int nelements[K];
            for (int k = 0 ; k < K ; ++k)
                nelements[k] = 0;
            nelements[node->k] = 1;

            for (int k = node->k ; k < K ; ++k)
            {
                for (int kk = 0 ; kk < K ; ++kk)
                {
                    int nl = node->st[kk].left  ? node->st[kk].left->includes.nelements[k]  : 0;
                    int nr = node->st[kk].right ? node->st[kk].right->includes.nelements[k] : 0;
                    nelements[k] += nr + nl;
                }
            }

            for (int k = 0 ; k < K ; ++k)
                tassert(nelements[k] == node->includes.nelements[k]);
        }

        void
        coherency_k_hierarchy(Node * node) const
        {
            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                tassert(child->k == k);
                tassert(node->k <= child->k);
                tassert(k == 0 || node->region[k-1] == child->region[k-1]);
                coherency_k_hierarchy(child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        void
        coherency_from(Node * root) const
        {
            /* 2. check per-node nelements */
            coherency_nelements(root);

            /* 3. If a node is red, then both its children are black */
            coherency_color(root);

            /* 4. Every path from a node to any of its descbant NULL nodes
             * has the same number of black nodes */
            coherency_black_height(root);
            // TODO : this fail
            // TODO : all new nodes for a new dimension should be black ?

            /* 5. region must be disjoint (weak check) */
            coherency_region_disjoint(root);

            /* 6. check includes region */
            coherency_region_includes(root);

            /* 7. includeness relationship between nodes dimension */
            coherency_k(root);

            /* 8. graph must be a tree (only 1 path from root to each node) */
            coherency_single_path(root);

            /* 9. children for a k-tree must have the same k-interval as their parent */
            coherency_k_hierarchy(root);
        }

        int
        coherency(void)
        {
            if (this->root)
            {
                /* 1. The root of the this is always black */
                for (int k = 0 ; k < K ; ++k)
                    tassert(this->root->colors[k] == BLACK);

                /* per-node checks */
                this->coherency_from(this->root);

                /* 7. check balance */
                this->coherency_balance();
            }

            return 1;
        }

#endif /* NDEBUG */

};

#undef tassert

#endif /* __ACCESS_INTERVAL_MULTI_TREE_H__ */
