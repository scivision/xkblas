#ifndef __INTERVAL_HPP__
# define __INTERVAL_HPP__

class Interval {

    public:
        int a, b;

    public:
        Interval() : Interval(0, 0) {}
        Interval(int aa, int bb) : a(aa), b(bb) {}
        virtual ~Interval() {}

        inline bool
        is_empty(void) const
        {
            assert(this->a <= this->b);
            return this->a == this->b;
        }

        inline int
        length(void) const
        {
            return this->b - this->a;
        }

        inline bool
        includes(const Interval & interval) const
        {
            return (this->a <= interval.a && interval.b <= this->b);
        }

        friend bool
        operator==(const Interval & lhs, const Interval & rhs)
        {
            return lhs.a == rhs.a && lhs.b == rhs.b;
        }

        friend bool
        operator!=(const Interval & lhs, const Interval & rhs)
        {
            return lhs.a != rhs.a || lhs.b != rhs.b;
        }

};

#endif /* __INTERVAL_HPP__ */
