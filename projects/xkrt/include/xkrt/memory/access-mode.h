/* ************************************************************************** */
/*                                                                            */
/*   access-mode.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 16:42:21 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ACCESS_MODE_H__
# define __ACCESS_MODE_H__

typedef enum    access_mode_t
{
    ACCESS_MODE_V       = 0b00000000,
    ACCESS_MODE_R       = 0b00000001,
    ACCESS_MODE_W       = 0b00000010,
    ACCESS_MODE_RW      = ACCESS_MODE_R | ACCESS_MODE_W,
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
//        in      = (ACCESS_MODE_R,  _, )
//  out|inout     = (ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL)
//  mutexinoutset = (ACCESS_MODE_RW, ACCESS_CONCURRENCY_COMMUTATIVE)
//       inoutset = (ACCESS_MODE_RW, ACCESS_CONCURRENCY_CONCURRENT)
//
// scope is sort of always 'ACCESS_SCOPE_UNIFIED' as we cannot specify
// dependency domains in ^.0

static inline const char *
access_mode_to_str(access_mode_t mode)
{
    switch (mode)
    {
        case (ACCESS_MODE_V):   return "ACCESS_MODE_V";
        case (ACCESS_MODE_R):   return "ACCESS_MODE_R";
        case (ACCESS_MODE_W):   return "ACCESS_MODE_W";
        case (ACCESS_MODE_RW):  return "ACCESS_MODE_RW";
        default:                return "ACCESS_MODE_UNKN";
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
