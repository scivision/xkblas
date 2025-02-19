# ifndef __TIME_H__
#  define __TIME_H__

# include <stdint.h>
# include <time.h>
# include <functional>

static int
size_t_cmp(const void * a, const void * b)
{
    const size_t * x = (const size_t *) a;
    const size_t * y = (const size_t *) b;
    return (*x < *y) ? -1 : (*x == *y) ? 0 : 1;
}

static void
report_element_default(size_t i, size_t min, size_t med, size_t max)
{
    printf("%8lu | %8lu | %8lu | %8lu\n", i, min, med, max);
}

template <size_t N_ELEMENTS, size_t N_ITERS>
struct  time_array_t
{
    static_assert(N_ITERS >= 3 && N_ITERS % 2 == 1);

    const size_t nelements;
    const size_t niters;
    size_t values[N_ELEMENTS][N_ITERS];

    time_array_t() : values{}, nelements(N_ELEMENTS), niters(N_ITERS)
    {
        xkbm_mem_touch(this->values, sizeof(this->values));
    }

    virtual ~time_array_t() {}

    inline void set(int element, int iter, size_t value)
    {
        this->values[element][iter] = value;
    }

    template <void (*REPORT_ELEMENT)(size_t, size_t, size_t, size_t) = report_element_default>
    inline void report(const int nelements = N_ELEMENTS)
    {
        for (size_t i = 0 ; i < nelements ; ++i)
        {
            qsort(this->values[i], N_ITERS, sizeof(size_t), size_t_cmp);
            size_t min = this->values[i][0];
            size_t med = this->values[i][N_ITERS / 2];
            size_t max = this->values[i][N_ITERS - 1];
            REPORT_ELEMENT(i, min, med, max);
        }
    }
};

# endif /* __TIME_H__ */
