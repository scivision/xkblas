/* ************************************************************************** */
/*                                                                            */
/*   driver.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/04/21 21:00:05 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/21 21:00:14 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

xkrt_driver_t *
xkrt_runtime_t::driver_get(
    const xkrt_driver_type_t type
) {
    assert(type >= 0);
    assert(type < XKRT_DRIVER_TYPE_MAX);
    return this->drivers.list[type];
}

xkrt_device_t *
xkrt_runtime_t::device_get(
    const xkrt_device_global_id_t device_global_id
) {
    assert(device_global_id >= 0);
    assert(device_global_id < this->drivers.devices.n);
    return this->drivers.devices.list[device_global_id];
}

