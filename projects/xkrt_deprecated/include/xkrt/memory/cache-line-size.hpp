/* ************************************************************************** */
/*                                                                            */
/*   cache-line-size.hpp                                          .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/16 17:16:12 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:04:20 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# if !defined(CACHE_LINE_SIZE)
#  if __cpp_lib_hardware_interference_size >= 201603
#   include <new>
#   define CACHE_LINE_SIZE std::hardware_constructive_interference_size
#  else
#   define CACHE_LINE_SIZE 64
#  endif
# endif
