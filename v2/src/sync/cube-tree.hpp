// TODO PERFORMANCES IDEA
//  - when inserting, it currently queue tasks to propagates 'includes' info before returning doing a bottom-up traversal
//    Instead, 'includes' info could be updated when
//      - 'going-down' during the insertion,
//      - 'rotating'
//    so there is no need for the bottom-up traversal...
//
//  - remove the use of 'virtual' to replace with template 'node' type and inlined functions
//
//  - check loop unrolling

#ifndef __CUBE_TREE_H__
# define __CUBE_TREE_H__

# if defined(CUBE_TREE_REBALANCE) && !defined(CUBE_TREE_CUT)
#  error "Defining 'CUBE_TREE_REBALANCE' but not 'CUBE_TREE_CUT' has no effects"
# endif

// tree assert, must be called within a member function
# if defined(NDEBUG) || defined(CUBE_TREE_DISABLE_COHERENCY_CHECKS)
#  define tassert(ignore) ((void)0)
# else /* NDEBUG */
#  pragma message("Define `NDEBUG` for max performance")
#  define tassert(expr)                                                         \
    do {                                                                        \
        if (!(expr))                                                            \
        {                                                                       \
            this->export_pdf("cube");                                      \
            fprintf(stdout, "%s:%d: assertion `" #expr "` failed in `%s`\n",    \
                   __FILE__,__LINE__,__func__);                                 \
            abort();                                                            \
        }                                                                       \
    } while (0)
# endif /* NDEBUG */

// coherency checks
# ifndef CUBE_TREE_DISABLE_COHERENCY_CHECKS
#  pragma message("Define `CUBE_TREE_DISABLE_COHERENCY_CHECKS` for max performance")
# endif

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
# endif /* MIN */

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

# define FOREACH_K_CHILD_BEGIN(N, C, I, D)                              \
do {                                                                    \
    for (int D = LEFT ; D < DIRECTION_MAX ; ++D)                        \
    {                                                                   \
        Node * C = reinterpret_cast<Node *>(N->st[I].children[D]);      \
        if (C)                                                          \
        {
# define FOREACH_K_CHILD_END(N, C, I, D)                                \
        }                                                               \
    }                                                                   \
} while (0)

# define FOREACH_CHILD_BEGIN(N, C, I, D)                                \
do {                                                                    \
    for (int I = 0 ; I < K ; ++I)                                       \
    {                                                                   \
        FOREACH_K_CHILD_BEGIN(N, C, I, D)                               \
        {
# define FOREACH_CHILD_END(N, C, I, D)                                  \
        }                                                               \
        FOREACH_K_CHILD_END(N, C, I, D);                                \
    }                                                                   \
} while (0)

/* K is the number of dimensions */
template<int K, typename T>
class KCubeTree {

    using Cube = KCube<K>;

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

                /* the cube represented by this node */
                Cube cube;

                /* the dimension represented by this node */
                int k;

                /* node's child */
                subtree_t st[K];

                struct {
                    Cube cube;    // subtree englobing cube
                    int nelements[K];       // subtree number of elements
                    int height[K];          // subtree height
                    int outdated;           // whether 'includes' struct must be recomputed
                } includes;

                #ifndef CUBE_TREE_DISABLE_COHERENCY_CHECKS
                struct {
                    int id;
                } checks;
                #endif /* CUBE_TREE_DISABLE_COHERENCY_CHECKS */

            public:

                Node() :
                    parent(nullptr),
                    colors{BLACK}
                {}

                Node(
                    const Cube & r,
                    const int k,
                    const Color color
                ) :
                    parent(nullptr),
                    colors{BLACK},
                    cube(r),
                    k(k)
                {
                    memset(this->st, 0, sizeof(this->st));

                    this->includes.cube.copy(r);
                    this->includes.outdated = 0;

                    memset(this->includes.nelements, 0, sizeof(this->includes.nelements));
                    this->includes.nelements[k] = 1;

                    memset(this->includes.height, 0, sizeof(this->includes.height));
                    for (int i = k ; i < K ; ++i)
                        this->includes.height[i] = 1;

                    this->colors[k] = color;
                }

                virtual ~Node() {}

                /////////////////////////
                // ABSTRACT INTERFACES //
                /////////////////////////

                /* called whenever this node is added to the tree with an
                 * access (this->cube, mode) */
                virtual void on_insert(T & t, const access_mode_t mode) = 0;

                /* called whenever this node is being shrinked on dimension 'k' to 'interval' */
                virtual void on_shrink(const Interval & interval, int k) = 0;

                /* called to detect whether the access intersects with 'this' node */
                virtual bool intersect_stop_test(T & t, const Cube & cube, const access_mode_t mode) const = 0;

                /* called whenever 'this' intersects with the access */
                virtual void on_intersect(T & t, const Cube & cube, const access_mode_t mode) = 0;

                ///////////////
                // Utilities //
                ///////////////

                inline Node *
                get_child(int k, Direction dir) const
                {
                    return reinterpret_cast<Node *>(this->st[k].children[dir]);
                }

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
                        this->includes.cube[k].a = this->cube[k].a;
                        this->includes.cube[k].b = this->cube[k].b;

                        FOREACH_CHILD_BEGIN(this, child, kk, dir)
                        {
                            this->includes.cube[k].a = MIN(
                                 this->includes.cube[k].a,
                                child->includes.cube[k].a
                            );

                            this->includes.cube[k].b = MAX(
                                 this->includes.cube[k].b,
                                child->includes.cube[k].b
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
                            "#EE3333",
                            "#3333EE",
                            "#33EE33",
                            "#FFFF00",
                            "#FF00FF",
                            "#00FFFF",
                        };
                        const char * color = COLORS[this->colors[this->k] == BLACK ? 0 : this->k+1];

                        fprintf(f, "    N%p[fontcolor=\"#ffffff\", label=\"--- node ---\\n", this);
                        this->dump_str(f);
                        fprintf(f, "\", style=filled, fillcolor=\"%s\"] ;\n", color);
                    }

                    FOREACH_CHILD_BEGIN(this, child, k, dir)
                    {
                        child->dump(f);
                        fprintf(f, "    N%p->N%p ; \n", this, child);
                    }
                    FOREACH_CHILD_END(this, child, k, dir);
                }

                virtual void
                dump_str(FILE * f) const
                {
                    char cube[1024];
                    this->cube.tostring(cube, sizeof(cube));

                    char include_cube[1024];
                    this->includes.cube.tostring(include_cube, sizeof(include_cube));

                    if (K == 2)
                    {
                        fprintf(f, "k=%d\\n%s\\n\\n--- includes ---\\n%s\\nsize=%d\\nnelements={%d, %d}\\nheight=%d",
                            this->k,
                            cube,
                            include_cube,
                            this->size(),
                            this->includes.nelements[0],
                            this->includes.nelements[1],
                            this->height()
                        );
                    }
                }

                void
                dump_cube(FILE * f) const
                {
                    assert(K == 1 || K == 2);
                    if (K == 1)
                    {
                        fprintf(f, "    \\draw (" INTERVAL_TYPE_MODIFIER ",-" INTERVAL_TYPE_MODIFIER ") rectangle (" INTERVAL_TYPE_MODIFIER ",-" INTERVAL_TYPE_MODIFIER ") node[midway] {[" INTERVAL_TYPE_MODIFIER ".." INTERVAL_TYPE_MODIFIER "[};\n",
                                this->cube[0].a, (INTERVAL_TYPE_T) 0,
                                this->cube[0].b, (INTERVAL_TYPE_T) 2,
                                this->cube[0].a, this->cube[0].b
                        );
                    }
                    else if (K == 2)
                    {
                        fprintf(f, "    \\draw (" INTERVAL_TYPE_MODIFIER ",-" INTERVAL_TYPE_MODIFIER ") rectangle (" INTERVAL_TYPE_MODIFIER ",-" INTERVAL_TYPE_MODIFIER ") node[midway] {",
                            this->cube[1].a, this->cube[0].a,
                            this->cube[1].b, this->cube[0].b
                        );
                        this->dump_cube_str(f);
                        fprintf(f, "};\n");
                    }

                    FOREACH_CHILD_BEGIN(this, child, k, dir)
                    {
                        child->dump_cube(f);
                    }
                    FOREACH_CHILD_END(this, child, k, dir);
                }

                virtual void
                dump_cube_str(FILE * f) const
                {
                    fprintf(f, "[" INTERVAL_TYPE_MODIFIER ".." INTERVAL_TYPE_MODIFIER "[ x [" INTERVAL_TYPE_MODIFIER ".." INTERVAL_TYPE_MODIFIER "[",
                        this->cube[0].a, this->cube[0].b,
                        this->cube[1].a, this->cube[1].b
                    );
                }

        }; /* class Node */

        /* pseudo node to implement Day-Stout-Warren algorithm. Abstract
         * interfaces are not used, so just implemented them as 'no-op' */
        class PseudoNode : public Node
        {
                void
                on_insert(T & t, const access_mode_t mode)
                {
                    (void) t;
                    (void) mode;
                    assert(0);
                }

                void
                on_shrink(const Interval & interval, int k)
                {
                    (void) interval;
                    (void) k;
                    assert(0);
                }

                bool
                intersect_stop_test(T & t, const Cube & cube, const access_mode_t mode) const
                {
                    (void) t;
                    (void) cube;
                    (void) mode;
                    assert(0);
                    return true;
                }

                void
                on_intersect(T & t, const Cube & cube, const access_mode_t mode)
                {
                    (void) t;
                    (void) cube;
                    (void) mode;
                    assert(0);
                }

        }; /* PseudoNode */

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
        KCubeTree() :
            root(nullptr),
            limbs(),
            outdated() {}

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
        cut(Node * node)
        {
            this->limbs.push_back(node);
        }

        inline void
        cut(Node * parent, int k, int dir)
        {
            this->cut(parent->st[k].children[dir]);
            parent->st[k].children[dir] = nullptr;
        }

        inline void
        garbage_collector_run(void)
        {
            for (Node * & node : this->limbs)
                subtree_delete(node);
            this->limbs.clear();
        }

        virtual ~KCubeTree()
        {
            subtree_delete(this->root);
            this->garbage_collector_run();
        }

        ///////////
        // UTILS //
        ///////////
        template <typename N>
        void
        foreach_k_child(
            N * root,
            int k,
            std::function<void(N *)> f
        ) {
            static_assert(std::is_base_of<Node, N>::value);

            for (int i = 0 ; i < 2 ; ++i)
            {
                N * child = reinterpret_cast<N *>(root->st[k].children[i]);
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
            std::function<void(Node *, void *)> f,
            void * args
        ) const {
            f(root, args);
            FOREACH_CHILD_BEGIN(root, child, k, dir)
            {
                foreach_node(child, f, args);
            }
            FOREACH_CHILD_END(root, child, k, dir);
        }

        void
        foreach_node(
            std::function<void(Node *, void *)> f,
            void * args
        ) {
            this->foreach_node(this->root, f, args);
        }

        void
        foreach_node_until(
            Node * root,
            std::function<void(Node *, void *, bool &)> f,
            void * args,
            bool & stop
        ) const {
            f(root, args, stop);
            if (stop) return ;
            FOREACH_CHILD_BEGIN(root, child, k, dir)
            {
                foreach_node_until(child, f, args, stop);
                if (stop) return ;
            }
            FOREACH_CHILD_END(root, child, k, dir);
        }

        void
        foreach_node_until(
            std::function<void(Node *, void *, bool &)> f,
            void * args
        ) {
            bool stop = false;
            this->foreach_node_until(this->root, f, args, stop);
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
        export_pdf(const char * label) const
        {
            char filename[512];

            snprintf(filename, sizeof(filename), "%s-tree.dot", label);
            FILE * file = fopen(filename, "w");
            this->dump(file);
            fclose(file);

            snprintf(filename, sizeof(filename),
                    "dot -Tpdf %s-tree.dot > %s-tree.pdf",
                    label, label
            );
            int r = system(filename);
            if (r)
                fprintf(stderr, "dot failed\n");

            snprintf(filename, sizeof(filename), "%s-cube.tex", label);
            file = fopen(filename, "w");
            this->dump_cube(file);
            fclose(file);

            snprintf(filename, sizeof(filename),
                    "pdflatex -interaction=nonstopmode %s-cube.tex > /dev/null 2>&1",
                    label
            );
            r = system(filename);
            if (r)
                fprintf(stderr, "failed: `%s`\n", filename);
        }

        //////////////////
        //  INTERSECT   //
        //////////////////
        inline void
        intersect_from(
            T & t,
            const Cube & cube,
            const access_mode_t mode,
            Node * node
        ) const {

            if (node == nullptr || !cube.intersects(node->includes.cube))
                return ;

            if (node->intersect_stop_test(t, cube, mode))
                return ;

            if (cube.intersects(node->cube))
                node->on_intersect(t, cube, mode);

            FOREACH_CHILD_BEGIN(node, child, k, dir)
            {
                this->intersect_from(t, cube, mode, child);
            }
            FOREACH_CHILD_END(node, child, k, dir);
        }

        inline void
        intersect(
            T & t,
            const Cube & cube,
            const access_mode_t mode
        ) const {
            this->intersect_from(t, cube, mode, this->root);
        }

        //////////////
        //  INSERT  //
        //////////////

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
            T & t,
            Cube cube,
            const access_mode_t mode,
            Node * parent,
            int k,
            Direction dir,
            const Node * inherit
        ) {
            Node * node;
            if (inherit)
                node = this->new_node(t, cube, k, RED, inherit);
            else
                node = this->new_node(t, cube, k, RED);
            tassert(node);

            parent->st[k].children[dir] = node;
            node->parent = parent;
            node->on_insert(t, mode);
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

# ifdef CUBE_TREE_REBALANCE
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

            PseudoNode pseudo_root;
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

# ifndef CUBE_TREE_DISABLE_COHERENCY_CHECKS
            this->coherency(root->includes.cube);
# endif /* CUBE_TREE_DISABLE_COHERENCY_CHECKS */
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

# endif /* CUBE_TREE_REBALANCE */

        /* called to create a new node with a cube that never appeared before. */
        virtual Node *
        new_node(
            T & t,
            const Cube & cube,
            const int k,
            const Color color
        ) const = 0;

        /* called to create a new node, that intersect with a previously insert
         * node 'inherit' */
        virtual Node *
        new_node(
            T & t,
            const Cube & cube,
            const int k,
            const Color color,
            const Node * inherit
        ) const = 0;

        void
        post_insert(const Cube & cube)
        {
            this->update();

# ifdef CUBE_TREE_REBALANCE
#  pragma message("Automatic rebalance enabled.")
            if (this->requires_rebalance())
                this->rebalance();
# else /* CUBE_TREE_REBALANCE */
#  pragma message("Automatic rebalance disabled. Enable it with '-DCUBE_TREE_REBALANCE'")
# endif /* CUBE_TREE_REBALANCE */

# ifndef CUBE_TREE_DISABLE_COHERENCY_CHECKS
            this->coherency(cube);
# else
            (void) cube;
# endif /* CUBE_TREE_DISABLE_COHERENCY_CHECKS */
        }

# ifdef CUBE_TREE_CUT
        inline void
        insert_from_cut(
            T & t,
            const Cube & cube,
            const access_mode_t mode,
            Node * parent
        ) {
            tassert(mode & ACCESS_MODE_W);

            FOREACH_CHILD_BEGIN(parent, child, k, dir)
            {
                this->cut(parent, k, dir);
            }
            FOREACH_CHILD_END(parent, child, k, dir);

            parent->cube.copy(cube);
            parent->on_insert(t, mode);

            this->outdate(parent);
        }
# endif /* CUBE_TREE_CUT */

        inline void
        insert_from(
            T & t,
            Cube & cube,
            const access_mode_t mode,
            Node * parent,
            int k,
            const Node * inherit
        ) {

            while (k < K)
            {
# ifdef CUBE_TREE_CUT
#  pragma message("Tree cut enable")
                // quick-way out, if the cube includes all subcube with an
                // 'out' access, we can discard all children
                if (mode & ACCESS_MODE_W)
                {
                    // the includes test is accelerated as we know we are
                    // already matching dimensions <k
                    if (cube.includes(parent->includes.cube, k))
                    {
                        // TODO : what if 'node' is not null ?  probably want to
                        // return something to callee for the case (3)
                        this->insert_from_cut(t, cube, mode, parent);
                        break ;
                    }
                }
# else
#  pragma message("Tree cut disabled. Enable it using '-DCUBE_TREE_CUT'")
# endif /* CUBE_TREE_CUT */

                // case (1)    J << I
                if (cube[k].b <= parent->cube[k].a)
                {
                    if (parent->st[k].left == nullptr)
                    {
                        this->insert_fixup(t, cube, mode, parent, k, LEFT, inherit);
                        break ;
                    }
                    else
                        parent = parent->get_child(k, LEFT);
                }
                // case (2)     J >> I
                else if (cube[k].a >= parent->cube[k].b)
                {
                    if (parent->st[k].right == nullptr)
                    {
                        this->insert_fixup(t, cube, mode, parent, k, RIGHT, inherit);
                        break ;
                    }
                    else
                        parent = parent->get_child(k, RIGHT);
                }
                // case (3)     J c I   (or I == J)
                else if (parent->cube[k].a <= cube[k].a && cube[k].b <= parent->cube[k].b)
                {
                    // I == J
                    if (cube[k].a == parent->cube[k].a && cube[k].b == parent->cube[k].b)
                    {
                        if (++k == K)
                        {
                            parent->on_insert(t, mode);
                            this->outdate(parent);
                            break ;
                        }
                    }
                    // J c I
                    else
                    {
                        assert(K == 1 || K == 2);

                        class ReinsertCube {
                            public:
                                Cube cube;
                                Node * inherit;
                            public:
                                ReinsertCube(const Cube & r, const Interval & interval, int k, Node * i) :
                                    cube(r),
                                    inherit(i)
                                {
                                    cube[k] = interval;
                                    inherit = cube.intersects(i->cube) ? i : nullptr;
                                }

                                ~ReinsertCube() {}
                        };

                        std::vector<ReinsertCube> to_reinsert;
                        const Interval intervals[] = {
                            Interval(parent->cube[k].a,         cube[k].a),
                            Interval(        cube[k].a,         cube[k].b),
                            Interval(        cube[k].b, parent->cube[k].b)
                        };

                        std::function<void(Node *)> f = [&f, &intervals, &k, &to_reinsert](Node * node)
                        {
                            assert(node->cube[k].includes(intervals[1]));

                            // reinsert sides
                            if (!intervals[0].is_empty())
                                to_reinsert.push_back(ReinsertCube(node->cube, intervals[0], k, node));
                            if (!intervals[2].is_empty())
                                to_reinsert.push_back(ReinsertCube(node->cube, intervals[2], k, node));

                            // shrink node
                            node->on_shrink(intervals[1], k);
                            node->cube[k] = intervals[1];

                            // shrink all child
                            for (int kk = k+1; kk < K ; ++kk)
                            {
                                FOREACH_K_CHILD_BEGIN(node, child, kk, dir)
                                {
                                    f(child);
                                }
                                FOREACH_K_CHILD_END(node, child, kk, dir);
                            }
                        };

                        f(parent);

                        assert(inherit == nullptr);
                        for (ReinsertCube & rr : to_reinsert)
                            this->insert_from(t, rr.cube, ACCESS_MODE_VOID, this->root, 0, rr.inherit);

                        return this->insert_from(t, cube, mode, this->root, 0, nullptr);

                    } /* I == J ||  J c I */
                }
                // case (4)     I n J != o is (1) + (2) + (3)
                else
                {
                    //  [           I           ]
                    //          [           J             ]
                    //
                    //          or
                    //
                    //  [           J           ]
                    //          [           I             ]
                    const INTERVAL_TYPE_T a = cube[k].a;
                    const INTERVAL_TYPE_T b = cube[k].b;

                    // (1)
                    if (cube[k].a < parent->cube[k].a)
                    {
                        cube[k].b = parent->cube[k].a;
                        this->insert_from(t, cube, mode, this->root, 0, inherit);
                        cube[k].b = b;
                    }

                    // (2)
                    if (parent->cube[k].b < cube[k].b)
                    {
                        cube[k].a = parent->cube[k].b;
                        this->insert_from(t, cube, mode, this->root, 0, inherit);
                        cube[k].a = a;
                    }

                    // (3)
                    {
                        cube[k].a = MAX(a, parent->cube[k].a);
                        cube[k].b = MIN(b, parent->cube[k].b);
                        this->insert_from(t, cube, mode, this->root, 0, inherit);
                        cube[k].a = a;
                        cube[k].b = b;
                    }

                    break ;
                }
            }
        }

        /* TODO: determine where to use a reference on 'cube' */
        inline void
        insert(
            T & t,
            Cube cube,
            const access_mode_t mode
        ) {
            tassert(!cube.is_empty());

            if (this->root == nullptr)
            {
                this->root = this->new_node(t, cube, 0, BLACK);
                this->root->on_insert(t, mode);
                this->root->update_includes();
            }
            else
            {
                this->insert_from(t, cube, mode, this->root, 0, nullptr);
            }

            this->post_insert(cube);
        }

        inline void
        clear(void)
        {
            this->cut(this->root);
            this->garbage_collector_run();
            this->root = nullptr;
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

        // Dump represented cube to the given file
        void
        dump_cube(FILE * f) const
        {
            fprintf(f, "\\documentclass[crop,tikz]{standalone}\n");
            fprintf(f, "\\usetikzlibrary{shapes.multipart}\n");
            fprintf(f, "\\begin{document}\n");
            fprintf(f, "\\begin{tikzpicture}[every text node part/.style={align=center}]\n"  );

            if constexpr (K == 1 || K == 2)
            {
                if (this->root)
                    this->root->dump_cube(f);
            }
            else
            {
                fprintf(f, " Output for K=%d is not supported", K);
            }

            fprintf(f, "  \\end{tikzpicture}\n");
            fprintf(f, "\\end{document}\n");
        }

#ifndef CUBE_TREE_DISABLE_COHERENCY_CHECKS

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
        coherency_cube_includes_check(Node * ref, void * args) const
        {
            Node * root = (Node *) args;
            tassert(root->includes.cube.includes(ref->cube));
        }

        void
        coherency_cube_includes_foreach(Node * node, void * args) const
        {
            (void) args;
            auto f = std::bind(&KCubeTree<K, T>::coherency_cube_includes_check, this, _1, _2);
            foreach_node(node, f, node);
        }

        void
        coherency_cube_includes(Node * root) const
        {
            auto f = std::bind(&KCubeTree<K, T>::coherency_cube_includes_foreach, this, _1, _2);
            foreach_node(root, f, root);
        }

        void
        coherency_cube_disjoint_compare(Node * ref, void * args) const
        {
            Node * node = (Node *) args;
            tassert(node == ref || !node->cube.intersects(ref->cube));
        }

        void
        coherency_cube_disjoint_for(Node * node, void * args) const
        {
            Node * root = (Node *) args;
            auto f = std::bind(&KCubeTree<K, T>::coherency_cube_disjoint_compare, this, _1, _2);
            foreach_node(root, f, node);
        }

        void
        coherency_cube_disjoint(Node * root) const
        {
            auto f = std::bind(&KCubeTree<K, T>::coherency_cube_disjoint_for, this, _1, _2);
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
#ifdef CUBE_TREE_CUT
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
                tassert(k == 0 || node->cube[k-1] == child->cube[k-1]);
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

            /* 5. cube must be disjoint (weak check) */
            coherency_cube_disjoint(root);

            /* 6. check includes cube */
            coherency_cube_includes(root);

            /* 7. includeness relationship between nodes dimension */
            coherency_k(root);

            /* 8. graph must be a tree (only 1 path from root to each node) */
            coherency_single_path(root);

            /* 9. children for a k-tree must have the same k-interval as their parent */
            coherency_k_hierarchy(root);
        }

        void
        coherency_cube_represented_check(Node * node, void * args) const
        {
            const Cube * cube = (const Cube *) args;
            tassert(cube->includes(node->cube) || !cube->intersects(node->cube));
        }

        // TODO : this test is incomplete, should test that all nodes union
        // also forms the cube
        void
        coherency_cube_represented(Cube cube)
        {
            auto f = std::bind(&KCubeTree<K, T>::coherency_cube_represented_check, this, _1, _2);
            foreach_node(this->root, f, &cube);
        }

        int
        coherency(const Cube & cube)
        {
            if (this->root)
            {
                /* 1. The root of the this is always black */
                for (int k = 0 ; k < K ; ++k)
                    tassert(this->root->colors[k] == BLACK);

                /* per-node checks */
                this->coherency_from(this->root);

                /* ensure the cube is represented in the tree */
                this->coherency_cube_represented(cube);

                /* 7. check balance */
                this->coherency_balance();
            }

            return 1;
        }

#endif /* CUBE_TREE_DISABLE_COHERENCY_CHECKS */

};

#endif /* __CUBE_TREE_H__ */
