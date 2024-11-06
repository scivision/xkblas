#ifndef __MEMORY_ACCESS_HPP__
# define __MEMORY_ACCESS_HPP__

# include "device/memory-view.hpp"
# include "sync/access.hpp"

/* a memory access */
template<int K>
class KMemoryAccess : public access_t<K>
{
    // static_assert(K == 2);

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
            access_t<K>(t, m),
            host_view(t),
            device_view()
        {
            /* to ensure bijection from memory space to cube space */
            switch (host_view.order)
            {
                case (MATRIX_COLMAJOR):
                {
                    assert((t.addr % (t.ld * t.sizeof_type)) + (t.m * t.sizeof_type) <= t.ld * t.sizeof_type);
                    break ;
                }

                case (MATRIX_ROWMAJOR):
                {
                    assert((t.addr % (t.ld * t.sizeof_type)) + (t.n * t.sizeof_type) <= t.ld * t.sizeof_type);
                    break ;
                }
            }
        }

        virtual ~KMemoryAccess() {}

}; /* KMemoryAccess */

using Access = KMemoryAccess<2>;

#endif /* __MEMORY_ACCESS_HPP__ */
