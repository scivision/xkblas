/* ************************************************************************** */
/*                                                                            */
/*   access.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 16:45:00 by Romain PEREIRA            \_)     (_/    */
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

class access_t
{
    public:
        using Cube = KCube<2>;

    public:

        /* the mode */
        access_mode_t mode;

        /* the concurrency */
        access_concurrency_t concurrency;

        /* the scope */
        access_scope_t scope;

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

        /* The region.
         * Currently always 2 cubes that represents a matrix in an XKTree (1 cube if aligned on ld.s, else 2 cubes) */
        Cube cubes[2];

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
            const ssize_t offset_m,
            const ssize_t offset_n,
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
            successors(8),
            task(task),
            cubes(),
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
            const size_t A = host_view.begin_addr();

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
                    this->cubes[0].set_list(list);
                    assert(!this->cubes[0].is_empty());
                }

                assert( this->cubes[1].is_empty());
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

                    this->cubes[0].set_list(list0);
                    assert(!this->cubes[0].is_empty());
                }

                {
                    Interval list1[2];
                    list1[ACCESS_CUBE_ROW_DIM] = Interval(x2, x3);
                    list1[ACCESS_CUBE_COL_DIM] = Interval(y2, y3);
                    this->cubes[1].set_list(list1);
                    assert(!this->cubes[1].is_empty());
                }
            }

        }

        ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
