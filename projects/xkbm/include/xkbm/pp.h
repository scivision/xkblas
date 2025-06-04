/* ************************************************************************** */
/*                                                                            */
/*   pp.h                                                         .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/05 16:28:42 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:37:30 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKBM_PP__
#  define __XKBM_PP__

void pp_1zu_1time(size_t i, size_t avg, size_t stdev);
void pp_1byte_1time(size_t i, size_t avg, size_t stdev);
void pp_1zu_1bw(size_t i, size_t avg, size_t stdev);

# endif /* __XKBM_PP__ */
