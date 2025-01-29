/* ************************************************************************** */
/*                                                                            */
/*   xkrt.h                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:50:13 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_H__
# define __XKRT_H__

// TODO : xkrt public API

extern "C" {

    int xkrt_init(void);
    int xkrt_deinit(void);
    int xkrt_sync(void);

    void xkrt_memory_invalidate(void);
    void xkrt_memory_coherent_async(int uplo, int memflag, int m, int n, void * addr, int ld, unsigned int sizeof_type);
    int xkrt_get_ngpus(int * count);
};

#endif /* __XKRT_H__ */
