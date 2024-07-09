#ifndef __HISTORY_HPP__
# define __HISTORY_HPP__

# include "access.hpp"
# include "region.hpp"

template<int K>
class History {

    public:
        History() {}
        virtual ~History() {}

        /**
         * Insert a new region 'region' in the history with the access mode
         * 'mode' and attach 'obj' to that region.
         */
        virtual void intersect(access_mode_t mode, Region<K> & region, void * obj) const = 0;

        /**
         *  Intersect previously inserted regions with 'region' with respect to
         *  the access 'mode' On each conflict found, the 'on_hazard'
         *  callback is called with (first argument) the previously attached
         *  objects of conflicting regions and (second argument) 'obj'
         */
        virtual void insert(access_mode_t mode, Region<K> & region, void * obj) = 0;

        /**
         *  Return the number of regions represented by the history
         */
        virtual int size(void) const = 0;

        /**
         *  Callback when a dependence is detected
         */
        virtual void on_hazard(const Region<K> & rx, void * x, const Region<K> & ry, void * y) const = 0;

}; /* class History */

#endif /* __HISTORY_HPP__ */
