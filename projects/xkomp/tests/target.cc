# include <assert.h>
# include <stdio.h>
# include <stdlib.h>

# include <omp.h>

# define N 1024

# define DEVICE_ID 0

int
main(void)
{
    # if 0
    double * x = (double *) calloc(1, sizeof(double) * N);
    assert(x);

    # pragma omp target device(DEVICE_ID)
    {
        printf("Running from device `%d` is initial: %d\n",
                omp_get_device_num(), omp_is_initial_device());
        assert(omp_is_initial_device() == 0);
    }

    # pragma omp target enter data map(alloc: x[0:N])  device(DEVICE_ID)

    # pragma omp target update to(x[0:N])              device(DEVICE_ID) depend(out: x) nowait
    # pragma omp target teams distribute parallel for  device(DEVICE_ID) depend(out: x) nowait
    for (int i = 0 ; i < N ; ++i)
        x[i] = i;
    # pragma omp target update from(x[0:N])            device(DEVICE_ID) nowait
    # pragma omp taskwait

    # pragma omp target exit data map(release: x[0:N]) device(DEVICE_ID)
    # endif

    return 0;
}
