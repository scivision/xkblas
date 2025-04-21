/* ************************************************************************** */
/*                                                                            */
/*   memory.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/21 20:59:32 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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

xkrt_area_chunk_t *
xkrt_runtime_t::memory_device_allocate(
    const xkrt_device_global_id_t device_global_id,
    const size_t size
) {
    xkrt_device_t * device = this->device_get(device_global_id);
    return device->memory_allocate(size);
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
