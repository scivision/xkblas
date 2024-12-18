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

#ifndef __PTR_CALLBACK_H__
# define __PTR_CALLBACK_H__

# define PTR_CALLBACK_ARGS_MAX 5

typedef struct  ptr_callback_t
{
    void (*func)(const void * [PTR_CALLBACK_ARGS_MAX]);
    const void * args[PTR_CALLBACK_ARGS_MAX];
}               ptr_callback_t;

# endif /* __PTR_CALLBACK_H__ */
