# include <stdio.h>

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            # pragma omp task depend(out: x)
                puts("Hello");

            # pragma omp task depend(in: y)
                puts("world");

            # pragma omp taskwait
        }
    }

    return 0;
}
