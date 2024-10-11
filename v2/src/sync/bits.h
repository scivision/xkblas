#ifndef __BITS_H__
# define __BITS_H__

/* utility: return the index+1 of random bit set to '1' */
static inline int
__random_set_bit(int bitfield)
{
    static unsigned int seed = 0x42;

    if (bitfield == 0)
        return 0;

    const int nb = __builtin_popcount(bitfield);
    int idx = 0;
    int k = rand_r(&seed) % nb;
    for (int i = 0; i <= k; ++i)
    {
        idx = __builtin_ffs(bitfield);
        bitfield &= ~(1 << (idx - 1));
    }

    return idx;
}

#endif /* __BITS_H__ */
