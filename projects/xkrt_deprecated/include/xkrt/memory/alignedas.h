/* ************************************************************************** */
/*                                                                            */
/*   alignedas.h                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/15 17:01:38 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:04:08 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ALIGNEDAS_H__
# define __ALIGNEDAS_H__

// Return the lowest integer 'X' so that 'X % B == 0' and 'S <= X'
# define alignedas(S, B) (((S) % (B) == 0) ? (S) : ((S) + (B) - ((S) % (B))))
# define is_alignedas(S, B) (((S) % (B)) == 0)

#endif /* __ALIGNEDAS_H__ */
