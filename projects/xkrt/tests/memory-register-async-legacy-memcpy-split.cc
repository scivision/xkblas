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

# define TYPE               double
# define S                  (sizeof(TYPE))
# define N                  (8192)
# define REGISTER_OFFSET    (123)
# define REGISTER_SIZE      (10000)

static_assert(REGISTER_OFFSET + REGISTER_SIZE <= N*S*N*S);

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    // allocate N x N bytes = a matrix of LD = N - forcing alignement on LD.s

    # if 0 /* force memory alignment on ld.s */
    const size_t ld = N;
    const size_t s = sizeof(TYPE);
    const uintptr_t alignon = s * ld;
    const uintptr_t memsize = (alignon + alignon/2 + s * ld * ld);
    const uintptr_t mem = (const uintptr_t) malloc(memsize);
    const uintptr_t p = mem + (alignon - (mem % alignon));
    assert(p % alignon == 0);
    TYPE * ptr = (TYPE *) p;
    # else
    TYPE * ptr = (TYPE *) malloc(N*N*S);
    # endif

    // register a portion of it
    xkrt_memory_register_async(&runtime, ptr + REGISTER_OFFSET, REGISTER_SIZE);

    // wait for registration
    runtime.task_wait();

    // submit data to devices
    xkrt_coherency_replicate_2D_async(&runtime, MATRIX_COLMAJOR, ptr, N, N, N, S);

    // wait
    runtime.task_wait();

    // finalize
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
