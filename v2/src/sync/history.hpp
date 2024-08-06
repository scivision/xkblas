#ifndef __HISTORY_HPP__
# define __HISTORY_HPP__

# include "access.hpp"
# include "intervals.hpp"

# include <type_traits>

/**
 *  type 'T' must implement :
 *
 *      class T {
 *          public:
 *              virtual T & operator=(const T & right) = 0;
 *      };
 */

/* abstract history */
template<int K, typename T>
class History {

    public:
        History() {}
        virtual ~History() {}

        /**
         * Insert a new intervals 'intervals' in the history with the access mode
         * 'mode' and attach 'obj' to that intervals.
         */
        virtual void intersect(access_mode_t mode, Intervals<K> & intervals, const T & obj) const = 0;

        /**
         *  Intersect previously inserted intervalss with 'intervals' with respect to
         *  the access 'mode' On each conflict found, the 'on_hazard'
         *  callback is called with (first argument) the previously attached
         *  objects of conflicting intervalss and (second argument) 'obj'
         */
        virtual void insert(access_mode_t mode, Intervals<K> & intervals, const T & obj) = 0;

        /**
         *  Callback when a dependence is detected
         */
        virtual void on_hazard(const Intervals<K> & rx, T const & x, const Intervals<K> & ry, T const & y) const = 0;

        /**
         *  Return the number of intervalss represented by the history
         */
        virtual int size(void) const = 0;

}; /* class History */

#endif /* __HISTORY_HPP__ */
