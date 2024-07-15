#ifndef __INTERVALS_HPP__
# define __INTERVALS_HPP__

# include "min-max.h"
# include "interval.hpp"

# include <cstdlib>
# include <ostream>
# include <iostream>
# include <cstring>

// TODO : ensure loops are enrolled, else use constexpr recursive

/* K is the number of dimensions */
template<int K>
class Intervals {

    public:
        interval_t list[K];

        Intervals()
        {
            memset(this->list, 0, sizeof(this->list));
        }

        Intervals(interval_t list[K])
        {
            this->set_intervals(list);
        }

        Intervals(const Intervals & copy)
        {
            this->set_intervals(copy.list);
        }

        virtual ~Intervals() {}

        void
        copy(const Intervals & other)
        {
            this->set_intervals(other.list);
        }

        void
        set_intervals(const interval_t list[K])
        {
            memcpy(this->list, list, sizeof(this->list));
        }

        interval_t   operator [](int i) const { return this->list[i]; }
        interval_t & operator [](int i) { return this->list[i]; }

        void
        tostring(char * buffer, int size) const
        {
            int r = 0;
            for (int k = 0 ; k < K ; ++k)
            {
                r += snprintf(buffer + r, size - r, "[%d..%d[%c",
                        this->list[k].a, this->list[k].b,
                        (k == K - 1) ? '\0' : '\n');
                if (r == size)
                    break ;
            }
        }

        inline bool
        empty(void) const
        {
            for (int i = 0 ; i < K ; ++i)
                if (this->list[i].a >= this->list[i].b)
                    return true;
            return false;
        }

        // return true if intervals intersects on each dimension
        inline bool
        intersects(const Intervals & intervals) const
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->list[k].a < intervals.list[k].b && this->list[k].b > intervals.list[k].a)
                    continue ;
                return false;
            }
            return true;
        }

        inline bool
        intersects(Intervals * & intervals) const
        {
            return this->intersects(*intervals);
        }

        inline Intervals
        intersection(Intervals & intervals) const
        {
            interval_t inter[K];
            for (int k = 0 ; k < K ; ++k)
            {
                inter[k].a = MAX(this->list[k].a, intervals.list[k].a);
                inter[k].b = MIN(this->list[k].b, intervals.list[k].b);
            }
            return Intervals(inter);
        }

        inline bool
        includes(Intervals & intervals) const
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->list[k].a <= intervals.list[k].a && intervals.list[k].b <= this->list[k].b)
                    continue ;
                return false;
            }
            return true;
        }

        friend std::ostream &
        operator<<(std::ostream & os, const Intervals & intervals)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                os << "(" << intervals[k].a << "," << intervals[k].b << ")";
                if (k != K-1)
                    os << "x";
            }
            return os;
        }

        friend bool
        operator==(const Intervals & lhs, const Intervals & rhs)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (lhs->list[k] == rhs.list[k])
                    continue ;
                return false;
            }
            return true;
        }
}; /* class Intervals */

#endif /* __INTERVALS_HPP__ */
