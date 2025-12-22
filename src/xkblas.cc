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

# include <atomic>
# include <stdlib.h>
# include <string.h>

# include <xkblas/xkblas.hpp>

# include <xkrt/runtime.h>
# include <xkrt/sync/spinlock.h>

XKRT_NAMESPACE_USE;

////////////////////////////
// Methods implementation //
////////////////////////////

void
xkblas_t::init(void)
{
    if (this->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(this->state.spinlock);
        {
            if (this->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
            {
                // init xkrt
                this->runtime.init();

                // create task formats
                # define CREATE(P, K)                                                   \
                    do {                                                                \
                        task_format_t format;                                           \
                        memset(&format, 0, sizeof(task_format_t));                      \
                        this->task_format_create_##K<P>(&format);                       \
                        snprintf(format.label, sizeof(format.label), #P#K);             \
                        this->formats.K[P] = this->runtime.task_format_create(&format); \
                    } while (0);
                memset(&this->formats, 0, sizeof(this->formats));
                XKBLAS_FORALL_PRECISIONS_AND_ROUTINES(CREATE);
                # undef CREATE

                // done
                this->state.current = XKBLAS_CONTEXT_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(this->state.spinlock);
    }
}

void
xkblas_t::deinit(void)
{
    if (this->state.current == XKBLAS_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(this->state.spinlock);
        {
            if (this->state.current == XKBLAS_CONTEXT_INITIALIZED)
            {
                this->state.current = XKBLAS_CONTEXT_DEINITIALIZED;
                this->runtime.deinit();
            }
        }
        SPINLOCK_UNLOCK(this->state.spinlock);
    }
}

void
xkblas_t::sync(void)
{
    this->runtime.task_wait();
}

void
xkblas_t::memory_invalidate_caches(void)
{
    this->matrices_reset();
    this->runtime.reset();
}

void
xkblas_t::memory_coherent_async(
    device_global_id_t device_global_id,
    void * ptr,
    size_t size
) {
    return this->runtime.memory_coherent_async(device_global_id, ptr, size);
}

void
xkblas_t::memory_coherent_async(
    device_global_id_t device_global_id,
    matrix_storage_t storage,
    void * ptr,
    size_t ld,
    size_t m,
    size_t n,
    size_t sizeof_type
) {
    return this->runtime.memory_coherent_async(device_global_id, storage, ptr, ld, m, n, sizeof_type);
}

void
xkblas_t::memory_coherent_sync(
    device_global_id_t device_global_id,
    void * ptr,
    size_t size
) {
    return this->runtime.memory_coherent_sync(device_global_id, ptr, size);
}

void
xkblas_t::memory_coherent_sync(
    device_global_id_t device_global_id,
    matrix_storage_t storage,
    void * ptr,
    size_t ld,
    size_t m,
    size_t n,
    size_t sizeof_type
) {
    return this->runtime.memory_coherent_sync(device_global_id, storage, ptr, ld, m, n, sizeof_type);
}

int
xkblas_t::memory_register(
    void * ptr,
    size_t size
) {
    return this->runtime.memory_register(ptr, size);
}

int
xkblas_t::memory_unregister(
    void * ptr,
    size_t size
) {
    return this->runtime.memory_unregister_async(ptr, size);
}

int
xkblas_t::memory_register_async(
    void * ptr,
    size_t size,
    int n
) {
    return this->runtime.memory_register_async(ptr, size, n);
}

int
xkblas_t::memory_unregister_async(
    void * ptr,
    size_t size,
    int n
) {
    return this->runtime.memory_unregister_async(ptr, size, n);
}

void *
xkblas_t::memory_unified_allocate(const device_global_id_t device_global_id, const size_t size)
{
    return this->runtime.memory_unified_allocate(device_global_id, size);
}

/* deallocate unified memory using the driver of the given device */
void
xkblas_t::memory_unified_deallocate(const device_global_id_t device_global_id, void * mem, const size_t size)
{
    return this->runtime.memory_unified_deallocate(device_global_id, mem, size);
}
