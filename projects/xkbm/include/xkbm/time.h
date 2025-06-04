/* ************************************************************************** */
/*                                                                            */
/*   time.h                                                       .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/18 22:56:54 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:37:59 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

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

struct  time_array_t
{
    const size_t nelements;
    const int niters;
    uint64_t * values;
    size_t avg;
    size_t variance;
    size_t stdev;

    time_array_t(size_t nelements, int niters) : nelements(nelements), niters(niters)
    {
        this->values = (uint64_t *) calloc(1, sizeof(uint64_t) * nelements * niters);
    }

    ~time_array_t()
    {
        free(this->values);
    }

    inline uint64_t get(int element, int iter)
    {
        return this->values[element * this->niters + iter];
    }

    inline void set(int element, int iter, uint64_t value)
    {
        this->values[element * this->niters + iter] = value;
    }

    // TODO : remove all other to keep only this one
    template <metric_t metric>
    inline void
    report(
        const char * label,
        const std::function<void (char *, size_t, int)>& convert_label
    ) {

        LOGGER_INFO("%26s | %10s +/- %10s | %10s | %10s | %10s | %6s", label, "avg", "stdev", "min", "med", "max", "n");
        for (size_t i = 0 ; i < nelements ; ++i)
        {
            qsort(this->values + i * this->niters, niters, sizeof(size_t), size_t_cmp);
            // size_t min = this->get(i, 0);
            // size_t med = this->get(i, niters / 2);
            // size_t max = this->get(i, niters - 1);

            this->avg = 0;
            for (int j = 0 ; j < niters ; ++j)
                this->avg += this->get(i, j);
            this->avg = this->avg / niters;

            this->variance = 0;
            for (int j = 0 ; j < niters ; ++j)
                this->variance += (this->get(i, j) - this->avg) * (this->get(i, j) - this->avg);
            this->variance = this->variance / niters;

            this->stdev = sqrt(this->variance);

            void (*func[METRIC_MAX])(char * buffer, int bufsize, size_t nbytes);
            func[METRIC_BYTE] = xkrt_metric_byte;
            func[METRIC_TIME] = xkrt_metric_time;
            func[METRIC_BW]   = xkrt_metric_bandwidth;

            char buffer[6][64];
            func[metric](buffer[0], sizeof(buffer[0]), this->avg);
            func[metric](buffer[1], sizeof(buffer[1]), this->stdev);
            func[metric](buffer[2], sizeof(buffer[2]), this->get(i, 0));
            func[metric](buffer[3], sizeof(buffer[3]), this->get(i, niters/2));
            func[metric](buffer[4], sizeof(buffer[4]), this->get(i, niters-1));
            convert_label(&buffer[5][0], sizeof(buffer[5]), i);
            LOGGER_INFO("%26s | %10s +/- %10s | %10s | %10s | %10s | %6d", buffer[5], buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], niters);
        }
    }

    template <void (*REPORT_ELEMENT)(time_array_t *, size_t)>
    inline void report(const char * label)
    {
        LOGGER_INFO("%12s | %10s +/- %10s | %10s | %10s | %10s | %6s", label, "avg", "stdev", "min", "med", "max", "n");
        for (size_t i = 0 ; i < nelements ; ++i)
        {
            qsort(this->values + i * niters, niters, sizeof(size_t), size_t_cmp);
            // size_t min = this->get(i, 0);
            // size_t med = this->get(i, niters / 2);
            // size_t max = this->get(i, niters - 1);

            this->avg = 0;
            for (int j = 0 ; j < niters ; ++j)
                this->avg += this->get(i, j);
            this->avg = this->avg / niters;

            this->variance = 0;
            for (int j = 0 ; j < niters ; ++j)
                this->variance += (this->get(i, j) - this->avg) * (this->get(i, j) - this->avg);
            this->variance = this->variance / niters;

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
        xkrt_metric_time(buffer[2], sizeof(buffer[2]), time->get(i, 0));
        xkrt_metric_time(buffer[3], sizeof(buffer[3]), time->get(i, time->niters/2));
        xkrt_metric_time(buffer[4], sizeof(buffer[4]), time->get(i, time->niters-1));
        LOGGER_INFO("%12zu | %10s +/- %10s | %10s | %10s | %10s | %6d", i, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], time->niters);
    }

    static void
    pp_1byte_1time(time_array_t * time, size_t i)
    {
        char buffer[6][64];
        xkrt_metric_byte(buffer[0], sizeof(buffer[0]), ((size_t) 1) << i);
        xkrt_metric_time(buffer[1], sizeof(buffer[1]), time->avg);
        xkrt_metric_time(buffer[2], sizeof(buffer[2]), time->stdev);
        xkrt_metric_time(buffer[3], sizeof(buffer[3]), time->get(i, 0));
        xkrt_metric_time(buffer[4], sizeof(buffer[4]), time->get(i, time->niters/2));
        xkrt_metric_time(buffer[5], sizeof(buffer[5]), time->get(i, time->niters-1));
        LOGGER_INFO("%12s | %10s +/- %10s | %10s | %10s | %10s | %6d", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], time->niters);
    }

    static void
    pp_1zu_1bw(time_array_t * time, size_t i)
    {
        char buffer[5][64];
        xkrt_metric_bandwidth(buffer[0], sizeof(buffer[0]), time->avg);
        xkrt_metric_bandwidth(buffer[1], sizeof(buffer[1]), time->stdev);
        xkrt_metric_bandwidth(buffer[2], sizeof(buffer[2]), time->get(i, 0));
        xkrt_metric_bandwidth(buffer[3], sizeof(buffer[3]), time->get(i, time->niters/2));
        xkrt_metric_bandwidth(buffer[4], sizeof(buffer[4]), time->get(i, time->niters-1));
        LOGGER_INFO("%12zu | %10s +/- %10s | %10s | %10s | %10s | %6d", i, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], time->niters);
    }


};

# endif /* __TIME_H__ */
