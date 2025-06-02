/* ************************************************************************** */
/*                                                                            */
/*   touch-register-unregister-async.cc                                       */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/06/02 20:49:42 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

# include <random>

static xkrt_runtime_t runtime;

static void *       ptr         = NULL;
static const size_t size        = (size_t) (0.5 * 1024 * 1024 * 1024);
static const int    nchunks     = 4;
static const size_t chunk_size  = size / nchunks;

int
main(void)
{
    if (xkrt_init(&runtime))
        LOGGER_FATAL("ERROR INIT");

    xkrt_driver_t * driver = runtime.driver_get(XKRT_DRIVER_TYPE_HOST);
    assert(driver);

    int nthreads = driver->team.priv.nthreads;

    LOGGER_INFO("Size is %.1f GB in %d chunks using %u threads",
            size/1e9, nchunks, nthreads);

    std::mt19937 rng(std::random_device{}());
    int done[3] = {0, 0, 0};

    while (done[0] + done[1] + done[2] < 3)
    {
        int i;
        do { i = rng() % 3; } while (done[i]);
        done[i] = 1;

        LOGGER_INFO("------------------------------");

        uint64_t t0 = xkrt_get_nanotime();
        ptr = malloc(chunk_size * nchunks);

        if (i == 0 || i == 1)
        {
            if (i == 0)
            {
                LOGGER_INFO("Running with pre-touch");

                uint64_t t0 = xkrt_get_nanotime();
                runtime.memory_touch_async(ptr, chunk_size, nchunks);
                runtime.task_wait();
                uint64_t tf = xkrt_get_nanotime();
                LOGGER_INFO("      Touch took %lf s.", (tf - t0) / 1e9);
            }
            else if (i == 1)
            {
                LOGGER_INFO("Running without pre-touch");
            }

            {
                uint64_t t0 = xkrt_get_nanotime();
                runtime.memory_register_async(ptr, chunk_size, nchunks);
                runtime.task_wait();
                uint64_t tf = xkrt_get_nanotime();
                LOGGER_INFO("    Pinning took %lf s.", (tf - t0) / 1e9);
            }
        }
        else if (i == 2)
        {
            LOGGER_INFO("Running with concurrent touch");
            uint64_t t0 = xkrt_get_nanotime();
            runtime.memory_register_async(ptr, chunk_size, nchunks);
            runtime.memory_touch_async(ptr, chunk_size, nchunks);
            runtime.task_wait();
            uint64_t tf = xkrt_get_nanotime();
            LOGGER_INFO("  Touch+Pin took %lf s.", (tf - t0) / 1e9);
        }

        {
            uint64_t t0 = xkrt_get_nanotime();
            runtime.memory_unregister_async(ptr, chunk_size, nchunks);
            runtime.task_wait();
            uint64_t tf = xkrt_get_nanotime();
            LOGGER_INFO("  Unpinning took %lf s.", (tf - t0) / 1e9);
        }

        uint64_t tf = xkrt_get_nanotime();
        LOGGER_INFO("Total took %lf s.", (tf - t0) / 1e9);
    }

    if (xkrt_deinit(&runtime))
        LOGGER_FATAL("ERROR DEINIT");

    return 0;
}
