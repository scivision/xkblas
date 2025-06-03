/* ************************************************************************** */
/*                                                                            */
/*   callback.h                                                   .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/17 14:41:47 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 17:59:01 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
