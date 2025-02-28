# ifndef __TIME_H__
#  define __TIME_H__

# include <stdint.h>
# include <math.h>
# include <time.h>
# include <functional>

# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static int
size_t_cmp(const void * a, const void * b)
{
    const size_t * x = (const size_t *) a;
    const size_t * y = (const size_t *) b;
    return (*x < *y) ? -1 : (*x == *y) ? 0 : 1;
}

template <size_t N_ELEMENTS, size_t N_ITERS>
struct  time_array_t
{
    static_assert(N_ITERS >= 3 && N_ITERS % 2 == 1);

    const size_t nelements;
    const int niters;
    size_t values[N_ELEMENTS][N_ITERS];
    size_t avg;
    size_t variance;
    size_t stdev;

    time_array_t() : values{}, nelements(N_ELEMENTS), niters(N_ITERS)
    {
        xkbm_mem_touch(this->values, sizeof(this->values));
    }

    virtual ~time_array_t() {}

    inline void set(int element, int iter, size_t value)
    {
        this->values[element][iter] = value;
    }

    static void report_element_default(time_array_t * time, size_t i)
    {
        printf("%12lu | %10lu +/- %10lu | %10lu | %10lu | %10lu\n", i, time->avg, time->stdev, time->values[i][0], time->values[i][N_ITERS/2], time->values[i][N_ITERS-1]);
    }

    template <void (*REPORT_ELEMENT)(time_array_t *, size_t) = report_element_default>
    inline void report(const char * label, const int nelements = N_ELEMENTS)
    {
        LOGGER_INFO("%12s | %10s +/- %10s | %10s | %10s | %10s", label, "avg", "stdev", "min", "med", "max");
        for (size_t i = 0 ; i < nelements ; ++i)
        {
            qsort(this->values[i], N_ITERS, sizeof(size_t), size_t_cmp);
            size_t min = this->values[i][0];
            size_t med = this->values[i][N_ITERS / 2];
            size_t max = this->values[i][N_ITERS - 1];

            this->avg = 0;
            for (int j = 0 ; j < N_ITERS ; ++j)
                this->avg += this->values[i][j];
            this->avg = this->avg / N_ITERS;

            this->variance = 0;
            for (int j = 0 ; j < N_ITERS ; ++j)
                this->variance += (this->values[i][j] - this->avg) * (this->values[i][j] - this->avg);
            this->variance = this->variance / N_ITERS;

            this->stdev = sqrt(this->variance);
            REPORT_ELEMENT(this, i);
        }
    }

    static void
    pp_1zu_1time(time_array_t * time, size_t i)
    {
        char buffer[5][64];
        xkrt_metric_time(buffer[0], sizeof(buffer[0]), time->avg);
        xkrt_metric_time(buffer[1], sizeof(buffer[1]), time->stdev);
        xkrt_metric_time(buffer[2], sizeof(buffer[2]), time->values[i][0]);
        xkrt_metric_time(buffer[3], sizeof(buffer[3]), time->values[i][N_ITERS/2]);
        xkrt_metric_time(buffer[4], sizeof(buffer[4]), time->values[i][N_ITERS-1]);
        LOGGER_INFO("%12zu | %10s +/- %10s | %10s | %10s | %10s", i, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    }

    static void
    pp_1byte_1time(time_array_t * time, size_t i)
    {
        char buffer[6][64];
        xkrt_metric_byte(buffer[0], sizeof(buffer[0]), ((size_t) 1) << i);
        xkrt_metric_time(buffer[1], sizeof(buffer[1]), time->avg);
        xkrt_metric_time(buffer[2], sizeof(buffer[2]), time->stdev);
        xkrt_metric_time(buffer[3], sizeof(buffer[3]), time->values[i][0]);
        xkrt_metric_time(buffer[4], sizeof(buffer[4]), time->values[i][N_ITERS/2]);
        xkrt_metric_time(buffer[5], sizeof(buffer[5]), time->values[i][N_ITERS-1]);
        LOGGER_INFO("%12s | %10s +/- %10s | %10s | %10s | %10s", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
    }

    static void
    pp_1zu_1bw(time_array_t * time, size_t i)
    {
        char buffer[5][64];
        xkrt_metric_bandwidth(buffer[0], sizeof(buffer[0]), time->avg);
        xkrt_metric_bandwidth(buffer[1], sizeof(buffer[1]), time->stdev);
        xkrt_metric_bandwidth(buffer[2], sizeof(buffer[2]), time->values[i][0]);
        xkrt_metric_bandwidth(buffer[3], sizeof(buffer[3]), time->values[i][N_ITERS/2]);
        xkrt_metric_bandwidth(buffer[4], sizeof(buffer[4]), time->values[i][N_ITERS-1]);
        LOGGER_INFO("%12zu | %10s +/- %10s | %10s | %10s | %10s", i, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    }
};

# endif /* __TIME_H__ */
