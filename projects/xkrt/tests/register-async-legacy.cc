/* ************************************************************************** */
/*                                                                            */
/*   register-async.cc                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/23 18:09:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;

static void *       ptr         = NULL;
static const size_t chunk_size  = 4096 * 64 + 123;
static const int    nchunks     = 16;

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);
    ptr = malloc(chunk_size * nchunks);
    runtime.memory_register_async(ptr, chunk_size, nchunks);
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
