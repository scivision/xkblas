/* ************************************************************************** */
/*                                                                            */
/*   coherency-controller.hpp                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/21 22:26:12 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __COHERENCY_CONTROLLER_HPP__
# define __COHERENCY_CONTROLLER_HPP__

# include <xkrt/consts.h>
# include <xkrt/memory/access/access.hpp>
# if 0
# include <xkrt/task/task.hpp>          // this should gtfo
# endif

template<int K>
class KMemoryCoherencyController {

    public:

        virtual ~KMemoryCoherencyController() {}

        /* returns a bitfield of devices that owns the most bytes of the given access */
        virtual xkrt_device_global_id_bitfield_t who_owns(access_t * access) = 0;

        /** all replicates must be invalidated */
        virtual void invalidate(void) = 0;

    # if 0
        /* fetch the given access on the given device */
        virtual void fetch(task_t * task, access_t * access, xkrt_device_global_id_t device_global_id) = 0;
    # endif
};

using MemoryCoherencyController = KMemoryCoherencyController<2>;

#endif /* __MEMORY_TREE_HPP__ */
