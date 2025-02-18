/* ************************************************************************** */
/*                                                                            */
/*   metric.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/18 15:08:36 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/18 15:26:10 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __METRICS_H__
#  define __METRICS_H__

#  include <stddef.h>
#  include <stdint.h>

extern "C" {
    uint64_t xkrt_get_nanotime(void);

    void xkrt_metric_byte(char * buffer, int bufsize, size_t nbytes);
    void xkrt_metric_time(char * buffer, int bufsize, size_t ns);
    void xkrt_metric_bandwidth(char * buffer, int bufsize, size_t byte_per_sec);
};

# endif /* __METRICS_H__ */
