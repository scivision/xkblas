# include <assert.h>
# include <stdio.h>
# include <unistd.h>
# include <omp.h>

int
main(void)
{
    for (int i = 0 ; i < 64 ; ++i)
    {
        int x = 42;
        int y = 43;

        # pragma omp parallel firstprivate(x) shared(y)
        {
            assert(x == 42);
            assert(y == 43);

            // printf("Hello from thread %d\n", omp_get_thread_num());
            // usleep(omp_get_thread_num() * 1000);

            # pragma omp for
            for (int i = 0 ; i < 2 * omp_get_num_threads() ; ++i)
            {
                // printf("Hello from thread %d - iteration %d\n", omp_get_thread_num(), i);
                int tid = omp_get_thread_num();
                assert(i == 2*tid+0 || i == 2*tid+1);
            }
        }
    }

    printf("done\n");

    return 0;
}
