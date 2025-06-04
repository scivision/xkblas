/* ************************************************************************** */
/*                                                                            */
/*   mode.h                                                       .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/15 17:01:38 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/04 16:27:59 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ACCESS_MODE_H__
# define __ACCESS_MODE_H__

typedef enum    access_mode_t
{
    ACCESS_MODE_V       = 0b00000000,   // virtual
    ACCESS_MODE_R       = 0b00000001,   // read
    ACCESS_MODE_W       = 0b00000010,   // write
    ACCESS_MODE_RW      = ACCESS_MODE_R | ACCESS_MODE_W,
    ACCESS_MODE_PIN     = 0b00000100,   // registration   (pin)
    ACCESS_MODE_UNPIN   = 0b00001000,   // unregistration (unpin)
}               access_mode_t;

typedef enum    access_concurrency_t
{
    ACCESS_CONCURRENCY_SEQUENTIAL  = 0b00000001,
    ACCESS_CONCURRENCY_COMMUTATIVE = 0b00000010,
    ACCESS_CONCURRENCY_CONCURRENT  = 0b00000100,
}               access_concurrency_t;

typedef enum    access_scope_t
{
    ACCESS_SCOPE_NONUNIFIED = 0,
    ACCESS_SCOPE_UNIFIED    = 1
}               access_scope_t;

// To OpenMP dependencies
//
// omp modifier | (access_mode_t, access_concurrency_t)
//
//        in      = (ACCESS_MODE_R, _, )
//  out|inout     = (ACCESS_MODE_W, ACCESS_CONCURRENCY_SEQUENTIAL)
//  mutexinoutset = (ACCESS_MODE_W, ACCESS_CONCURRENCY_COMMUTATIVE)
//       inoutset = (ACCESS_MODE_W, ACCESS_CONCURRENCY_CONCURRENT)
//
// scope is sort of always 'ACCESS_SCOPE_UNIFIED' as we cannot specify
// dependency domains in 6.0

static inline const char *
access_mode_to_str(access_mode_t mode)
{
    switch (mode)
    {
        case (ACCESS_MODE_V):     return "ACCESS_MODE_V";
        case (ACCESS_MODE_R):     return "ACCESS_MODE_R";
        case (ACCESS_MODE_W):     return "ACCESS_MODE_W";
        case (ACCESS_MODE_RW):    return "ACCESS_MODE_RW";
        case (ACCESS_MODE_PIN):   return "ACCESS_MODE_PIN";
        case (ACCESS_MODE_UNPIN): return "ACCESS_MODE_UNPIN";
        default:                  return "ACCESS_MODE_UNKN";
    }
}

static inline const char *
access_concurrency_to_str(access_concurrency_t concurrency)
{
    switch (concurrency)
    {
        case (ACCESS_CONCURRENCY_SEQUENTIAL):   return "ACCESS_CONCURRENCY_SEQUENTIAL";
        case (ACCESS_CONCURRENCY_COMMUTATIVE):  return "ACCESS_CONCURRENCY_COMMUTATIVE";
        case (ACCESS_CONCURRENCY_CONCURRENT):   return "ACCESS_CONCURRENCY_CONCURRENT";
        default:                                return "ACCESS_CONCURRENCY_UNKN";
    }
}

static inline const char *
access_scope_to_str(access_scope_t scope)
{
    switch (scope)
    {
        case (ACCESS_SCOPE_NONUNIFIED): return "ACCESS_SCOPE_NONUNIFIED";
        case (ACCESS_SCOPE_UNIFIED):    return "ACCESS_SCOPE_UNIFIED";
        default:                        return "ACCESS_SCOPE_UNKN";
    }
}
#endif /* __ACCESS_MODE_H__ */
