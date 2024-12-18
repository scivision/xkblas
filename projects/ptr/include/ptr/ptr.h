/* ************************************************************************** */
/*                                                                            */
/*   ptr.h                                                                    */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/18 15:33:18 by Romain PEREIRA            \_)     (_/    */
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

#endif /* __PTR_H__ */
