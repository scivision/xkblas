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
        Cube cube;
        access_mode_t mode;

    public:
        access_t() : cube(), mode(ACCESS_MODE_VOID) {}

        access_t(const access_t & access) : access_t(access.mode, access.cube) {}
        access_t(const KCube<K> & r, const access_mode_t m) : cube(r), mode(m) {}

        access_t(
            const matrix_tile_t & t,
            const access_mode_t & m
        ) :
            cube(t),
            mode(m)
        {}

        virtual ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
