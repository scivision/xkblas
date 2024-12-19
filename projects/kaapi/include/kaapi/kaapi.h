/* ************************************************************************** */
/*                                                                            */
/*   kaapi.h                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:50:13 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __KAAPI_H__
# define __KAAPI_H__

// TODO : kaapi public API

extern "C" {

    int kaapi_init(void);
    int kaapi_deinit(void);
    void kaapi_memory_coherent_async(int uplo, int memflag, int m, int n, void * addr, int ld, unsigned int sizeof_type);
    void kaapi_memory_invalidate(void);
    void kaapi_sync(void);
    int kaapi_get_ngpus(int * count);
};

#endif /* __KAAPI_H__ */
