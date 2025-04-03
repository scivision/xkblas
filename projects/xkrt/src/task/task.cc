/* ************************************************************************** */
/*                                                                            */
/*   task.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/04/03 01:40:27 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 01:42:32 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/task/dependency-tree.hpp>
# include <xkrt/task/task.hpp>

/**
 * Retrieve or (insert and return) the dependency domain of the passed access
 * on the currently executing task
 */
DependencyDomain *
task_get_dependency_domain(task_t * task, const access_t * access)
{
    assert(task->flags & TASK_FLAG_DOMAIN);

    task_dom_info_t * dom = TASK_DOM_INFO(task);
    assert(dom);

    /* find previous deptree for that ld */
    for (DependencyDomain * domain : dom->domains)
        if (domain->can_resolve(access))
            return domain;

    /* create a new domain */
    DependencyDomain * domain = new DependencyTree(access->host_view.ld, access->host_view.sizeof_type);
    dom->domains.push_back(domain);

    return domain;
}

