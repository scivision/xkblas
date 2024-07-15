#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include "access-mode.h"
# include "intervals.hpp"

template<int K>
struct access_t
{
    access_mode_t mode;
    Intervals<K> intervals;

    access_t() : mode(ACCESS_MODE_VOID), intervals() {}
    access_t(const access_t & access) : mode(access.mode), intervals(access.intervals) {}
    access_t(access_mode_t mode, const Intervals<K> & intervals) : mode(mode), intervals(intervals) {}
    ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
