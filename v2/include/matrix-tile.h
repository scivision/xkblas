#ifndef __MATRIX_TILE_H__
# define __MATRIX_TILE_H__

# define XKBLAS_NUM_OF_TILES(N, TILE_SIZE) (((N)+(TILE_SIZE)-1)/(TILE_SIZE))

# include <stdint.h>

typedef struct  matrix_tile_t
{
    /* matrix address (passed to the BLAS kernel) */
    uintptr_t addr;

    /* matrix LD */
    int LD;

    /* tile accessed (in [0..ntiles[) */
    int tm;
    int tn;

    /* tile size (number of element per row/col) */
    int bs_m;
    int bs_n;

    /* size of type in bytes (eg float == 4, double == 8) */
    int sizeof_type;

    /* constructors */
    matrix_tile_t() : matrix_tile_t(static_cast<uintptr_t>(0), 0, 0, 0, 0, 0, 0) {}

    matrix_tile_t(
        const void * & addr,
        const int & LD,
        const int & tm,
        const int & tn,
        const int & bs_m,
        const int & bs_n,
        const int & sizeof_type
    ) :
        matrix_tile_t((uintptr_t)addr, LD, tm, tn, bs_m, bs_n, sizeof_type)
    {}

    matrix_tile_t(
        const uintptr_t & addr,
        const int & LD,
        const int & tm,
        const int & tn,
        const int & bs_m,
        const int & bs_n,
        const int & sizeof_type
    ) :
        addr(addr),
        LD(LD),
        tm(tm),
        tn(tn),
        bs_m(bs_m),
        bs_n(bs_n),
        sizeof_type(sizeof_type)
    {}

    matrix_tile_t(const matrix_tile_t & src) :
        addr(src.addr),
        LD(src.LD),
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
        return this->addr +
            (this->tn * this->bs_n * this->sizeof_type) +
            (this->tm * this->bs_m * this->sizeof_type * this->LD);
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
