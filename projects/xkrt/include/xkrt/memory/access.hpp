/* ************************************************************************** */
/*                                                                            */
/*   access.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/07 19:33:52 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include <xkrt/memory/access-mode.h>
# include <xkrt/memory/cube.hpp>
# include <xkrt/memory/memory-view.hpp>

# include <vector>

/**
 *  We assume col major, dim 0 is for rows; dim 1 is for cols.
 *  These variables controls how a matrix (A, m, n, ld) is converted to an
 *  hypercube for the xktree.
 *
 *  e.g the matrix (0, 4, 8, 8) can whether be represented as the hypercube
 *      (0:4, 0:8) - if ACCESS_CUBE_ROW_DIM == 0
 *   or (0:8, 0:4) - if ACCESS_CUBE_ROW_DIM == 1
 */
# define ACCESS_CUBE_ROW_DIM 0
# define ACCESS_CUBE_COL_DIM (1 - ACCESS_CUBE_ROW_DIM)

// task and accesses depends to one another, breaking chicken/egg problem here
struct task_t;

template<int K>
static inline void
memory_view_from_cube(
    memory_view_t & view,
    const KCube<K> & cube,
    const size_t ld,
    const size_t sizeof_type
) {
    static_assert(K == 2);

    const INTERVAL_TYPE_T       x = cube[ACCESS_CUBE_ROW_DIM].a;
    const INTERVAL_DIFF_TYPE_T dx = cube[ACCESS_CUBE_ROW_DIM].length();
    const INTERVAL_TYPE_T       y = cube[ACCESS_CUBE_COL_DIM].a;
    const INTERVAL_DIFF_TYPE_T dy = cube[ACCESS_CUBE_COL_DIM].length();
    assert(dx > 0);
    assert(dy > 0);

    view.order         = MATRIX_COLMAJOR;
    view.addr          = x + y * ld * sizeof_type;
    view.ld            = ld;
    view.m             = dx / sizeof_type;
    view.n             = dy;
    view.sizeof_type   = sizeof_type;

    // accesses must be aligned on sizeof(type)
    assert(view.m * sizeof_type == dx);
}

template<int K>
static inline void
memory_view_from_cubes(
    memory_view_t & view,
    const KCube<K> & cube0,
    const KCube<K> & cube1,
    const size_t ld,
    const size_t sizeof_type
) {
    const INTERVAL_DIFF_TYPE_T x0 = (INTERVAL_DIFF_TYPE_T) cube0[ACCESS_CUBE_ROW_DIM].a;
    const INTERVAL_DIFF_TYPE_T xf = (INTERVAL_DIFF_TYPE_T) cube1[ACCESS_CUBE_ROW_DIM].b;
    const INTERVAL_DIFF_TYPE_T y0 = (INTERVAL_DIFF_TYPE_T) cube0[ACCESS_CUBE_COL_DIM].a;
    const INTERVAL_DIFF_TYPE_T yf = (INTERVAL_DIFF_TYPE_T) cube1[ACCESS_CUBE_COL_DIM].b;
    assert(0 <= x0 && x0 <= ld * sizeof_type);
    assert(0 <= xf && xf <= ld * sizeof_type);
    assert(y0 < yf);

    INTERVAL_DIFF_TYPE_T n = yf - y0;
    INTERVAL_DIFF_TYPE_T m = xf - x0;
    if (m < 0)
    {
        m += ld * sizeof_type;
        n -= 1;
    }
    m = m / sizeof_type;

    view.order        = MATRIX_COLMAJOR;
    view.addr         = x0 + y0 * ld * sizeof_type;
    view.ld           = ld;
    view.m            = (size_t) m;
    view.n            = (size_t) n;
    view.sizeof_type  = sizeof_type;
}

template<int K>
static inline void
memory_view_to_cubes(
    const memory_view_t & view,
    KCube<K> (& cubes) [2]
) {
    static_assert(K == 2);

    const size_t  A = view.begin_addr();
    const size_t ld = view.ld;
    const size_t  m = view.m;
    const size_t  n = view.n;
    const size_t  s = view.sizeof_type;

    # if ACCESS_FORCE_ALIGNMENT
    assert((A % (ld * s)) + (m * s) <= ld * s);
    # endif /* ACCESS_FORCE_ALIGNMENT */

    // only 1 cube is needed
    if ((A % (ld * s)) + m * s <= ld * s)
    {
        /**
         *        ^               y0       y1
         *        |         |  .   .   .   .   .
         *        |      x0 |  .   x   x   x   .
         *   ld.s |         |  .   x   x   x   .
         *        |         |  .   x   x   x   .
         *        |      x1 |  .   x   x   x   .
         *        v         v  .   .   .   .   .
         */
        const uintptr_t x0 = A % (ld * s);
        const uintptr_t x1 = x0 + m * s;
        const uintptr_t y0 = A / (ld * s);
        const uintptr_t y1 = y0 + n;

        {
            Interval list[2];
            list[ACCESS_CUBE_ROW_DIM] = Interval(x0, x1);
            list[ACCESS_CUBE_COL_DIM] = Interval(y0, y1);
            cubes[0].set_list(list);
            assert(!cubes[0].is_empty());
        }

        assert(cubes[1].is_empty());
    }
    // 2 cubes are needed
    else
    {
        /**
         *                     y2          y3
         *      x2   |  .   .   x   x   x   x   .
         *      x3   |  .   .   x   x   x   x   .
         *           |  .   .   .   .   .   .   .
         *           |  .   .   .   .   .   .   .
         *      x0   |  .   x   x   x   x   .   .
         *      x1   v  .   x   x   x   x   .   .
         *                 y0          y1
         */
        const uintptr_t x0 = A % (ld * s);
        const uintptr_t x1 = ld * s;
        const uintptr_t x2 = 0;
        const uintptr_t x3 = m*s - (x1 - x0);

        const uintptr_t y0 = A / (ld * s);
        const uintptr_t y1 = y0 + n;
        const uintptr_t y2 = y0 + 1;
        const uintptr_t y3 = y1 + 1;

        {
            Interval list0[2];
            list0[ACCESS_CUBE_ROW_DIM] = Interval(x0, x1);
            list0[ACCESS_CUBE_COL_DIM] = Interval(y0, y1);

            cubes[0].set_list(list0);
            assert(!cubes[0].is_empty());
        }

        {
            Interval list1[2];
            list1[ACCESS_CUBE_ROW_DIM] = Interval(x2, x3);
            list1[ACCESS_CUBE_COL_DIM] = Interval(y2, y3);
            cubes[1].set_list(list1);
            assert(!cubes[1].is_empty());
        }
    }
}

class access_t
{
    public:
        using Cube = KCube<2>;

    public:

        //////////////
        // the mode //
        //////////////

        /* the mode (READ, WRITE) */
        access_mode_t mode;

        /* the concurrency (SEQUENTIAL, COMMUTATIVE, CONCURRENT)*/
        access_concurrency_t concurrency;

        /* the scope (UNIFIED or NONUNIFIED) */
        access_scope_t scope;

        ////////////////
        // the region //
        ////////////////

        /* Currently always 2 cubes that represents a matrix in an XKTree
         * (1 cube if access is aligned on ld x sizeof_type, else 2 cubes) */
        Cube cubes[2];

        ////////////////////////////////////////////////////

        /* As opposed to kaapi/v1, we have no data handle to attach a sync access onto.
         * How to remove that vector and have a similar 'sync access' logic instead ?
         * For now, this implementation will be good enough pre-reserving 8 successors */
        std::vector<access_t *> successors;

        /* The owning task.
         * Instead, we could use a smaller type (uint8_t) with the number of
         * accesses  + the index of that access in the task struct accessses
         * array, allowing to retrieve the original task */
        # define ACCESS_GET_TASK(A) (A->task)
        task_t * task;

        /* host view of the access */
        memory_view_t host_view;

        /* device view of the access - set after fetching the data */
        memory_replicate_view_t device_view;

    public:

        // Welcome traveller \o/ I am glad you found me
        //
        // Here is all the super-dirty pile of shit on which relies the world
        // Hopefully you are only here by curiosity, and not trying to fix something.
        //
        // Well ... you have a wonderful day :-)

        access_t(
            task_t * task,
            const matrix_order_t & order,
            const void * addr,
            const size_t ld,
            const size_t offset_m,
            const size_t offset_n,
            const size_t m,
            const size_t n,
            const size_t s, // sizeof_type,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            cubes(),
            successors(8),
            task(task),
            host_view(order, addr, ld, offset_m, offset_n, m, n, s),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            /* virtual access, no need to compute the region */
            if (mode == ACCESS_MODE_V)
                return ;

            /* Only ACCESS_CONCURRENCY_SEQUENTIAL is supported yet */
            assert(concurrency == ACCESS_CONCURRENCY_SEQUENTIAL);

            /* Only ACCESS_MODE_R|ACCESS_MODE_W supported yet */
            assert(mode == ACCESS_MODE_R || mode == ACCESS_MODE_W || mode == ACCESS_MODE_RW);

            // not sure about what to do if other ordering
            assert(host_view.order == MATRIX_COLMAJOR);

            // creates the two cubes of that memory view
            memory_view_to_cubes(host_view, cubes);
        }

         access_t(
            task_t * task,
            const matrix_order_t & order,
            const void * addr,
            const size_t ld,
            const size_t m,
            const size_t n,
            const size_t s, // sizeof_type,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) : access_t(task, order, addr, ld, 0, 0, m, n, s, mode, concurrency, scope) {}

        access_t(
            task_t * task,
            const matrix_order_t & order,
            const Cube & cube,
            const size_t ld,
            const size_t s,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            mode(mode),
            concurrency(concurrency),
            scope(scope),
            cubes(),
            successors(8),
            task(task),
            host_view(order, 0, ld, 0, 0, 0, 0, s),
            device_view()
        {
            /* clear preallocated empty successors */
            successors.clear();

            assert(order == MATRIX_COLMAJOR);
            assert(mode == ACCESS_MODE_R); // not a big deal, but right now only called from `coherent_async`
            if (!cube.is_empty())
            {
                memory_view_from_cube(this->host_view, cube, ld, s);
                new (this->cubes + 0) Cube(cube);
            }
        }

        access_t(
            task_t * task,
            const access_t * other,
            access_mode_t mode,
            access_concurrency_t concurrency = ACCESS_CONCURRENCY_SEQUENTIAL,
            access_scope_t scope = ACCESS_SCOPE_NONUNIFIED
        ) :
            access_t(
                task,
                other->host_view.order,
                (void *) other->host_view.addr,
                other->host_view.ld,
                other->host_view.m,
                other->host_view.n,
                other->host_view.sizeof_type,
                mode,
                concurrency,
                scope
            )
        {}

        ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
