/* ************************************************************************** */
/*                                                                            */
/*   dependency-domain.hpp                                                    */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/01 23:37:45 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEPENDENCY_DOMAIN_HPP__
# define __DEPENDENCY_DOMAIN_HPP__

# include <xkrt/memory/access.hpp>

class DependencyDomain
{
    public:
        virtual void resolve(access_t * access, int naccesses) = 0;
        virtual bool can_resolve(const access_t * access) const = 0;
};

#endif /* __DEPENDENCY_DOMAIN_HPP__ */
