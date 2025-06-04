/* ************************************************************************** */
/*                                                                            */
/*   hyperrect.hpp                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/15 17:01:38 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/04 02:13:01 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __HYPERCUBE_HPP__
# define __HYPERCUBE_HPP__

# include <xkrt/utils/min-max.h>
# include <xkrt/memory/access/common/interval.hpp>

# include <cassert>
# include <cstdlib>
# include <ostream>
# include <iostream>
# include <cstring>

/* K is the number of dimensions */
template<int K>
class KHyperrect {

    public:

        Interval list[K];

    public:

        KHyperrect() : list() {}

        KHyperrect(const Interval & interval) requires (K == 1)
        {
            assert(K == 1);
            this->set_list(&interval);
        }

        KHyperrect(const Interval list[K])
        {
            this->set_list(list);
        }

        KHyperrect(const KHyperrect & copy)
        {
            this->set_list(copy.list);
        }

        virtual ~KHyperrect() {}

        void
        copy(const KHyperrect & other)
        {
            this->set_list(other.list);
        }

        void
        set_list(const Interval src[K])
        {
            // TODO : I'd rather memcpy the whole thing, but not standard c++
            # if 0
            memcpy(this->list, src, sizeof(this->list));
            # else
            for (int k = 0 ; k < K ; ++k)
                this->list[k] = src[k];
            # endif
        }

        Interval   operator [](int i) const { return this->list[i]; }
        Interval & operator [](int i) { return this->list[i]; }

        void
        tostring(char * buffer, int size) const
        {
            int r = 0;
            for (int k = 0 ; k < K ; ++k)
            {
                r += snprintf(buffer + r, size - r, "[" INTERVAL_TYPE_MODIFIER ".." INTERVAL_TYPE_MODIFIER "[%c",
                        this->list[k].a, this->list[k].b,
                        (k == K - 1) ? '\0' : '\n');
                if (r == size)
                    break ;
            }
        }

        // return true if intervals intersects on each dimension
        inline bool
        intersects(const KHyperrect & intervals) const
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->list[k].a < intervals.list[k].b && this->list[k].b > intervals.list[k].a)
                    continue ;
                return false;
            }
            return true;
        }

        static inline void
        intersection(
            KHyperrect * dst,
            const KHyperrect & x,
            const KHyperrect & y
        ) {
            for (int k = 0 ; k < K ; ++k)
            {
                dst->list[k].a = MAX(x.list[k].a, y.list[k].a);
                dst->list[k].b = MIN(x.list[k].b, y.list[k].b);
            }
        }

        inline bool
        intersects(const KHyperrect * & intervals) const
        {
            return this->intersects(*intervals);
        }

        inline bool
        equals(const KHyperrect & intervals) const
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->list[k] == intervals.list[k])
                    continue ;
                return false;
            }
            return true;
        }

        inline bool
        includes(const KHyperrect & intervals, int k) const
        {
            for ( ; k < K ; ++k)
            {
                if (this->list[k].includes(intervals.list[k]))
                    continue ;
                return false;
            }
            return true;
        }

        inline bool
        includes(const KHyperrect & intervals) const
        {
            return this->includes(intervals, 0);
        }

        inline bool
        is_empty(void) const
        {
            for (int i = 0 ; i < K ; ++i)
                if (this->list[i].a >= this->list[i].b)
                    return true;
            return false;
        }

        inline uint64_t
        size(void) const
        {
            uint64_t s = this->list[0].length();
            for (int i = 1 ; i < K ; ++i)
                s *= this->list[i].length();
            return s;
        }

        /* return the distance | y - x | between 'left-top' corners of each
         * dimensions of the interval */
        static inline void
        distance_manhattan(
            const KHyperrect & x,
            const KHyperrect & y,
            INTERVAL_DIFF_TYPE_T d[K]
        ) {
            for (int k = 0 ; k < K ; ++k)
                d[k] = (INTERVAL_DIFF_TYPE_T) (y.list[k].a - x.list[k].a);
        }

        friend std::ostream &
        operator<<(std::ostream & os, const KHyperrect & intervals)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                os << "(" << intervals[k].a << "," << intervals[k].b << ")";
                if (k != K-1)
                    os << "x";
            }
            return os;
        }

}; /* class KHyperrect */

#endif /* __HYPERCUBE_HPP__ */
