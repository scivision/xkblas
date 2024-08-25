#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include "access-mode.h"
# include "intervals.hpp"
# include "matrix-tile.h"

template<int K>
class access_t
{
    using Region = Intervals<K>;

    public:
        Region region;
        access_mode_t mode;

    public:
        access_t() :
            region(),
            mode(ACCESS_MODE_VOID)
        {}

        access_t(const access_t & access) :
            region(access.region),
            mode(access.mode)
        {}

        access_t(const access_mode_t m, const Intervals<K> & r) :
            region(r),
            mode(m)
        {}

        access_t(
            const matrix_tile_t & t,
            const access_mode_t & m
        ) :
            region(t),
            mode(m)
        {}

        virtual ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
