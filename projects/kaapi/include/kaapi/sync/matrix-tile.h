/* ************************************************************************** */
/*                                                                            */
/*   matrix-tile.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:12:04 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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

    /* beginning of the tile accessed */
    ssize_t offset_m; // offset begin row
    ssize_t offset_n; // offset begin col

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
        const ssize_t & offset_m,
        const ssize_t & offset_n,
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
        const ssize_t & offset_m,
        const ssize_t & offset_n,
        const size_t & m,
        const size_t & n,
        const size_t & sizeof_type
    ) :
        order(order),
        addr(addr),
        ld(ld),
        offset_m(offset_m),
        offset_n(offset_n),
        m(m),
        n(n),
        sizeof_type(sizeof_type)
    {}

    matrix_tile_t(const matrix_tile_t & src) :
        order(src.order),
        addr(src.addr),
        ld(src.ld),
        offset_m(src.offset_m),
        offset_n(src.offset_n),
        m(src.m),
        n(src.n),
        sizeof_type(src.sizeof_type)
    {}

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
        assert(this->order == MATRIX_ROWMAJOR || this->order == MATRIX_COLMAJOR);
        assert(this->offset_n >= 0);
        assert(this->offset_m >= 0);

        switch (this->order)
        {
            case (MATRIX_ROWMAJOR):
                return this->addr +
                    ((size_t)this->offset_n * this->sizeof_type) +
                    ((size_t)this->offset_m * this->sizeof_type * this->ld);

            case (MATRIX_COLMAJOR):
                return this->addr +
                    ((size_t)this->offset_n * this->sizeof_type * this->ld) +
                    ((size_t)this->offset_m * this->sizeof_type);
        }
        abort();
    }

    /* return end address */
    inline uintptr_t
    end_addr(void) const
    {
        assert(this->order == MATRIX_ROWMAJOR || this->order == MATRIX_COLMAJOR);
        assert(this->offset_n >= 0);
        assert(this->offset_m >= 0);

        switch (this->order)
        {
            case (MATRIX_ROWMAJOR):
                return this->addr +
                    (((size_t)this->offset_n + this->n) * this->sizeof_type) +
                    (((size_t)this->offset_m + this->m) * this->sizeof_type * this->ld);

            case (MATRIX_COLMAJOR):
                return this->addr +
                    (((size_t)this->offset_n + this->n) * this->sizeof_type * this->ld) +
                    (((size_t)this->offset_m + this->m) * this->sizeof_type);
        }
        abort();
    }

}               matrix_tile_t;

#endif /* __MATRIX_TILE_H__ */
