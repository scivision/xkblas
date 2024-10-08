#ifndef __MEMORY_ACCESS_HPP__
# define __MEMORY_ACCESS_HPP__

# include "device/memory-view.hpp"
# include "sync/access.hpp"

/* a memory access */
template<int K>
class KMemoryAccess : public access_t<K>
{
    static_assert(K == 2);

    public:

        /* host view of the access */
        memory_view_t host_view;

        /* device view of the access - set after fetching the data */
        memory_replicate_view_t device_view;

    public:

        KMemoryAccess() {}

        KMemoryAccess(
            const matrix_order_t & order,
            const void * addr,
            const size_t & ld,
            const ssize_t & offset_m,
            const ssize_t & offset_n,
            const size_t & m,
            const size_t & n,
            const size_t & sizeof_type,
            const access_mode_t & mode
        ) :
            KMemoryAccess(memory_view_t(order, addr, ld, offset_m, offset_n, m, n, sizeof_type), mode)
        {}

        KMemoryAccess(
            const memory_view_t & t,
            const access_mode_t & m
        ) :
            host_view(t),
            device_view(),
            access_t<K>(t, m)
        {}

        virtual ~KMemoryAccess() {}

        /* shrink 'this' to its intersection with 'other'
         * Behavior is undefined if
         *  - 'this' and 'other' does not intersect
         *  - 'device_view' are used after shrinking
         */
        void
        shrink(const KMemoryAccess & othr)
        {
            // must intersect
            assert(
                (this->host_view.begin_addr() <= othr.host_view.begin_addr() && othr.host_view.begin_addr() <= this->host_view.end_addr()) ||
                (othr->host_view.begin_addr() <= this.host_view.begin_addr() && this.host_view.begin_addr() <= othr->host_view.end_addr())
            );

            // device_view is invalid now
            assert(memset(&this->device_view, 0, sizeof(memory_replicate_view_t)));

            // shrink

        }

}; /* KMemoryAccess */

using Access = KMemoryAccess<2>;

#endif /* __MEMORY_ACCESS_HPP__ */
