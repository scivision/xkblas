#ifndef __MATRIX_TILE_H__
# define __MATRIX_TILE_H__

# define XKBLAS_NUM_OF_TILES(N, TILE_SIZE) (((N)+(TILE_SIZE)-1)/(TILE_SIZE))

# include <stdint.h>

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
    int ld;

    /* tile accessed (in [0..ntiles[) */
    int tm; // row
    int tn; // col

    /* tile size (number of element per row/col) */
    int bs_m;   // row
    int bs_n;   // col

    /* size of type in bytes (eg float == 4, double == 8) */
    int sizeof_type;

    /* constructors */
    matrix_tile_t() : matrix_tile_t(MATRIX_COLMAJOR, static_cast<uintptr_t>(0), 0, 0, 0, 0, 0, 0) {}

    matrix_tile_t(
        const matrix_order_t & order,
        const void * & addr,
        const int & ld,
        const int & tm,
        const int & tn,
        const int & bs_m,
        const int & bs_n,
        const int & sizeof_type
    ) :
        matrix_tile_t(order, (uintptr_t)addr, ld, tm, tn, bs_m, bs_n, sizeof_type)
    {}

    matrix_tile_t(
        const matrix_order_t & order,
        const uintptr_t & addr,
        const int & ld,
        const int & tm,
        const int & tn,
        const int & bs_m,
        const int & bs_n,
        const int & sizeof_type
    ) :
        order(order),
        addr(addr),
        ld(ld),
        tm(tm),
        tn(tn),
        bs_m(bs_m),
        bs_n(bs_n),
        sizeof_type(sizeof_type)
    {}

    matrix_tile_t(const matrix_tile_t & src) :
        order(src.order),
        addr(src.addr),
        ld(src.ld),
        tm(src.tm),
        tn(src.tn),
        bs_m(src.bs_m),
        bs_n(src.bs_n),
        sizeof_type(src.sizeof_type)
    {}

    ~matrix_tile_t() {}

    /* return begin address */
    inline uintptr_t
    begin_addr(void) const
    {
        assert(this->order == MATRIX_ROWMAJOR || this->order == MATRIX_COLMAJOR);

        switch (this->order)
        {
            case (MATRIX_ROWMAJOR):
                return this->addr +
                    (this->tn * this->bs_n * this->sizeof_type) +
                    (this->tm * this->bs_m * this->sizeof_type * this->ld);

            case (MATRIX_COLMAJOR):
                return this->addr +
                    (this->tn * this->bs_n * this->sizeof_type * this->ld) +
                    (this->tm * this->bs_m * this->sizeof_type);
        }
        abort();
    }

    # if 0
    /* size in memory (in bytes) */
    inline size_t
    size(void) const
    {
        return this->bs_n * this->bs_m * this->sizeof_type;
    }
    # endif

}               matrix_tile_t;

#endif /* __MATRIX_TILE_H__ */
