#ifndef __HISTORY_HPP__
# define __HISTORY_HPP__

# include "access.hpp"
# include "intervals.hpp"

template<int K, typename T>
class History {

    public:
        History() {}
        virtual ~History() {}

        /**
         * Insert a new intervals 'intervals' in the history with the access mode
         * 'mode' and attach 'obj' to that intervals.
         */
        virtual void intersect(access_mode_t mode, Intervals<K> & intervals, T * obj) const = 0;

        /**
         *  Intersect previously inserted intervalss with 'intervals' with respect to
         *  the access 'mode' On each conflict found, the 'on_hazard'
         *  callback is called with (first argument) the previously attached
         *  objects of conflicting intervalss and (second argument) 'obj'
         */
        virtual void insert(access_mode_t mode, Intervals<K> & intervals, T * obj) = 0;

        /**
         *  Return the number of intervalss represented by the history
         */
        virtual int size(void) const = 0;

        /**
         *  Callback when a dependence is detected
         */
        virtual void on_hazard(const Intervals<K> & rx, T * x, const Intervals<K> & ry, T * y) const = 0;

}; /* class History */

#endif /* __HISTORY_HPP__ */
