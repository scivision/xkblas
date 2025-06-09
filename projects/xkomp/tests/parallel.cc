# include <stdio.h>
# include <omp.h>

int
main(void)
{
    # pragma omp parallel
    {
        printf("Hello from thread %d\n", omp_get_thread_num());
        // printf("Hello thread\n");
    }

    return 0;
}
