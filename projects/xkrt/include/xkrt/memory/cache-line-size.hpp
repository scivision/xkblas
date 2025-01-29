/* ************************************************************************** */
/*                                                                            */
/*   cache-line-size.hpp                                                      */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
