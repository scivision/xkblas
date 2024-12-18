/* ************************************************************************** */
/*                                                                            */
/*   bits.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/18 14:21:49 by                           \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __BITS_H__
# define __BITS_H__

# include "device/consts.h"

/* utility: return the index+1 of random bit set to '1' */
static inline ptr_device_global_id_t
__random_set_bit(ptr_device_global_id_bitfield_t bitfield)
{
    static unsigned int seed = 0x42;

    if (bitfield == 0)
        LOGGER_FATAL("Tried to get a random bit from a NULL bitfield");

    /* must be true, as 'builtin_popcount' works on 'int' type */
    static_assert(sizeof(ptr_device_global_id_bitfield_t) <= sizeof(int));

    const int nb = __builtin_popcount(bitfield);
    ptr_device_global_id_t idx = 0;
    int k = rand_r(&seed) % nb;
    for (int i = 0; i <= k; ++i)
    {
        idx = static_cast<ptr_device_global_id_t>(__builtin_ffs(static_cast<int>(bitfield)));
        bitfield &= ~(1 << (idx - 1));
    }

    return idx;
}

#endif /* __BITS_H__ */
