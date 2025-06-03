/* ************************************************************************** */
/*                                                                            */
/*   metric.h                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/18 22:56:54 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:02:05 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __METRICS_H__
#  define __METRICS_H__

#  include <stddef.h>
#  include <stdint.h>

extern "C" {
    uint64_t xkrt_get_nanotime(void);

    void xkrt_metric_byte(char * buffer, int bufsize, size_t nbytes);
    void xkrt_metric_time(char * buffer, int bufsize, uint64_t ns);
    void xkrt_metric_bandwidth(char * buffer, int bufsize, size_t byte_per_sec);
};

typedef enum    metric_t
{
    METRIC_BYTE,
    METRIC_TIME,
    METRIC_BW,
    METRIC_MAX
}               metric_t;

typedef struct  xkrt_power_counter_t
{
    uint64_t b1, b2, b3 ,b4;
}               xkrt_power_counter_t;

typedef struct  xkrt_power_t
{
    struct {
        /* start/stop times */
        uint64_t t1, t2;

        /* power values */
        xkrt_power_counter_t c1, c2;
    } priv;

    /* delta time between a start/stop */
    double dt;

    /* power (J/s <=> Watt) between a start/stop */
    double P;

}               xkrt_power_t;

# endif /* __METRICS_H__ */
