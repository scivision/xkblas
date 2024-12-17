# Notations
We use the following notations for kernel variables:
- `m` indexes rows, `n` indexes columns
- `A` - is a matrix
- `(m, n, k)` - are sizes (whether of the passed matrix, whether of the passed tiles)
- `(Amt, Ant)` - are respectively the number of tiles per row and columns
- `(tm, tn, tk)` - stands for 'tile m' - and is the index of the tile (in [0..Amt[)
- `(Amb, Anb, Akb)` - stands for block sizes - constant
- `(bs_m, bs_n, bs_k)` - stands for 'block size n' - but that is variable for boundary conditions
