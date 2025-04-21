/* ************************************************************************** */
/*                                                                            */
/*   power.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/21 21:01:40 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
