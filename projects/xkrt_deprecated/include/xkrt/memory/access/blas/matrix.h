/* ************************************************************************** */
/*                                                                            */
/*   matrix.h                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/17 11:04:10 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:02:27 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __MATRIX_TILE_H__
# define __MATRIX_TILE_H__

# define NUM_OF_TILES(N, TILE_SIZE) (((N)+(TILE_SIZE)-1)/(TILE_SIZE))

# include <assert.h>
# include <stddef.h>
# include <stdint.h>
# include <sys/types.h>
# include <stdlib.h>

typedef enum    matrix_order_t
{
    /****************
     *  0   1   2   *
     *  3   4   5   *
     *  6   7   8   *
     ****************/
    MATRIX_ROWMAJOR, /* C */

    /****************
     *  0   3   6   *
     *  1   4   7   *
     *  2   5   8   *
     ****************/
    MATRIX_COLMAJOR, /* Fortran */

}               matrix_order_t;

typedef struct  matrix_tile_t
{
    /* matrix_order_t */
    matrix_order_t order;

    /* matrix address (passed to the BLAS kernel) */
    uintptr_t addr;

    /* matrix ld */
    size_t ld;

    /* tile size (number of element per row/col) */
    size_t m;   // size row
    size_t n;   // size col

    /* size of type in bytes (eg float == 4, double == 8) */
    size_t sizeof_type;

    /* constructors */
    matrix_tile_t() : matrix_tile_t(MATRIX_COLMAJOR, static_cast<uintptr_t>(0), 0, 0, 0, 0, 0, 0) {}

    matrix_tile_t(
        const matrix_order_t & order,
        const void * & addr,
        const size_t & ld,
        const size_t & offset_m,
        const size_t & offset_n,
        const size_t & m,
        const size_t & n,
        const size_t & sizeof_type
    ) :
        matrix_tile_t(order, (uintptr_t)addr, ld, offset_m, offset_n, m, n, sizeof_type)
    {}

    matrix_tile_t(
        const matrix_order_t & order,
        const uintptr_t & addr,
        const size_t & ld,
        const size_t & offset_m,
        const size_t & offset_n,
        const size_t & m,
        const size_t & n,
        const size_t & sizeof_type
    ) :
        order(order),
        addr(addr),
        ld(ld),
        m(m),
        n(n),
        sizeof_type(sizeof_type)
    {
        assert(this->order == MATRIX_ROWMAJOR || this->order == MATRIX_COLMAJOR);
        this->addr = this->offset_addr(offset_m, offset_n);
    }

    matrix_tile_t(const matrix_tile_t & src) :
        order(src.order),
        addr(src.addr),
        ld(src.ld),
        m(src.m),
        n(src.n),
        sizeof_type(src.sizeof_type)
    {
    }

    ~matrix_tile_t() {}

    /* size of the memory represented */
    inline size_t
    size(void) const
    {
        return (this->m * this->n * this->sizeof_type);
    }

    /* return begin address */
    inline uintptr_t
    begin_addr(void) const
    {
        return this->addr;
    }

    inline uintptr_t
    offset_addr(const size_t offset_m, const size_t offset_n) const
    {
        assert(this->order == MATRIX_ROWMAJOR || this->order == MATRIX_COLMAJOR);
        assert(offset_n >= 0);
        assert(offset_m >= 0);

        switch (this->order)
        {
            case (MATRIX_ROWMAJOR):
                return this->addr + ((size_t)offset_n * this->sizeof_type) +
                                    ((size_t)offset_m * this->sizeof_type * this->ld);

            case (MATRIX_COLMAJOR):
                return this->addr + ((size_t)offset_n * this->sizeof_type * this->ld) +
                                    ((size_t)offset_m * this->sizeof_type);
            default:
                abort();
        }
    }

    /* return end address */
    inline uintptr_t
    end_addr(void) const
    {
        assert(this->order == MATRIX_ROWMAJOR || this->order == MATRIX_COLMAJOR);
        switch (this->order)
        {
            case (MATRIX_ROWMAJOR):
                return this->addr +
                    (this->n * this->sizeof_type) +
                    (this->m * this->sizeof_type * this->ld);

            case (MATRIX_COLMAJOR):
                return this->addr +
                    (this->n * this->sizeof_type * this->ld) +
                    (this->m * this->sizeof_type);

            default:
                abort();
        }
    }

    /* return true if this includes the other tile */
    inline bool
    equals(const matrix_tile_t & x)
    {
        return this->addr == x.addr && this->ld == x.ld && this->sizeof_type == x.sizeof_type && this->m == x.m && this->n == x.n;
    }

    inline bool
    includes(const matrix_tile_t & x)
    {
        (void) x;
        abort();
    }

}               matrix_tile_t;

#endif /* __MATRIX_TILE_H__ */
