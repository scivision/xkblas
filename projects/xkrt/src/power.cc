/* ************************************************************************** */
/*                                                                            */
/*   power.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/04/21 21:22:03 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:57:49 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver.h>
# include <xkrt/logger/metric.h>

void
xkrt_runtime_t::power_start(
    const xkrt_device_global_id_t device_global_id,
    xkrt_power_t * power
) {
    assert(power);

    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_power_start)
        driver->f_power_start(device->driver_id, power);
}

void
xkrt_runtime_t::power_stop(
    const xkrt_device_global_id_t device_global_id,
    xkrt_power_t * power
) {
    assert(power);

    xkrt_device_t * device = this->device_get(device_global_id);
    xkrt_driver_t * driver = this->driver_get(device->driver_type);
    if (driver->f_power_stop)
        driver->f_power_stop(device->driver_id, power);
    else
    {
        power->dt = -1.0;
        power->P  = -1.0;
    }
}
