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

        Intervals(const interval_t list[K])
        {
            this->set_list(list);
        }

        Intervals(const Intervals & copy)
        {
            this->set_list(copy.list);
        }

        // TODO : super-dirty stuff, to make xkblas code looks good
        Intervals(const uintptr_t & A, const int & LD, const int & BX, const int & BY)
        {
            if constexpr(K == 2)
            {
                this->list[0].a = (uint64_t)(A % LD);
                this->list[0].b = (uint64_t)(A / LD);
                this->list[1].a = this->list[0].a + BX;
                this->list[1].b = this->list[0].b + BY;
            }
            else
            {
                // # pragma message("Constructor not supported for K != 2")
            }
        }

        virtual ~Intervals() {}

        void
        copy(const Intervals & other)
        {
            this->set_list(other.list);
        }

        void
        set_list(const interval_t list[K])
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

        inline Intervals
        intersection(const Intervals & intervals) const
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
        intersects(const Intervals * & intervals) const
        {
            return this->intersects(*intervals);
        }

        inline bool
        equals(const Intervals & intervals)
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
        includes(const Intervals & intervals) const
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->list[k].a <= intervals.list[k].a && intervals.list[k].b <= this->list[k].b)
                    continue ;
                return false;
            }
            return true;
        }

        inline bool
        is_empty(void) const
        {
            for (int i = 0 ; i < K ; ++i)
                if (this->list[i].a >= this->list[i].b)
                    return true;
            return false;
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

}; /* class Intervals */

#endif /* __INTERVALS_HPP__ */
