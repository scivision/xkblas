/* ************************************************************************** */
/*                                                                            */
/*   router.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/07 16:13:33 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/07 16:29:17 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# ifndef __ROUTER_HPP__
#  define __ROUTER_HPP__

# include <xkrt/consts.h>

class Router
{
    /**
     *  Retrieve the source to use for a data transfer to 'dst' where the
     *  valid sources are amongst the 'valid' bitfield
     */
    virtual xkrt_device_global_id_t
    get_source(
        const xkrt_device_global_id_t dst,
        const xkrt_device_global_id_bitfield_t valid
    ) const = 0;

};

# endif /* __ROUTER_HPP__ */
