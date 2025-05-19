/* ************************************************************************** */
/*                                                                            */
/*   bits.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/19 00:25:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __BITS_H__
# define __BITS_H__

# include <xkrt/consts.h>

/* utility: return the index+1 of random bit set to '1' */
static inline xkrt_device_global_id_t
__random_set_bit(xkrt_device_global_id_bitfield_t bitfield)
{
    static unsigned int seed = 0x42;

    if (bitfield == 0)
        LOGGER_FATAL("Tried to get a random bit from a NULL bitfield");

    /* must be true, as 'builtin_popcount' works on 'int' type */
    static_assert(sizeof(xkrt_device_global_id_bitfield_t) <= sizeof(int));

    const int nb = __builtin_popcount(bitfield);
    xkrt_device_global_id_t idx = 0;
    int k = rand_r(&seed) % nb;
    for (int i = 0; i <= k; ++i)
    {
        idx = static_cast<xkrt_device_global_id_t>(__builtin_ffs(static_cast<int>(bitfield)));
        bitfield &= ~(1u << (idx - 1));
    }

    return idx;
}

#endif /* __BITS_H__ */
