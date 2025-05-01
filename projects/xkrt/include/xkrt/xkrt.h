/* ************************************************************************** */
/*                                                                            */
/*   xkrt.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/21 22:21:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_H__
# define __XKRT_H__

// TODO : xkrt.h should become the C interface of XKaapi

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

    /* return the maximum number of devices available */
    int xkrt_get_ndevices_max(xkrt_runtime_t * runtime, int * count);

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

    /* Submit 1 task per device to replicate the data */
    void xkrt_coherency_replicate_2D_async(
        xkrt_runtime_t * runtime,
        matrix_order_t order,
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    /* Submit a task that reads Region(order, m, n, addr, ld, sizeof_type) onto the host */
    void xkrt_coherency_host_async(
        xkrt_runtime_t * runtime,
        matrix_order_t order,
        void * addr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    ////////////////
    // DISTRIBUTE //
    ////////////////

    // DISTRIBUTION //
    typedef enum    xkrt_distribution_type_t
    {
        XKRT_DISTRIBUTION_TYPE_CYCLIC2D,
        XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK,
    }               xkrt_distribution_type_t;

    typedef struct  xkrt_distribution_t
    {
        xkrt_distribution_type_t type;
        size_t count;
        size_t m, n;
        size_t mb, nb;
        size_t mt, nt;

        union {

            // for XKRT_DISTRIBUTION_TYPE_CYCLIC2D
            // struct { } ;

            // for XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK
            struct {
                size_t blkm, blkn;
                size_t gm, gn;
            };
        };
    }               xkrt_distribution_t;

    xkrt_device_global_id_t xkrt_distribution_get(
        xkrt_distribution_t * d,
        size_t tm, size_t tn
    );

    void
    xkrt_distribution_init(
        xkrt_distribution_t * d,
        xkrt_distribution_type_t type,
        size_t count,
        size_t m, size_t n,
        size_t mb, size_t nb
    );

    // DISTRIBUTE //

    void xkrt_distribute_async(
        xkrt_runtime_t * runtime,
        xkrt_distribution_type_t type,
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

    /* see cudaHostRegister */
    int xkrt_memory_register_async(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    /* see cudaHostUnregister */
    int xkrt_memory_unregister_async(xkrt_runtime_t * runtime, void * ptr, uint64_t size);

    /* see cudaHostUnregister */
    int xkrt_memory_register_waitall(xkrt_runtime_t * runtime);

};

#endif /* __XKRT_H__ */
