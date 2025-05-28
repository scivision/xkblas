/* ************************************************************************** */
/*                                                                            */
/*   interval.hpp                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/28 05:22:13 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __INTERVAL_HPP__
# define __INTERVAL_HPP__

# include <assert.h>
# include <stddef.h>

# define INTERVAL_TYPE_T        uintptr_t
# define INTERVAL_DIFF_TYPE_T   ptrdiff_t
# define INTERVAL_TYPE_MODIFIER "%lu"

class Interval {

    public:
        INTERVAL_TYPE_T a, b;

    public:
        Interval() : Interval(0, 0) {}
        Interval(INTERVAL_TYPE_T aa, INTERVAL_TYPE_T bb) : a(aa), b(bb) {}
        virtual ~Interval() {}

        inline bool
        is_empty(void) const
        {
            assert(this->a <= this->b);
            return this->a == this->b;
        }

        inline INTERVAL_DIFF_TYPE_T
        length(void) const
        {
            return (INTERVAL_DIFF_TYPE_T)(this->b - this->a);
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

        inline bool
        intersects(const Interval & other) const
        {
            return this->a <= other.b && other.a <= this->b;
        }

        bool
        operator<(const Interval & other) const
        {
            // this class should only be used to represent disjoint intervals
            assert(!this->intersects(other));
            return this->b <= other.a;
        }

};

#endif /* __INTERVAL_HPP__ */
