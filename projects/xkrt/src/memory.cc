/* ************************************************************************** */
/*                                                                            */
/*   memory.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/04/21 21:22:03 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:56:59 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mem.h>

# include <cassert>
# include <cstring>
# include <cerrno>

void
xkrt_runtime_t::memory_device_preallocate_ensure(
    const xkrt_device_global_id_t device_global_id,
    const int memory_id
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    if (!device->memories[memory_id].allocated)
    {
        xkrt_driver_t * driver = this->driver_get(device->driver_type);

        XKRT_MUTEX_LOCK(device->memories[memory_id].area.lock);
        {
            if (!device->memories[memory_id].allocated)
            {
                const size_t size = (size_t) ((double)device->memories[memory_id].capacity * (double)(this->conf.device.gpu_mem_percent / 100.0));
                assert(driver->f_memory_device_allocate);
                const void * device_ptr = driver->f_memory_device_allocate(device->driver_id, size, memory_id);
                device->memory_set_chunk0((uintptr_t) device_ptr, size, memory_id);
                device->memories[memory_id].allocated = 1;
            }
        }
        XKRT_MUTEX_UNLOCK(device->memories[memory_id].area.lock);
    }
}

xkrt_area_chunk_t *
xkrt_runtime_t::memory_device_allocate_on(
    const xkrt_device_global_id_t device_global_id,
    const size_t size,
    const int memory_id
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    this->memory_device_preallocate_ensure(device_global_id, memory_id);
    return device->memory_allocate_on(size, memory_id);
}

xkrt_area_chunk_t *
xkrt_runtime_t::memory_device_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    return this->memory_device_allocate_on(device_global_id, size, 0);
}

void
xkrt_runtime_t::memory_device_deallocate(
    const xkrt_device_global_id_t device_global_id,
    xkrt_area_chunk_t * chunk
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_deallocate(chunk);
}

void
xkrt_runtime_t::memory_device_deallocate_all(
    const xkrt_device_global_id_t device_global_id
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_reset();
}

void *
xkrt_runtime_t::memory_host_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_host_allocate)
        return driver->f_memory_host_allocate(device->driver_id, size);
    else
    {
        LOGGER_WARN("Driver `%s` does not implement memory_alloc_host", driver->f_get_name());
        return malloc(size);
    }
}

void
xkrt_runtime_t::memory_host_deallocate(
    const xkrt_device_global_id_t device_global_id,
    void * mem,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_host_deallocate)
        driver->f_memory_host_deallocate(device->driver_id, mem, size);
    else
    {
        LOGGER_WARN("Driver `%s` does not implement memory_dealloc_host", driver->f_get_name());
        free(mem);
    }
}

void *
xkrt_runtime_t::memory_unified_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_unified_allocate)
        return driver->f_memory_unified_allocate(device->driver_id, size);
    else
    {
        LOGGER_FATAL("Driver `%s` does not implement memory_alloc_unified", driver->f_get_name());
    }
}

void
xkrt_runtime_t::memory_unified_deallocate(
    const xkrt_device_global_id_t device_global_id,
    void * mem,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_memory_unified_deallocate)
        driver->f_memory_unified_deallocate(device->driver_id, mem, size);
    else
    {
        LOGGER_FATAL("Driver `%s` does not implement memory_dealloc_unified", driver->f_get_name());
    }
}

void
xkrt_runtime_t::copy(
    const xkrt_device_global_id_t   device_global_id,
    const memory_view_t           & host_view,
    const xkrt_device_global_id_t   dst_device_global_id,
    const memory_replicate_view_t & dst_device_view,
    const xkrt_device_global_id_t   src_device_global_id,
    const memory_replicate_view_t & src_device_view,
    const xkrt_callback_t         & callback
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    device->offloader_stream_instruction_submit_copy<memory_view_t, memory_replicate_view_t>(
        host_view,
        dst_device_global_id,
        dst_device_view,
        src_device_global_id,
        src_device_view,
        callback
    );
}

void
xkrt_runtime_t::copy(
    const xkrt_device_global_id_t   device_global_id,
    const size_t                    size,
    const xkrt_device_global_id_t   dst_device_global_id,
    const uintptr_t                 dst_device_addr,
    const xkrt_device_global_id_t   src_device_global_id,
    const uintptr_t                 src_device_addr,
    const xkrt_callback_t         & callback
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    device->offloader_stream_instruction_submit_copy<size_t, uintptr_t>(
        size,
        dst_device_global_id,
        dst_device_addr,
        src_device_global_id,
        src_device_addr,
        callback
    );
}
