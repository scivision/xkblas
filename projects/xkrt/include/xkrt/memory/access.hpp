/* ************************************************************************** */
/*                                                                            */
/*   access.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/27 21:43:39 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include <xkrt/memory/access-mode.h>
# include <xkrt/memory/cube.hpp>
# include <xkrt/memory/matrix-tile.h>

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

template<int K>
class access_t
{
    public:
        using Cube = KCube<K>;

    public:

        /* The region: a matrix tile is up to 2 cubes in an XKTree of coordinates */
        Cube cubes[2];  /* have 1 or 2 cubes depending whether the access is aligned on ld.s or not */

        /* the mode */
        access_mode_t mode;

    public:
        access_t() : cubes(), mode(ACCESS_MODE_VOID) {}

        // Welcome traveller \o/ I am glad you found me
        //
        // Here is all the super-dirty pile of shit on which relies all ptr
        // Hopefully you are only here by curiosity, and not trying to fix something.
        //
        // Well ... you have a wonderful day :-)

        access_t(
            const matrix_tile_t & t,
            const access_mode_t & m
        ) :
            cubes(),
            mode(m)
        {
            if constexpr (K == 2)
            {
                /* use notations from the paper */
                const size_t  A = t.begin_addr();
                const size_t  m = t.m;
                const size_t  n = t.n;
                const size_t  s = t.sizeof_type;
                const size_t ld = t.ld;

                // not sure about what to do if other ordering
                assert(t.order == MATRIX_COLMAJOR);

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
                        Interval list[K];
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
                        Interval list0[K];
                        list0[ACCESS_CUBE_ROW_DIM] = Interval(x0, x1);
                        list0[ACCESS_CUBE_COL_DIM] = Interval(y0, y1);

                        this->cubes[0].set_list(list0);
                        assert(!this->cubes[0].is_empty());
                    }

                    {
                        Interval list1[K];
                        list1[ACCESS_CUBE_ROW_DIM] = Interval(x2, x3);
                        list1[ACCESS_CUBE_COL_DIM] = Interval(y2, y3);
                        this->cubes[1].set_list(list1);
                        assert(!this->cubes[1].is_empty());
                    }
                }

            } /* K == 2 */
            else
            {
                assert(0 && "Wtf are you even trying to do");
            }
        }

        virtual ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
