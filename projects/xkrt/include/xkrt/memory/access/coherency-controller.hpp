/* ************************************************************************** */
/*                                                                            */
/*   coherency-controller.hpp                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/08 00:21:00 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/04 02:23:05 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __COHERENCY_CONTROLLER_HPP__
# define __COHERENCY_CONTROLLER_HPP__

# include <xkrt/consts.h>
# include <xkrt/memory/access/access.hpp>

class MemoryCoherencyController {

    public:

        virtual ~MemoryCoherencyController() {}

        /* returns a bitfield of devices that owns the most bytes of the given access */
        virtual xkrt_device_global_id_bitfield_t who_owns(access_t * access) = 0;

        /** all replicates must be invalidated */
        virtual void invalidate(void) = 0;

        /* fetch the given access on the given device */
        virtual void fetch(access_t * access, xkrt_device_global_id_t device_global_id) = 0;

};

#endif /* __MEMORY_TREE_HPP__ */
