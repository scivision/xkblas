# include <stdio.h>
# include <unistd.h>
# include <omp.h>

int
main(void)
{
    # pragma omp parallel
    {
        printf("Hello from thread %d\n", omp_get_thread_num());
        usleep(omp_get_thread_num() * 1000);

        # pragma omp for
        for (int i = 0 ; i < 2 * omp_get_num_threads() ; ++i)
            printf("Hello from thread %d - iteration %d\n", omp_get_thread_num(), i);
    }

    return 0;
}
