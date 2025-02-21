/* ************************************************************************** */
/*                                                                            */
/*   xkrt.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 17:24:40 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_H__
# define __XKRT_H__


// TODO : this include the whole world, this is bad
# include <xkrt/runtime.h>

// xkrt public API
extern "C" {

    int xkrt_init(xkrt_runtime_t * runtime);
    int xkrt_deinit(xkrt_runtime_t * runtime);
    int xkrt_sync(xkrt_runtime_t * runtime);

    void xkrt_memory_invalidate(xkrt_runtime_t * runtime);
    void xkrt_memory_coherent_async(xkrt_runtime_t * runtime, int uplo, int memflag, int m, int n, void * addr, int ld, unsigned int sizeof_type);
    void xkrt_memory_distribute_cyclic_2D_async(xkrt_runtime_t * runtime, matrix_order_t order, void * ptr, size_t ld, size_t m, size_t n, size_t mb, size_t nb, size_t sizeof_type);

    int xkrt_memory_register(xkrt_runtime_t * runtime, void * ptr, uint64_t size);
    int xkrt_memory_unregister(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    int xkrt_get_ngpus(xkrt_runtime_t * runtime, int * count);
};

#endif /* __XKRT_H__ */
