/* ************************************************************************** */
/*                                                                            */
/*   xkrt.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/27 22:35:49 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_H__
# define __XKRT_H__

// TODO : This file should become the C interface of XKaapi

// TODO : this include the whole world, including C++ this is bad
# include <xkrt/runtime.h>

// xkrt public API
extern "C" {

    ////////////////////////
    // Runtime management //
    ////////////////////////
    /* Initialize a runtime instance */
    int xkrt_init(xkrt_runtime_t * runtime);

    /* Deinitialize the runtime interface */
    int xkrt_deinit(xkrt_runtime_t * runtime);

    /* return the number of gpus running */
    int xkrt_get_ndevices(xkrt_runtime_t * runtime, int * count);

    //////////////////////
    // SYNCHRONIZATIONS //
    //////////////////////

    /* Wait for all tasks and devices instructions to complete */
    int xkrt_sync(xkrt_runtime_t * runtime);

    ///////////////
    // COHERENCY //
    ///////////////

    /* Make all device memory non-coherent */
    void xkrt_coherency_reset(xkrt_runtime_t * runtime);

    /* Submit a task that reads Region(order, m, n, addr, ld, sizeof_type) onto the host */
    void xkrt_coherency_host_async(
        xkrt_runtime_t * runtime,
        matrix_order_t order,
        void * addr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    /* Creates a partition of Region(order, ptr, ld, m, n, sizeof_type) with 1 partite per device
     * and create 1 empty task on each device that reads its partite */
    void xkrt_coherency_distribute_packed_2D_async(
        xkrt_runtime_t * runtime,
        matrix_order_t order,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    // TODO
    void xkrt_coherency_distribute_packed_2D_halo_async(
        xkrt_runtime_t * runtime,
        matrix_order_t order,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type,
        size_t hx, size_t hy
    );

    /* equivalent to xkrt_memory_distribute_cyclic_2D_halo_async(..., ox=0, oy=0) */
    void xkrt_coherency_distribute_cyclic_2D_async(
        xkrt_runtime_t * runtime,
        matrix_order_t order,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t mb, size_t nb,
        size_t sizeof_type
    );

    /* Create a partition of 'mb x nb' sub-regions (tiles) of Region(order, ptr, ld, m, n, sizeof_type)
     * and create 1 task that reads each sub-region offseted by +/-(ox, oy)
     * with a static schedule onto a device in a cyclic manner */
    void xkrt_coherency_distribute_cyclic_2D_halo_async(
        xkrt_runtime_t * runtime,
        matrix_order_t order,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t mb, size_t nb,
        size_t sizeof_type,
        size_t hx, size_t hy
    );

    //////////////////////////////////////////////////
    // EXPLICIT MEMORY MANAGMENT (bypass coherency) //
    //////////////////////////////////////////////////

    /* Create a task that launches a memory copy instruction from 'src' to 'dst' into 'device' streams */
    void xkrt_memory_copy_async(
        xkrt_runtime_t              * runtime,
        const xkrt_device_global_id_t device_global_id,
        const xkrt_device_global_id_t dst_device_global_id,
        const uintptr_t               dst_device_mem,
        const xkrt_device_global_id_t src_device_global_id,
        const uintptr_t               src_device_mem,
        const size_t                  size
    );

    /* see cudaHostRegister */
    int xkrt_memory_register(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    /* see cudaHostUnregister */
    int xkrt_memory_unregister(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

};

#endif /* __XKRT_H__ */
