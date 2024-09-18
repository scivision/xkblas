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
            const int & LD,
            const int & tm,
            const int & tn,
            const int & bm,
            const int & bn,
            const uint32_t & sizeof_type,
            const access_mode_t & m
        ) :
            KMemoryAccess(memory_view_t(order, addr, LD, tm, tn, bm, bn, sizeof_type), m)
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

}; /* KMemoryAccess */

using Access = KMemoryAccess<2>;

#endif /* __MEMORY_ACCESS_HPP__ */
