# include <stdio.h>

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            # pragma omp task
                puts("Hello world");

            # pragma omp taskwait
        }
    }

    return 0;
}
