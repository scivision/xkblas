/* ************************************************************************** */
/*                                                                            */
/*   dependency-tree.hpp                                                      */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/11 22:17:10 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

// OpenMP 6.0-alike dependencies

#ifndef __DEPENDENCY_MAP_HPP__
# define __DEPENDENCY_MAP_HPP__

# include <xkrt/memory/access/dependency-domain.hpp>
# include <xkrt/task/task.hpp>

# include <vector>
# include <unordered_map>

class DependencyMap : public DependencyDomain
{

    class Node {

        public:

            std::vector<access_t *> last_seq_reads;
            access_t *  last_seq_write;

        public:

            Node(
            ) :
                last_seq_reads(8),
                last_seq_write()
            {
                last_seq_reads.clear();
            }

            ~Node() {}

    }; /* Node */

     private:
        std::unordered_map<const void *, Node> map;

     public:
        DependencyMap(const int n = 4096) : map()
        {
            if (n)
                map.reserve(n);
        }

        ~DependencyMap() {}

    public:

        inline void
        link(access_t * access)
        {
            assert(this->can_resolve(access));

            // retrieve previous accesses on that point
            auto it = map.find(access->point);

            // if none, no dependencies, return
            if (it == map.end())
                return ;

            // else
            const Node & node = it->second;

            // the generated task is dependent of previous 'reads'
            if ((access->mode & ACCESS_MODE_W) & node.last_seq_reads.size())
            {
                for (access_t * read : node.last_seq_reads)
                    __access_precedes(read, access);

            }
            else if (node.last_seq_write)
            {
                __access_precedes(node.last_seq_write, access);
            }
        }

        inline void
        put(access_t * access)
        {
            assert(this->can_resolve(access));

            // TODO : redundancy check, if we allow redundant dependencies - see
            // https://github.com/cea-hpc/mpc/blob/master/src/MPC_OpenMP/src/mpcomp_task.c#L1274

            // ensure a node exists on that address
            auto result = map.insert({access->point, Node()});
            if (result.second)
            {
                // node got inserted
            }
            else
            {
                // node already existed
            }

            Node & node = result.first->second;
            if (access->mode & ACCESS_MODE_W)
            {
                node.last_seq_reads.clear();
                node.last_seq_write = access;
            }
            else if (access->mode == ACCESS_MODE_R)
                node.last_seq_reads.push_back(access);
        }

        bool
        can_resolve(const access_t * access) const
        {
            assert(access);
            return (access->type == ACCESS_TYPE_POINT);
        }
};

#endif /* __DEPENDENCY_MAP_HPP__ */
