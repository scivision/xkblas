/* ************************************************************************** */
/*                                                                            */
/*   dummy-coherency-controller.hpp                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/05/19 00:09:44 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:03:46 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DUMMY_COHERENCY_CONTROLLER_HPP__
# define __DUMMY_COHERENCY_CONTROLLER_HPP__

# include <xkrt/consts.h>
# include <xkrt/logger/logger.h>
# include <xkrt/memory/access/access.hpp>

class DummyMemoryCoherencyController {

    public:

        /* returns a bitfield of devices that owns the most bytes of the given access */
        xkrt_device_global_id_bitfield_t
        who_owns(access_t * access)
        {
            LOGGER_FATAL("Tried to run coherency controller on an unsupported access");
        }

        /** all replicates must be invalidated */
        void
        invalidate(void)
        {
            LOGGER_FATAL("Tried to run coherency controller on an unsupported access");
        }

        /* fetch the given access on the given device */
        void
        fetch(
            access_t * access,
            xkrt_device_global_id_t device_global_id
        ) {
            LOGGER_FATAL("Tried to run coherency controller on an unsupported access");
        }

        /* return true if that memory coherency controller can resolve that access */
        bool
        can_resolve(const access_t * access) const
        {
            LOGGER_FATAL("Tried to run coherency controller on an unsupported access");
        }
};

#endif /* __DUMMY_COHERENCY_CONTROLLER_HPP__ */
