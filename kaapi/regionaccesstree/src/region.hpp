#ifndef __REGION_HPP__
# define __REGION_HPP__

# include <cstdlib>
# include <ostream>
# include <iostream>

# ifndef MIN
#  define MIN(X, Y) ((Y) < (X) ? (Y) : (X))
# endif /* MIN */

# ifndef MAX
#  define MAX(X, Y) ((X) < (Y) ? (Y) : (X))
# endif /* MAX */

typedef struct  interval_s
{
    int a, b;

    friend bool
    operator==(const struct interval_s & lhs, const struct interval_s & rhs)
    {
        return lhs.a == rhs.a && lhs.b == rhs.b;
    }

    friend bool
    operator!=(const struct interval_s & lhs, const struct interval_s & rhs)
    {
        return lhs.a != rhs.a || lhs.b != rhs.b;
    }

}               interval_t;

// TODO : ensure loops are enrolled, else use constexpr recursive

/* K is the number of dimensions */
template<int K>
class Region {

    public:
        interval_t intervals[K];

        Region()
        {
            memset(this->intervals, 0, sizeof(this->intervals));
        }

        Region(interval_t pintervals[K])
        {
            this->set_intervals(pintervals);
        }

        Region(Region & copy)
        {
            this->set_intervals(copy.intervals);
        }

        virtual ~Region() {}

        void
        copy(const Region & other)
        {
            this->set_intervals(other.intervals);
        }

        void
        set_intervals(const interval_t pintervals[K])
        {
            memcpy(this->intervals, pintervals, sizeof(this->intervals));
        }

        interval_t   operator [](int i) const { return intervals[i]; }
        interval_t & operator [](int i) { return intervals[i]; }

        void
        tostring(char * buffer, int size) const
        {
            int r = 0;
            for (int k = 0 ; k < K ; ++k)
            {
                r += snprintf(buffer + r, size - r, "[%d..%d[%c",
                        this->intervals[k].a, this->intervals[k].b,
                        (k == K - 1) ? '\0' : '\n');
                if (r == size)
                    break ;
            }
        }

        inline bool
        empty(void)
        {
            for (int i = 0 ; i < K ; ++i)
                if (intervals[i].a >= intervals[i].b)
                    return true;
            return false;
        }

        // return true if intervals intersects on each dimension
        inline bool
        intersects(const Region & region) const
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->intervals[k].a < region.intervals[k].b && this->intervals[k].b > region.intervals[k].a)
                    continue ;
                return false;
            }
            return true;
        }

        inline bool
        intersects(Region * & region)
        {
            return this->intersects(*region);
        }

        inline Region
        intersection(Region & region)
        {
            interval_t inter[K];
            for (int k = 0 ; k < K ; ++k)
            {
                inter[k].a = MAX(this->intervals[k].a, region.intervals[k].a);
                inter[k].b = MIN(this->intervals[k].b, region.intervals[k].b);
            }
            return Region(inter);
        }

        inline bool
        includes(Region & region)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (this->intervals[k].a <= region.intervals[k].a && region.intervals[k].b <= this->intervals[k].b)
                    continue ;
                return false;
            }
            return true;
        }

        friend std::ostream &
        operator<<(std::ostream & os, const Region & region)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                os << "(" << region[k].a << "," << region[k].b << ")";
                if (k != K-1)
                    os << "x";
            }
            return os;
        }

        friend bool
        operator==(const Region & lhs, const Region & rhs)
        {
            for (int k = 0 ; k < K ; ++k)
            {
                if (lhs->intervals[k] == rhs.intervals[k])
                    continue ;
                return false;
            }
            return true;
        }
}; /* class Region */

#endif /* __REGION_HPP__ */
