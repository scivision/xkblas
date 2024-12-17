/* ************************************************************************** */
/*                                                                            */
/*   demangle.hpp                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEMANGLE_HPP__
# define __DEMANGLE_HPP__

# include <cxxabi.h>
# include <cstdlib>
# include <memory>
# include <string>

template<class T>
static std::string
demangle(T & t)
{
    auto ptr = std::unique_ptr<char, decltype(& std::free)>{
        abi::__cxa_demangle(typeid(t).name(), nullptr, nullptr, nullptr),
            std::free
    };
    return {ptr.get()};
}


#endif /* __DEMANGLE_HPP__ */
