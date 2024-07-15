#ifndef __INTERVAL_HPP__
# define __INTERVAL_HPP__

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

#endif /* __INTERVAL_HPP__ */
