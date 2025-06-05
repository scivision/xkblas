/* ************************************************************************** */
/*                                                                            */
/*   stream-type.h                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/17 14:41:47 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:00:53 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STREAM_TYPE_HPP__
# define __STREAM_TYPE_HPP__

/* DONT CHANGE ORDER HERE !! Can have side effects (in the Offloader class for instance) */
typedef enum    xkrt_stream_type_t
{
    XKRT_STREAM_TYPE_H2D  = 0, /* from CPU to GPU */
    XKRT_STREAM_TYPE_D2H  = 1, /* from GPU to CPU */
    XKRT_STREAM_TYPE_D2D  = 2, /* from GPU to GPU */
    XKRT_STREAM_TYPE_KERN = 3,
    XKRT_STREAM_TYPE_ALL       /* internal purpose */

}               xkrt_stream_type_t;

const char * xkrt_stream_type_to_str(xkrt_stream_type_t type);

# endif /* __STREAM_TYPE_HPP__ */
