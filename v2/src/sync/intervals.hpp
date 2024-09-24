#ifndef __INTERVALS_HPP__
# define __INTERVALS_HPP__

# include "matrix-tile.h"
# include "min-max.h"
# include "interval.hpp"

# include <cassert>
# include <cstdlib>
# include <ostream>
# include <iostream>
# include <cstring>

// TODO : ensure loops are enrolled, else use constexpr recursive

/* K is the number of dimensions */
template<int K>
class Intervals {

    public:

        Interval list[K];

    public:

        Intervals() : list() {}

        Intervals(const Interval list[K])
        {
            this->set_list(list);
        }

        Intervals(const Intervals & copy)
        {
            this->set_list(copy.list);
        }

        // TODO : super-dirty stuff, to make xkblas code looks good
        Intervals(const matrix_tile_t & tile)
        {
            if constexpr(K == 2)
            {
                // not sure about this if other ordering
                assert(tile.order == MATRIX_COLMAJOR);

                const uintptr_t PP = tile.begin_addr();
                const int x0 = PP / (tile.ld * tile.sizeof_type);
                const int x1 = x0 + tile.m;
                const int y0 = PP % (tile.ld * tile.sizeof_type);
                const int y1 = y0 + tile.n * tile.sizeof_type;

                this->list[0].a = y0;
                this->list[0].b = y1;
                this->list[1].a = x0;
                this->list[1].b = x1;
            }
            else
            {
                assert(0 && "Not supported");
                // ERROR
            }
        }

        virtual ~Intervals() {}

        void
        copy(const Intervals & other)
        {
            this->set_list(other.list);
        }

        void
        set_list(const Interval src[K])
        {
            // TODO : I'd rather memcpy the whole thing
            # if 0
            memcpy(this->list, src, sizeof(this->list));
            # else
            for (int k = 0 ; k < K ; ++k)
            {
                this->list[k] = src[k];
            }
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
            Interval inter[K];
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
        equals(const Intervals & intervals) const
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
        includes(const Intervals & intervals, int k) const
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
        includes(const Intervals & intervals) const
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
        size(void)
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
            const Intervals & x,
            const Intervals & y,
            int d[K]
        ) {
            for (int k = 0 ; k < K ; ++k)
                d[k] = y.list[k].a - x.list[k].a;
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
