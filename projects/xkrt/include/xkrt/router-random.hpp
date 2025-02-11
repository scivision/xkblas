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
        virtual ~RouterRandom() {}

        xkrt_device_global_id_t
        get_source(
            const xkrt_device_global_id_t dst,
            const xkrt_device_global_id_bitfield_t valid
        ) {
    # if 0
        /** The lower the rank, the higher the performance */
            /* fast way out: valid on that device already */
            if (valid & (1 << dst_device_global_id))
                return dst_device_global_id;

            /* find a device for P2P transfer - lowest rank <=> best performance */
            for (int rank = 0 ; rank < XKRT_DEVICE_PERF_RANK_MAX - 1 ; ++rank)
            {
                /* get valid devices for this perf */
                const xkrt_device_global_id_bitfield_t mask = valid & this->affinity[rank];
                if (mask == 0)
                    continue ;

                /* return a random device with this affinity */
                return __random_set_bit(mask) - 1;
            }
    # endif /* 0 */

            /* get any random device */
            return __random_set_bit(valid) - 1;
        }

};

# endif /* __ROUTER_RANDOM_HPP__ */
