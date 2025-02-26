/* ************************************************************************** */
/*                                                                            */
/*   pp.cc                                                                    */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/26 16:53:13 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/26 16:53:57 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

void
pp_1zu_1time(size_t i, size_t avg, size_t stdev)
{
    char buffer[2][64];
    xkrt_metric_time(buffer[0], sizeof(buffer[1]), avg);
    xkrt_metric_time(buffer[1], sizeof(buffer[2]), stdev);
    LOGGER_INFO("%10zu | %10s +/- %10s", i, buffer[0], buffer[1]);
}

void
pp_1byte_1time(size_t i, size_t avg, size_t stdev)
{
    char buffer[3][64];
    xkrt_metric_byte(buffer[0], sizeof(buffer[0]), ((size_t) 1) << i);
    xkrt_metric_time(buffer[1], sizeof(buffer[1]), avg);
    xkrt_metric_time(buffer[2], sizeof(buffer[2]), stdev);
    LOGGER_INFO("%12s | %10s +/- %10s", buffer[0], buffer[1], buffer[2]);
}

void
pp_1zu_1bw(size_t i, size_t avg, size_t stdev)
{
    char buffer[2][64];
    xkrt_metric_bandwidth(buffer[0], sizeof(buffer[0]), avg);
    xkrt_metric_bandwidth(buffer[1], sizeof(buffer[1]), stdev);
    LOGGER_INFO("%12lu | %10s +/- %10s", i, buffer[0], buffer[1]);
}
