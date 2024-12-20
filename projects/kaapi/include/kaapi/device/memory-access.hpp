/* ************************************************************************** */
/*                                                                            */
/*   memory-access.hpp                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:33:04 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __MEMORY_ACCESS_HPP__
# define __MEMORY_ACCESS_HPP__

# include <kaapi/device/memory-view.hpp>
# include <kaapi/memory/access.hpp>

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
        {}

        virtual ~KMemoryAccess() {}

}; /* KMemoryAccess */

using Access = KMemoryAccess<2>;

#endif /* __MEMORY_ACCESS_HPP__ */
