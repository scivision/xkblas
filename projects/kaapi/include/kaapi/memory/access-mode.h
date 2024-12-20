/* ************************************************************************** */
/*                                                                            */
/*   access-mode.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ACCESS_MODE_H__
# define __ACCESS_MODE_H__

typedef enum    access_mode_t
{
    ACCESS_MODE_VOID    = 0b00000000,
    ACCESS_MODE_R       = 0b00000001,
    ACCESS_MODE_W       = 0b00000010,
    ACCESS_MODE_RW      = ACCESS_MODE_R | ACCESS_MODE_W,
}               access_mode_t;

static inline const char *
access_mode_to_str(access_mode_t mode)
{
    switch (mode)
    {
        case (ACCESS_MODE_R):
            return "ACCESS_MODE_R";
        case (ACCESS_MODE_W):
            return "ACCESS_MODE_W";
        case (ACCESS_MODE_RW):
            return "ACCESS_MODE_RW";
        default:
            return "unkn";
    }
}

#endif /* __ACCESS_MODE_H__ */
