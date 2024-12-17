/* ************************************************************************** */
/*                                                                            */
/*   xkblas-callback.h                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKBLAS_CALLBACK_H__
# define __XKBLAS_CALLBACK_H__

# define XKBLAS_CALLBACK_ARGS_MAX 5

typedef struct  xkblas_callback_t
{
    void (*func)(const void * [XKBLAS_CALLBACK_ARGS_MAX]);
    const void * args[XKBLAS_CALLBACK_ARGS_MAX];
}               xkblas_callback_t;

# endif /* __XKBLAS_CALLBACK_H__ */
