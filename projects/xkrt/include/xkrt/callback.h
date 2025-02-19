/* ************************************************************************** */
/*                                                                            */
/*   callback.h                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/19 15:19:22 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_CALLBACK_H__
# define __XKRT_CALLBACK_H__

# define XKRT_CALLBACK_ARGS_MAX 5

typedef struct  xkrt_callback_t
{
    void (*func)(const void * [XKRT_CALLBACK_ARGS_MAX]);
    const void * args[XKRT_CALLBACK_ARGS_MAX];
}               xkrt_callback_t;

# define xkrt_callback_raise(c) (c.func(c.args))

# endif /* __XKRT_CALLBACK_H__ */
