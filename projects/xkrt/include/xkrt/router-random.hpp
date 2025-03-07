# ifndef __ROUTER_RANDOM_HPP__
#  define __ROUTER_RANDOM_HPP__

# include <xkrt/memory/router.hpp>
# include <xkrt/sync/bits.h>

/** The higher the rank, the lower the performance */
class RouterRandom : public Router
{
    # if 0
    private:
        /** The lower the rank, the higher the performance */
        const xkrt_device_global_id_bitfield_t affinity[XKRT_DEVICES_PERF_RANK_MAX];
    # endif /* 0 */

    public:
        RouterRandom() {}
        ~RouterRandom() {}

        xkrt_device_global_id_t
        get_source(
            const xkrt_device_global_id_t dst,
            const xkrt_device_global_id_bitfield_t valid
        ) const {
            /* get any random device */
            return (xkrt_device_global_id_t) (__random_set_bit(valid) - 1);
        }

};

# endif /* __ROUTER_RANDOM_HPP__ */
