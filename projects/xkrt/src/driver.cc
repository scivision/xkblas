/* ************************************************************************** */
/*                                                                            */
/*   driver.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/26 19:40:36 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:55:05 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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

