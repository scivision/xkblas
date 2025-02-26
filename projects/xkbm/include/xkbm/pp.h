/* ************************************************************************** */
/*                                                                            */
/*   pp.h                                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/26 16:51:02 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/26 16:55:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKBM_PP__
#  define __XKBM_PP__

void pp_1zu_1time(size_t i, size_t avg, size_t stdev);
void pp_1byte_1time(size_t i, size_t avg, size_t stdev);
void pp_1zu_1bw(size_t i, size_t avg, size_t stdev);

# endif /* __XKBM_PP__ */
