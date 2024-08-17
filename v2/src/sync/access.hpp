#ifndef __ACCESS_HPP__
# define __ACCESS_HPP__

# include "access-mode.h"
# include "intervals.hpp"

template<int K>
class access_t
{
    public:
        access_mode_t mode;
        Intervals<K> region;

    public:
        access_t() :
            mode(ACCESS_MODE_VOID),
            region()
        {}

        access_t(const access_t & access) :
            mode(access.mode),
            region(access.region)
        {}

        access_t(const access_mode_t m, const Intervals<K> & r) :
            mode(m),
            region(r)
        {}

        access_t(
            const access_mode_t & m,
            const uintptr_t & P,
            const int & LD,
            const int & tm,   const int & tn,
            const int & bs_m, const int & bs_n
        ) :
            mode(m),
            region(P, LD, tm, tn, bs_m, bs_n)
        {}

        virtual ~access_t() {}
};

#endif /* __ACCESS_HPP__ */
