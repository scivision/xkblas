#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include "access-mode.h"
# include "intervals.hpp"

template<int K>
struct access_t
{
    access_mode_t mode;
    Intervals<K> region;

    access_t() : mode(ACCESS_MODE_VOID), region() {}
    access_t(const access_t & access) : mode(access.mode), region(access.region) {}
    access_t(const access_mode_t mode, const Intervals<K> & region) : mode(mode), region(region) {}
    ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
