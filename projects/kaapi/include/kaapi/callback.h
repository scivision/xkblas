/* ************************************************************************** */
/*                                                                            */
/*   callback.h                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __KAAPI_CALLBACK_H__
# define __KAAPI_CALLBACK_H__

# define KAAPI_CALLBACK_ARGS_MAX 5

typedef struct  kaapi_callback_t
{
    void (*func)(const void * [KAAPI_CALLBACK_ARGS_MAX]);
    const void * args[KAAPI_CALLBACK_ARGS_MAX];
}               kaapi_callback_t;

# endif /* __KAAPI_CALLBACK_H__ */
