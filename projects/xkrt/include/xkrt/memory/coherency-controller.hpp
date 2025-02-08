/* ************************************************************************** */
/*                                                                            */
/*   coherency-controller.hpp                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:06:16 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __COHERENCY_CONTROLLER_HPP__
# define __COHERENCY_CONTROLLER_HPP__

# include <xkrt/consts.h>                   // this should gtfo
# include <xkrt/task/task.hpp>              // this should gtfo
# include <xkrt/driver/memory-access.hpp>   // this should gtfo

template<int K>
class KMemoryCoherencyController {

    using Access = KMemoryAccess<K>;

    public:

        /** all replicates must be invalidated */
        virtual void invalidate(void) = 0;

        /* returns a bitfield of devices that owns the most bytes of the given access */
        virtual xkrt_device_global_id_bitfield_t who_owns(Access * access) = 0;

        /* fetch the given access on the given device */
        virtual void fetch(Task * task, Access * access, xkrt_device_global_id_t device_global_id) = 0;

};

using MemoryCoherencyController = KMemoryCoherencyController<2>;

#endif /* __MEMORY_TREE_HPP__ */
