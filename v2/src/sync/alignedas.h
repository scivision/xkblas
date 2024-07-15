#ifndef __ALIGNEDAS_H__
# define __ALIGNEDAS_H__

// Return the lowest integer 'X' so that 'X % B == 0' and 'S <= X'
# define alignedas(S, B) (((S) % (B) == 0) ? (S) : ((S) + (B) - ((S) % (B))))

# define is_alignedas(S, B) (((S) % (B)) == 0)

#endif /* __ALIGNEDAS_H__ */
