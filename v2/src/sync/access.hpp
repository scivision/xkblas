#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include "access-mode.h"
# include "cube.hpp"
# include "matrix-tile.h"

template<int K>
class access_t
{
    public:
        using Cube = KCube<K>;

    public:

        /* The region: a matrix tile is up to 2 cubes in an XKTree of coordinates */
        Cube cubes[2];  /* have 1 or 2 cubes depending whether the access is aligned on LD.s or not */

        /* the mode */
        access_mode_t mode;

    public:
        access_t() : cubes(), mode(ACCESS_MODE_VOID) {}
        access_t(const access_t & access) : access_t(access.cube, access.mode) {}

        // Welcome traveller \o/ I am glad you found me
        //
        // Here is all the super-dirty pile of shit on which relies all xkblas/v2.
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
                const size_t LD = t.ld;

                # if XKBLAS_ACCESS_FORCE_ALIGNMENT
                assert((A % (LD * s)) + (m * s) <= LD * s);
                # endif /* XKBLAS_ACCESS_FORCE_ALIGNMENT */

                // not sure about what to do if other ordering
                assert(t.order == MATRIX_COLMAJOR);

                // only 1 cube is needed
                if ((A % (LD * s)) + m * s <= LD * s)
                {
                    /**
                     *        ^               y0       y1
                     *        |         |  .   .   .   .   .
                     *        |      x0 |  .   x   x   x   .
                     *   LD.s |         |  .   x   x   x   .
                     *        |         |  .   x   x   x   .
                     *        |      x1 |  .   x   x   x   .
                     *        v         v  .   .   .   .   .
                     */
                    const uintptr_t x0 = A % (LD * s);
                    const uintptr_t x1 = x0 + m * s;
                    const uintptr_t y0 = A / (LD * s);
                    const uintptr_t y1 = y0 + n;

                    const Interval list[K] = {
                        Interval(x0, x1),
                        Interval(y0, y1)
                    };

                    this->cubes[0].set_list(list);
                    assert(!this->cubes[0].is_empty());

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
                    const uintptr_t x0 = A % (LD * s);
                    const uintptr_t x1 = LD * s;
                    const uintptr_t x2 = 0;
                    const uintptr_t x3 = m*s - (x1 - x0);

                    const uintptr_t y0 = A / (LD * s);
                    const uintptr_t y1 = y0 + n;
                    const uintptr_t y2 = y0 + 1;
                    const uintptr_t y3 = y1 + 1;

                    {
                        const Interval list0[K] = { Interval(x0, x1), Interval(y0, y1) };
                        this->cubes[0].set_list(list0);
                        assert(!this->cubes[0].is_empty());
                    }

                    {
                        const Interval list1[K] = { Interval(x2, x3), Interval(y2, y3) };
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
