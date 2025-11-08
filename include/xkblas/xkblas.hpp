/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

#ifndef __XKBLAS_HPP__
# define __XKBLAS_HPP__

# include <xkblas/conf.h>
# include <xkblas/routine.hpp>
# include <xkblas/support.h>

# include <xkrt/runtime.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdint.h>

typedef enum    xkblas_state_t : uint8_t
{
    XKBLAS_CONTEXT_DEINITIALIZED = 0,
    XKBLAS_CONTEXT_INITIALIZED,
}               xkblas_state_t;

# define TYPED            template <xkblas_precision_t P>
# define TYPED_WITH_INDEX template <xkblas_precision_t P, xkblas_index_t T>
# define TYPE             xkblas_precision_type_t<P>
# define TYPE_REAL        xkblas_precision_type_real_t<P>
# define INDEX            xkblas_index_type_t<T>

/* xkblas instance */
typedef struct  xkblas_t
{
    /* the xkrt runtime */
    xkrt::runtime_t runtime;

    /* state */
    struct {
        spinlock_t spinlock;
        volatile std::atomic<xkblas_state_t> current;
    } state;

    /* conf */
    xkblas_conf_t conf;

    /* matrix metadata, to cache stuff for spmv in particular */
    struct {
        struct {
            std::map<const void *, void *> metadata;
            pthread_rwlock_t rwlock;
        } csr;
    } matrices;

    //////////////////
    // Task formats //
    //////////////////

    /* task formats */
    struct {
        # define DEFINE(K) xkrt::task_format_id_t K[XKBLAS_PRECISION_MAX];
        XKBLAS_FORALL_ROUTINES(DEFINE);
        # undef DEFINE
    } formats;

    // Utilities to set / get task formats
    # define XKBLAS_TASK_FORMAT_GET(P, K) this->formats.K[P]

    # define DEFINE(K) TYPED void task_format_create_##K(xkrt::task_format_t * format);
    XKBLAS_FORALL_ROUTINES(DEFINE);
    # undef DEFINE

    ////////////////
    // Management //
    ////////////////

    void init(void);
    void deinit(void);

    ////////////
    // Memory //
    ////////////

    /* spawn tasks to make the replica coherent on the passed device */
    void memory_coherent_async(xkrt::device_global_id_t device_global_id, void * ptr, size_t size);
    void memory_coherent_async(xkrt::device_global_id_t device_global_id, matrix_storage_t storage, void * ptr, size_t ld, size_t m, size_t n, size_t sizeof_type);

    void memory_invalidate_caches(void);

    int memory_register(void * ptr, size_t size);
    int memory_unregister(void * ptr, size_t size);

    /**
     * memory registration async
     *  ptr is base address
     *  size is the number of total bytes
     *  n is the number of continugous intervals to pin in separate tasks
     */
    int memory_register_async  (void * ptr, size_t size, int n);
    int memory_unregister_async(void * ptr, size_t size, int n);

    /* (de)allocate unified memory using the driver of the given device */
    void * memory_unified_allocate(const xkrt::device_global_id_t device_global_id, const size_t size);
    void memory_unified_deallocate(const xkrt::device_global_id_t device_global_id, void * mem, const size_t size);

    /////////////////////
    // Synchronization //
    /////////////////////

    void sync(void);

    /////////////
    // Kernels //
    /////////////

    // define both sync and async version, e.g.,
    //      sgemm
    //      sgemm_async
    # define DEF(TPLT, RTYPE, NAME, ...)        \
        TPLT RTYPE NAME        (__VA_ARGS__);   \
        TPLT RTYPE NAME##_async(__VA_ARGS__);
    # define XKDEF(RTYPE, NAME, ...)  DEF(TYPED,            RTYPE, NAME, __VA_ARGS__)
    # define XKDEFI(RTYPE, NAME, ...) DEF(TYPED_WITH_INDEX, RTYPE, NAME, __VA_ARGS__)
    # define XKTYPE TYPE
    # define XKTYPE_REAL TYPE_REAL
    # define XKINDEX INDEX
    # define XKDEVICE xkrt_device_global_id_t
    #  include <xkblas/for-all-routines.h>
    # undef XKDEVICE
    # undef XKTYPE_REAL
    # undef XKTYPE
    # undef XKINDEX
    # undef XKDEFI
    # undef XKDEF
    # undef DEF
}               xkblas_t;

// TODO : currently using a global variable to preserve previous 'xkblas_init'
// and 'xkblas_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'xkblas_t' argument that the user must keep
// track of
xkblas_t * xkblas_get(void);
xkrt::runtime_t * xkblas_xkrt_runtime_get(void);

#endif /* __XKBLAS_HPP__ */
