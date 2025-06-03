/* ************************************************************************** */
/*                                                                            */
/*   metric.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/18 22:56:54 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:56:49 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/logger/metric.h>
# include <time.h>
# include <stdio.h>

extern "C"
uint64_t
xkrt_get_nanotime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t) ts.tv_nsec;
}

extern "C"
void
xkrt_metric_byte(char * buffer, int bufsize, size_t nbytes)
{
    const char * suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    unsigned int i = 0;
    double size = (double) nbytes;
    while (size >= 1000 && i < sizeof(suffixes) / sizeof(*suffixes))
    {
        size /= 1000;
        i++;
    }
    snprintf(buffer, bufsize, "%.2lf%s", size, suffixes[i]);
}

extern "C"
void
xkrt_metric_time(char * buffer, int bufsize, uint64_t ns)
{
    const char * suffixes[] = {"ns", "us", "ms", "s"};
    unsigned int i = 0;
    double size = (double) ns;
    while (size >= 1000 && i < sizeof(suffixes) / sizeof(*suffixes))
    {
        size /= 1000;
        i++;
    }
    snprintf(buffer, bufsize, "%.2lf%s", size, suffixes[i]);
}

extern "C"
void
xkrt_metric_bandwidth(char * buffer, int bufsize, size_t byte_per_sec)
{
    const char * suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    unsigned int i = 0;
    double size = (double) byte_per_sec;
    while (size >= 1000 && i < sizeof(suffixes) / sizeof(*suffixes))
    {
        size /= 1000;
        i++;
    }
    snprintf(buffer, bufsize, "%.2lf%s/s", size, suffixes[i]);
}
