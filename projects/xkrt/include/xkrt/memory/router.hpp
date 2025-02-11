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
    ) = 0;

};

# endif /* __ROUTER_HPP__ */
