# include <assert.h>
# include <stdio.h>
# include <stdlib.h>

# define N 1024

int
main(void)
{
    double * x = (double *) calloc(1, sizeof(double) * N);
    assert(x);

    # pragma omp target enter data map(alloc: x[0:N])

    # pragma omp target update to(x[0:N])             depend(out: x) device(0) nowait
    # pragma omp target teams distribute parallel for depend(out: x) device(0) nowait
    for (int i = 0 ; i < N ; ++i)
        x[i] = i;
    # pragma omp target update from(x[0:N])           depend(out: x) device(0) nowait
    # pragma omp taskwait

    # pragma omp target exit data map(release: x[0:N])

    return 0;
}
