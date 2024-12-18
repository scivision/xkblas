/* ************************************************************************** */
/*                                                                            */
/*   ptr.h                                                                    */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/18 16:05:15 by                           \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __PTR_H__
# define __PTR_H__

// TODO : PTR public api

extern "C"
int ptr_init(void);

extern "C"
int ptr_deinit(void);

extern "C"
void ptr_sync(void);

extern "C"
int ptr_get_ngpus(int * count);

#endif /* __PTR_H__ */
