/* ************************************************************************** */
/*                                                                            */
/*   alignas.h                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/05 16:28:42 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:04:04 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __ALIGNAS_H__
# define __ALIGNAS_H__

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
// 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

#endif /* __ALIGNAS_H__ */
