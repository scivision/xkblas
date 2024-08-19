#ifndef __KINTERVAL_BTREE_H__
# define __KINTERVAL_BTREE_H__

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

# include <type_traits>

# include <ostream>
# include <iostream>

# ifndef MIN
#  define MIN(X, Y) ((Y) < (X) ? (Y) : (X))
# endif /* MINode */

# ifndef MAX
#  define MAX(X, Y) ((X) < (Y) ? (Y) : (X))
# endif /* MAX */

#ifdef DEBUG
# undef DEBUG
# define DEBUG(...)             \
    do {                        \
        printf(__VA_ARGS__);    \
        printf("\n");           \
    } while (0);
#else
# define DEBUG(...)
#endif

# include <cstdint>

static inline uint64_t
get_nanotime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t) ts.tv_nsec;
}

static inline int
log2(int n)
{
    return 31 - __builtin_clz(n);
}

static inline int
twopow(int n)
{
    return (1 << n);
}

# include "access-mode.h"
# include "color.h"
# include "direction.h"

# define FOREACH_CHILD_BEGIN(N, C, I, D)                                \
do {                                                                    \
    for (int I = 0 ; I < K ; ++I)                                       \
    {                                                                   \
        for (int D = LEFT ; D < DIRECTION_MAX ; ++D)                    \
        {                                                               \
            Node * C = reinterpret_cast<Node *>(N->st[I].children[D]);  \
            if (C)                                                      \
            {
# define FOREACH_CHILD_END(N, C, I, D)                                  \
            }                                                           \
        }                                                               \
    }                                                                   \
} while (0)

/* K is the number of dimensions */
template<int K>
class KIntervalBtree {

    using Region = Intervals<K>;

    public:

        class Node {

            public:

                /* a node subtree */
                typedef union
                {
                    Node * children[2];
                    struct {
                        Node * left;
                        Node * right;
                    };
                } subtree_t;

                /* node's parent */
                Node * parent;

                /* node's color in each subtree */
                Color colors[K];

                /* the region represented by this node */
                Region region;

                /* the dimension represented by this node */
                int k;

                /* node's child */
                subtree_t st[K];

                struct {
                    Region region;    // subtree englobing region
                    int nelements[K];       // subtree number of elements
                    int height[K];          // subtree height
                    int outdated;           // whether 'includes' struct must be recomputed
                } includes;

                #ifndef NDEBUG
                struct {
                    int id;
                } checks;
                #endif /* NDEBUG */

            public:

                Node() :
                    parent(nullptr),
                    colors{BLACK}
                {}

                Node(const Region & r, int k, Color color) :
                    parent(nullptr),
                    colors{BLACK},
                    region(r),
                    k(k)
                {
                    memset(this->st, 0, sizeof(this->st));

                    this->includes.region.copy(r);
                    this->includes.outdated = 0;

                    memset(this->includes.nelements, 0, sizeof(this->includes.nelements));
                    this->includes.nelements[k] = 1;

                    memset(this->includes.height, 0, sizeof(this->includes.height));
                    for (int i = k ; i < K ; ++i)
                        this->includes.height[i] = 1;

                    this->colors[k] = color;
                }

                virtual ~Node() {}

                inline void
                update_includes_height(void)
                {
                    for (int k = 0 ; k < K ; ++k)
                    {
                        int hleft  = this->st[k].left  ? this->st[k].left->includes.height[k]   : 0;
                        int hright = this->st[k].right ? this->st[k].right->includes.height[k]  : 0;
                        this->includes.height[k] = 1 + MAX(hleft, hright);
                    }
                }

                // TODO : maintaining the size (n-k) per k-tree is a bothersome O(K²)
                // It is currently used to detecting imbalance on a k-subtree
                //
                // Another way would to be maintain the size (n) for the entire b-tree,
                // and if load imbalance is detected - h >=2*K*log(n) - then compute
                // the n-k's and rebalance where it needs
                inline void
                update_includes_nelements(void)
                {
                    for (int k = 0 ; k < K ; ++k)
                        this->includes.nelements[k] = 0;
                    this->includes.nelements[this->k] = 1;

                    for (int k = this->k ; k < K ; ++k)
                    {
                        for (int kk = 0 ; kk < K ; ++kk)
                        {
                            int nl = this->st[kk].left  ? this->st[kk].left->includes.nelements[k]  : 0;
                            int nr = this->st[kk].right ? this->st[kk].right->includes.nelements[k] : 0;
                            this->includes.nelements[k] += nr + nl;
                        }
                    }
                }

                inline void
                update_includes_interval(void)
                {
                    for (int k = 0 ; k < K ; ++k)
                    {
                        this->includes.region[k].a = this->region[k].a;
                        this->includes.region[k].b = this->region[k].b;

                        FOREACH_CHILD_BEGIN(this, child, kk, dir)
                        {
                            this->includes.region[k].a = MIN(
                                 this->includes.region[k].a,
                                child->includes.region[k].a
                            );

                            this->includes.region[k].b = MAX(
                                 this->includes.region[k].b,
                                child->includes.region[k].b
                            );
                        }
                        FOREACH_CHILD_END(this, child, kk, dir);
                    }
                }

                virtual inline void
                update_includes(void)
                {
                    this->update_includes_interval();
                    this->update_includes_nelements();
                    this->update_includes_height();
                }

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

                void
                dump(FILE * f) const
                {
                    {
                        const char * COLORS[] = {
                            "#000000",
                            "#FF0000",
                            "#00FF00",
                            "#0000FF",
                            "#FFFF00",
                            "#FF00FF",
                            "#00FFFF",
                        };
                        const char * color = COLORS[this->colors[this->k] == BLACK ? 0 : this->k+1];

                        char region[1024];
                        this->region.tostring(region, sizeof(region));

                        char include_region[1024];
                        this->includes.region.tostring(include_region, sizeof(include_region));

                        fprintf(f, "    N%p[fontcolor=\"#ffffff\", label=\"--- node ---\\nk=%d\\n%s\\n\\n--- includes ---\\n%s\\\nsize=%d\\nnelements={%d, %d}\\nheight=%d\", style=filled, fillcolor=\"%s\"] ;\n", this, this->k, region, include_region, this->size(), this->includes.nelements[0], this->includes.nelements[1], this->height(), color);
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
                        fprintf(f, "    \\draw (%d,-%d) rectangle (%d,-%d) node[midway] {[%d..%d[};\n",
                                this->region[0].a, 0,
                                this->region[0].b, 2,
                                this->region[0].a, this->region[0].b
                        );
                    }
                    else if (K == 2)
                    {
                        fprintf(f, "    \\draw (%d,-%d) rectangle (%d,-%d) node[midway] {[%d..%d[ x [%d..%d[};\n",
                                this->region[0].a, this->region[1].a,
                                this->region[0].b, this->region[1].b,
                                this->region[0].a, this->region[0].b,
                                this->region[1].a, this->region[1].b
                        );
                    }

                    FOREACH_CHILD_BEGIN(this, child, k, dir)
                    {
                        child->dump_region(f);
                    }
                    FOREACH_CHILD_END(this, child, k, dir);
                }

        }; /* class Node */

    /* class tree */
    public:

        /* Root node */
        Node * root;

    private:
        /* List of cut-out branches whose subtree requires deletion from memory */
        std::vector<Node *> limbs;

        /* Buffer of nodes inserted by an insert() call */
        std::vector<Node *> outdated;

    public:
        KIntervalBtree() : root(nullptr), limbs(), outdated() {}

        inline void
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

        inline void
        cut(Node * parent, int k, int dir)
        {
            this->limbs.push_back(parent->st[k].children[dir]);
            parent->st[k].children[dir] = nullptr;
        }

        inline void
        garbage_collector_run(void)
        {
            for (Node * & node : this->limbs)
                subtree_delete(node);
            this->limbs.clear();
        }

        virtual ~KIntervalBtree()
        {
            subtree_delete(this->root);
            this->garbage_collector_run();
        }

        ///////////
        // UTILS //
        ///////////
        template <typename T>
        void
        foreach_k_child(
            T * root,
            int k,
            std::function<void(T *)> f
        ) {
            static_assert(std::is_base_of<Node, T>::value);

            for (int i = 0 ; i < 2 ; ++i)
            {
                T * child = reinterpret_cast<T *>(root->st[k].children[i]);
                if (child)
                {
                    f(child);
                    foreach_k_child(child, k, f);
                }
            }
        }

        void
        foreach_node(
            Node * root,
            std::function<void(Node *, void *)> f, void * args
        ) const {
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
                        node->update_includes();
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

         // B->update_includes();
         // D->update_includes();
            A->update_includes();
         // E->update_includes();
            C->update_includes();
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
         // E->update_includes();
         // C->update_includes();
            A->update_includes();
         // D->update_includes();
            B->update_includes();
        }

        inline void
        balance_fixup(int k, Node * z)
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

        inline void
        insert_fixup(
            Node * parent,
            int k,
            Direction dir,
            Node * node
        ) {
            tassert(node);
            parent->st[k].children[dir] = node;
            node->parent = parent;
            this->outdate(node);

            // inserting a new k-subtree, this k-root is black
            if (parent->k < k)
            {
                node->colors[k] = BLACK;
            }
            // rebalance the k-subtree
            else
            {
                this->balance_fixup(k, node);
            }
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
            node->update_includes();
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

        // if the btree at 'root' requires rebalance
        inline int
        requires_rebalance(Node * root) const
        {
            const int nelements = root->size();
            const int height = root->height();
            return this->requires_rebalance<K>(nelements, height);
        }

        // if 'this' btree requires rebalance
        inline bool
        requires_rebalance(void) const
        {
            return this->root && requires_rebalance(this->root);
        }
# endif /* REBALANCE */

        void
        post_insert(void)
        {
            this->update();

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
            auto f = std::bind(&KIntervalBtree<K>::coherency_region_includes_check, this, _1, _2);
            foreach_node(node, f, node);
        }

        void
        coherency_region_includes(Node * root) const
        {
            auto f = std::bind(&KIntervalBtree<K>::coherency_region_includes_foreach, this, _1, _2);
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
            auto f = std::bind(&KIntervalBtree<K>::coherency_region_disjoint_compare, this, _1, _2);
            foreach_node(root, f, node);
        }

        void
        coherency_region_disjoint(Node * root) const
        {
            auto f = std::bind(&KIntervalBtree<K>::coherency_region_disjoint_for, this, _1, _2);
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

#endif /* __KINTERVAL_BTREE_H__ */
