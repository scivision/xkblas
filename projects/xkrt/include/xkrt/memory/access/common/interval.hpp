/* ************************************************************************** */
/*                                                                            */
/*   interval.hpp                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/15 17:01:38 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:03:28 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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

};

#endif /* __INTERVAL_HPP__ */
