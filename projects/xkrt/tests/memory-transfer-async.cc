/* ************************************************************************** */
/*                                                                            */
/*   memory-transfer-async.cc                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/05 05:19:56 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 22:24:56 by Romain PEREIRA         / _______ \       */
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
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    # include "memory-register-async.conf.cc"

    const xkrt_device_global_id_t device_global_id = 1; // to the first device
    const int       device_memory_id = 0;               // to the first device's memory
    const uintptr_t device_addr      = (const uintptr_t) runtime.memory_device_allocate_on(device_global_id, size, device_memory_id);
    assert(device_addr);

    const uintptr_t host_addr = (const uintptr_t) ptr;

    const bool h2d = true;

    runtime.memory_transfer_async(
        device_global_id,
        device_addr,
        host_addr,
        size,
        h2d
    );
    runtime.task_wait();

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
