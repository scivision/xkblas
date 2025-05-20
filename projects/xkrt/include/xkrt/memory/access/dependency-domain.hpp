/* ************************************************************************** */
/*                                                                            */
/*   dependency-domain.hpp                                                    */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 04:54:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEPENDENCY_DOMAIN_HPP__
# define __DEPENDENCY_DOMAIN_HPP__

# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/access/access.hpp>

class DependencyDomain
{
    public:

        virtual ~DependencyDomain() {}

    public:

        // return true if the dependency domain can resolve
        virtual bool can_resolve(const access_t * access) const = 0;

        // set edges with previous accesses
        virtual void link(access_t * access) = 0;

        // insert access so future accesses intersection
        virtual void put(access_t * access) = 0;

    public:

        template<int AC>
        inline void
        link(access_t * accesses)
        {
            for (int i = 0 ; i < AC ; ++i)
                this->link(accesses + i);
        }

        template<int AC>
        inline void
        put(access_t * accesses)
        {
            for (int i = 0 ; i < AC ; ++i)
                this->put(accesses + i);
        }

        template<int AC>
        inline void
        resolve(access_t * accesses)
        {
            # pragma message(TODO "If we semantically force a accesses region to be disjoint, then these 2 loops can be merged with no risks of dependency cycle")
            this->link<AC>(accesses);
            this->put<AC>(accesses);
        }
};

#endif /* __DEPENDENCY_DOMAIN_HPP__ */
