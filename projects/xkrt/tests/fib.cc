/* ************************************************************************** */
/*                                                                            */
/*   fib.cc                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/04 03:50:49 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

# define N 20
# define CUTOFF_DEPTH 5

static const int fib_values[] = {
    1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610,
    987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368, 75025,
    121393, 196418, 317811, 514229, 832040, 1346269, 2178309, 3524578,
    5702887, 9227465, 14930352, 24157817, 39088169, 63245986, 102334155,
    165580141, 267914296, 433494437, 701408733, 1134903170, 1836311903
};

static_assert(N < sizeof(fib_values) / sizeof(int));

static xkrt_runtime_t runtime;

constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
constexpr size_t task_size = task_compute_size(flags, 0);

static int
fib(int n, int depth=0)
{
    if (n <= 2)
        return n;

    int fn1, fn2;
    if (depth >= CUTOFF_DEPTH)
    {
        fn1 = fib(n-1, depth+1);
        fn2 = fib(n-2, depth+1);
    }
    else
    {
        LOGGER_FATAL("impl me");
    }

    return fn1 + fn2;
}

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    // warmup
    fib(8);

    // run
    double t0 = xkrt_get_nanotime();
    int r = fib(N);
    double tf = xkrt_get_nanotime();
    LOGGER_INFO("Fib(%d) = %d - took %.2lf s", N, r, (tf - t0) / (double)1e9);

    assert(r == fib_values[N]);
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
