/* ************************************************************************** */
/*                                                                            */
/*   alignedas.h                                                              */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ALIGNEDAS_H__
# define __ALIGNEDAS_H__

// Return the lowest integer 'X' so that 'X % B == 0' and 'S <= X'
# define alignedas(S, B) (((S) % (B) == 0) ? (S) : ((S) + (B) - ((S) % (B))))

# define is_alignedas(S, B) (((S) % (B)) == 0)

#endif /* __ALIGNEDAS_H__ */
